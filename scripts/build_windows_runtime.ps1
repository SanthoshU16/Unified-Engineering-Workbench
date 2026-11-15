$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build-windows-x64"

cmake -S $repoRoot -B $buildDir -A x64 -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

cmake --build $buildDir --config Release --target package_windows_runtime
if ($LASTEXITCODE -ne 0) { throw "Windows runtime build failed." }

$runtimeDir = Join-Path $repoRoot "dist\windows-x64"
$rootLauncher = Join-Path $repoRoot "Launch.exe"
$builtLauncher = Join-Path $runtimeDir "Launch.exe"
Copy-Item -LiteralPath $builtLauncher -Destination $rootLauncher -Force

$requiredFiles = @("Launch.exe", "workbench_server.exe")
foreach ($name in $requiredFiles) {
    $path = Join-Path $runtimeDir $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Expected runtime file was not created: $path"
    }
    Get-FileHash -Algorithm SHA256 -LiteralPath $path
}

Get-FileHash -Algorithm SHA256 -LiteralPath $rootLauncher

Write-Host "Windows x64 runtime files are ready in $runtimeDir"
