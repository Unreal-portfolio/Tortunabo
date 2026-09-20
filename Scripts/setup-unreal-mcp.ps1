<#
.SYNOPSIS
    Instala el plugin de editor UnrealMCPython (MCP de Unreal) en Plugins/.

.DESCRIPTION
    El plugin NO se versiona (binarios de terceros, solo editor). En el .uproject
    figura como "Optional": quien no lo tenga abre el proyecto con normalidad.
    Este script lo descarga de la release oficial, verifica su SHA-256 y lo
    descomprime en Plugins/UnrealMCPython.

    Con -WithClaude ademas clona el servidor MCP y lo registra en Claude Code
    (requiere git, uv y claude en el PATH).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\setup-unreal-mcp.ps1
    powershell -ExecutionPolicy Bypass -File Scripts\setup-unreal-mcp.ps1 -WithClaude
#>
[CmdletBinding()]
param(
    [switch]$WithClaude,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$PLUGIN_VERSION = '2.2.0'
$ENGINE_VERSION = '5.6'
$ZIP_NAME       = "UnrealMCPython_${ENGINE_VERSION}_${PLUGIN_VERSION}.zip"
$ZIP_URL        = "https://github.com/GenOrca/unreal-mcp/releases/download/v$PLUGIN_VERSION/$ZIP_NAME"
$ZIP_SHA256     = '449c5c822a7fcdeede554afe30439e4057db3dd109c6b5c15dd719295b02fab6'
$SERVER_REPO    = 'https://github.com/GenOrca/unreal-mcp.git'

$repoRoot   = Split-Path -Parent $PSScriptRoot
$pluginsDir = Join-Path $repoRoot 'Plugins'
$pluginDir  = Join-Path $pluginsDir 'UnrealMCPython'

function Install-Plugin {
    if ((Test-Path (Join-Path $pluginDir 'UnrealMCPython.uplugin')) -and -not $Force) {
        Write-Host "[plugin] Ya instalado en $pluginDir (usa -Force para reinstalar)."
        return
    }
    if (Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue) {
        throw 'Cierra Unreal Editor antes de instalar el plugin.'
    }

    $zipPath = Join-Path $env:TEMP $ZIP_NAME
    Write-Host "[plugin] Descargando $ZIP_URL"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $ZIP_URL -OutFile $zipPath -UseBasicParsing

    $actualHash = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $ZIP_SHA256) {
        Remove-Item $zipPath -Force
        throw "SHA-256 inesperado ($actualHash). Descarga descartada."
    }

    if (Test-Path $pluginDir) {
        Remove-Item $pluginDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force $pluginsDir | Out-Null
    Expand-Archive -Path $zipPath -DestinationPath $pluginsDir -Force
    Remove-Item $zipPath -Force
    Write-Host "[plugin] Instalado en $pluginDir"
}

function Install-ClaudeServer {
    foreach ($tool in 'git', 'uv', 'claude') {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "Falta '$tool' en el PATH; es necesario para -WithClaude."
        }
    }

    $serverRoot = Join-Path $env:LOCALAPPDATA 'Tortunabo\unreal-mcp'
    if (-not (Test-Path (Join-Path $serverRoot '.git'))) {
        Write-Host "[server] Clonando $SERVER_REPO (v$PLUGIN_VERSION)"
        git clone --depth 1 --branch "v$PLUGIN_VERSION" $SERVER_REPO $serverRoot
        if ($LASTEXITCODE -ne 0) { throw 'git clone ha fallado.' }
    }
    else {
        Write-Host "[server] Ya clonado en $serverRoot"
    }

    $serverDir = (Join-Path $serverRoot 'mcp-server') -replace '\\', '/'
    Push-Location $repoRoot
    try {
        # Via cmd: en PowerShell 5.1 redirigir el stderr de un nativo con
        # ErrorActionPreference=Stop lanza excepcion aunque el fallo sea esperado.
        cmd /c 'claude mcp remove unreal -s local >nul 2>&1'
        claude mcp add unreal -s local -- uv --directory $serverDir run src/unreal_mcp/main.py
        if ($LASTEXITCODE -ne 0) { throw 'claude mcp add ha fallado.' }
    }
    finally {
        Pop-Location
    }
    Write-Host '[server] Registrado como "unreal" en Claude Code (scope local de este repo).'
}

Install-Plugin
if ($WithClaude) {
    Install-ClaudeServer
}
Write-Host 'Hecho. Abre el proyecto en Unreal; el plugin escucha en 127.0.0.1:12029.'
