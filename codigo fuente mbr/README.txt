VOS - SISTEMA OPERATIVO EN DESARROLLO
======================================

VOS es un sistema x86-64 en desarrollo, escrito principalmente en C, que
arranca mediante Limine y presenta una interfaz gráfica propia. La meta es que
usar el sistema sea sencillo: las funciones se abren desde iconos y ventanas,
sin una terminal para el usuario.

ESTADO ACTUAL
-------------

- Version del proyecto: 0.9.
- La pantalla de la foto es el menu de Limine, no una terminal ni el escritorio
  de VOS. Ahora Limine arranca automaticamente sin mostrar el menu ni el
  contador; las advertencias y los errores importantes siguen visibles.
- El inicio muestra un escritorio gráfico con Inicio, Sistema, Memoria,
  Centro VOS, Ajustes, Ayuda, Archivos RAM, Aplicaciones y Calculadora.
- Antes del escritorio aparece una pantalla negra sin texto, con el logotipo
  de VOS y una barra de progreso centrados; Limine no muestra su menú ni una
  cuenta atrás.
- Las ventanas se pueden mover, ocultar, cerrar o maximizar/restaurar.
- El asistente inicial permite elegir español o inglés y propone teclado ES o
  US. Ajustes recorre cinco mapas PS/2: ES QWERTY, US QWERTY, UK QWERTY,
  FR AZERTY y DE QWERTZ. Idioma y preferencias duran solo la sesión.
- La fuente del escritorio muestra vocales acentuadas y la ñ.
- Si no hay controlador PS/2, el escritorio intenta abrirse igualmente; el
  teclado y el ratón pueden no responder en ese equipo.
- El Centro VOS muestra el estado real de los componentes inicializados y
  marca las funciones que aun no existen.
- El kernel tiene una API de bloques para futuros controladores y un lector
  GPT primario de solo lectura que comprueba CRC, limites y solapamientos.
- Hay una capa FAT32 de solo lectura para montar una particion, listar archivos
  de la raiz con nombre 8.3 y leer su contenido con limites de sector y cadena.
- Hay SHA-256 y un lector del formato VOSPKG1 que comprueba tamano, arquitectura,
  firma Ed25519 estricta y hash del contenido. El proveedor usa la biblioteca
  estandar de Zig 0.17, pero la lista de claves confiables aun esta vacia.
- Ningun controlador registra discos todavia; no se leen ni escriben discos.
- El acceso a la consola y a la shell se ha eliminado de la interfaz y del
  codigo del proyecto. Tambien se han quitado los comandos que reiniciaban o
  detenian la maquina.
- El kernel se compila como ELF x86-64. La ISO UEFI/GPT se comprueba
  estructuralmente; el arranque gráfico se revisa aparte en Bochs.

COMO USAR EL ESCRITORIO
----------------------

En cada arranque aparece una pantalla sencilla para elegir idioma y distribución
del teclado. Selecciona una opción y pulsa Continuar. Las preferencias se
pierden al apagar VOS. Luego pulsa un icono del dock para
abrir una aplicacion. Tambien puedes usar el
teclado:

1  Inicio
2  Sistema
3  Memoria
4  Centro VOS
5  Ajustes
6  Ayuda
7  Archivos RAM
8  Aplicaciones
9  Calculadora

Tab cambia a la siguiente aplicación. Escape vuelve a Inicio; desde Inicio,
Escape oculta o muestra la ventana. F1 abre Ayuda. H, S, M, A, F y T abren
Inicio, Sistema, Memoria, Ajustes, Archivos RAM y Aplicaciones. Dentro de Ajustes,
Enter cambia el tema, C el acento, L el idioma y K el teclado.

El boton rojo vuelve a Inicio (o cierra Inicio), el amarillo oculta la ventana
y el verde maximiza o restaura. Los iconos del dock vuelven a abrir cualquier
aplicacion. Si el raton PS/2 no esta disponible, se puede usar el teclado.

