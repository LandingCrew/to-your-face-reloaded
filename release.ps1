# To Your Face Reloaded - Release Script
# Usage: .\release.ps1 [-Version "1.0.0"] [-SkipBuild] [-IncludePDB]

param(
    [string]$Version = "",
    [switch]$SkipBuild,
    [switch]$IncludePDB
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot

# Get version from vcpkg.json if not specified
if (-not $Version) {
    $vcpkg = Get-Content "$ProjectRoot/vcpkg.json" | ConvertFrom-Json
    $Version = $vcpkg."version-string"
}

Write-Host "Building To Your Face Reloaded v$Version" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Build Release configuration
if (-not $SkipBuild) {
    Write-Host "`n[1/4] Building Release configuration..." -ForegroundColor Yellow
    cmake --build "$ProjectRoot/build" --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
    Write-Host "Build successful!" -ForegroundColor Green
} else {
    Write-Host "`n[1/4] Skipping build (using existing DLL)" -ForegroundColor Yellow
}

# Verify DLL exists
$dllPath = "$ProjectRoot/build/src/Release/to-your-face-reloaded.dll"
if (-not (Test-Path $dllPath)) {
    Write-Host "ERROR: DLL not found at $dllPath" -ForegroundColor Red
    Write-Host "Run without -SkipBuild to compile first." -ForegroundColor Red
    exit 1
}

# Create release directory structure
Write-Host "`n[2/4] Creating package structure..." -ForegroundColor Yellow
$releaseDir = "$ProjectRoot/release"
$pkgName = "ToYourFaceReloaded-$Version"
$pkgDir = "$releaseDir/$pkgName"

# Clean previous release
if (Test-Path $pkgDir) {
    Remove-Item -Recurse -Force $pkgDir
}

# Create directories
New-Item -ItemType Directory -Force -Path "$pkgDir/SKSE/Plugins" | Out-Null

Write-Host "Package directory: $pkgDir" -ForegroundColor Gray

# Copy DLL and config
Write-Host "`n[3/4] Copying plugin files..." -ForegroundColor Yellow
Copy-Item $dllPath "$pkgDir/SKSE/Plugins/"
Copy-Item "$ProjectRoot/config/to-your-face-reloaded.ini" "$pkgDir/SKSE/Plugins/"

if ($IncludePDB) {
    $pdbPath = "$ProjectRoot/build/src/Release/to-your-face-reloaded.pdb"
    if (Test-Path $pdbPath) {
        Copy-Item $pdbPath "$pkgDir/SKSE/Plugins/"
        Write-Host "  Included PDB for debugging" -ForegroundColor Gray
    }
}

# Create zip archive
Write-Host "`n[4/4] Creating zip archive..." -ForegroundColor Yellow
$zipPath = "$releaseDir/$pkgName.zip"
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
Compress-Archive -Path "$pkgDir/*" -DestinationPath $zipPath

# Summary
$zipSize = (Get-Item $zipPath).Length / 1MB
Write-Host "`n==========================================" -ForegroundColor Cyan
Write-Host "Release package created successfully!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  Version:  $Version"
Write-Host "  Package:  $zipPath"
Write-Host "  Size:     $([math]::Round($zipSize, 2)) MB"
Write-Host ""
Write-Host "Contents:" -ForegroundColor Yellow
Get-ChildItem -Recurse $pkgDir | Where-Object { -not $_.PSIsContainer } | ForEach-Object {
    $relativePath = $_.FullName.Replace("$pkgDir\", "")
    Write-Host "  $relativePath"
}
