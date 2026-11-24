# ZeroTVBGone

TV-B-Gone para el **M5Stack Cardputer Zero** (Raspberry Pi CM0, pantalla de 320×170, Debian 13
arm64): manda por el LED infrarrojo los códigos de apagado de casi todas las teles. En el
lanzador se llama **TV-B-Gone**. Qué hace y cómo se usa está en `README.md`; aquí va lo que hace
falta para tocarla.

No es una app de Android: de `~/CLAUDE.md` valen el estilo (comentarios, documentación y
commits en español explicando el porqué; pocas dependencias; lógica pura con pruebas), que el
repositorio es **público** (ni secretos ni datos personales) y la entrega; lo de Gradle, firma
y GrapheneOS no aplica.

## Compilar y comprobar

`Tools/compilar.sh` usa Docker si lo hay (así se hizo en el servidor, que no tiene CMake ni
compilador cruzado) y, si no, las herramientas de la máquina: los paquetes de `Tools/Dockerfile`.

```sh
Tools/compilar.sh pruebas   # simulador (SDL2) + ctest; tiene que pasar
Tools/compilar.sh deb       # dist/ZeroTVBGone_<versión>_rev1_arm64.deb
```

- La primera vez descarga LVGL 9.5 y el *sysroot* de M5Stack a `.cache/` (no se versiona). Si
  `../ZeroGallery/.cache` existe, copiarlo ahorra la descarga: es lo mismo.
- GitHub Actions hace lo mismo en cada push (`.github/workflows/build.yml`).
- **Probar sin pantalla**: el simulador corre bajo `Xvfb` dentro de la misma imagen
  (`SDL_RENDER_DRIVER=software`), se le mandan teclas con `xdotool key` y se captura con
  `import -window root`; la pantalla del aparato queda en `320x170+540+268` de esa captura
  (de ahí salen las de `screenshot/`).
- **El simulador no emite**: hace la pasada con sus tiempos y pone «Simulating». Lo que es
  hablar con el LED (`src/platform/ir_blaster.cpp`) sólo se puede probar en el aparato.
  `ZEROTVBGONE_LIRC=/dev/lircN` fuerza un emisor y `=sim` la simulación, también en el aparato.
- **OJO**: en el simulador la configuración es `config/zerotvbgone.conf` del propio
  repositorio, y cambiar de región o de tema la reescribe. Dejarla en `dark_mode=yes`,
  `region=eu` antes de hacer commit.
- **El aparato** es `cpzero` en la red de Tailscale (`ssh cpzero`, usuario `bruno`, con
  `sudo` sin contraseña): el `.deb` se copia y se instala con `sudo apt-get install -y`. Lo
  que haya que mirar en la pantalla lo confirma Bruno: hasta entonces se dice («sin probar en
  el aparato») en el «Estado».

## De dónde viene

- **La base** es la de `../ZeroGallery` (la plantilla oficial `CardputerZero/Template`, commit
  `477fa6d`, ya renombrada y con `Tools/compilar.sh`): CMake, simulador de escritorio
  (`src/app/desktop_*`), registro, *bindings* reactivos, barra de título, aviso flotante y ayuda
  se conservan tal cual. Un arreglo en esas piezas comunes conviene llevarlo a las dos.
- **Los códigos** son `firmware/WORLDcodes.c` de `github.com/adafruit/TV-B-Gone-kit` (commit
  `4197eeb`), convertidos por `Tools/generar_codigos.py` a `src/model/tvbgone_codes.cpp`. Ese
  fichero no se edita a mano. Es CC BY-SA 3.0 (el resto, MIT): `LICENSE` lo explica y el `.deb`
  lo lleva dentro; no quitar la autoría.
- **Cómo emitir** está sacado de la app oficial `CardputerZero/IR-Remote`: LIRC del núcleo a
  pelo (`open`/`ioctl`/`write` sobre `/dev/lircN`), eligiendo el dispositivo por lo que sabe
  hacer (`LIRC_CAN_SEND_PULSE`) y no por su número, porque el receptor es otro `/dev/lircN`.
  El emisor es `gpio-ir-tx` en el GPIO 12 (el receptor, GPIO 13).

