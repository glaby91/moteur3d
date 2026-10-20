# QBasic CPC6128 — éditeur QEDIT v0.2

Éditeur BASIC **sans numéros de ligne**, développé en assembleur Z80, au format **ROM d'extension 16 Ko** pour l'Amstrad CPC6128.

## État réel

- **Validé par l'utilisateur sur WinAPE** : la commande `|QEDIT` démarre l'éditeur ; la sauvegarde `Ctrl+S` fonctionne après la correction de v0.2.
- **Compilé sous RASM** : image ROM de 16 384 octets ; code source et ROM présents dans `retro/cpc6128-qbasic/editor/`.
- **À tester** : chargement `Ctrl+O` et comportement sur matériel physique.
- **Défauts connus** : `ESC` rend la main au BASIC sans réinitialiser correctement l'écran ; la barre de menus est décorative et non interactive.
- **Non implémentés** : interpréteur QBasic, exécution `RUN`, coloration syntaxique, overscan et menus déroulants (prévus ultérieurement).

## Installation WinAPE

1. Dans **Settings > Memory**, choisir l'emplacement **Upper ROM 1** et y sélectionner `editor/QEDIT.ROM`.
2. Conserver les ROMs d'origine **OS6128**, **BASIC 1.1 (ROM 0)** et **AMSDOS (ROM 7)**.
3. Redémarrer le CPC6128, puis depuis le BASIC :

```basic
MEMORY &5FFF
|QEDIT
```

La commande MEMORY réserve la RAM utilisée par l'éditeur avant son lancement.

## Contrôles implémentés

`Ctrl+S` : sauvegarde `DEMO.BAS`. `Ctrl+O` : ouverture de `DEMO.BAS`. `Ctrl+N` : nouveau document. Flèches : déplacement. Entrée : nouvelle ligne. Suppr : effacement. `ESC` : retour au BASIC (écran non restauré dans cette version).

## Architecture

- **ROM** d'extension mappée à `#C000–#FFFF`, code de l'éditeur compilé pour ce segment.
- **RAM** `#6000` et au-dessus réservée par `MEMORY &5FFF`, buffer de texte et zone de travail.
- Mode 2 standard **80 × 25**, sans overscan pour la version initiale.
- Correction v0.2 : nom `DEMO.BAS` copié en RAM `#8810` avant les appels AMSDOS, afin de ne pas faire référence à la ROM 1 lorsqu'AMSDOS commute sur la ROM 7.

## Compilation

Avec **RASM v3.2.4bis**, depuis la racine du dépôt :

```sh
rasm retro/cpc6128-qbasic/editor/QEDIT_ROM.asm -ob retro/cpc6128-qbasic/editor/QEDIT.ROM
```

SHA-256 de la ROM v0.2 attendue : `eca1db1c27cc5e877161381aec359165a479b8ac6c07b8da4e8d247dab6632d8`.

Le workflow `.github/workflows/build-qedit.yml` vérifie automatiquement cette compilation. Licence source indiquée : MIT.

L'éditeur et la ROM d'exécution QBasic seront développés séparément ; cette branche est réservée au projet QBasic CPC6128.
