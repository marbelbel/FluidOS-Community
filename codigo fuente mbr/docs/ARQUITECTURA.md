# Arquitectura actual de VOS

VOS es un sistema x86-64 en desarrollo. Esta estructura separa responsabilidades
para facilitar el trabajo; no implica aislamiento de procesos ni que los
componentes estén listos para uso diario o distribución comercial.

## Mapa del código

| Carpeta | Responsabilidad actual | Límite importante |
| --- | --- | --- |
| `src/boot/` | Tipos y solicitudes del protocolo Limine | Limine carga el kernel; VOS no implementa su propio bootloader |
| `src/kernel/` | Entrada e inicialización del kernel | Sin planificador, procesos ni llamadas al sistema |
| `src/arch/x86_64/` | Detección de fabricante Intel, excepciones y operaciones de bajo nivel PS/2 | El kernel solo acepta CPUs Intel x86-64; aún no cubre dispositivos modernos |
| `src/drivers/input/` | Eventos, teclado y ratón PS/2 | Sin USB HID, Bluetooth ni pantalla táctil |
| `src/memory/` | Asignador inicial de páginas físicas | Sin memoria virtual completa, heap general ni aislamiento |
| `src/storage/` | API de bloques, GPT, lectura FAT32 y RAMFS temporal | No hay un controlador de disco registrado; FAT32 es solo lectura |
| `src/security/` | SHA-256, Ed25519 y validación VOSPKG1 | El almacén de claves confiables está vacío; no se acepta ningún paquete |
| `src/ui/` | Framebuffer, escritorio, ventanas y aplicaciones | La interfaz corre en modo kernel y puede bloquear todo el sistema |

El asistente inicial permite seleccionar español o inglés y propone teclado
ES o US. Ajustes recorre cinco mapas PS/2 parciales: ES QWERTY, US QWERTY,
UK QWERTY, FR AZERTY y DE QWERTZ; las teclas no definidas caen al mapa
estadounidense. Las notas temporales aceptan UTF-8 y caracteres latinos básicos.
Faltan otros idiomas, muchas escrituras, teclados USB y persistencia.

## Orden de inicialización

1. Limine entrega al kernel el framebuffer, el mapa de memoria y la dirección
   HHDM.
2. El kernel instala el diagnóstico de excepciones e inicializa el dibujo.
3. Inicializa la API de bloques (sin registrar discos) y el asignador de páginas.
4. Prepara entrada PS/2. Si no hay teclado, muestra un aviso y se detiene.
5. Comprueba que el fabricante de CPU sea Intel mediante CPUID.
6. Entra en el escritorio gráfico.

## Reglas de seguridad vigentes

- Ningún módulo accede a un disco físico: no existe controlador que lo registre.
- La API de bloques limita rangos y tamaños; la escritura requiere permiso
  explícito del controlador. En esta versión no hay controlador de disco.
- GPT y FAT32 solo leen. FAT32 limita sectores y recorridos de clústeres.
- El verificador de paquetes comprueba estructura, firma y hash, pero una lista
  de claves vacía significa que todos los paquetes se rechazan.
- No hay red, descarga, instalación ni actualización automática.
- La salida gráfica usa el framebuffer que entrega Limine. No hay un
  controlador de GPU Intel propio ni aceleración gráfica.
- Los documentos de Archivos RAM y los ajustes son volátiles; se pierden al
  apagar. No se anuncia persistencia en disco.
- No se debe arrancar VOS en un equipo real. La siguiente validación de arranque
  debe usar una imagen de disco virtual desechable en QEMU.

## Próximas capas

El siguiente camino seguro es compilar y arrancar la imagen en QEMU; añadir un
controlador VirtIO-blk aislado para el dispositivo virtual; validar GPT y FAT32
contra esa imagen; y solo después diseñar escrituras con recuperación. La red,
la tienda y las actualizaciones esperan una raíz de confianza, almacenamiento
persistente y pruebas de recuperación. Las aplicaciones necesitan modo usuario
y permisos antes de considerarse aisladas.

La interfaz puede adoptar patrones de escritorio conocidos —dock, ventanas,
ajustes claros y aplicaciones accesibles— sin copiar recursos ni código de
macOS. La compatibilidad internacional también requiere catálogos traducibles,
fuentes Unicode y diseños de teclado, no solo cambiar las etiquetas visibles.
