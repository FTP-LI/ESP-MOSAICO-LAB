$ErrorActionPreference='Stop'
$projectDir=Split-Path -Parent $PSScriptRoot
$cmake='C:/Espressif/tools/tools/cmake/4.0.3/bin/cmake.exe'
Push-Location $projectDir
try {
    & $cmake -S tests -B build-ui -G Ninja '-DCMAKE_MAKE_PROGRAM=C:/Espressif/tools/tools/ninja/1.12.1/ninja.exe' '-DCMAKE_C_COMPILER=C:/Tools/winlibs/mingw64/bin/gcc.exe' -DCMAKE_BUILD_TYPE=Release
    if($LASTEXITCODE -ne 0){throw 'UI configuration failed'}
    & $cmake --build build-ui -j 8
    if($LASTEXITCODE -ne 0){throw 'UI build failed'}
    & ./build-ui/ui_check.exe
    if($LASTEXITCODE -ne 0){throw 'UI checks failed'}
} finally {Pop-Location}
