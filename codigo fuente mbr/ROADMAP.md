# VOS: ruta hacia un sistema utilizable

VOS es un sistema en desarrollo. Para acercarse a una versión estable, cada
etapa debe compilarse y probarse en una máquina virtual antes de añadir la
siguiente. El diseño prioriza una interfaz gráfica sencilla y mantiene la
terminal fuera de la experiencia de usuario.

## 1. Arranque y diagnóstico

- [x] Compilar el kernel de forma reproducible con Zig y revisar el ELF.
- [x] Preparar un script de PowerShell que construye la ISO en `build` cuando
  están disponibles xorriso y los binarios de Limine.
- [x] Conectar la entrada del kernel al escritorio; retirar el bucle de la
  consola antigua y solicitar el mapa de memoria y el HHDM a Limine.
- [ ] Crear la ISO con Limine y xorriso.
- [ ] Arrancar en QEMU con almacenamiento temporal y sin red.
- Probar errores de framebuffer, memoria y teclado y mostrar mensajes claros.
- No arrancar en hardware real durante esta etapa.

## 2. Núcleo y hardware

- Añadir tabla TSS y pilas de excepción dedicadas antes de depender de ellas.
- Completar memoria virtual, heap y comprobaciones de límites.
- Añadir temporizador y planificador solo después de definir sincronización.
- Enumerar PCI y añadir controladores en pasos pequeños; el teclado USB requiere
  USB host y HID, no solo leer el controlador PS/2.
- Mantener los controladores en modo seguro y fallar con un aviso entendible.

## 3. Almacenamiento, archivos y aplicaciones

- [x] Añadir una API común de dispositivos de bloques con comprobación de
  límites, tamaño de transferencia y escritura desactivada por defecto.
- [x] Añadir un lector GPT primario de solo lectura que valida CRC, rangos y
  solapamientos antes de entregar particiones.
- [x] Añadir una base FAT32 de solo lectura: montaje acotado, listado de la raiz
  con nombres 8.3 y lectura de archivos con comprobacion de cadenas de clusters.
- [x] Añadir SHA-256 incremental y el formato de paquete VOSPKG1 con limites
  estrictos y comprobacion del hash de la carga una vez validada la firma.
- [x] Hacer que VOSPKG1 falle cerrado mientras el proveedor interno no tenga
  una clave confiable y un verificador implementado.
- [ ] Añadir un controlador VirtIO-blk para QEMU; no descubrir ni registrar
  discos físicos en esta etapa.
- [x] Integrar Ed25519 estricto desde la biblioteca estandar de Zig y validar el
  vector 1 de RFC 8032; la prueba rechaza firma y mensaje alterados.
- [ ] Definir y proteger las claves de release, e incluir la clave publica oficial
  de VOS. Hasta entonces la lista esta vacia y no se acepta ningun paquete.
- [ ] Ejecutar los bancos GPT, FAT32 y SHA-256 en memoria cuando haya un
  compilador C anfitrion y volver a verificar el almacenamiento dentro de QEMU.
- Probar la capa solo con una imagen de disco temporal de QEMU.
- Leer la GPT secundaria y añadir soporte MBR como recuperación y compatibilidad.
- [x] Montar primero FAT32 en modo de solo lectura.
- Habilitar escrituras después de validar límites, sincronización y recuperación.
- Persistir documentos, ajustes y aplicaciones en disco. Reservar la RAM para
  cachés con límite, buffers de E/S y datos temporales que se puedan reconstruir.
- Añadir formatos por etapas, empezando por los que se puedan probar y mantener;
  cada formato requiere sus propios límites y recuperación de errores.
- [x] Crear el lanzador Aplicaciones para mostrar y abrir las aplicaciones
  incluidas sin simular descargas ni instalaciones externas.
- Habilitar descargas solo después de tener red, un controlador probado,
  almacenamiento persistente, verificación Ed25519 con claves confiables y
  desinstalación recuperable. El formato VOSPKG1 existe, pero la verificación
  criptográfica real y las claves oficiales aun no estan incorporadas.
- No incluir navegador hasta tener un dispositivo de red y una pila de red
  probada; después hacen falta DNS, TLS con certificados confiables, tiempo
  seguro y un motor de páginas aislado del kernel.
- Sustituir la app de notas RAM actual por el explorador persistente cuando
  exista un dispositivo probado. Las notas RAM actuales son solo una demo volátil.
- Mantener Ajustes como aplicación gráfica y añadir funciones cuando tengan
  soporte real del kernel.

## 4. Idiomas y accesibilidad

