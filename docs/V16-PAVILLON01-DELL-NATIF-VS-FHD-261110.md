# CSE WARP Beta X V16 — comparaison Dell natif / Full HD

Date : 10 novembre 2026. Machine utilisateur : Dell Intel HD Graphics 620, pilote 31.0.101.2141.

## Modes disponibles dans l'interface Qt
- **Dell natif : 1366 × 768** (1 049 088 pixels ; valeur sélectionnée au lancement).
- **Full HD : 1920 × 1080** (2 073 600 pixels ; conservé).
- QHD 2560 × 1440 et UHD 3840 × 2160 (conservés).

Les mêmes choix fonctionnent pour **CPU direct, CPU GI et GPU D3D11 Compute**. La barre d'état affiche le dernier temps (persistent) et la résolution du rendu. Les deux résolutions ne sont pas exactement du même ratio : 1366/768 ≈ 1,778646, 1920/1080 ≈ 1,777778. Les dimensions de caméra sont recalculées correctement pour chacune.

## Procédure de mesure sur HD 620 réel
1. Lancer `moteur3d_v16.exe`, scène `Pavillon 01`.
2. Fixer une caméra (par exemple 02 « Jardin et terrasse »), l'heure solaire **14,75 h**, les matériaux et le backend **GPU D3D11 Compute**.
3. Sélectionner **1366 × 768**, calculer deux fois ; relever le **deuxième temps**, qui réutilise les ressources cachées. Exporter l'image PNG.
4. Sélectionner **Full HD 1920 × 1080**, calculer deux fois ; relever le deuxième temps. Exporter l'image PNG.
5. Refaire les deux mesures dans l'ordre inverse ou 3 fois pour réduire les effets de variation thermique. Ne pas mélanger « cold » (création/cache) et « warm ».
6. Si souhaité, recommencer en CPU Direct 1 spp ou CPU GI (plus long), à qualité et réglages identiques.
7. Comparer les images à la **même échelle d'affichage**. L'augmentation de netteté en Full HD réduit vers une fenêtre de ~632×356 ne signifie pas nécessairement une meilleure qualité visible.

## Repères chiffrés
- HD 620 GPU Full HD caméra 02 mesuré auparavant : **441 ms** (avec branche GI adaptative).
- Nombre de pixels natif / FHD : **0,50593** (~49,4 % de pixels en moins).
- Projection **linéaire uniquement** : 441 × 0,50593 ≈ **223 ms** à la résolution native. **Ce n'est pas un benchmark réel**.
- L'objectif **25 ms** doit être testé avec les mesures sur le vrai GPU Intel après changement d'architecture ; la simple baisse de résolution ne suffit vraisemblablement pas.

## Validation CI
Le test `v16_gpu_compute_warp` calcule une image native 1366×768 et une image FHD 1920×1080 avec la **même scène/caméra/lumière**, affiche les durées **WARP software** initiales et cachées, conserve les deux PNG et vérifie les tailles. Les mesures WARP **ne remplacent pas** les relevés du GPU Intel physique.

La sélection 1366×768 n'apporte **aucune modification aux shaders**, à la GI, au ray tracing CPU ou à la scène. Elle ne concerne que la résolution de rendu et le contrôle UI.