APLICACIONES
------------

Inicio ofrece accesos visuales a las aplicaciones.
Sistema muestra arquitectura, arranque, pantalla y disponibilidad del
controlador PS/2 y del raton.
Memoria muestra las paginas que conoce el asignador inicial, no una lectura
completa de toda la RAM.
Centro VOS muestra framebuffer, entrada y gestor de paginas, y marca
almacenamiento, red, audio, USB y servicios de fondo como pendientes.
Ajustes cambia el tema, el acento, el idioma y la distribución del teclado.
Pulsa C para alternar azul/violeta, L para español/inglés y K para español/US,
o usa los botones. Los cambios solo duran hasta apagar o reiniciar VOS; no se
guardan en disco.
Ayuda explica los controles.
Archivos RAM permite abrir LEEME.TXT y crear hasta siete notas de 255 caracteres.
Pulsa N para crear una nota, usa las flechas para elegir un archivo, Enter para
editar y Escape para volver a la lista. El texto se guarda solo en la RAM de
VOS y se pierde al apagarlo; esta aplicacion no lee ni modifica archivos del
disco de Windows.
Aplicaciones muestra accesos a las aplicaciones incluidas y permite abrirlas.
Calculadora permite realizar operaciones enteras con el ratón o el teclado.
el icono VOS del menu superior abre Inicio y el indicador "SIN RED" abre Centro VOS.
Las descargas externas estan desactivadas: VOS aun no tiene red ni un controlador
de disco. FAT32 solo permite lectura y todavia no guarda cambios. VOSPKG1
verifica Ed25519 y SHA-256, pero el almacen de claves confiables esta vacio;
por eso todos los paquetes se rechazan y no se pueden instalar.
VOS tampoco tiene navegador funcional: faltan red, DNS, TLS y un motor para
mostrar páginas web.

SEGURIDAD Y LIMITES
-------------------

Quitar la shell elimina una superficie de entrada de comandos y las acciones
de reinicio/parada que tenia. La aplicacion Memoria solo presenta contadores;
no deja al usuario leer o escribir direcciones fisicas. La pantalla de fallo
del kernel muestra un diagnostico y detiene la CPU ante una excepcion.

Esto no convierte VOS en un sistema seguro para uso diario. Todavia no tiene
aislamiento entre aplicaciones, permisos, almacenamiento persistente, memoria
virtual completa, planificador, multitarea, actualizaciones verificadas ni
una arquitectura de servicios. La interfaz grafica se ejecuta dentro del
kernel y no tiene compositor ni proteccion contra fallos de una aplicacion.
El controlador PS/2 es basico; no cubre USB, Bluetooth ni paneles tactiles.

La API de bloques limita cada lectura o escritura al tamano del dispositivo y
al buffer indicado. La escritura solo se acepta si un controlador la declara
de forma explicita. El lector GPT no escribe y no entrega particiones si la
cabecera o su tabla no pasan las comprobaciones. Aun falta probar estos modulos
contra un disco virtual: compilar no demuestra que funcionen al arrancar.
El lector FAT32 no tiene ninguna funcion de escritura. Comprueba limites de la
particion, la FAT y las cadenas de clusters, limita el tamano de las lecturas y
rechaza estructuras corruptas. Solo ofrece nombres 8.3 y entradas de la raiz;
no ofrece aun nombres largos ni navegacion de subdirectorios.
El verificador VOSPKG1 valida Ed25519 con la biblioteca estandar de Zig y luego
calcula SHA-256 por bloques sobre el contenido. Se comprobo Ed25519 con el
vector 1 de RFC 8032, y tambien se rechazo al modificar la firma o el mensaje.
No se han definido claves oficiales para VOS; con la lista vacia, el kernel
rechaza todos los paquetes. SHA-256 por si solo no autentica al editor. El
contenido firmado sigue siendo opaco: aun no existe formato de manifiesto ni
instalador. La prueba C del formato usa un proveedor falso solo para comprobar
el flujo; no demuestra la firma real.
Hay bancos de pruebas con discos ficticios en memoria (`tests\gpt_test.c` y
`tests\storage_crypto_test.c`); `test-storage.ps1` los ejecuta si hay `cl.exe`
o `gcc.exe`, y ejecuta el vector RFC 8032 con Zig. En este entorno el vector
Ed25519 paso, pero las pruebas C de GPT, FAT32 y SHA-256 quedaron aplazadas por
falta de compilador anfitrion.

