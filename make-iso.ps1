param(
    [string]$LimineDirectory,
    [string]$PythonPath = $env:VOS_PYTHON,
    [string]$IsoName = 'VOS-Desktop-UEFI-GPT.iso',
    [string]$LegacyIsoName = 'VOS-Desktop-Legacy-MBR.iso'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
$toolsDir = Join-Path $projectRoot '.tools'

if ([string]::IsNullOrWhiteSpace($LimineDirectory)) {
    $LimineDirectory = Join-Path $projectRoot 'limine-binary'
}
$limineRoot = $LimineDirectory
if (-not (Test-Path -LiteralPath (Join-Path $limineRoot 'limine-uefi-cd.bin') -PathType Leaf)) {
    $nestedLimineRoot = Join-Path $limineRoot 'limine-binary'
    if (Test-Path -LiteralPath (Join-Path $nestedLimineRoot 'limine-uefi-cd.bin') -PathType Leaf) {
        $limineRoot = $nestedLimineRoot
    }
}

$requiredFiles = @(
    'limine-bios.sys',
    'limine-bios-cd.bin',
    'limine-uefi-cd.bin',
    'BOOTX64.EFI'
)
$missing = @($requiredFiles | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $limineRoot $_) -PathType Leaf)
})
if ($missing.Count -ne 0) {
    throw ("Faltan archivos del paquete binario de Limine en ${limineRoot}:`n - " + ($missing -join "`n - "))
}
$limineTool = Join-Path $limineRoot 'limine-tool-windows-x86\limine.exe'
if (-not (Test-Path -LiteralPath $limineTool -PathType Leaf)) {
    throw "Falta la herramienta limine.exe en $limineTool; es necesaria para preparar la imagen Legacy."
}

if ([string]::IsNullOrWhiteSpace($PythonPath)) {
    $pythonCommand = Get-Command 'python.exe' -ErrorAction SilentlyContinue
    if ($pythonCommand -and $pythonCommand.Source -notlike '*\WindowsApps\*') {
        $PythonPath = $pythonCommand.Source
    }
}
if ([string]::IsNullOrWhiteSpace($PythonPath)) {
    $bundledPythonCandidates = @()
    if (-not [string]::IsNullOrWhiteSpace($env:USERPROFILE)) {
        $bundledPythonCandidates += Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    }
    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        $bundledPythonCandidates += Join-Path $env:LOCALAPPDATA '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    }
    foreach ($candidate in $bundledPythonCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $PythonPath = $candidate
            break
        }
    }
}
if ([string]::IsNullOrWhiteSpace($PythonPath) -or
    -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw 'No encuentro Python. Instala Python 3 o ejecuta: .\make-iso.ps1 -PythonPath "C:\ruta\python.exe".'
}

$isoBuilder = Join-Path $projectRoot 'tools\create_iso.py'
$isoVerifier = Join-Path $projectRoot 'tools\verify_iso.py'
$legacyIsoVerifier = Join-Path $projectRoot 'tools\verify_legacy_iso.py'
$pycdlibPackage = Join-Path $toolsDir 'pycdlib\pycdlib\__init__.py'
if (-not (Test-Path -LiteralPath $isoBuilder -PathType Leaf) -or
    -not (Test-Path -LiteralPath $isoVerifier -PathType Leaf) -or
    -not (Test-Path -LiteralPath $legacyIsoVerifier -PathType Leaf) -or
    -not (Test-Path -LiteralPath $pycdlibPackage -PathType Leaf)) {
    throw 'Faltan las herramientas locales para crear o validar las ISO (create_iso.py, verify_iso.py, verify_legacy_iso.py o .tools\pycdlib).'
}

