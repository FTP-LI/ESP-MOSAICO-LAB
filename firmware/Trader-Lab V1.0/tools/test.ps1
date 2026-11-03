param([switch]$LiveFixtures)
$ErrorActionPreference='Stop'
$projectDir=Split-Path -Parent $PSScriptRoot
Push-Location $projectDir
try {
    New-Item -ItemType Directory -Force -Path build-tests | Out-Null
    $jsonDir='managed_components/espressif__cjson/cJSON'
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -Imain "-I$jsonDir" tests/test_model.c main/model.c main/watchlist_seed.c main/gbk.c main/alarm_model.c "$jsonDir/cJSON.c" -lm -o build-tests/test_model.exe
    if($LASTEXITCODE -ne 0){throw 'Model test compile failed'}
    if($LiveFixtures){& ./build-tests/test_model.exe tests/fixtures.local/eastmoney.txt tests/fixtures.local/tencent.txt}
    else {& ./build-tests/test_model.exe}
    if($LASTEXITCODE -ne 0){throw 'Model tests failed'}
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -Imain "-I$jsonDir" tests/test_global.c main/global_market.c main/model.c main/watchlist_seed.c main/gbk.c "$jsonDir/cJSON.c" -lm -o build-tests/test_global.exe
    if($LASTEXITCODE -ne 0){throw 'Global test compile failed'}
    if($LiveFixtures){& ./build-tests/test_global.exe tests/fixtures.local}
    else {& ./build-tests/test_global.exe}
    if($LASTEXITCODE -ne 0){throw 'Global tests failed'}
} finally {Pop-Location}
