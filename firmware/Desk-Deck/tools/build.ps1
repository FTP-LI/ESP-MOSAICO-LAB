param([switch]$Bundle)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$env:IDF_PATH = 'C:/Espressif/esp-idf-v6.2-mosaico/esp-idf'
$env:IDF_TOOLS_PATH = 'C:/Espressif/tools'
$env:IDF_PYTHON_ENV_PATH = 'C:/Espressif/tools/python/esp-idf-v6.2-mosaico/venv'
$env:IDF_TARGET = 'esp32s31'
$env:ESP_IDF_VERSION = '6.2'
$env:PYTHONUTF8 = '1'
$env:IDF_COMPONENT_STORAGE_URL = 'https://components-file.espressif.com'
$env:ESP_ROM_ELF_DIR = 'C:/Espressif/tools/tools/esp-rom-elfs/20260528'
$env:ESP_IRIS_PYTHON = Join-Path $env:LOCALAPPDATA 'esp-mosaico/runtimes/esp-iris/9406289d6ea7c1db/py3.12/Scripts/python.exe'
$toolDirs = @("$env:IDF_PYTHON_ENV_PATH/Scripts", 'C:/Espressif/tools/tools/cmake/4.0.3/bin', 'C:/Espressif/tools/tools/ninja/1.12.1', 'C:/Espressif/tools/tools/riscv32-esp-elf/esp-16.1.0_20260609/riscv32-esp-elf/bin')
$env:PATH = ($toolDirs -join ';') + ';' + $env:PATH
Push-Location $projectRoot
try {
    & "$env:IDF_PYTHON_ENV_PATH/Scripts/python.exe" tools/generate_profiles.py
    if ($LASTEXITCODE -ne 0) { throw 'Profile generation failed' }
    $target = if ($Bundle) {'system-update-bundle'} else {'build'}
    & "$env:IDF_PYTHON_ENV_PATH/Scripts/python.exe" "$env:IDF_PATH/tools/idf.py" -B build-idf62 $target
    if ($LASTEXITCODE -ne 0) { throw 'Desk-Deck build failed' }
} finally { Pop-Location }
