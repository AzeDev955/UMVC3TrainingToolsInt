A tool designed to incrase and improve quality of life of Ultimate Marvel vs Capcom 3's training mode.
This tool adds more options to practice and setup situations that weren't present before.
The recent build uses Mid 2025 revisions Imgui & Sokol with a D3D11 backend.
The public build has 5 pages.

[English](#windows-installation) · [Español](#instalación-en-windows)

## Disclaimer / Nota de autoría

This fork builds on the original UMVC3TrainingToolsInt mod by EternalYoshi. Credit for the original mod goes to EternalYoshi and the contributors to that project. [Original repository](https://github.com/EternalYoshi/UMVC3TrainingToolsInt)

Este fork parte del mod original UMVC3TrainingToolsInt, creado por EternalYoshi. El crédito por el mod original corresponde a EternalYoshi y a quienes contribuyeron a ese proyecto. [Repositorio original](https://github.com/EternalYoshi/UMVC3TrainingToolsInt)

## Windows installation

1. Close the game and run `UMVC3TrainingToolsInt-Setup.exe`.
2. The installer searches your Steam libraries for UMVC3. If it cannot find the game, choose **Browse** and select `umvc3.exe`.
3. Choose **English** or **Español**, click **Install**, then click **Play** to launch the game.

Press **F4** in the game's main menu to open Quick Training. Press **F5** to open or close the tools window.

In Quick Training, enter a name and choose **Save preset** to keep both teams, their assists and order, and the selected stage. Choose a saved setup from **Load preset**, then press **Start Training**. Presets are stored in `%LOCALAPPDATA%\UMVC3TrainingToolsInt\QuickTraining`.

After loading a preset, edit the teams, stage or name and choose **Update preset**. Use **Save as new** to keep a separate copy or **Delete preset** to remove the saved preset after confirming.

When MVC3 starts, the mod checks GitHub Releases in the background. If you accept an update, it downloads and verifies the installer, closes MVC3, installs the update and relaunches the game. Windows may ask for administrator permission if the game folder is protected. Install v0.9.3 manually once to enable this flow on older installations.

### Controller

- **View / Select** opens Quick Training from the main menu. **R3** opens the tools window.
- Use the D-pad or left stick to navigate and **A / Cross** to confirm. **B / Circle** closes the active dropdown. With no dropdown open, it closes the mod menu and returns focus to the game.
- **L3** restarts Training using the P1 and P2 X positions configured under **Extra → Positioning**, even with the mod menu closed. To change or disable this shortcut, use **Reset Training** under **Controller shortcuts**. You can also reassign or disable the shortcuts for opening either window there. Changes are saved in `UMVC3TrainingToolsInt.ini`.
- Hold **left** or **right** on the D-pad or left stick while pressing **Reset Training** to put P2 in that corner for this reset.
- The mod detects XInput controllers, including controllers that Steam Input presents as XInput devices.

While the mod menu has focus, it captures XInput so controller navigation does not affect the game. Release the controls after closing the menu to resume play.

The mod writes its log to `logs/UMVC3TrainingToolsInt-Teams.log` inside the game's folder. On startup it moves the log from older versions into `logs/`.

The installer includes Ultimate ASI Loader x64 for installations without `dinput8.dll`. When it updates mod files, it backs up the originals in `UMVC3TrainingToolsInt-Backups`.

To build the installer from a mod release ZIP, pass its version with `-Tag`, for example `.\installer\Build-Installer.ps1 -ModZip path-to-zip -Tag v0.9.3`.

To compile and verify an installer directly from the source checkout, run `.\installer\Build-Release.ps1` in PowerShell. It writes `installer/UMVC3TrainingToolsInt-Setup-vX.Y.Z.exe` using the source version. See the [release workflow](#publicación-en-github) for GitHub Actions.

## Instalación en Windows

**F5** abre o cierra la ventana de herramientas durante el juego.

En Quick Training, escribe un nombre y pulsa **Save preset** para guardar ambos equipos, su orden, las asistencias y el escenario. Elige una configuración en **Load preset** y pulsa **Start Training**. Los presets se guardan en `%LOCALAPPDATA%\UMVC3TrainingToolsInt\QuickTraining`.

Tras cargar un preset, edita los equipos, el escenario o el nombre y pulsa **Update preset**. Usa **Save as new** para guardar una copia o **Delete preset** para borrar el preset guardado tras confirmarlo.

Al iniciar MVC3, el mod consulta GitHub Releases en segundo plano. Al aceptar una actualización, descarga y verifica el instalador, cierra MVC3, instala la versión y vuelve a abrir el juego. Windows puede pedir permiso de administrador si la carpeta del juego está protegida. Instala la v0.9.3 manualmente una vez para habilitar este flujo desde versiones anteriores.

### Mando

- **View / Select** abre Quick Training en el menú principal y **R3** abre la ventana de herramientas.
- Usa la cruceta o el stick izquierdo para navegar y **A / ✕** para confirmar. **B / ○** cierra el desplegable activo o, si no hay ninguno, cierra el menú del mod y devuelve el foco al juego.
- **L3** reinicia Training con las posiciones X de P1 y P2 configuradas en **Extra → Positioning**, incluso con el menú del mod cerrado. Puedes cambiar o desactivar cada atajo en **Controller shortcuts**. Los cambios se guardan en `UMVC3TrainingToolsInt.ini`.
- Mantén **izquierda** o **derecha** en la cruceta o el stick izquierdo al pulsar **Reset Training** para colocar a P2 en esa esquina en ese intento.
- El mod detecta mandos XInput, incluidos los que Steam Input presenta como XInput.

Mientras el menú del mod tiene el foco, captura la entrada XInput del mando. Al cerrar el menú, suelta los controles para volver a jugar.

El mod guarda su registro en `logs/UMVC3TrainingToolsInt-Teams.log`, dentro de la carpeta del juego. Al iniciar, traslada el registro de las versiones anteriores a `logs/`.

Con el juego cerrado, abre `UMVC3TrainingToolsInt-Setup.exe`. El instalador busca UMVC3 en las bibliotecas de Steam. Si hace falta, usa **Examinar** para seleccionar `umvc3.exe`. Puedes cambiar el idioma del instalador entre español e inglés. Pulsa **Instalar** y luego **Jugar** para abrir el juego. Desde el menú principal, **F4** abre Quick Training.

El instalador incluye Ultimate ASI Loader x64 para instalaciones sin `dinput8.dll`. Si actualiza archivos del mod, guarda los originales en la carpeta `UMVC3TrainingToolsInt-Backups`.

Para generar el instalador a partir del ZIP de una versión del mod, indica su versión con `-Tag`. Por ejemplo, `.\installer\Build-Installer.ps1 -ModZip ruta-al-zip -Tag v0.9.3`.

## Publicación en GitHub

Para compilar y verificar el `.exe` desde el código fuente, ejecuta `.\installer\Build-Release.ps1` en PowerShell. El script actualiza `installer/UMVC3TrainingToolsInt-Setup-vX.Y.Z.exe` con la versión del código.

En GitHub, abre **Actions → Instalador y release de UMVC3 → Run workflow** para generar un instalador de prueba. Puedes descargarlo en **Artifacts** de la ejecución. Las PR a `main` también generan ese artefacto.

Para publicar, aumenta `VersionTag` en `UMVC3TrainingToolsInt/code/ReleaseUpdate.h`, añade `changelog/vX.Y.Z.md` y fusiona la PR de `dev` a `main`. GitHub Actions compila el instalador, crea la etiqueta sobre el commit que compiló y publica la release estable con el changelog y `UMVC3TrainingToolsInt-Setup.exe`. El workflow omite versiones ya publicadas o anteriores a una release estable publicada.

The next build will return to being internal.

Explanations:
================================================================================================================================
Extra:
- Restart To Intros: Plays the match the intros once more.

- Restart to Ready: Restarts the Match to the point where the announcer yells READY!!!

- Restart Training mode: Same as before.

- Positioning: Changes the start position of the players. The farthest left is -800 and the farthest right is 800. Below are preset buttons 
that set the positions to specific common scenarios.

Etc: 
- Endless X Factor Toggles.

Status:

- Character HP: Should set Life Recovery off in the pause menu.

- Health value gauges: Multiplier. 1.000 is full health, while 0.5 is half health. The Minimum is 0.01.

- Changeable Hyper Meter. Each level of meter is 10,000. Max is 50,000 while minimum is 0.

- Jamming Bomb/Reversed Controls: Should speak for itself, has no visuals but it works.

================================================================================================================================
Character:

FRANK WEST
- You can set Frank West's Level, and his Prestige Points will be set to the minimum required for that level.
lvl 1 -   0PP
lvl 2 -   5PP
lvl 3 -  20PP
lvl 4 -  50PP
lvl 5 - 100PP

PHOENIX WRIGHT
- You can choose both Mr. Wright's Evidence and have a Turnabout Toggle. Iconography is off but functionally works.
- Can choose to lock Evidence as well, letting you keep evidence regardless of Training Mode Restart, Turnabout Mode ending, discarding evidence.
   
MODOK
- Can Control level of Understanding. Affects his Hyper Psionic Blaster and the Big Barrier.

DORMAMMU
- Can set level of spells and will lock it to level. Set both to 0 to return behavior to normal.

DEADPOOL
- Can set Teleport Counter and can lock it to that number.

- Also included is an Endless Install toggle that should work with all installs from the default cast excluding Vale of Mist.

Lastly there's File->Reset to Default which will change all the above settings back to their default values.

Notes:
================================================================================================================================
Dark Phoenix lacks the low wind sound when using the Dark Phoenix Toggle.
Mr. Wright lacks the Gold Aura when using the Turnabout Toggle.
Frank West will keep the level 5 aura when changing his level from 5 to <5. That's not something that can happen
normally in game so there's no real code to handle that. Just restart Training Mode at that point.
MODOK's head flame won't update visuals when setting Level of Understanding.
The Endless Install/X Factor locks the timers at a silly high value, so turning it off WILL allow the timer to decrement, but the timer
is so large it may be wiser to just restart training mode.

Thanks to HKHaan, Sheep, Techs, anotak, and Ermaccer for their contributions that made this and all previous incarnations of this possible.

Features will be added to the next release when they are confirmed working problem free.
