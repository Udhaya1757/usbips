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

$sources = @(
    (Join-Path $srcDir "main.cpp"),
    (Join-Path $srcDir "usb/USBMonitor.cpp"),
    (Join-Path $srcDir "device/DeviceInfoExtractor.cpp")
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
