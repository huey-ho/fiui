param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [string]$BuildDir = "build",
    [string]$InstallPrefix = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($InstallPrefix)) {
    $InstallPrefix = "out/fiui-$($Config.ToLowerInvariant())"
}

cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
cmake --build $BuildDir --config $Config
ctest --test-dir $BuildDir -C $Config --output-on-failure
cmake --install $BuildDir --config $Config --prefix $InstallPrefix

$PackageSmokeBuildDir = Join-Path $BuildDir "package-smoke-$($Config.ToLowerInvariant())"
if ([System.IO.Path]::IsPathRooted($InstallPrefix)) {
    $PackageSmokePrefix = $InstallPrefix
} else {
    $PackageSmokePrefix = Join-Path (Get-Location) $InstallPrefix
}
$PackageSmokeConfigDir = Join-Path $PackageSmokePrefix "lib/cmake/fiui"
cmake -S tests/package_smoke -B $PackageSmokeBuildDir -G "Visual Studio 17 2022" -A x64 -Dfiui_DIR="$PackageSmokeConfigDir"
cmake --build $PackageSmokeBuildDir --config $Config
ctest --test-dir $PackageSmokeBuildDir -C $Config --output-on-failure

Write-Host "fiui verification completed: config=$Config build=$BuildDir install=$InstallPrefix"
