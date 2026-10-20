# QBasic CPC6128 — QEDIT v0.3

Éditeur QBasic sans numéros de ligne, réalisé en Z80 sous forme de ROM d'extension **16 Ko**. Interface légère en **Mode 2 standard 80 × 25**.

## Fonctionnement confirmé et validation

- **QEDIT v0.2** : lancement `|QEDIT` et sauvegarde `Ctrl+S` validés par l'utilisateur dans WinAPE.
- **QEDIT v0.3** : ROM compilée par RASM v3.2.4bis, image 16 384 octets, empreinte SHA-256 vérifiée ; **menus et nouvelle sortie ESC pas encore testés en émulation par l'utilisateur**.
- L'édition, les déplacements, les fichiers `DEMO.BAS`, les zones RAM et les routines AMSDOS restent issus de la v0.2.

## Installation WinAPE

1. Dans **Settings → Memory**, installez `editor/QEDIT.ROM` dans **Upper ROM 1**.
2. Conservez les ROMs originales **OS6128**, **BASIC 1.1 (ROM 0)** et **AMSDOS (ROM 7)**.
3. Redémarrez et tapez :

```basic
MEMORY &5FFF
|QEDIT
```

Le `MEMORY` protège la zone RAM `#6000–#97FF` utilisée pour le programme et les buffers.

## Changements v0.3

- **Sortie ESC corrigée** : mémorise le mode graphique et les encres 0/1 ainsi que la bordure, puis restaure ces paramètres au retour BASIC. L'écran QEDIT est effacé ; le contenu précédent du BASIC n'est pas sauvegardé.
- **Menus interactifs** : Fichier, Édition, Recherche, Exécution, Débogage, Options, Fenêtre, Aide.
- **Ctrl+T** ouvre les menus ; flèches gauche/droite changent de rubrique ; haut/bas choisissent une option ; Entrée confirme ; ESC ferme un menu sans quitter QEDIT.
- Dans WinAPE, la touche **F10 est réservée par l'émulateur** au plein écran. `Ctrl+T` est le raccourci utilisable dans QEDIT.
- Actions fonctionnelles : nouveau document, ouvrir, enregistrer, quitter, insérer une ligne, effacer un caractère, début/fin de ligne, réaffichage Mode 2, aide, à-propos.
- Les entrées Recherche, Exécution, Débogage et Variables affichent un message explicite tant que le moteur QBasic n'existe pas.

## Commandes directes

`Ctrl+S` : enregistrer `DEMO.BAS` ; `Ctrl+O` : ouvrir ; `Ctrl+N` : nouveau ; `Ctrl+H` : aide ; `Ctrl+T` : menus ; `ESC` hors menu : sortir.

## Code et compilation

- Source : [QEDIT_ROM.asm](editor/QEDIT_ROM.asm).
- ROM compilée : [QEDIT.ROM](editor/QEDIT.ROM).
- Workflow : [build-qedit.yml](../../.github/workflows/build-qedit.yml).
- Empreinte SHA-256 de référence : `52af91a4776513e878faad74a6fbcbcdc9112ba1005062baaeb62ff35fe5c5ff`.
- `rasm retro/cpc6128-qbasic/editor/QEDIT_ROM.asm -ob retro/cpc6128-qbasic/editor/QEDIT.ROM`

## Limites

Pas de ROM d'exécution QBasic, de coloration syntaxique, de menus à la souris ni d'overscan pour le moment. Le comportement de ESC et des huit menus dans WinAPE doit encore être testé. Le mode vidéo et certaines couleurs sont restaurés, pas le contenu de l'écran précédent.