El teclado tiene cinco mapas PS/2 iniciales y parciales: español,
estadounidense, británico, francés y alemán. Las teclas aún no definidas en
cada mapa pueden usar la distribución estadounidense; hay que completar y
probar cada mapa. Archivos RAM codifica texto en UTF-8. La interfaz solo está
traducida al español e inglés. Aún faltan otros idiomas, muchas escrituras y
teclados USB.

El gestor RAM actual es una tabla fija de ocho archivos pequenos dentro del
kernel; no es todavia un VFS general y no escribe en disco. La capa FAT32 es un
lector separado que aun no se conecta a la aplicacion Archivos.
El kernel consulta el mapa de memoria entregado por Limine. El asignador
inicial solo considera regiones marcadas como utilizables, registra hasta
256 entradas de mapa, 128 regiones y 16 GiB de paginas, y pone a cero cada
pagina que asigna. El framebuffer se valida y se limita a la superficie que
entrega Limine. Estas comprobaciones reducen errores comunes, pero necesitan
revision y pruebas antes de usar VOS fuera de un emulador.

El script crea dos imagenes para medios distintos. Para el Sony VAIO que solo
arranca con UEFI, usa `VOS-Desktop-UEFI-GPT.iso` en Rufus: GPT, UEFI (sin CSM),
FAT32 y modo ISO. `VOS-Desktop-Legacy-MBR.iso` lleva el cargador BIOS instalado
en el MBR del propio archivo y es para equipos antiguos con BIOS Legacy; si Rufus
pregunta, elige DD. Una sola configuracion de Rufus no cubre todas las
combinaciones de firmware y tabla de particiones.

Ambas imagenes contienen entradas BIOS y UEFI para CD virtual. La comprobacion
estructural no garantiza el arranque en todos los equipos; el VAIO aún debe
probarse por separado. VOS no tiene controlador de disco activo ni red. Los
archivos y ajustes no persisten al apagar; no hay navegador, descargas ni
actualizaciones. El diseño usa elementos propios de VOS —fondo abstracto, dock,
ventanas y Ajustes—, no recursos ni logotipos de macOS.

CARPETAS DEL PROYECTO
---------------------

src/boot/             Protocolo de arranque Limine; no es un bootloader propio.
src/kernel/           Entrada e inicializacion del kernel.
src/arch/x86_64/      Excepciones y acceso de bajo nivel a PS/2.
src/drivers/input/    Eventos, teclado y raton PS/2.
src/memory/           Asignador inicial de paginas.
src/storage/          API de bloques, GPT, lector FAT32 y archivos temporales.
src/security/         SHA-256, Ed25519 y verificacion estructural de paquetes.
src/ui/               Framebuffer, escritorio, ventanas y aplicaciones.
docs/                 Arquitectura, limites y decisiones de seguridad.
tests/gpt_test.c   Casos para probar GPT contra un disco ficticio en memoria.
tests/storage_crypto_test.c  Casos para SHA-256, VOSPKG1 y FAT32 de prueba.
test-storage.ps1   Ejecuta esos casos con cl.exe o gcc.exe, si estan disponibles.
boot/limine.conf   Configuracion del menu de arranque.
linker.ld          Organizacion del kernel ELF x86-64.
build.ps1          Compila recursivamente el kernel y guarda la salida en build/.
make-iso.ps1       Compila y crea las imagenes UEFI/GPT y BIOS/MBR.
run-bochs.ps1      Arranca la imagen Legacy en Bochs sin conectar discos duros.
tools/create_iso.py Crea la ISO UEFI/GPT con entradas BIOS/UEFI para CD.
tools/verify_iso.py Comprueba la estructura de la imagen UEFI/GPT.
tools/verify_legacy_iso.py Comprueba el MBR y archivos de arranque Legacy.
Makefile           Reglas GNU para kernel, ISO y prueba opcional en QEMU.