- [x] Separar los textos de la interfaz en un catálogo inicial y permitir elegir
  español o inglés al inicio y desde Ajustes. La elección dura solo la sesión.
- Añadir catálogos revisados para más idiomas y escrituras; guardar la selección
  solo después de tener almacenamiento persistente fiable. El idioma actual de
  la interfaz es español o inglés; las demás traducciones aún no existen.
- [x] Añadir una base parcial para mapas PS/2 ES QWERTY, US QWERTY, UK QWERTY,
  FR AZERTY y DE QWERTZ; las teclas no definidas usan el mapa estadounidense.
  Ajustes recorre los mapas. El idioma es una preferencia separada.
- [x] Codificar escritura básica de notas en UTF-8 y dibujar las letras latinas
  cubiertas por la fuente integrada.
- Añadir los demás mapas de teclado y soporte USB HID antes de anunciar
  compatibilidad con más idiomas, equipos o distribuciones de teclado.
- Probar diseños de teclado, formatos de fecha, hora y números por región.

## 5. Aislamiento e instalación

- Separar aplicaciones del kernel mediante modo usuario, memoria protegida y
  llamadas al sistema pequeñas.
- Ejecutar aplicaciones sin privilegios y conceder capacidades mínimas.
- Instalar solo paquetes con manifiesto y firma válidos; validar tamaño, hash,
  arquitectura y permisos antes de activar un paquete.
- Preparar actualización y desinstalación reversibles, con registro claro y
  recuperación si una aplicación falla.

## 6. Actualizaciones para varios equipos

- Definir primero quién publica VOS y cómo se protege la clave privada de firma.
- Publicar manifiestos firmados con versión, arquitectura, tamaño y hash del
  paquete. El equipo solo necesita la clave pública para verificar releases.
- Usar HTTPS para transportar datos y firma digital para comprobar el origen;
  HTTPS por sí solo no autoriza una actualización.
- Descargar en una zona temporal, verificar todo, instalar en una ranura
  inactiva y cambiar a ella solo al final.
- Confirmar el arranque nuevo; volver automáticamente a la versión anterior si
  falla. Añadir opción de aplazar, historial y recuperación local.
- Probar releases gradualmente en máquinas virtuales y equipos de prueba antes
  de ofrecerlas al resto de usuarios.

La interfaz de actualización debe mostrar estados sencillos: disponible,
descargando, verificando, lista para reiniciar, completada o revertida. Nunca
debe ejecutar paquetes sin verificar ni borrar la única versión arrancable.

## 7. Distribución y licencias

- [ ] Elegir y declarar la licencia del código propio de VOS antes de publicar
  una versión. No usar licencias "solo para uso no comercial" si el objetivo
  es vender o permitir que otros vendan el sistema.
- [ ] Mantener un inventario de código, binarios, fuentes, iconos, tipografías,
  documentación y herramientas incluidas en cada imagen publicada.
- [ ] Fijar versiones y hashes de las dependencias; incluir sus textos de
  licencia y avisos de terceros dentro de cada distribución.
- [ ] Revisar las licencias y avisos del paquete exacto de Limine que se vaya a
  incluir, además de la licencia principal del proyecto Limine.
- [ ] Confirmar la titularidad y autorización de todas las contribuciones y
  recursos antes de ofrecer VOS a clientes.
- [ ] Revisar el nombre, logotipo y materiales visuales de VOS para que sean
  propios y no se confundan con marcas o recursos de terceros.

## Estado de hoy

El kernel tiene framebuffer, escritorio básico, entrada PS/2 sondeada,
diagnóstico de excepciones, un asignador inicial de páginas, una API de bloques,
un analizador GPT, un lector FAT32 de solo lectura, SHA-256 y el validador
estructural VOSPKG1. Aún no tiene un controlador que registre discos, escritura
persistente, claves de firma de VOS, red, procesos aislados,
permisos ni actualizador. Por eso VOS
no está listo para uso diario, distribución comercial ni uso en el PC real.

La aplicación Archivos RAM es solo una demostración temporal. Todavía no hay
controlador de disco ni sistema de archivos persistente. La compatibilidad
internacional también es parcial: la fuente actual cubre ASCII y algunas letras
latinas, no todas las escrituras ni todos los diseños de teclado.

El kernel se compila con `build.ps1`. `make-iso.ps1` crea una imagen UEFI/GPT y
otra BIOS/MBR, ambas con escritorio gráfico y pantalla de inicio VOS. Las
comprobaciones estructurales de ambas imágenes pasan. Falta validar el nuevo
arranque gráfico en más emuladores y en hardware de prueba. Red y almacenamiento
persistente siguen pendientes; no se deben presentar como funciones existentes.
