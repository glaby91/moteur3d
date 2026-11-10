# CSE WARP V17 — optimisation de la préparation PBR et compatibilité Compute

## Contexte

Campagne de référence Dell V17.281 : 6 scènes, 72 configurations Raster, 5 184 images mesurées. Le pavillon avait nécessité environ **66,25 s de préparation géométrie/PBR** sur la machine de test ; la scène Photo2Scene avait déclenché l’erreur Compute `Chaîne mip couleur manquante sur un matériau`. Ces durées sont des mesures précédentes, **pas des gains revendiqués**.

## Changements réellement livrés

1. `m3d_v16_bridge.cpp` : génération de l’albedo, des normales et de la rugosité procédurales en bandes de lignes sur **jusqu’à huit travailleurs CPU** (nombre borné par les threads matériels et la taille de la texture), sans duplicata des tableaux de sortie. Le calcul d’un pixel reste inchangé. Retour au mode séquentiel : variable d’environnement `M3D_V17_PBR_SERIAL=1`.
2. `m3d_compute11.cpp` : l’atlas Compute limité à une base 512² ne demande plus artificiellement une mip 512 pour des matériaux valides en 256² ou moins. Les niveaux de résolution disponibles sont choisis sans changer leurs valeurs ; seuls les niveaux manquants sont rééchantillonnés pour l’atlas de taille fixe. Le Raster haute résolution est indépendant et inchangé.
3. `m3d_v16_bridge.h/.cpp` : trois métriques dans `Runtime` : `pbrBuildMs`, `geometryBuildMs`, `bvhBuildMs`. Le benchmark complet les inscrit dans le JSON et la sortie console.
4. `v17/tests/pbr_compute_smoke.cpp` : vérifie tous les texels et toutes les mips des deux générateurs (séquentiel/parallèle), puis effectue un rendu Compute WARP à 96×54 à partir de textures en 256².
5. `.github/workflows/build-moteur3d-v17.yml` : test de régression PBR/Compute obligatoire avant publication du ZIP.

## Validation

- Compilation Microsoft Visual C++ et Raster DirectX 11 Windows : réussis au build **V17.298**.
- Test d’identité **bit-à-bit** PBR 512² entre générateur séquentiel et parallèle : réussi sur le banc CI Windows.
- Test de compatibilité Compute avec chaînes mips 256² : réussi sur Microsoft WARP.
- Benchmark 1/2/3 buffers, parité shaders, chargement des scènes et publication du paquet Windows : réussis.
- L’essai sur le matériel Intel HD 620 **reste à faire** : pas de gain annoncé en FPS ni en secondes tant que la nouvelle campagne Dell n’est pas mesurée.

CI : https://github.com/glaby91/moteur3d/actions/runs/42905260474

## Prochain protocole sur le Dell

Exécuter **V17.298** avec les mêmes scènes et paramètres que V17.281, si possible alimentation secteur et applications concurrentes fermées. Lancer une campagne répétée, conserver les fichiers `summary.json`, `campaign_results.csv`, `campaign_frames.csv`, `campaign_report.html` et PNG, puis comparer :

- Durée PBR du pavillon (`pbr_mip_generation`) et durée géométrie/BVH, séparément.
- RAM du processus et pression mémoire totale, sans réduire la finesse des textures.
- Temps GPU Draw et FPS de pipeline à **qualité identique**.
- Rendu Compute fonctionnel au moins sur la scène où l’erreur de mipmaps survenait.

Le but est de **réduire le coût réel**, et non d’obtenir artificiellement davantage de FPS en abaissant la qualité des images.