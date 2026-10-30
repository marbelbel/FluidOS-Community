param(
    [string]$BochsDirectory,
    [string]$IsoName = 'VOS-Desktop-Legacy-MBR.iso'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'

if ([string]::IsNullOrWhiteSpace($BochsDirectory)) {
    $BochsDirectory = Join-Path $projectRoot '.tools\bochs'
}
$bochsExe = Join-Path $BochsDirectory 'bochs.exe'
$biosRom = Join-Path $BochsDirectory 'BIOS-bochs-latest'
$vgaRom = Join-Path $BochsDirectory 'VGABIOS-lgpl-latest.bin'
$isoExtension = [IO.Path]::GetExtension($IsoName)
if ([IO.Path]::GetFileName($IsoName) -cne $IsoName -or
    $isoExtension -ine '.iso' -or
    [string]::IsNullOrWhiteSpace([IO.Path]::GetFileNameWithoutExtension($IsoName))) {
    throw 'IsoName debe ser solo un nombre de archivo terminado en .iso, sin carpetas.'
}
$isoPath = Join-Path $buildDir $IsoName

foreach ($required in @($bochsExe, $biosRom, $vgaRom, $isoPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Falta $required. Primero ejecuta .\make-iso.ps1."
    }
}

$bochsFullPath = [IO.Path]::GetFullPath($bochsExe)
$runningBochs = @(Get-Process -Name 'bochs' -ErrorAction SilentlyContinue | Where-Object {
    try { [IO.Path]::GetFullPath($_.Path) -eq $bochsFullPath }
    catch { $false }
})
if ($runningBochs.Count -gt 0) {
    Write-Host "Bochs ya está abierto (PID $($runningBochs[0].Id)). No iniciaré otra instancia."
    return
}

$configPath = Join-Path $buildDir 'bochs-vm.conf'
$logPath = Join-Path $buildDir 'bochs.log'
$bochsIsoPath = [IO.Path]::GetRelativePath($projectRoot, $isoPath).Replace('\', '/')
$config = @'
megs: 512
cpu: model=core2_penryn_t9600, count=1, ips=50000000, reset_on_triple_fault=1, ignore_bad_msrs=1
romimage: file=".tools/bochs/BIOS-bochs-latest", options=fastboot
vgaromimage: file=".tools/bochs/VGABIOS-lgpl-latest.bin"
mouse: enabled=1, type=ps2
ata0-master: type=cdrom, path="__VOS_ISO_PATH__", status=inserted
boot: cdrom
display_library: win32
log: build/bochs.log
panic: action=ask
error: action=report
info: action=report
debug: action=ignore
'@
$config = $config.Replace('__VOS_ISO_PATH__', $bochsIsoPath)
Set-Content -LiteralPath $configPath -Value $config -Encoding ASCII

# This VM has no hard-disk image or host drive configured. It boots the ISO only.
$process = Start-Process -FilePath $bochsExe `
    -ArgumentList @('-q', '-noconsole', '-f', $configPath) `
    -WorkingDirectory $projectRoot `
    -WindowStyle Normal `
    -PassThru
Start-Sleep -Seconds 3
$process.Refresh()
if ($process.HasExited) {
    $logTail = if (Test-Path -LiteralPath $logPath -PathType Leaf) {
        (Get-Content -LiteralPath $logPath -Tail 25) -join "`n"
    }
    else {
        'Bochs no creó su archivo de registro.'
    }
    throw "Bochs terminó al iniciar (código $($process.ExitCode)).`n$logTail"
}

Write-Host "Bochs está abierto (PID $($process.Id)). La máquina virtual solo tiene la ISO como CD."
Write-Host "Configuración: $configPath"
Write-Host "Registro: $logPath"
