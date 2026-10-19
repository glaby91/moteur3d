# Branche raster — sauvegarde indépendante

- **Moteur HTML figé :** [engine/index.html](engine/index.html), copie de V8.1.
- **Programme QB64 :** [qb64/raster/RasterEngine.bas](qb64/raster/RasterEngine.bas).
- **Chronologie :** [VERSIONING.txt](VERSIONING.txt).
- **Mode :** triangles, Z-buffer caméra, UV et textures, et optimisations en tuiles.
- **QB64 :** ouvrir le fichier `.bas` dans QB64-PE, compiler puis exécuter. A/D = tourner ; +/- = zoom ; H = basculer 320×200 / 640×400 interne ; ESC = quitter.
- **Portée :** adaptation QB64 pédagogique, pas un port intégral pixel à pixel du simulateur HTML.
- **Tests :** absence de compilateur QB64-PE dans cet environnement ; source à compiler et valider localement.

L'autre famille est isolée sur la branche `raycasting`.
