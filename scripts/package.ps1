# Builds a portable Release package for ALTRun.
#
# Usage:
#   pwsh -File scripts/package.ps1                # build + package
#   pwsh -File scripts/package.ps1 -SkipBuild     # package existing build
#   pwsh -File scripts/package.ps1 -Version 2.0.1
#
# Output:
#   dist/ALTRun-<version>-win64/   (portable folder: exe + res + docs)
#   dist/ALTRun-<version>-win64.zip

[CmdletBinding()]
param(
    [string]$Version = '2.0.5',
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [switch]$SkipBuild,
    [string]$CMakePath = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    # --- locate CMake -----------------------------------------------------
    if (-not $CMakePath) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path $vswhere) {
            $vsPath = & $vswhere -latest -products * -property installationPath
            if ($vsPath) {
                $candidate = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
                if (Test-Path $candidate) { $CMakePath = $candidate }
            }
        }
    }
    if (-not $CMakePath) {
        $cmd = Get-Command cmake -ErrorAction SilentlyContinue
        if ($cmd) { $CMakePath = $cmd.Source }
    }
    if (-not $CMakePath) {
        throw 'CMake not found. Pass -CMakePath explicitly or install CMake (Visual Studio ships one).'
    }

    # --- build ------------------------------------------------------------
    if (-not $SkipBuild) {
        Write-Host "==> Configuring (Visual Studio 18 2026, x64)" -ForegroundColor Cyan
        & $CMakePath -S . -B build/vs -G 'Visual Studio 18 2026' -A x64 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }

        Write-Host "==> Building $Configuration" -ForegroundColor Cyan
        & $CMakePath --build build/vs --config $Configuration | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
    }

    $buildDir = Join-Path $repoRoot "build/vs/$Configuration"
    $exe = Join-Path $buildDir 'ALTRun.exe'
    if (-not (Test-Path $exe)) {
        throw "ALTRun.exe not found at $exe (build first or drop -SkipBuild)"
    }

    # --- stage ------------------------------------------------------------
    $distRoot = Join-Path $repoRoot 'dist'
    $stageName = "ALTRun-$Version-win64"
    $stage = Join-Path $distRoot $stageName
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    Write-Host "==> Staging portable folder" -ForegroundColor Cyan
    Copy-Item $exe $stage
    if (Test-Path (Join-Path $buildDir 'res')) {
        Copy-Item (Join-Path $buildDir 'res') (Join-Path $stage 'res') -Recurse
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
    Write-Host "==> Compressing" -ForegroundColor Cyan
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal

    $sizeMb = [math]::Round((Get-Item $zip).Length / 1MB, 2)
    Write-Host ""
    Write-Host "Package ready:" -ForegroundColor Green
    Write-Host "  folder: $stage"
    Write-Host "  zip   : $zip  ($sizeMb MB)"
} finally {
    Pop-Location
}
