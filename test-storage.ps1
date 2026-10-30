$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
$gptTestPath = Join-Path $buildDir 'gpt-tests.exe'
$storageTestPath = Join-Path $buildDir 'storage-crypto-tests.exe'
$platformTestPath = Join-Path $buildDir 'platform-input-tests.exe'
$cl = Get-Command 'cl.exe' -ErrorAction SilentlyContinue
$gcc = Get-Command 'gcc.exe' -ErrorAction SilentlyContinue
$zig = $env:VOS_ZIG
if (-not $zig -or -not (Test-Path -LiteralPath $zig -PathType Leaf)) {
    $zig = Join-Path $projectRoot '..\..\..\herramientas\zig-x86_64-windows-0.17.0\zig.exe'
}
$hasZig = Test-Path -LiteralPath $zig -PathType Leaf
$storageInclude = "/I$projectRoot\src\storage"
$securityInclude = "/I$projectRoot\src\security"
$storageIncludeGcc = "-I$projectRoot\src\storage"
$securityIncludeGcc = "-I$projectRoot\src\security"

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
Push-Location -LiteralPath $projectRoot
try {
    if ($cl) {
        $objectDir = $buildDir + '\'
        & $cl.Source /nologo /std:c11 /W4 "/I$projectRoot\src" $storageInclude "/Fo$objectDir" "/Fe$gptTestPath" `
            src\storage\block.c src\storage\gpt.c tests\gpt_test.c
        if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar el banco GPT.' }
        & $cl.Source /nologo /std:c11 /W4 "/I$projectRoot\src" $storageInclude $securityInclude "/Fo$objectDir" "/Fe$storageTestPath" `
            src\storage\block.c src\storage\fat32.c src\security\package_verify.c src\security\sha256.c tests\storage_crypto_test.c
        if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar el banco de almacenamiento.' }
        & $cl.Source /nologo /std:c11 /W4 `
            "/I$projectRoot\src\arch\x86_64" "/I$projectRoot\src\drivers\input" `
            "/Fo$objectDir" "/Fe$platformTestPath" `
            src\arch\x86_64\cpu.c src\drivers\input\keyboard.c tests\platform_input_test.c
        if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar la prueba de CPU y teclados.' }
        & $gptTestPath
        if ($LASTEXITCODE -ne 0) { throw "Fallo la prueba GPT numero $LASTEXITCODE." }
        & $storageTestPath
        if ($LASTEXITCODE -ne 0) { throw "Fallo la prueba de almacenamiento numero $LASTEXITCODE." }
        & $platformTestPath
        if ($LASTEXITCODE -ne 0) { throw "Fallo la prueba de CPU y teclados ($LASTEXITCODE)." }
        Write-Host 'Pruebas C correctas: GPT, paquetes, FAT32, CPU Intel y mapas de teclado.'
    }
    elseif ($gcc) {
        & $gcc.Source -std=c11 -O2 -Wall -Wextra "-I$projectRoot\src" $storageIncludeGcc `
            src\storage\block.c src\storage\gpt.c tests\gpt_test.c -o $gptTestPath
        if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar el banco GPT.' }
        & $gcc.Source -std=c11 -O2 -Wall -Wextra "-I$projectRoot\src" $storageIncludeGcc $securityIncludeGcc `
            src\storage\block.c src\storage\fat32.c src\security\package_verify.c src\security\sha256.c tests\storage_crypto_test.c `
            -o $storageTestPath
        if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar el banco de almacenamiento.' }
        & $gcc.Source -std=c11 -O2 -Wall -Wextra "-I$projectRoot\src\arch\x86_64" `
            "-I$projectRoot\src\drivers\input" src\arch\x86_64\cpu.c `
            src\drivers\input\keyboard.c tests\platform_input_test.c -o $platformTestPath
        if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar la prueba de CPU y teclados.' }
        & $gptTestPath
        if ($LASTEXITCODE -ne 0) { throw "Fallo la prueba GPT numero $LASTEXITCODE." }
        & $storageTestPath
        if ($LASTEXITCODE -ne 0) { throw "Fallo la prueba de almacenamiento numero $LASTEXITCODE." }
        & $platformTestPath
        if ($LASTEXITCODE -ne 0) { throw "Fallo la prueba de CPU y teclados ($LASTEXITCODE)." }
        Write-Host 'Pruebas C correctas: GPT, paquetes, FAT32, CPU Intel y mapas de teclado.'
    }
    else {
        Write-Host 'Pruebas C aplazadas: no hay cl.exe ni gcc.exe; no se instalo nada.'
    }

    if ($hasZig) {
        $oldGlobalCache = $env:ZIG_GLOBAL_CACHE_DIR
        $oldLocalCache = $env:ZIG_LOCAL_CACHE_DIR
        try {
            $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $buildDir 'zig-test-global-cache'
            $env:ZIG_LOCAL_CACHE_DIR = Join-Path $buildDir 'zig-test-local-cache'
            & $zig test src\security\package_crypto.zig
            if ($LASTEXITCODE -ne 0) { throw "Fallo la prueba RFC 8032 numero $LASTEXITCODE." }
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
    }
    else {
        Write-Host 'Prueba Ed25519 RFC 8032 omitida: no se encontro Zig.'
    }
}
finally {
    Pop-Location
}

Write-Host 'Los bancos C usan discos ficticios en memoria; no acceden a discos fisicos.'
