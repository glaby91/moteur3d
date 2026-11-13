# CSE WARP V17.430 — Neuf anomalies V17.428 corrigées

## Livraison
- Branche : `feature/v17-430-nine-anomalies-261113`.
- Workflow Windows validé : https://github.com/glaby91/moteur3d/actions/runs/43335003809 — **SUCCESS**.
- Archive GitHub Actions : `CSE-WARP-V17.430-9-ANOMALIES-Windows-x64` (artifact ID `13996174360`).
- ZIP matériel : `CSE-WARP-V17.430-9-ANOMALIES-Windows-x64.zip` — **102 589 325 octets**, **282 entrées**, test CRC **OK**.
- SHA-256 ZIP : `28a9b0518a995f957972089b67296f7164e7ec64dcb866c699a9b5265abce871`.
- Source auditable : `tools/upgrade_scene_v8.py`.
- La V17.429 compilée avec **Maj ×3** est conservée sans modification ; les fichiers de scène V17.428 précédents sont conservés.

## Corrections ciblées
1. **Garde-corps interrompu :** fermeture de 6,35 m de la travée vide héritée d'un ancien escalier.
2. **Poutres inutiles :** retrait des trois poutres de béton non soutenues ; les éléments de charpente utiles restent.
3. **Accès R+1 à R+3 :** trois vols d'escalier latéraux, **64 marches** et trois trémies connectées avec paliers, nouveaux garde-corps. Le contrôle automatisé vérifie la continuité des niveaux, pas encore la navigation utilisateur HD620.
4. **Mur rideau :** suppression de 466 cadres dupliqués/mal alignés, création d'une structure métallique sombre et de 112 montants raccordés aux vitrages existants.
5. **Piscine et échelles :** bassin représenté à **2,1 m** de profondeur, finition mosaïque céramique, suppression de **171 sphères parasites**, neuf lignes d'eau continues, deux échelles avec quatre mains courantes cintrées et douze barreaux en inox satiné, réduction des segments sombres de l'ancienne géométrie. La véritable réfraction physique de l'eau demeure une limitation du moteur.
6. **Plots sportifs :** dix plots rapprochés du bord et numérotés par couloir, avec dessus antidérapant et finition céramique.
7. **Échelles dégagées :** plots concentrés à l'extrémité des départs, échelles à l'extrémité opposée ; séparation de plus de 45 m contrôlée au niveau des positions et des maillages.
8. **Bancs :** retrait des **18 éléments** constituant six bancs en bois des plages de la piscine.
9. **Plafond et lumières :** suppression de **203 faux blocs de toiture**, préservation de la verrière et de sa structure, installation de six vrais luminaires liés à six sources utilisables en soirée.

## Fresque et éléments conservés
- Fresque originale 2048px préservée, **SHA-256** `ac0dfafe2a77316ee2e4c9a0cde3d01e56bee85f29cfb7a4c6e5ffc1bdbbb275`.
- Les huit panneaux 3D de la fresque restent inchangés.
- Le moteur V17.429, ses options DirectX 11 et la navigation ×3 sont conservés.
- Les versions V5, V6, V17.428 et les ressources GLB antérieures ne sont pas altérées.

## Vérifications réelles
- GitHub Actions Windows 2022 : générateur Python, **neuf familles d'assertions** sur la géométrie/ressources, sorties de matériaux et lumières.
- Dix rendus DirectX 11 **logiciel WARP** des caméras piscine, fresque, vitrages, marches et paliers des deux premiers niveaux, plafond jour et soirée, tous produits et contrôlés visuellement.
- Archive complète ZIP testée CRC et présences EXE/DLL/launcher/scène validées localement.

**Limites de validation** : les rendus fixes et assertions de scènes ne remplacent pas un parcours interactif RDC→R+3 et les contrôles de collisions réels sur HD620. L'aspect inox, la transparence effective, le confort des 64 marches et les FPS doivent être évalués dans le programme sur la machine cible. Les neuf modifications sont implémentées et testées en scène, pas certifiées sans anomalies résiduelles en exploration.

## Lancement
Extraire toute l'archive dans un nouveau dossier ; double-cliquer `DEMARRER_CSE_WARP_V7.bat`. Le fichier de scène chargé sous son chemin historique `scene-v7-corrections.json` pointe maintenant vers **CSE WARP V17.430 / V8**. Ne pas utiliser un ancien dossier d'extraction pour éviter les mélanges de versions.
