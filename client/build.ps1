# ============================================================
# build.ps1 — USBIPS Client Build Script
# ============================================================

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$srcDir = Join-Path $scriptDir "src"
$outputExe = Join-Path $scriptDir "usbips_client.exe"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Building USBIPS Client..." -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$sqliteC = Join-Path $srcDir "database/sqlite3.c"
$sqliteObj = Join-Path $srcDir "database/sqlite3.o"

# Compile SQLite C amalgamation if object file is missing or outdated
if (-not (Test-Path $sqliteObj) -or ((Get-Item $sqliteC).LastWriteTime -gt (Get-Item $sqliteObj).LastWriteTime)) {
    Write-Host "Compiling SQLite amalgamation (C)..." -ForegroundColor Yellow
    $compileSqlite = "gcc -c `"$sqliteC`" -o `"$sqliteObj`" -O2"
    Invoke-Expression $compileSqlite
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Failed to compile sqlite3.c" -ForegroundColor Red
        exit 1
    }
}

$sources = @(
    (Join-Path $srcDir "main.cpp"),
    (Join-Path $srcDir "usb/USBMonitor.cpp"),
    (Join-Path $srcDir "device/DeviceInfoExtractor.cpp"),
    (Join-Path $srcDir "classifier/DeviceClassifier.cpp"),
    (Join-Path $srcDir "database/LocalDatabase.cpp"),
    (Join-Path $srcDir "access/AccessController.cpp"),
    "`"$sqliteObj`""
)

$libs = @(
    "-lsetupapi",
    "-lcfgmgr32",
    "-luser32",
    "-lkernel32"
)

$cmd = "g++ -std=c++17 -DUNICODE -D_UNICODE $($sources -join ' ') -o `"$outputExe`" $($libs -join ' ')"
Write-Host "Running: $cmd" -ForegroundColor Gray

Invoke-Expression $cmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n[SUCCESS] Built successfully: $outputExe" -ForegroundColor Green
} else {
    Write-Host "`n[ERROR] Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
}
