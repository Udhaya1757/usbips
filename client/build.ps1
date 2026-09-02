# ============================================================
# build.ps1 — USBIPS Client Build Script
# ============================================================

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$srcDir = Join-Path $scriptDir "src"
$outputExe = Join-Path $scriptDir "usbips_client.exe"
$clExe = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe"
$vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat"
$vcpkgRoot = "C:\vcpkg\installed\x64-windows"
$includeDir = Join-Path $vcpkgRoot "include"
$libDir = Join-Path $vcpkgRoot "lib"
$binDir = Join-Path $vcpkgRoot "bin"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Building USBIPS Client..." -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$sqliteC = Join-Path $srcDir "database/sqlite3.c"
$sqliteObj = Join-Path $srcDir "database/sqlite3.obj"

if (-not (Test-Path $clExe)) {
    Write-Host "[ERROR] MSVC compiler not found: $clExe" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $vsDevCmd)) {
    Write-Host "[ERROR] Visual Studio environment script not found: $vsDevCmd" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path (Join-Path $libDir "libcurl.lib"))) {
    Write-Host "[ERROR] vcpkg libcurl library not found: $(Join-Path $libDir "libcurl.lib")" -ForegroundColor Red
    exit 1
}

# Import the x64 MSVC environment so this script also works from ordinary PowerShell.
$msvcEnvironment = & cmd.exe /d /s /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && set"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Failed to initialize the x64 MSVC environment" -ForegroundColor Red
    exit 1
}

foreach ($environmentEntry in $msvcEnvironment) {
    if ($environmentEntry -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
    }
}

# Compile SQLite C amalgamation if object file is missing or outdated
if (-not (Test-Path $sqliteObj) -or ((Get-Item $sqliteC).LastWriteTime -gt (Get-Item $sqliteObj).LastWriteTime)) {
    Write-Host "Compiling SQLite amalgamation (C)..." -ForegroundColor Yellow
    & $clExe /nologo /c /O2 /MD /TC "/Fo$sqliteObj" $sqliteC
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
    (Join-Path $srcDir "logging/EventLogger.cpp"),
    (Join-Path $srcDir "network/ServerClient.cpp"),
    (Join-Path $srcDir "client/ClientManager.cpp"),
    (Join-Path $srcDir "console/ConsoleOutput.cpp"),
    (Join-Path $srcDir "runtime/ApplicationRuntime.cpp"),
    (Join-Path $srcDir "service/WindowsService.cpp"),
    $sqliteObj
)

$libs = @(
    "libcurl.lib",
    "setupapi.lib",
    "cfgmgr32.lib",
    "user32.lib",
    "kernel32.lib",
    "advapi32.lib"
)

$compileArgs = @(
    "/nologo",
    "/utf-8",
    "/std:c++17",
    "/EHsc",
    "/O2",
    "/MD",
    "/DUNICODE",
    "/D_UNICODE",
    "/I$includeDir"
) + $sources + @(
    "/Fe:$outputExe",
    "/link",
    "/LIBPATH:$libDir"
) + $libs

Write-Host "Running: $clExe $($compileArgs -join ' ')" -ForegroundColor Gray

& $clExe @compileArgs

if ($LASTEXITCODE -eq 0) {
    $runtimeDlls = @(Get-ChildItem -Path $binDir -Filter "*.dll" -File -ErrorAction SilentlyContinue)
    if ($runtimeDlls.Count -eq 0) {
        Write-Host "`n[ERROR] No vcpkg runtime DLLs found in $binDir" -ForegroundColor Red
        exit 1
    }

    foreach ($runtimeDll in $runtimeDlls) {
        Copy-Item -Path $runtimeDll.FullName -Destination $scriptDir -Force
    }

    Write-Host "`n[SUCCESS] Built successfully: $outputExe" -ForegroundColor Green
} else {
    Write-Host "`n[ERROR] Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
}