Consulta docs/ARQUITECTURA.md y docs/COMPATIBILIDAD.md para los límites de
arquitectura y hardware. El kernel detecta el fabricante Intel de la CPU,
pero todavía no incluye un controlador gráfico Intel.

Consulta docs/ARQUITECTURA.md para el mapa de inicializacion y las fronteras
entre componentes. Todos los modulos siguen ejecutandose en modo kernel; las
carpetas ordenan el codigo, pero todavia no aislan aplicaciones ni servicios.

COMPILAR EL KERNEL EN WINDOWS
----------------------------

Con Zig 0.17.0 extraido en `Documentos\Codex\herramientas`, abre PowerShell
en la carpeta `Vos` y ejecuta `.\build.ps1`. El script compila los archivos C
de las subcarpetas junto con el verificador Zig y crea `build\kernel.elf`.
Tambien guarda las cachés de Zig en `build`; no
cambia la configuracion de Windows, no crea una ISO y no arranca el kernel en
el PC. Si Zig esta en otra ruta, define `VOS_ZIG` en esa ventana de PowerShell
con la ruta completa a `zig.exe`.

La compilacion se verifico con Zig 0.17.0: produjo un ELF de 64 bits para
x86-64 y su punto de entrada cae en la direccion alta definida por
`linker.ld`. La nueva ISO incluye una pantalla gráfica de inicio antes del
asistente de idioma. Las dos imágenes se comprueban estructuralmente; eso no
demuestra el arranque en hardware real.

El Makefile conserva los pasos para un entorno GNU/Linux. En Windows, las
herramientas de esta prueba estan en `.tools` dentro del proyecto; no se
instalaron globalmente ni se cambiaron opciones de Windows.

En Windows, abre PowerShell en la carpeta `Vos` y ejecuta:

```powershell
.\make-iso.ps1
.\run-bochs.ps1
```

`make-iso.ps1` compila el kernel y genera:

- `build\VOS-Desktop-UEFI-GPT.iso`: para el VAIO, en Rufus usa GPT, UEFI (sin
  CSM), FAT32 y modo ISO.
- `build\VOS-Desktop-Legacy-MBR.iso`: para BIOS Legacy, en Rufus elige DD si
  aparece la pregunta de modo.

El script solo crea archivos en `build`; no escribe en la USB. Rufus borrará la
memoria cuando pulses Empezar.

`run-bochs.ps1` arranca la imagen Legacy como CD virtual. Bochs no conecta discos
virtuales ni unidades del PC. El generador no sobrescribe ISO existentes: elige
otros nombres con `-IsoName` y `-LegacyIsoName` para generar nuevas.
PROXIMOS PASOS DEL PROYECTO
--------------------------

El siguiente paso es repetir el arranque en QEMU y completar las pruebas de
pantalla, teclado y raton. El paso de almacenamiento es implementar un
controlador VirtIO-blk para QEMU y probar la GPT con una imagen temporal; despues se puede montar un sistema de archivos
de solo lectura. Luego hacen falta escritura recuperable, procesos aislados y
servicios con permisos. Hasta que existan esas protecciones y pruebas, VOS
sigue en desarrollo y no está listo para uso diario ni para comercializarse.
Consulta ROADMAP.md para ver el trabajo pendiente y docs/LICENCIAS.md para el
inventario preliminar de licencias.
