param(
    [string]$IdfPath = 'C:\Espressif\esp-idf-v6.2-mosaico\esp-idf',
    [string]$ToolsPath = 'C:\Espressif\tools',
    [string]$PythonEnv = 'C:\Espressif\tools\python\esp-idf-v6.2-mosaico\venv',
    [switch]$UseGlobalMirror
)
$ErrorActionPreference = 'Stop'
$expectedIdf = '1c42e514c90ae621af33bde3b6ad3dac22edcbfd'
if (!(Test-Path -LiteralPath "$IdfPath\tools\idf.py")) { throw "ESP-IDF not found: $IdfPath" }
$actualIdf = & git -C $IdfPath rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualIdf -ne $expectedIdf) {
    throw "ESP-IDF revision mismatch. Expected $expectedIdf, found $actualIdf"
}
$env:IDF_PATH = $IdfPath
$env:IDF_TOOLS_PATH = $ToolsPath
$env:IDF_PYTHON_ENV_PATH = $PythonEnv
$env:PYTHONUTF8 = '1'
$env:IDF_COMPONENT_STORAGE_URL = if ($UseGlobalMirror) { 'https://components-file.espressif.com' } else { 'https://components-file.espressif.cn' }
$env:IDF_COMPONENT_API_TIMEOUT = '30'
$env:Path = "$PythonEnv\Scripts;$env:Path"
. "$IdfPath\export.ps1"
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF activation failed' }
function global:idf.py { & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" @args }
Write-Host "Mosaico IDF revision: $actualIdf"
