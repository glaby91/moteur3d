# Moteur 3D natif x64 — Rev 1 | Branch 3 | 261101-0850

## Repères de version

- **Logiciel natif : Rev 1, Branch 3**
- **Identifiant de compilation : 261101-0850**
- **Scène et géométrie source : V13.14**
- Le numéro d’exécution GitHub Actions est un identifiant CI indépendant de la version du logiciel.

La référence de version est définie dans `version.h`, puis réutilisée dans l’interface et dans les métadonnées Windows de l’exécutable.

## Téléchargement et lancement

L’archive `Moteur3D-Natif-x64-Rev1-Branch3-261101-0850.zip` contient :

- `Moteur3D-Natif-x64-Rev1-Branch3-261101-0850.exe`
- `moteur3d_v1314_scene.bin`

Extraire les deux fichiers dans le même dossier, puis lancer le fichier EXE. La compilation est x64.

## Compilation

Le workflow Windows exporte la scène depuis `source_v1314.html`, compile l’application native avec MSVC et empaquette l’exécutable avec sa scène. Le mode **Ultra** conserve le rendu haute qualité, avec ombres adoucies et réflexions à un rebond sur l’eau, la peinture et les vitrages.
