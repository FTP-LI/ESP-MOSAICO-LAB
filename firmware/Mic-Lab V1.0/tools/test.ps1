$ErrorActionPreference='Stop'
$projectDir=Split-Path -Parent $PSScriptRoot
Push-Location $projectDir
try {
    New-Item -ItemType Directory -Force -Path build-tests | Out-Null
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -Imain tests/test_core.c main/mic_core.c -lm -o build-tests/test_core.exe
    if($LASTEXITCODE -ne 0){throw 'Core test compile failed'}
    & ./build-tests/test_core.exe
    if($LASTEXITCODE -ne 0){throw 'Core test failed'}
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -Imain tests/test_clip.c main/clip_file.c -o build-tests/test_clip.exe
    if($LASTEXITCODE -ne 0){throw 'Clip test compile failed'}
    & ./build-tests/test_clip.exe
    if($LASTEXITCODE -ne 0){throw 'Clip test failed'}
    $tiny='managed_components/espressif__tinyusb/src'
    if(!(Test-Path $tiny)){throw 'Run tools/build.ps1 first to resolve pinned components'}
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -Imain -Itests "-I$tiny" tests/test_usb.c main/usb_layout.c -o build-tests/test_usb.exe
    if($LASTEXITCODE -ne 0){throw 'USB test compile failed'}
    & ./build-tests/test_usb.exe
    if($LASTEXITCODE -ne 0){throw 'USB test failed'}
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -Imain -Itests/mocks -Itests "-I$tiny" tests/test_usb_control.c main/usb_layout.c "$tiny/common/tusb_fifo.c" -o build-tests/test_usb_control.exe
    if($LASTEXITCODE -ne 0){throw 'USB control test compile failed'}
    & ./build-tests/test_usb_control.exe
    if($LASTEXITCODE -ne 0){throw 'USB control test failed'}
} finally {Pop-Location}
