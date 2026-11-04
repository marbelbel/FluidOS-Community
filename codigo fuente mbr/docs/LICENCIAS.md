# Licencias y distribución comercial de VOS

Este documento es un inventario preliminar del árbol actual de código. No es
asesoramiento jurídico ni una certificación de que una imagen concreta pueda
publicarse. Hay que repetir el inventario para cada versión y para cada
binario, recurso o paquete que se agregue.

## Estado del codigo propio

En la raíz actual no hay un archivo `LICENSE`, `COPYING` ni una declaración que
asigne una licencia a todo el código de VOS. Por tanto, el proyecto todavía no
comunica de forma clara los derechos de redistribución del código propio. Esto
no equivale a una licencia «no comercial» ni impide por sí solo vender el
producto: significa que antes de publicar hay que aclarar quién posee cada
contribución y elegir una licencia.

No se ha añadido una licencia para el código de VOS en este cambio. La elección
depende del modelo del producto:

- Una licencia permisiva puede permitir vender el sistema y redistribuir el
  código, sujeto a sus condiciones y avisos.
- Un modelo propietario también puede venderse, pero requiere derechos claros
  sobre todas las contribuciones y condiciones de distribución redactadas para
  VOS.
- No elegir una licencia «solo uso no comercial» si se quiere vender VOS o
  permitir que terceros lo comercialicen.

## Componentes identificados

| Componente | Uso actual | Licencia / estado | Acción para publicar |
| --- | --- | --- | --- |
| Declaraciones del protocolo Limine en `src/boot/limine.h` | Tipos y solicitudes de arranque | El archivo indica que se basa en el encabezado oficial del protocolo, publicado bajo 0BSD | Guardar la referencia y registrar la versión exacta del encabezado de origen |
| Cargador Limine | La receta de ISO espera archivos de un paquete externo; ese paquete no está en el árbol actual | El proyecto Limine declara BSD-2-Clause y enumera software de terceros con licencias propias | Revisar el paquete exacto que se redistribuya; incluir sus licencias y avisos de terceros, sin asumir que basta con la licencia principal |
| Biblioteca estándar de Zig | `src/security/package_crypto.zig` usa `std.crypto.sign.Ed25519` | MIT en la distribución local de Zig consultada; el compilador se usa para construir y no se incluye actualmente en la ISO | Si el binario publicado incorpora código de la biblioteca, conservar el texto MIT y comprobar si el build final incorpora otros componentes |
| Resto del código y recursos de VOS | Kernel, interfaz, textos, fuentes y pruebas presentes | No se encontró una licencia global declarada; este inventario no certifica autoría ni procedencia de cada recurso | Registrar autor y origen de cada archivo; completar una auditoría de recursos antes de cada versión |

Las referencias oficiales consultadas son el [protocolo Limine y su licencia
0BSD](https://github.com/Limine-Bootloader/limine-protocol), el [proyecto
Limine, su licencia principal y sus avisos de software de terceros](https://github.com/limine-bootloader/limine),
y el [texto MIT de Zig incluido en la distribución local](https://github.com/ziglang/zig/blob/master/LICENSE).

## Lista previa a una distribucion

1. Acordar quién es titular de VOS y confirmar los derechos de cada contribución.
2. Elegir y publicar la licencia del codigo propio; conservar el texto completo.
3. Crear un inventario por versión de fuentes, binarios, librerías, tipografías,
   iconos, imágenes, sonidos, documentación y marcas.
4. Fijar versiones y hashes de Limine, Zig y cualquier otra dependencia.
5. Incorporar en la imagen distribuida los textos y avisos que exijan esas
   licencias. Para Limine, usar los avisos del paquete exacto que se empaquete.
6. Revisar que no se incluyan recursos de macOS/Apple ni marcas ajenas sin
   autorizacion. Se pueden crear patrones de escritorio propios inspirados en
   convenciones conocidas, con nombre, codigo y recursos visuales de VOS.
7. Hacer una revisión legal de la versión y sus materiales de venta en los
   territorios donde se vaya a ofrecer.

## Limite de la revision actual

La ISO no se ha generado y el paquete binario de Limine no está presente, así
que no se puede certificar el conjunto final de avisos ni el contenido de una
imagen publicable. El proyecto tampoco está listo funcionalmente para venderse:
no tiene almacenamiento persistente conectado a un disco, navegador, red,
aislamiento de aplicaciones ni actualizador operativo. Consulta
`README.txt` y `ROADMAP.md` para los limites tecnicos vigentes.
