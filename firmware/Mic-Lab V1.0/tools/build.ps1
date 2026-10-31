param([switch]$Bundle, [switch]$UseEnvironment)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($UseEnvironment) {
    if (-not $env:IDF_PATH -or -not $env:IDF_PYTHON_ENV_PATH) {
        throw 'Activate the pinned ESP-IDF environment (export.ps1) before using -UseEnvironment.'
    }
} else {
    # Original validated workstation; other PCs should use -UseEnvironment.
    $env:IDF_PATH = 'C:/Espressif/esp-idf-v6.2-mosaico/esp-idf'
    $env:IDF_TOOLS_PATH = 'C:/Espressif/tools'
    $env:IDF_PYTHON_ENV_PATH = 'C:/Espressif/tools/python/esp-idf-v6.2-mosaico/venv'
    $env:ESP_ROM_ELF_DIR = 'C:/Espressif/tools/tools/esp-rom-elfs/20260528'
    $env:ESP_IRIS_PYTHON = Join-Path $env:LOCALAPPDATA 'esp-mosaico/runtimes/esp-iris/9406289d6ea7c1db/py3.12/Scripts/python.exe'
    $toolDirs = @('C:/Espressif/tools/tools/cmake/4.0.3/bin', 'C:/Espressif/tools/tools/ninja/1.12.1', 'C:/Espressif/tools/tools/riscv32-esp-elf/esp-16.1.0_20260609/riscv32-esp-elf/bin')
    $env:PATH = ($toolDirs -join ';') + ';' + $env:PATH
}
$revision = & git -C $env:IDF_PATH rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $revision.Trim() -ne '1c42e514c90ae621af33bde3b6ad3dac22edcbfd') {
    throw 'ESP-IDF must be pinned to 1c42e514c90ae621af33bde3b6ad3dac22edcbfd (ESP32-S31).'
}
if ($Bundle -and (-not $env:ESP_IRIS_PYTHON -or -not (Test-Path -LiteralPath $env:ESP_IRIS_PYTHON))) {
    throw 'Set ESP_IRIS_PYTHON to an interpreter with the pinned Iris tools/requirements.lock installed. See docs/HANDOFF.md.'
}
$env:IDF_TARGET = 'esp32s31'
$env:ESP_IDF_VERSION = '6.2'
$env:PYTHONUTF8 = '1'
$env:IDF_COMPONENT_STORAGE_URL = 'https://components-file.espressif.com'
$env:IDF_COMPONENT_CHECK_NEW_VERSION = '0' # Use the lock file; do not solve newer versions on every build.
$env:PATH = "$env:IDF_PYTHON_ENV_PATH/Scripts;" + $env:PATH
Push-Location $projectRoot
try {
    $target = if ($Bundle) {'system-update-bundle'} else {'build'}
    & "$env:IDF_PYTHON_ENV_PATH/Scripts/python.exe" "$env:IDF_PATH/tools/idf.py" -B build-idf62 $target
    if ($LASTEXITCODE -ne 0) { throw 'Mic-Lab V1.0 build failed' }
} finally { Pop-Location }
