# Verificación local del mod

Cuando se prepare un cambio del mod para probarlo, compila Release x64 y actualiza `installer/UMVC3TrainingToolsInt-Setup.exe` con el `.asi` recién generado. Comprueba que el `.asi` incluido en el instalador coincide con la compilación que se va a probar. Entrega el enlace al instalador actualizado e indica qué se ha verificado en el juego.

Antes de compilar para una release pública, ajusta `VersionTag` en `UMVC3TrainingToolsInt/code/ReleaseUpdate.h` para que coincida con la etiqueta `vX.Y.Z`. Adjunta el instalador con el nombre exacto `UMVC3TrainingToolsInt-Setup.exe` a la release estable de GitHub.