Convenciones del aparato que la app respeta (documentadas en `CardputerZero/launcher` y
`m5stack/CardputerZero-AppBuilder`):

- Se instala como `.deb` con un `.desktop` en `/usr/share/APPLaunch/applications/`; el
  lanzador ejecuta el binario y le cede la pantalla (`/dev/fb0`) y el teclado (evdev).
- `Esc` corto es «atrás» (aquí, parar) y `Esc` mantenido sale; `Fn`+`H` ayuda, `Fn`+`J` captura.
- Las flechas están en `F`/`Z`/`X`/`C` con `Fn`. Aquí valen también sin `Fn`: las letras
  llegan tal cual y `tvbgone::action_for_key` las trata como flechas.

## Decisiones

- **Los tiempos los lleva el núcleo**, no la app: se le escriben pulsos y silencios en
  microsegundos y él genera la portadora. Límites de cada `write` (`lirc_dev.c`): número impar
  de tiempos, ninguno a cero, medio segundo en total. Por eso `tvbgone::split_bursts` corta
  cada código por sus silencios de ≥10 ms (entre tramas repetidas) y esos los espera la app.
- **Portadora en hercios tal como la escribe el original**, no el valor redondeado del
  temporizador del AVR. Ciclo de trabajo al 50 %, como el original: es lo que más alcance da.
- **Hay códigos sin portadora** (10 distintos, 18 entre las dos listas) (`carrier_hz == 0`). `gpio-ir-tx` los admite; si el emisor
  no (pasa con `pwm-ir-tx`), se saltan y la pantalla dice cuántos al acabar.
- **205 ms entre códigos** (`kGapBetweenCodesMs`). El firmware 1.2 del kit pone 250; 205 es lo
  que usan las versiones posteriores y los ports, y ahorra unos segundos por pasada.
- **Un código empezado se acaba** aunque se pida parar; lo que se interrumpe es la espera.
- **Si los tres primeros códigos fallan al escribir, se deja** y se enseña el error, en vez de
  contar 138 códigos que no salen.
- **La pantalla pregunta al hilo cada 100 ms** (`MainScreen::poll_timer_cb`): el hilo no puede
  tocar LVGL y así no hace falta pasar avisos entre hilos.
- **No se cambia de región a media pasada.**
- **La interfaz está en inglés**, como las demás apps del aparato.

## Versiones y entrega

- La versión está en `project(... VERSION x.y.z)` de `CMakeLists.txt` y en `app-builder.json`:
  se suben a la vez con cada cambio que llegue al aparato, y se anota en «Estado» de aquí y
  del `README.md`.
- El `.deb` se copia a `~/Documentos/Software/ZeroTVBGone-<versión>.deb`, sustituyendo al anterior.
- Commit en `main` con mensaje en español que empieza por la versión, y push.
- `app-builder.json` ya tiene la ficha para la tienda del Cardputer Zero (`czdev publish`),
  pero **no está publicada**: la primera publicación pide un vídeo de la app en el aparato.
- El icono sale de `Tools/generar_icono.py` (Pillow).

## Estado

- **0.1.1 (20-nov-2026)**: la app ya no tiene **un núcleo al 100 %** en el aparato. Sin
  `lv_delay_set_cb`, `lv_delay_ms(5)` del bucle principal esperaba dando vueltas (lo cazó
  ZeroGPS con `top` y gdb: el hilo de LVGL siempre dentro de `lv_delay_ms`); venía de la
  plantilla. Ahora `app.cpp` da a LVGL `clock_gettime` y `usleep` en el aparato y duerme lo
  que `lv_timer_handler()` dice que falta (1-50 ms). Instalada en el aparato; sin probar a mano.
- **0.1.0 (16-nov-2026)**: primera versión. Las dos listas del original, elección de región
  (se recuerda), cuenta y barra de progreso, parar con `Enter` o `Esc`, tema claro/oscuro.
  Probada en el simulador y **en el aparato: Bruno la probó con su tele LG y la apaga**. Le
  pareció poco potente: no hay nada que subir desde el programa (ya va al 50 % de ciclo de
  trabajo, que es el máximo útil; la corriente del LED la fija el hardware).
