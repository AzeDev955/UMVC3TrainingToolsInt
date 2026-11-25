UMVC3 Training Tools is a Windows mod for the PC version of Ultimate Marvel vs. Capcom 3. It adds team editing, reusable practice setups, dummy reactions, and tools for inspecting and adjusting Training mode.

[English](#windows-installation) · [Español](#instalación-en-windows) · [Report a bug](#report-a-bug) · [Reportar un fallo](#reportar-un-fallo)

This fork builds on the mod by EternalYoshi. Thanks to EternalYoshi, HKHaan, Sheep, Techs, anotak, Ermaccer, and the other contributors. [Original repository](https://github.com/EternalYoshi/UMVC3TrainingToolsInt)

Este fork parte del mod de EternalYoshi. Gracias a EternalYoshi, HKHaan, Sheep, Techs, anotak, Ermaccer y el resto de colaboradores. [Repositorio original](https://github.com/EternalYoshi/UMVC3TrainingToolsInt)

## Windows installation

1. Close the game, download `UMVC3TrainingToolsInt-Setup.exe`, and run it. [GitHub Releases](https://github.com/AzeDev955/UMVC3TrainingToolsInt/releases)
2. The installer searches your Steam libraries. Use **Browse** and select `umvc3.exe` if needed.
3. Choose your language, click **Install**, then click **Play**. You can change the mod's interface language later from the **Language** menu.

The installer adds the x64 ASI Loader if `dinput8.dll` is missing and backs up replaced files in `UMVC3TrainingToolsInt-Backups`. At startup the mod checks GitHub Releases. If you accept an update, it downloads and verifies the installer, closes the game, installs the update, and relaunches the game. Windows may request administrator permission for protected folders.

Use windowed mode if the tools window fails to open in exclusive fullscreen. Timestone can provide a borderless fullscreen window.

### Quick Training and presets

- In the main menu, press **F4** to open **Quick Training**. Choose the stage and three different fighters per team. Expand each fighter's row to choose their assist, one of the six base colors, or an installed **Alternate costume**. Use the arrows to change team order.
- Choose **Start Training** to enter the match. Where both launch methods are supported, the **Direct launch (experimental)** checkbox lets you switch between them. When direct launch is the only compatible route, the panel indicates that it will use it.
- Give the setup a name and choose **Save preset**. To include the starting and reset position and the dummy's reactions, enable **Include exercise settings** and open **Configure exercise**. Choose center or either corner, then configure each reaction's move and delay. Saved exercises support grid moves and custom commands. You can save recording slots separately in the **Recording** tab.
- Choose **Load preset**, then start Training. Use **Update preset** to replace or rename the selected preset, **Save as new** to make a copy, or **Delete preset** to remove it. Older team presets are supported. Files are stored in `%LOCALAPPDATA%\UMVC3TrainingToolsInt\QuickTraining`.
- During a match, press **F5** and open **Preset** to save the current setup or apply a saved preset. When saving an exercise here, choose its reset position and the mod captures the dummy reactions currently configured in **Recording**. You can apply a preset on the current stage. To play on the preset's saved stage, use **F4** from the main menu.
- In **F5 → Teams**, expand the fighter rows to edit the current teams and choose **Apply and reload**. Use **Reset draft** to discard unsubmitted edits. Stop recording or playback before applying team changes.

### Recording and dummy reactions

1. Open **F5 → Recording** and choose one of the five slots. You can record and play back P1, P2, or both. Use the save and load buttons to keep a recording file for either player.
2. To record P2 with P1's controller, choose **Record dummy with my controller**. Reopen **Recording** and choose **Stop recording** to give the controller back to P1.
3. Under **Dummy reactions**, configure what P2 does after hitstun, blockstun, wakeup, or when coming in after a KO. Each reaction can be disabled or use a P2 recording, a grid move, or a custom input. Set its delay from 0 to 120 frames. The incoming reaction can start before P2 lands.
4. For custom inputs, use numpad directions `1–9` and buttons `L M H S A1 A2`. Use `+` for simultaneous buttons, as in `623L+M`, and press **Apply** or **Enter**.

### In-match tools

| Tab | Controls |
| --- | --- |
| **Extra** | Restart at the intro, at **Ready**, or directly into Training. Set horizontal positions for both players, use center and corner presets, enable unlimited X-Factor per player, or move the input display to the left. Adjust game speed or either team's character speed, including the Viewtiful Joe and Vale of Mist slowdown presets. |
| **Status** | Set each fighter's starting health on restart and optionally apply it to red health. Set or lock each team's Hyper Meter and toggle Jamming Bomb's reversed controls per fighter. Turn off **Life Recovery** in the game's pause menu when setting individual health. |
| **Character** | Set Frank West's level, choose and lock Phoenix Wright's evidence and toggle Turnabout, toggle Dark Phoenix, set and lock MODOK's Understanding, Dormammu's spell charges, or Deadpool's teleport count, and break Wesker's glasses now or at restart. Enable unlimited timed transformations for base-game fighters. |
| **Data** | Inspect remaining hitstun and blockstun for each fighter and ground and air hitstun counters. |
| **Incoming** | Practice entries after a KO. Set the KO delay and its X/Y position, use a player's start position or a center/corner shortcut, KO either active fighter on restart, or toggle KO per fighter. Turn off **Life Recovery** in the game's pause menu for these experimental controls. |

Some effects keep their original visual or audio state when set by the mod, including Jamming Bomb, Turnabout, Dark Phoenix, Frank West's level aura, and MODOK's flame. Use **File → Reset Settings To Default** to restore the mod's settings.

### Controller shortcuts

- By default, **View / Select** opens Quick Training from the main menu, **R3** opens the tools window, and **L3** resets Training. Change or disable these shortcuts in **Controller shortcuts**. Bindings are saved in `UMVC3TrainingToolsInt.ini`.
- Hold **left/right** on the D-pad or left stick while resetting to put P2 in that corner. **Down** returns both players to center, and **up** swaps their reset positions. A reset with no direction uses the exercise position if active, or repeats the last directional reset. Before the first directional reset, it uses the positions in **Extra → Positioning**.
- Use the D-pad or left stick to navigate, **A / Cross** to confirm, and **B / Circle** to close a dropdown or the tools window. The mod supports XInput and controllers exposed as XInput through Steam Input. Release the controls after closing a tools window before resuming play.

### Report a bug

1. Reproduce the issue and note the steps, what you expected, and what happened. Include the mod version and any other enabled mods. For input issues, specify whether you used a keyboard or a controller.
2. Open **File → Open logs folder** from the F4 or F5 window. If the window is unavailable, look in `logs` next to `umvc3.exe`, then in `%TEMP%\UMVC3TrainingToolsInt\logs`.
3. Collect the relevant logs.

   - `UMVC3TrainingToolsInt-Teams.log` covers startup, Quick Training, presets, team changes, and restarts.
   - `UMVC3TrainingToolsInt-Recording.log` covers controller input, recording, playback, and dummy reactions. If **File → Open recording logs folder** appears, use it to find this file.
   - If the current log starts after the incident, check the corresponding `.previous.log`. The logs use UTC.

4. Open an issue with your notes and attach the relevant files. If GitHub rejects the `.log` attachments, put them in a ZIP. [GitHub Issues](https://github.com/AzeDev955/UMVC3TrainingToolsInt/issues)

## Instalación en Windows

1. Cierra el juego, descarga `UMVC3TrainingToolsInt-Setup.exe` y ejecútalo. [GitHub Releases](https://github.com/AzeDev955/UMVC3TrainingToolsInt/releases)
2. El instalador busca el juego en tus bibliotecas de Steam. Si hace falta, usa **Examinar** para seleccionar `umvc3.exe`.
3. Elige el idioma, pulsa **Instalar** y después **Jugar**. Puedes cambiar el idioma de la interfaz del mod más tarde desde el menú **Idioma**.

El instalador añade el cargador ASI x64 si falta `dinput8.dll` y guarda los archivos reemplazados en `UMVC3TrainingToolsInt-Backups`. Al iniciar, el mod consulta GitHub Releases. Si aceptas una actualización, descarga y verifica el instalador, cierra el juego, instala la versión y vuelve a abrirlo. Windows puede solicitar permisos de administrador si la carpeta está protegida.

Usa el modo ventana si la ventana del mod no se abre en pantalla completa exclusiva. Timestone permite jugar en una ventana a pantalla completa sin bordes.

### Entrenamiento rápido y presets

- En el menú principal, pulsa **F4** para abrir **Entrenamiento rápido**. Elige escenario y tres personajes distintos por equipo. Despliega la fila de cada personaje para elegir su asistencia, uno de los seis colores base o el **Traje alternativo** si está instalado. Las flechas cambian el orden del equipo.
- Pulsa **Iniciar entrenamiento** para entrar al combate. Cuando las dos rutas son compatibles, la casilla **Carga directa (experimental)** permite alternar entre ellas. Si la carga directa es la única disponible, el panel lo indica.
- Pon un nombre a la configuración y pulsa **Guardar preset**. Para incluir la posición inicial y de reinicio y las reacciones de J2, activa **Incluir ajustes del ejercicio** y abre **Configurar ejercicio**. Elige centro o una esquina y configura el movimiento y el retardo de cada reacción. Los ejercicios guardados admiten movimientos de la cuadrícula y comandos personalizados. Puedes guardar las grabaciones por separado desde **Grabación**.
- Usa **Cargar preset** y después inicia el entrenamiento. **Actualizar preset** reemplaza o renombra el seleccionado. **Guardar como nuevo** crea una copia y **Borrar preset** lo elimina. Los presets de equipo anteriores se pueden seguir cargando. Los archivos se guardan en `%LOCALAPPDATA%\UMVC3TrainingToolsInt\QuickTraining`.
- Durante un combate, pulsa **F5** y abre **Preset** para guardar la configuración actual o aplicar un preset guardado. Si guardas un ejercicio desde aquí, elige la posición de reinicio y el mod captura las reacciones configuradas en **Grabación**. Puedes aplicar un preset en el escenario actual. Para jugar en el escenario guardado, usa **F4** desde el menú principal.
- En **F5 → Equipos**, despliega las filas para editar los equipos actuales y pulsa **Aplicar y recargar**. Usa **Restablecer selección** para descartar los cambios pendientes. Detén cualquier grabación o reproducción antes de aplicar cambios de equipo.

### Grabación y reacciones del dummy

1. Abre **F5 → Grabación** y elige una de las cinco ranuras. Puedes grabar y reproducir los controles de J1, J2 o ambos. Usa los botones de guardar y cargar para conservar una grabación de cualquiera de los dos jugadores como archivo.
2. Para grabar a J2 con el mando de J1, elige **Grabar dummy con mi mando**. Vuelve a **Grabación** y pulsa **Detener grabación** para devolver el mando a J1.
3. En **Reacciones del dummy**, configura qué hace J2 tras recibir un golpe, bloquear, levantarse o entrar tras un KO. Cada reacción puede estar desactivada o usar una grabación de J2, un movimiento de la cuadrícula o una secuencia personalizada. El retardo va de 0 a 120 fotogramas. La reacción de entrada puede empezar antes de que J2 aterrice.
4. En una secuencia personalizada, usa las direcciones del teclado numérico `1–9` y los botones `L M H S A1 A2`. Une los botones simultáneos con `+`, como en `623L+M`, y pulsa **Aplicar** o **Enter**.

### Herramientas dentro del combate

| Pestaña | Opciones |
| --- | --- |
| **Extra** | Reinicia desde la introducción, desde **Ready** o directamente en Entrenamiento. Ajusta la posición horizontal de ambos jugadores, usa accesos al centro y las esquinas, activa X-Factor ilimitado por jugador o mueve el historial de controles a la izquierda. Ajusta la velocidad del juego o la de los personajes de cada equipo, con las ralentizaciones de Viewtiful Joe y Vale of Mist. |
| **Estado** | Fija la vida de cada personaje al reiniciar y aplícala también a la vida roja si quieres. Ajusta o fija la barra de especiales de ambos equipos y activa los controles invertidos de Jamming Bomb por personaje. Desactiva **Recuperación de vida** en el menú de pausa al fijar la vida individual. |
| **Personaje** | Ajusta el nivel de Frank West, elige y fija las pruebas de Phoenix Wright y activa Turnabout o Fénix Oscura. Ajusta y fija la comprensión de MODOK, las cargas de Dormammu o el contador de teletransportes de Deadpool. Rompe las gafas de Wesker al momento o al reiniciar, y activa transformaciones temporales ilimitadas para personajes del juego base. |
| **Datos** | Consulta el aturdimiento restante por impacto o bloqueo de cada personaje y los contadores de aturdimiento en suelo y aire. |
| **Entradas** | Practica entradas tras un KO. Configura el retardo y la posición X/Y del KO, usa la posición inicial de un jugador o un acceso al centro o la esquina, derrota al personaje activo de cualquiera de los dos equipos al reiniciar o activa KO por personaje. Desactiva **Recuperación de vida** en el menú de pausa para usar estas opciones experimentales. |

Algunos efectos conservan su estado visual o sonoro anterior al activarlos desde el mod, como Jamming Bomb, Turnabout, Fénix Oscura, el aura de nivel de Frank West o la llama de MODOK. Usa **Archivo → Restablecer ajustes** para recuperar los ajustes del mod.

### Atajos del mando

- Por defecto, **Vista / Select** abre Entrenamiento rápido desde el menú principal, **R3** abre la ventana del mod y **L3** reinicia el entrenamiento. Cambia o desactiva las asignaciones en **Atajos del mando**. Se guardan en `UMVC3TrainingToolsInt.ini`.
- Mantén **izquierda/derecha** en la cruceta o el stick izquierdo al reiniciar para colocar a J2 en esa esquina. **Abajo** devuelve a ambos al centro y **arriba** intercambia sus posiciones de reinicio. Al reiniciar sin dirección se usa la posición del ejercicio activo o se repite el último reinicio direccional. Antes del primero se usan las posiciones de **Extra → Posiciones**.
- Usa la cruceta o el stick izquierdo para navegar, **A / Cruz** para confirmar y **B / Círculo** para cerrar un desplegable o la ventana del mod. El mod admite XInput y los mandos que Steam Input presenta como XInput. Suelta los controles al cerrar la ventana antes de volver al combate.

### Reportar un fallo

1. Reproduce el fallo y anota los pasos, qué esperabas y qué ocurrió. Indica la versión del mod y qué otros mods tenías activados. Si afecta a los controles, indica si usabas teclado o mando.
2. Abre **Archivo → Abrir carpeta de registros** desde la ventana de F4 o F5. Si no puedes abrirla, busca en `logs` junto a `umvc3.exe` y después en `%TEMP%\UMVC3TrainingToolsInt\logs`.
3. Recoge los registros pertinentes.

   - `UMVC3TrainingToolsInt-Teams.log` recoge el arranque, el acceso rápido, los presets, los cambios de equipo y los reinicios.
   - `UMVC3TrainingToolsInt-Recording.log` recoge la entrada del mando, la grabación, la reproducción y las reacciones de J2. Si aparece **Archivo → Abrir carpeta de registros de grabación**, usa esa opción para encontrarlo.
   - Si el registro actual empieza después del fallo, busca el archivo `.previous.log` correspondiente. Las horas se registran en UTC.

4. Abre una incidencia con los pasos y adjunta los archivos pertinentes. Si GitHub rechaza los archivos `.log`, comprímelos en un ZIP. [Incidencias en GitHub](https://github.com/AzeDev955/UMVC3TrainingToolsInt/issues)

## Build and release / Compilación y publicación

Run `.\installer\Build-Release.ps1` from PowerShell to build Release x64 and verify that the installer contains the compiled ASI. It writes `installer/UMVC3TrainingToolsInt-Setup-vX.Y.Z.exe` using the source version. To package an existing mod ZIP, use `.\installer\Build-Installer.ps1 -ModZip path-to-zip -Tag vX.Y.Z`.

Ejecuta `.\installer\Build-Release.ps1` desde PowerShell para compilar Release x64 y comprobar que el instalador contiene el ASI compilado. El script genera `installer/UMVC3TrainingToolsInt-Setup-vX.Y.Z.exe` con la versión del código. Para empaquetar un ZIP existente, usa `.\installer\Build-Installer.ps1 -ModZip ruta-al-zip -Tag vX.Y.Z`.

The **Instalador y release de UMVC3** workflow builds an artifact for PRs to `main` and for manual runs. To publish, update `VersionTag` in `UMVC3TrainingToolsInt/code/ReleaseUpdate.h`, add `changelog/vX.Y.Z.md`, and merge `dev` into `main` via a PR. The workflow tags the release commit and publishes `UMVC3TrainingToolsInt-Setup.exe` for the updater.

El workflow **Instalador y release de UMVC3** genera un artefacto para las PR a `main` y para las ejecuciones manuales. Para publicar, actualiza `VersionTag` en `UMVC3TrainingToolsInt/code/ReleaseUpdate.h`, añade `changelog/vX.Y.Z.md` y fusiona `dev` en `main` mediante una PR. El workflow etiqueta el commit y publica `UMVC3TrainingToolsInt-Setup.exe` para el actualizador.
