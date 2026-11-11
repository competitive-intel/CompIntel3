#requires -Version 7.0
param(
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$')]
    [string]$Version = '0.1.0-local',
    [string]$InnoSetupCompiler,
    [switch]$SkipInstaller
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
$releaseDirectory = Join-Path $projectRoot 'artifacts\x64\Release'
$outputDirectory = Join-Path $projectRoot "artifacts\packages\$Version"
$packageName = "CompIntel3-$Version-windows-x64"
$stagingDirectory = Join-Path $projectRoot "artifacts\package-staging\$([guid]::NewGuid())"
$payloadDirectory = Join-Path $stagingDirectory $packageName
$numericParts = ($Version -split '-')[0].Split('.')
foreach ($part in $numericParts) {
    if ([long]$part -gt 65535) { throw 'Version components must be between 0 and 65535.' }
}
$numericVersion = ($numericParts -join '.') + '.0'

if (-not (Test-Path -LiteralPath (Join-Path $releaseDirectory 'CompIntel3.exe'))) {
    throw 'Release output is missing. Run scripts/build.ps1 -Configuration Release first.'
}
if (-not $SkipInstaller) {
    if (-not $InnoSetupCompiler) {
        $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
        if ($command) { $InnoSetupCompiler = $command.Source }
        foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
            foreach ($edition in @('Inno Setup 6', 'Inno Setup 7')) {
                $candidate = Join-Path $base "$edition\ISCC.exe"
                if (-not $InnoSetupCompiler -and (Test-Path -LiteralPath $candidate)) {
                    $InnoSetupCompiler = $candidate
                }
            }
        }
    }
    if (-not $InnoSetupCompiler -or -not (Test-Path -LiteralPath $InnoSetupCompiler)) {
        throw 'Inno Setup 6.3+ is required for Setup.exe. Use -SkipInstaller to create only the portable ZIP.'
    }
}

New-Item -ItemType Directory -Path $payloadDirectory, $outputDirectory -Force | Out-Null
# Preserve every runtime subdirectory and PRI file; exclude only build/debug products.
foreach ($file in Get-ChildItem -LiteralPath $releaseDirectory -File -Recurse) {
    if ($file.Extension -in @('.pdb', '.ilk', '.lib', '.exp', '.obj', '.log')) { continue }
    $relative = [IO.Path]::GetRelativePath($releaseDirectory, $file.FullName)
    $destination = Join-Path $payloadDirectory $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination
}

# App-local CRT deployment makes the installer and portable ZIP usable without
# requiring a separate Visual C++ Redistributable installation.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installation) { throw 'Could not find the Visual Studio C++ redistributable directory.' }
$redistRoot = Join-Path $installation 'VC\Redist\MSVC'
$redistVersions = @(Get-ChildItem -LiteralPath $redistRoot -Directory |
    Where-Object Name -Match '^\d+\.\d+\.\d+$' | Sort-Object { [version]$_.Name } -Descending)
$crtDirectory = $null
foreach ($redistVersion in $redistVersions) {
    $candidates = @(Get-ChildItem -Path (Join-Path $redistVersion.FullName 'x64\Microsoft.VC*.CRT') -Directory)
    if ($candidates.Count -gt 0) { $crtDirectory = $candidates[0].FullName; break }
}
if (-not $crtDirectory) { throw 'No x64 release CRT redistributable files were found.' }
Get-ChildItem -LiteralPath $crtDirectory -Filter '*.dll' -File |
    Copy-Item -Destination $payloadDirectory

Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md'), (Join-Path $projectRoot 'COMPINTEL_LICENSE.txt') -Destination $payloadDirectory
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs'), (Join-Path $projectRoot 'examples') -Destination $payloadDirectory -Recurse
[xml]$packages = Get-Content -LiteralPath (Join-Path $projectRoot 'packages.config') -Raw
foreach ($package in $packages.packages.package) {
    $packageId = "$($package.id).$($package.version)"
    $packageDirectory = Join-Path $projectRoot "packages\$packageId"
    $notices = @(Get-ChildItem -LiteralPath $packageDirectory -File | Where-Object Name -Match 'license|notice|copying')
    if ($notices.Count -gt 0) {
        $noticeDirectory = Join-Path $payloadDirectory "ThirdPartyNotices\$packageId"
        New-Item -ItemType Directory -Path $noticeDirectory -Force | Out-Null
        $notices | Copy-Item -Destination $noticeDirectory
    }
}
$commit = & git -C $projectRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Could not determine the source commit.' }
[ordered]@{
    application = 'CompIntel3'
    version = $Version
    architecture = 'x64'
    commit = $commit
    builtAtUtc = [DateTime]::UtcNow.ToString('o')
    runtimeDeployment = 'Windows App SDK self-contained; app-local MSVC CRT'
    packages = @($packages.packages.package | ForEach-Object { "$($_.id)/$($_.version)" })
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $payloadDirectory 'build-info.json') -Encoding utf8
@'
CompIntel3 for Windows x64

Extract the entire portable ZIP, then run CompIntel3.exe from the extracted folder.
Keep all DLL, PRI and runtime subdirectories next to the application.
Windows 10 version 2004 (19041) or later is required.
The Windows App SDK and Visual C++ runtime files are included.
Built-in opponents work without a compiler. Compiling your own Player requires
MSVC, clang++ or g++ with C++20 support.
Your drafts, program versions and replays are stored in %LOCALAPPDATA%\CompIntel3.
See docs/usage.md and docs/protocol.md for instructions.
'@ | Set-Content -LiteralPath (Join-Path $payloadDirectory 'START-HERE.txt') -Encoding utf8

foreach ($required in @('CompIntel3.exe', 'resources.pri', 'Microsoft.ui.xaml.dll', 'Microsoft.UI.Xaml.Controls.dll',
    'Microsoft.WindowsAppRuntime.dll', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $payloadDirectory $required))) { throw "Package is missing $required" }
}
$archivePath = Join-Path $outputDirectory "$packageName.zip"
Compress-Archive -LiteralPath $payloadDirectory -DestinationPath $archivePath -CompressionLevel Optimal -Force
$distributionFiles = @($archivePath)
if (-not $SkipInstaller) {
    & $InnoSetupCompiler "/DPackageSource=$payloadDirectory" "/DPackageOutput=$outputDirectory" "/DPackageVersion=$Version" "/DPackageNumericVersion=$numericVersion" (Join-Path $projectRoot 'packaging\CompIntel3.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compilation failed.' }
    $installerPath = Join-Path $outputDirectory "CompIntel3-$Version-Setup-x64.exe"
    if (-not (Test-Path -LiteralPath $installerPath)) { throw 'Inno Setup did not produce the expected installer.' }
    $distributionFiles += $installerPath
}
$checksums = foreach ($file in $distributionFiles) {
    $hash = Get-FileHash -LiteralPath $file -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant()) *$([IO.Path]::GetFileName($file))"
}
$checksums | Set-Content -LiteralPath (Join-Path $outputDirectory 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Packages created in $outputDirectory"
$distributionFiles | ForEach-Object { Get-Item -LiteralPath $_ } | Select-Object Name, Length
