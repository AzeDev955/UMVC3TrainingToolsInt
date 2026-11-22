# Verificación local del mod

Cuando se prepare un cambio del mod para probarlo, ejecuta `installer/Build-Release.ps1`. El script compila Release x64, actualiza `installer/UMVC3TrainingToolsInt-Setup.exe` y comprueba que el `.asi` incluido coincide con la compilación. Entrega el enlace al instalador actualizado e indica qué se ha verificado en el juego.

Antes de publicar, ajusta `VersionTag` en `UMVC3TrainingToolsInt/code/ReleaseUpdate.h` y añade `changelog/vX.Y.Z.md`. GitHub Actions publica el instalador al subir la etiqueta `vX.Y.Z` de un commit de `main`. Para generar un artefacto de prueba, ejecuta el workflow manualmente.
