$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
$kernelPath = Join-Path $buildDir 'kernel.elf'
$zig = $env:VOS_ZIG

if (-not $zig -or -not (Test-Path -LiteralPath $zig -PathType Leaf)) {
    $zig = Join-Path $projectRoot '..\..\..\herramientas\zig-x86_64-windows-0.17.0\zig.exe'
}

if (-not (Test-Path -LiteralPath $zig -PathType Leaf)) {
    throw "No encuentro zig.exe. Revisa la carpeta Documentos\Codex\herramientas o define VOS_ZIG con la ruta de zig.exe."
}

$sourceRoot = Join-Path $projectRoot 'src'
$sources = @(Get-ChildItem -LiteralPath $sourceRoot -Filter '*.c' -File -Recurse |
    Sort-Object FullName |
    ForEach-Object { $_.FullName })
$zigSources = @(Join-Path $projectRoot 'src\security\package_crypto.zig')
$includeDirs = @($sourceRoot) + @(
    Get-ChildItem -LiteralPath $sourceRoot -Directory -Recurse |
        Sort-Object FullName |
        ForEach-Object { $_.FullName }
)
$includeArgs = @($includeDirs | ForEach-Object { "-I$_" })

if ($sources.Count -eq 0) {
    throw 'No hay archivos C dentro de src.'
}
if (-not (Test-Path -LiteralPath $zigSources[0] -PathType Leaf)) {
    throw 'No encuentro src\security\package_crypto.zig, necesario para verificar Ed25519.'
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

# Guardar y restaurar el estado de esta ventana de PowerShell al terminar.
$oldGlobalCache = $env:ZIG_GLOBAL_CACHE_DIR
$oldLocalCache = $env:ZIG_LOCAL_CACHE_DIR
$zigExitCode = 1

$zigArgs = @(
    'build-exe',
    '-target', 'x86_64-freestanding-none',
    '-O', 'ReleaseFast',
    '-fentry=kernel_main',
    '-T', 'linker.ld',
    "-femit-bin=$kernelPath",
    '-cflags',
    '-std=gnu11',
    '-O2',
    '-Wall',
    '-Wextra',
    '-ffreestanding',
    '-fno-builtin',
    '-fno-stack-protector',
    '-fno-pic',
    '-fno-pie',
    '-m64',
    '-mno-red-zone',
    '-mgeneral-regs-only',
    '-mno-sse',
    '-mno-sse2',
    '-mno-mmx',
    '-mcmodel=kernel'
) + $includeArgs + @('--') + $zigSources + $sources

try {
    # Las cachés quedan dentro de build y estas variables no persisten al salir.
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $buildDir 'zig-global-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $buildDir 'zig-local-cache'

    Push-Location -LiteralPath $projectRoot
    try {
        & $zig @zigArgs
        $zigExitCode = $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
}
finally {
    if ($null -eq $oldGlobalCache) {
        Remove-Item Env:ZIG_GLOBAL_CACHE_DIR -ErrorAction SilentlyContinue
    }
    else {
        $env:ZIG_GLOBAL_CACHE_DIR = $oldGlobalCache
    }

    if ($null -eq $oldLocalCache) {
        Remove-Item Env:ZIG_LOCAL_CACHE_DIR -ErrorAction SilentlyContinue
    }
    else {
        $env:ZIG_LOCAL_CACHE_DIR = $oldLocalCache
    }
}

if ($zigExitCode -ne 0) {
    throw "Zig terminó con el código $zigExitCode. No se creó una ISO ni se arrancó VOS."
}

$kernel = Get-Item -LiteralPath $kernelPath
Write-Host "Kernel ELF compilado: $($kernel.FullName) ($($kernel.Length) bytes)"
Write-Host 'La compilación solo genera archivos dentro de la carpeta build del proyecto.'
