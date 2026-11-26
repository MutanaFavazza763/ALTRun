# Builds a portable Release package for ALTRun.
#
# Usage:
#   pwsh -File scripts/package.ps1                # build + package
#   pwsh -File scripts/package.ps1 -SkipBuild     # package existing build
#   pwsh -File scripts/package.ps1 -Version 2.1.0
#
# Output:
#   dist/ALTRun-<version>-win64/   (portable folder: exe + res + docs)
#   dist/ALTRun-<version>-win64.zip

[CmdletBinding()]
param(
    [string]$Version = '2.1.0',
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [switch]$SkipBuild,
    [string]$CMakePath = '',
    [string]$BuildDir = 'build'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    # --- locate tools -----------------------------------------------------
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vsPath = ''
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -property installationPath
    }
    if (-not $CMakePath) {
        if ($vsPath) {
            $candidate = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path $candidate) { $CMakePath = $candidate }
        }
    }
    if (-not $CMakePath) {
        $cmd = Get-Command cmake -ErrorAction SilentlyContinue
        if ($cmd) { $CMakePath = $cmd.Source }
    }
    $msbuild = ''
    if ($vsPath) {
        $candidate = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
        if (Test-Path $candidate) { $msbuild = $candidate }
    }

    $buildRoot = Join-Path $repoRoot $BuildDir
    $buildDir = Join-Path $buildRoot $Configuration
    if (-not (Test-Path $buildRoot)) { New-Item -ItemType Directory -Path $buildRoot | Out-Null }

    # --- build ------------------------------------------------------------
    if (-not $SkipBuild) {
        if (-not (Test-Path (Join-Path $buildRoot 'CMakeCache.txt'))) {
            if (-not $CMakePath) { throw 'CMake not found. Install CMake or pass -CMakePath.' }
            Write-Host '==> Configuring (Visual Studio 18 2026, x64)' -ForegroundColor Cyan
            & $CMakePath -S . -B $BuildDir -G 'Visual Studio 18 2026' -A x64 | Out-Host
            if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
        }

        Write-Host "==> Building main program ($Configuration)" -ForegroundColor Cyan
        if (-not $CMakePath) { throw 'CMake not found. Install CMake or pass -CMakePath.' }
        & $CMakePath --build $BuildDir --config $Configuration --target ALTRun | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "Main build failed ($LASTEXITCODE)" }

        Write-Host "==> Building WinUI settings ($Configuration)" -ForegroundColor Cyan
        if (-not $msbuild) { throw 'MSBuild not found in the Visual Studio installation.' }
        & $msbuild 'src/settings/ALTRun.Settings.vcxproj' `
            "/p:Configuration=$Configuration" '/p:Platform=x64' '/m' '/v:minimal' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "Settings build failed ($LASTEXITCODE)" }
    }

    $exe = Join-Path $buildDir 'ALTRun.exe'
    if (-not (Test-Path $exe)) {
        throw "ALTRun.exe not found at $exe (build first or drop -SkipBuild)"
    }
    $settingsDir = Join-Path $buildRoot "settings/x64/$Configuration"
    $settingsExe = Join-Path $settingsDir 'ALTRun.Settings.exe'
    if (-not (Test-Path $settingsExe)) {
        throw "ALTRun.Settings.exe not found at $settingsExe (build the settings project first)"
    }

    # --- stage ------------------------------------------------------------
    $distRoot = Join-Path $repoRoot 'dist'
    $stageName = "ALTRun-$Version-win64"
    $stage = Join-Path $distRoot $stageName
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    Write-Host '==> Staging portable folder' -ForegroundColor Cyan
    Copy-Item $exe $stage
    if (Test-Path (Join-Path $buildDir 'res')) {
        Copy-Item (Join-Path $buildDir 'res') (Join-Path $stage 'res') -Recurse
    }

    # The WinUI settings executable is framework-dependent: keep the
    # bootstrap DLL, compiled XAML and PRI next to it, but leave the huge pdb
    # and the self-contained Windows App SDK payload behind.
    $settingsFiles = @(
        'ALTRun.Settings.exe',
        'ALTRun.Settings.pri',
        'ALTRunSettings.winmd',
        'Microsoft.WindowsAppRuntime.Bootstrap.dll',
        'App.xbf',
        'MainWindow.xbf',
        'Microsoft.Web.WebView2.Core.dll',
        'Microsoft.Web.WebView2.Core.winmd'
    )
    foreach ($name in $settingsFiles) {
        $source = Join-Path $settingsDir $name
        if (Test-Path $source) { Copy-Item $source $stage }
    }

    $requiredSettings = @('ALTRun.Settings.exe', 'ALTRun.Settings.pri',
                          'ALTRunSettings.winmd', 'Microsoft.WindowsAppRuntime.Bootstrap.dll',
                          'App.xbf', 'MainWindow.xbf')
    foreach ($name in $requiredSettings) {
        if (-not (Test-Path (Join-Path $stage $name))) {
            throw "Required WinUI settings file missing: $name"
        }
    }

    Copy-Item 'README.md' $stage
    Copy-Item 'docs/USAGE.md' $stage
    Copy-Item 'docs/ARCHITECTURE.md' $stage
    Copy-Item 'docs/ROADMAP.md' $stage

    # Sample settings for reference only. Keep it as .sample: an actual
    # ALTRun.ini would make Settings::Load() treat the folder as an existing
    # install and suppress the first-run guide.
    $ini = @'
[Config]
HotKey1=Alt + R
HotKey2=Pause
LastItemHotKey=ALT+L
AutoRun=0
AddToSendTo=1
Regex=1
HideDelay=15
MatchAnywhere=1
NumberKey=1
IndexFrom0to9=0
RememberFavouratMatch=0
ParamHistoryLimit=50
ShowOperationHint=1
ShowCommandLine=1
ShowStartNotification=1
ShowTopTen=1
PlayPopupNotify=1
ExitWhenExecute=0
ShowSkin=1
ShowMeWhenStart=0
ShowTrayIcon=1
ExecuteIfOnlyOne=0
[GUI]
FormWidth=420
Alpha=240
RoundBorderRadius=12
ListFormat=%-25s| %s
BGFileName=BG.jpg
Lang=en_us
Theme=system
TitleFont=Segoe UI|[]|12|65535|0
KeywordFont=Segoe UI|[]|16|16777215|0
ListFont=Segoe UI|[]|14|16777215|0
[DEBUG]
TraceEnable=0
ShortCutRunCount=0
LatestList=
'@
    Set-Content -Path (Join-Path $stage 'ALTRun.ini.sample') -Value $ini -Encoding Default

    # --- zip --------------------------------------------------------------
    $zip = Join-Path $distRoot "$stageName.zip"
    if (Test-Path $zip) { Remove-Item -Force $zip }
    Write-Host '==> Compressing' -ForegroundColor Cyan
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal

    $sizeMb = [math]::Round((Get-Item $zip).Length / 1MB, 2)
    Write-Host ''
    Write-Host 'Package ready:' -ForegroundColor Green
    Write-Host "  folder: $stage"
    Write-Host "  zip   : $zip  ($sizeMb MB)"
} finally {
    Pop-Location
}
