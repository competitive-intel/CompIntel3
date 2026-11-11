#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [switch]$PortableOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$packageRoot = (Resolve-Path -LiteralPath $PackageDirectory).Path
$checksumPath = Join-Path $packageRoot 'SHA256SUMS.txt'
$archives = @(Get-ChildItem -LiteralPath $packageRoot -Filter '*.zip' -File)
if ($archives.Count -ne 1) { throw 'Expected exactly one portable ZIP in the package directory.' }
$checksums = @(Get-Content -LiteralPath $checksumPath)
if ($checksums.Count -ne $(if ($PortableOnly) { 1 } else { 2 })) { throw 'Unexpected number of distribution checksums.' }
foreach ($line in $checksums) {
    if ($line -notmatch '^([0-9a-f]{64}) \*([^/\\]+)$') { throw "Invalid checksum entry: $line" }
    $actual = (Get-FileHash -LiteralPath (Join-Path $packageRoot $Matches[2]) -Algorithm SHA256).Hash
    if ($actual -ne $Matches[1]) { throw "Checksum mismatch: $($Matches[2])" }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archives[0].FullName)
try {
    $names = @($zip.Entries.FullName | ForEach-Object { $_.Replace('\', '/') })
    $prefix = $archives[0].BaseName + '/'
    foreach ($required in @('CompIntel3.exe', 'resources.pri', 'Microsoft.ui.xaml.dll', 'Microsoft.UI.Xaml.Controls.dll',
        'Microsoft.WindowsAppRuntime.dll', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll',
        'build-info.json', 'START-HERE.txt', 'COMPINTEL_LICENSE.txt', 'docs/usage.md', 'examples/gomoku.cpp')) {
        if (($prefix + $required) -notin $names) { throw "Portable ZIP is missing $required" }
    }
    if ($names | Where-Object { $_ -match '\.(pdb|ilk|obj|lib|exp)$' }) { throw 'Debug or build products leaked into the package.' }
    if (-not ($names | Where-Object { $_ -like "${prefix}ThirdPartyNotices/*" })) { throw 'Third-party license notices are missing.' }
    $metadataEntry = $zip.Entries | Where-Object { $_.FullName.Replace('\', '/') -eq ($prefix + 'build-info.json') }
    $reader = [IO.StreamReader]::new($metadataEntry.Open())
    try { $metadata = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    if ($metadata.commit -notmatch '^[0-9a-f]{40}$' -or $metadata.architecture -ne 'x64') { throw 'Invalid source provenance in the package.' }
    if ($archives[0].BaseName -ne "CompIntel3-$($metadata.version)-windows-x64") { throw 'Package version does not match build metadata.' }
} finally { $zip.Dispose() }
if (-not $PortableOnly) {
    $installer = Join-Path $packageRoot "CompIntel3-$($metadata.version)-Setup-x64.exe"
    $stream = [IO.File]::OpenRead($installer)
    try {
        if ($stream.ReadByte() -ne 0x4D -or $stream.ReadByte() -ne 0x5A) { throw 'Installer is not a Windows executable.' }
    } finally { $stream.Dispose() }
    # Windows file versions use four numeric components. CI/prerelease suffixes
    # remain in the package filename, AppVersion and build-info.json.
    $versionInfo = (Get-Item -LiteralPath $installer).VersionInfo
    $expectedVersion = [version](($metadata.version -split '-')[0] + '.0')
    $actualVersion = [version]::new($versionInfo.FileMajorPart, $versionInfo.FileMinorPart,
        $versionInfo.FileBuildPart, $versionInfo.FilePrivatePart)
    Write-Host "Installer version: $actualVersion; package version: $($metadata.version)"
    if ($actualVersion -ne $expectedVersion) {
        throw "Installer file version $actualVersion does not match package version $expectedVersion."
    }
}
Write-Host 'PASS distribution checksums, runtime dependencies, notices, source provenance and package versions'