$isoExtension = [IO.Path]::GetExtension($IsoName)
if ([IO.Path]::GetFileName($IsoName) -cne $IsoName -or
    $isoExtension -ine '.iso' -or
    [string]::IsNullOrWhiteSpace([IO.Path]::GetFileNameWithoutExtension($IsoName))) {
    throw 'IsoName debe ser solo un nombre de archivo terminado en .iso, sin carpetas.'
}
if ([IO.Path]::GetFileName($LegacyIsoName) -cne $LegacyIsoName -or
    [IO.Path]::GetExtension($LegacyIsoName) -ine '.iso' -or
    [string]::IsNullOrWhiteSpace([IO.Path]::GetFileNameWithoutExtension($LegacyIsoName))) {
    throw 'LegacyIsoName debe ser solo un nombre de archivo terminado en .iso, sin carpetas.'
}
if ([string]::Equals($IsoName, $LegacyIsoName, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'IsoName y LegacyIsoName deben ser distintos.'
}
$isoPath = Join-Path $buildDir $IsoName
if (Test-Path -LiteralPath $isoPath) {
    throw "Ya existe $isoPath. Renómbrala desde el Explorador si quieres generar una nueva."
}
$legacyIsoPath = Join-Path $buildDir $LegacyIsoName
if (Test-Path -LiteralPath $legacyIsoPath) {
    throw "Ya existe $legacyIsoPath. Renómbrala desde el Explorador si quieres generar una nueva."
}

& (Join-Path $projectRoot 'build.ps1')
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "Falló la compilación del kernel (código $LASTEXITCODE)."
}
$kernelPath = Join-Path $buildDir 'kernel.elf'
if (-not (Test-Path -LiteralPath $kernelPath -PathType Leaf)) {
    throw 'La compilación no produjo build\kernel.elf.'
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$stageName = 'iso-stage-' + [Guid]::NewGuid().ToString('N')
$stageRoot = Join-Path $buildDir $stageName
$stageFullPath = [IO.Path]::GetFullPath($stageRoot)
$buildPrefix = [IO.Path]::GetFullPath($buildDir).TrimEnd('\') + '\'
if (-not $stageFullPath.StartsWith($buildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'La carpeta temporal de ISO sale de build; se cancela por seguridad.'
}

$isoCreated = $false
$legacyIsoCreated = $false
$isoFullPath = [IO.Path]::GetFullPath($isoPath)
if (-not $isoFullPath.StartsWith($buildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'La ruta de la ISO sale de build; se cancela por seguridad.'
}
$legacyIsoFullPath = [IO.Path]::GetFullPath($legacyIsoPath)
if (-not $legacyIsoFullPath.StartsWith($buildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'La ruta de la ISO Legacy sale de build; se cancela por seguridad.'
}

try {
    New-Item -ItemType Directory -Path (Join-Path $stageRoot 'boot') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $stageRoot 'EFI\BOOT') -Force | Out-Null
    Copy-Item -LiteralPath $kernelPath -Destination (Join-Path $stageRoot 'boot\kernel.elf')
    Copy-Item -LiteralPath (Join-Path $projectRoot 'boot\limine.conf') `
        -Destination (Join-Path $stageRoot 'limine.conf')
    Copy-Item -LiteralPath (Join-Path $limineRoot 'limine-bios.sys') `
        -Destination $stageRoot
    Copy-Item -LiteralPath (Join-Path $limineRoot 'limine-bios-cd.bin') `
        -Destination (Join-Path $stageRoot 'boot')
    Copy-Item -LiteralPath (Join-Path $limineRoot 'limine-uefi-cd.bin') `
        -Destination (Join-Path $stageRoot 'boot')
    Copy-Item -LiteralPath (Join-Path $limineRoot 'BOOTX64.EFI') `
        -Destination (Join-Path $stageRoot 'EFI\BOOT')

    $isoCreated = $true
    & $PythonPath $isoBuilder --stage $stageRoot --output $isoPath --pycdlib (Join-Path $toolsDir 'pycdlib')
    if ($LASTEXITCODE -ne 0) { throw "Falló la creación de ISO (código $LASTEXITCODE)." }

    & $PythonPath $isoVerifier --iso $isoPath --pycdlib (Join-Path $toolsDir 'pycdlib')
    if ($LASTEXITCODE -ne 0) { throw "Falló la comprobación de la ISO (código $LASTEXITCODE)." }

    # bios-install receives only an ISO file under build, never a physical disk.
    $legacyIsoCreated = $true
    Copy-Item -LiteralPath $isoPath -Destination $legacyIsoPath
    & $limineTool 'bios-install' '--quiet' $legacyIsoPath
    if ($LASTEXITCODE -ne 0) {
        throw "Limine no pudo preparar el arranque Legacy dentro de $legacyIsoPath (código $LASTEXITCODE)."
    }
    & $PythonPath $legacyIsoVerifier --iso $legacyIsoPath --pycdlib (Join-Path $toolsDir 'pycdlib')
    if ($LASTEXITCODE -ne 0) { throw "Falló la comprobación de la ISO Legacy (código $LASTEXITCODE)." }

    $uefiIso = Get-Item -LiteralPath $isoPath
    $legacyIso = Get-Item -LiteralPath $legacyIsoPath
    Write-Host "ISO UEFI/GPT creada: $($uefiIso.FullName) ($($uefiIso.Length) bytes)"
    Write-Host "ISO BIOS/MBR creada: $($legacyIso.FullName) ($($legacyIso.Length) bytes)"
    Write-Host 'Sony VAIO UEFI: usa la ISO UEFI/GPT en Rufus con GPT, UEFI (sin CSM), FAT32 y modo ISO.'
    Write-Host 'PC antiguo con BIOS Legacy: usa la ISO BIOS/MBR; selecciona DD si Rufus lo ofrece.'
    Write-Host 'Solo se han creado archivos dentro de build; no se ha escrito en ningún USB ni disco físico.'
}
catch {
    if ($legacyIsoCreated -and (Test-Path -LiteralPath $legacyIsoPath -PathType Leaf)) {
        Remove-Item -LiteralPath $legacyIsoPath -Force
    }
    if ($isoCreated -and (Test-Path -LiteralPath $isoPath -PathType Leaf)) {
        Remove-Item -LiteralPath $isoPath -Force
    }
    throw
}
finally {
    if (Test-Path -LiteralPath $stageFullPath -PathType Container) {
        Remove-Item -LiteralPath $stageFullPath -Recurse -Force
    }
}
