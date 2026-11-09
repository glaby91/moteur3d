param(
    [ValidateSet("ui","engine","benchmark","benchmark-ui","all")]
    [string]$Target = "ui",
    [string]$QtPrefix = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root "build-v16"
if (!(Test-Path (Join-Path $build "CMakeCache.txt"))) {
    $args = @("-S", $PSScriptRoot, "-B", $build, "-G", "Visual Studio 17 2022", "-A", "x64")
    if ($QtPrefix) { $args += "-DCMAKE_PREFIX_PATH=$QtPrefix" }
    & cmake @args
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
}
$targets = @{
    "ui" = "moteur3d_v16"
    "engine" = "moteur3d_v16_core"
    "benchmark" = "moteur3d_v16_benchmark"
    "benchmark-ui" = "moteur3d_v16_benchui"
}
$arguments = @("--build", $build, "--config", "Release", "--parallel")
if ($Target -ne "all") { $arguments += @("--target", $targets[$Target]) }
& cmake @arguments
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
Write-Host "V16 Windows / $Target built. Shared engine stays unchanged when only UI files change."
