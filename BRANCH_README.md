# Branche raycasting — sauvegarde indépendante

- **Moteur HTML figé :** [engine/index.html](engine/index.html), copie de V9.2.
- **Programme QB64 :** [qb64/raycasting/RayCastingEngine.bas](qb64/raycasting/RayCastingEngine.bas).
- **Version ultra-légère Z80 CPC6128 :** [retro/cpc6128-z80/](retro/cpc6128-z80/).
- **Chronologie :** [VERSIONING.txt](VERSIONING.txt).

La version QB64 calcule **un rayon primaire par pixel interne**, conserve le premier impact analytique et colore ciel ou surface. Elle utilise 320×200 pour la vitesse et peut basculer à 640×400.

La version CPC6128 est volontairement **un ray marcher 2,5D sur 80 colonnes**, et non une conversion complète de V9.2. Elle exploite la RAM bancaire 128 Ko sans aucun appel firmware une fois exécutée.

**Compilation QB64-PE / assembleur CPC non testée dans cet environnement.** Valider sur QB64-PE et un assembleur/émulateur CPC avant utilisation sur matériel réel.
