param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$Run,
    [switch]$Tests,
    [switch]$UiTests,
    [switch]$RestoreOnly
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw '请先安装 Visual Studio C++ 桌面生成工具和 Windows SDK。' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw '未找到 MSVC C++ 工具链。' }
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
$toolset = if (Test-Path -LiteralPath (Join-Path $installation 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.v145.default.txt')) { 'v145' } else { 'v143' }
$toolsDir = Join-Path $projectRoot '.tools'
New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
$nuget = Join-Path $toolsDir 'nuget.exe'
if (-not (Test-Path -LiteralPath $nuget)) {
    Write-Host 'Downloading NuGet CLI...'
    Invoke-WebRequest 'https://dist.nuget.org/win-x86-commandline/v6.14.0/nuget.exe' -OutFile $nuget
}
& $nuget restore (Join-Path $projectRoot 'packages.config') -PackagesDirectory (Join-Path $projectRoot 'packages') -ConfigFile (Join-Path $projectRoot 'NuGet.config') -NonInteractive -Verbosity quiet
if ($LASTEXITCODE -ne 0) { throw 'NuGet restore failed.' }
if ($RestoreOnly) { return }
& $msbuild (Join-Path $projectRoot 'CompIntel3.vcxproj') /m /nologo /v:minimal "/p:Configuration=$Configuration" /p:Platform=x64 "/p:PlatformToolset=$toolset"
if ($LASTEXITCODE -ne 0) { throw 'CompIntel3 build failed.' }
if ($Tests) {
    $testBuild = Join-Path $projectRoot 'build\tests'
    $vsMajor = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion).Split('.')[0]
    $generator = if ($vsMajor -eq '18') { 'Visual Studio 18 2026' } else { 'Visual Studio 17 2022' }
    & cmake -S $projectRoot -B $testBuild -G $generator -A x64 "-T$toolset" "-DCMAKE_GENERATOR_INSTANCE=$installation"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & cmake --build $testBuild --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }
    & ctest --test-dir $testBuild -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
}
if ($UiTests) {
    & $msbuild (Join-Path $projectRoot 'CompIntel3.vcxproj') /m /nologo /v:minimal "/p:Configuration=$Configuration" /p:Platform=x64 "/p:PlatformToolset=$toolset" /p:UiSmokeTests=true
    if ($LASTEXITCODE -ne 0) { throw 'WinUI integration test build failed.' }
    $uiTestExecutable = Join-Path $projectRoot "artifacts\x64\$Configuration.UiTests\CompIntel3.UiTests.exe"
    $uiTestProcess = Start-Process -FilePath $uiTestExecutable -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $uiTestDeadline = [DateTime]::UtcNow.AddSeconds(120)
    while (-not $uiTestProcess.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -ge $uiTestDeadline) {
            $uiTestProcess.Kill()
            throw 'WinUI integration tests exceeded 120 seconds.'
        }
    }
    $uiTestLog = Join-Path $projectRoot 'artifacts\ui-smoke.log'
    if (Test-Path -LiteralPath $uiTestLog) { Get-Content -LiteralPath $uiTestLog -Tail 15 }
    if ($uiTestProcess.ExitCode -ne 0) { throw "WinUI integration tests failed ($($uiTestProcess.ExitCode)). See artifacts\ui-smoke.log." }
}
if ($Run) {
    Start-Process -FilePath (Join-Path $projectRoot "artifacts\x64\$Configuration\CompIntel3.exe") -WorkingDirectory $projectRoot
}
