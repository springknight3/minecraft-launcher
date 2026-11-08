[CmdletBinding()]
param(
    [string]$SourceDir = "$PSScriptRoot\..",
    [string]$BuildDir = "$PSScriptRoot\..\build\windows-release",
    [string]$OutputDir = "$PSScriptRoot\..\dist"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Write-Host "=== StrataLauncher Windows Build Script ===" -ForegroundColor Cyan

# Resolve absolute paths
$SourceDir = (Resolve-Path $SourceDir).Path
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}
$BuildDir = (Resolve-Path $BuildDir).Path

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}
$OutputDir = (Resolve-Path $OutputDir).Path

Write-Host "Source directory: $SourceDir"
Write-Host "Build directory:  $BuildDir"
Write-Host "Output directory: $OutputDir"

# Verify required tools
foreach ($cmd in @('cmake', 'cpack')) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        throw "Required tool '$cmd' not found in PATH."
    }
}

if (-not (Get-Command makensis -ErrorAction SilentlyContinue)) {
    Write-Warning "'makensis' was not found in PATH. Make sure NSIS is installed if building the installer package."
}

# Read version from CMakeLists.txt
$cmakeLists = Get-Content (Join-Path $SourceDir "CMakeLists.txt") -Raw
if ($cmakeLists -match 'project\(StrataLauncher\s+VERSION\s+([0-9\.]+)') {
    $version = $Matches[1]
    Write-Host "Detected project version: $version" -ForegroundColor Green
} else {
    throw "Could not extract version from CMakeLists.txt"
}

# Configure
Write-Host "`n--> Configuring CMake (Release)..." -ForegroundColor Yellow
& cmake -S "$SourceDir" -B "$BuildDir" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE"
}

# Build
Write-Host "`n--> Building StrataLauncher..." -ForegroundColor Yellow
& cmake --build "$BuildDir" --config Release --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

# Package installer with CPack NSIS
Write-Host "`n--> Creating NSIS installer with CPack..." -ForegroundColor Yellow
$packageTemp = Join-Path $BuildDir "installer-stage"
if (Test-Path $packageTemp) {
    Remove-Item -Recurse -Force $packageTemp
}
New-Item -ItemType Directory -Path $packageTemp -Force | Out-Null

& cpack --config (Join-Path $BuildDir "CPackConfig.cmake") -G NSIS -C Release -B "$packageTemp"
if ($LASTEXITCODE -ne 0) {
    throw "CPack packaging failed with exit code $LASTEXITCODE"
}

$installer = Get-ChildItem -Path $packageTemp -Filter "*.exe" -File | Select-Object -First 1
if (-not $installer) {
    throw "CPack did not produce an .exe installer in $packageTemp"
}

$destName = "stratalauncher-$version-windows-x86_64.exe"
$destPath = Join-Path $OutputDir $destName
Copy-Item -LiteralPath $installer.FullName -Destination $destPath -Force
Write-Host "Installer created: $destPath" -ForegroundColor Green

# Compute SHA256 checksum
$hash = (Get-FileHash -LiteralPath $destPath -Algorithm SHA256).Hash.ToLower()
$shaFile = "$destPath.sha256"
"$hash  $destName" | Out-File -FilePath $shaFile -Encoding utf8
Write-Host "SHA256: $hash" -ForegroundColor Green

Write-Host "`n=== Build completed successfully! ===" -ForegroundColor Cyan
