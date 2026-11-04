# Arquitectura y compatibilidad objetivo

## Objetivo de esta etapa

VOS se construye únicamente como ELF de 64 bits para x86-64. Al arrancar,
comprueba CPUID y se detiene con un mensaje si la CPU no informa el fabricante
Intel (`GenuineIntel`). El binario de 64 bits tampoco puede ejecutarse en una
CPU de 32 bits.

El objetivo inicial de hardware son equipos Intel x86-64 con firmware capaz de
entregar a Limine un framebuffer RGB. La interfaz dibuja sobre ese framebuffer;
VOS todavía no controla directamente una GPU Intel ni ofrece aceleración 2D/3D.
Que el firmware entregue imagen no demuestra que todos los gráficos Intel, sus
resoluciones, suspensión, energía o varios monitores sean compatibles.

## Estado comprobado

| Área | Estado en el código actual | Validación pendiente |
| --- | --- | --- |
| Arquitectura | ELF `x86_64-freestanding`; enlazador ELF64 | Arranque en emulador y en varios modelos Intel |
| Fabricante de CPU | Rechaza fabricantes distintos de `GenuineIntel` | Probar CPUID en QEMU configurado con fabricante Intel y luego en hardware de prueba |
| Gráficos | Usa la superficie framebuffer que entrega Limine | Intel iGPU por generación, UEFI/GOP y modos de pantalla |
| Teclado | Cinco mapas PS/2 iniciales y parciales: ES, US, UK, FR y DE; teclas no definidas usan el mapa estadounidense | Completar mapas, añadir USB HID, probar portátiles y validar físicamente cada mapa |
| Almacenamiento | No hay controlador de disco activo; FAT32 solo tiene lectura y no está conectado a Archivos | NVMe/SATA, persistencia, escritura segura y recuperación |
| Idiomas | Interfaz traducida al español e inglés; glifos limitados a ASCII y algunos caracteres latinos | Unicode completo, formas de escritura, derecha a izquierda, fuentes y métodos de entrada |

## Cómo ampliar la compatibilidad

1. Probar primero el arranque x86-64 en QEMU con CPU Intel simulada y discos
   virtuales desechables. La prueba no debe pasar por discos físicos.
2. Crear una tabla de equipos Intel de prueba que cubra varias generaciones,
   firmware UEFI, pantallas y gráficos integrados.
3. Añadir controladores por familias con aislamiento y pruebas, empezando por
   almacenamiento y entrada USB HID.
4. Para idiomas, agregar catálogos revisados, fuentes Unicode con sus licencias,
   composición de texto, soporte bidireccional y métodos de entrada adecuados.
   Las traducciones deben probarse en el escritorio completo.

«Todos los idiomas y teclados» no es una lista que se pueda completar con una
sola traducción: algunas escrituras requieren composición, modelado de texto,
dirección derecha-a-izquierda y métodos de entrada. Cada combinación debe
probarse antes de anunciarla como compatible.

## Límites de la prueba actual

La compilación verifica el formato x86-64 del kernel, no su compatibilidad con
modelos concretos de Intel. `make-iso.ps1` produce una imagen UEFI/GPT para
Rufus en modo ISO, y otra BIOS/MBR con las etapas de Limine instaladas en la
copia Legacy para escritura raw. Ambas incluyen entradas BIOS/UEFI para CD
virtual. Las comprobaciones estructurales no garantizan el arranque en todos
los equipos. La imagen USB aún debe probarse en el Sony VAIO; VOS no tiene
controlador de disco, red ni soporte amplio de teclados y gráficos. Una imagen
BIOS/GPT requeriría una partición BIOS Boot adicional y no se ofrece en esta
etapa.
