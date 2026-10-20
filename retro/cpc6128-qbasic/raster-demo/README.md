# QEDIT raster MODE 2 - demonstration experimentale

Prototype isole de la ROM QEDIT v0.4.1. CPC6128 640x200, 80 colonnes, deux encres **simultanement par scanline**. Le Gate Array change la couleur de l'encre 1 selon la hauteur (blanc, rouge pour erreur, jaune pour statut). Ce n'est **pas** une validation syntaxique : les symboles + X ? sont fixes.

## WinAPE

Assembler avec RASM v3.2.4bis : `rasm RASTER.asm -ob RASTER.BIN`. Copier RASTER.BIN et DEMO.BAS sur une disquette, puis `RUN "DEMO.BAS"`. En l'absence de source BASIC directement reconnue par WinAPE, saisir/importer ses lignes. Quitter avec Q ou ESC.

**Non valide dans WinAPE a ce stade.** Calage des bandes par les constantes de `RASTER.asm` (marge initiale, nombre de scanlines) a ajuster sur capture. Horloge CPC ~256 T-states/scanline; attente par boucle Z80 approximative. Le firmware peut restaurer la palette pendant les interruptions, raison pour laquelle elles sont bloquees pendant les bandes. Cette technique perturbe potentiellement firmware, clavier et disque : **ne pas integrer telle quelle dans QEDIT**.

Les rasters changent la palette de tout le balayage horizontal. Impossible de colorer librement mot par mot sur la meme ligne uniquement avec ces changements de palette. Source: https://cpctech.cpcwiki.de/docs/garray.html
