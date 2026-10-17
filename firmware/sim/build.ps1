# Builds the simulator and runs host tests with MSVC.
# Usage: .\build.ps1        build + test
#        .\build.ps1 -Run   build + test + launch the simulator
param([switch]$Run)

$installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$env:Path = "$installer;$env:Path"  # vcvars64.bat calls vswhere from PATH
$vs = & "$installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'

Push-Location $PSScriptRoot
try {
    # stderr is merged inside cmd so PowerShell 5.1 doesn't treat it as an error.
    cmd /c "(call `"$vcvars`" >nul && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure) 2>&1"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($Run) { & .\build\desk_buddy_sim.exe }
} finally {
    Pop-Location
}
