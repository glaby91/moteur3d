# V17.023 — validation du filtrage CPU miroir (configuration C)

Décision validée par le concepteur le 27/11/2026. Branche expérimentale Z Reflect ; aucune fusion vers master ni modification de la scène. 

## Campagne de référence
Archive utilisateur : parcours30-20261127-132811907.zip, resultats.csv, SCENE_VERSION.json et SYSTEM_INFO.json relus.
Moteur V17.022 build 11, commit 8bf1f23f54c3f57897d4bef7ff32358831e3f010, run 45993848542 (succès).
Scène V17.631, commit db26889c6c3aa4abf8a380fefac0d338fdad83ce.
Intel i5-7200U / HD Graphics 620, pilote 31.0.101.2145, 1280x720, VSync OFF, préparation géométrique ON, lumière indirecte ON, 14.75 h, Z Reflect ON, aucun readback de pixels. 32 lignes OK : 4 vues x 4 configurations x 2 répétitions de 30 s. Aucune variable optique héritée dans SYSTEM_INFO.
A = filtrage CPU OFF / prépass miroir OFF ; B = ON/ON ; C = ON/OFF ; D = OFF/ON.
Moyennes arithmétiques des deux répétitions, puis moyenne non pondérée des quatre vues (pas un FPS de parcours continu).

| Vue | FPS A | FPS B | FPS C | FPS D | GPU A (ms) | GPU B (ms) | GPU C (ms) | GPU D (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Piscine | 30.93 | 35.23 | 35.50 | 27.42 | 6.37 | 2.40 | 1.77 | 11.80 |
| Façade | 16.57 | 13.55 | 17.05 | 13.00 | 20.11 | 33.63 | 18.07 | 37.16 |
| Verrière | 16.10 | 13.97 | 17.60 | 12.22 | 27.87 | 36.61 | 21.99 | 47.69 |
| Palier | 14.45 | 15.78 | 16.68 | 12.55 | 19.83 | 13.66 | 10.54 | 30.89 |
| Moyenne FPS | 19.51 | 19.63 | 21.71 | 16.30 | | | | |

C : +10.57 % de FPS moyen vs B, +11.25 % vs A. C est la meilleure moyenne sur chacune des quatre vues, et le prépass miroir augmente le temps GPU des réflexions dans les quatre vues avec ou sans filtrage. La différence B/C en piscine est faible (0.27 FPS), donc ne pas prétendre à une significativité statistique avec seulement deux répétitions.

| Vue | Triangles couleur A | Triangles couleur C | Réduction |
|---|---:|---:|---:|
| Piscine | 1691650 | 102400 | 93.95 % |
| Façade | 3617580 | 3048230 | 15.74 % |
| Verrière | 5372500 | 3093290 | 42.42 % |
| Palier | 4163750 | 1245180 | 70.10 % |

Triangles soumis par frame, cumul des caméras actives (pas des triangles uniques, ni une mesure de fragments visibles). C : zéro triangle dans le prépass miroir. Temps GPU cumulé des réflexions, moyenne des mesures par répétition ; ne pas additionner naïvement aux temps CPU. C : rapport indicatif GPU miroir / mean_render_ms de 6.3 % / 30.8 % / 38.7 % / 17.6 % respectivement. Objectif de conception des reflets 5–10 % du budget frame, non garanti.

## Réglages V17.023
- Par défaut configuration C : filtrage CPU conservatif du plan miroir activé, prépass secondaire désactivé. Z Prepass principal et Frustum Culling classiques inchangés.
- M3D_V17_ZREFLECT_CPU_CULL et M3D_V17_ZREFLECT_PREPASS (0/1) permettent toujours toutes les configurations. M3D_V17_DISABLE_ZREFLECT_DEPTH_OPT=1 désactive les deux sauf override explicite. Absence de variable ou ancienne valeur 0 suit le nouveau défaut C.
- LANCER_ZREFLECT_MATRIX.bat conserve la matrice A/B/C/D et ses vérifications.
- CI V17.022 réussie : parité pixel à pixel WARP pour A/B/C/D sur quatre vues fixes ; pas une preuve pour toutes les positions et tous les GPU. V17.023 ajoute une vérification du comportement sans variables d'environnement et de la parité sur les mêmes vues. Pas de réduction volontaire de la qualité, de la résolution, du LOD ni changement de matériaux.
- Pas de Z Prepass adaptatif : non justifié par cette campagne. Filtrage géométrique plus sélectif pour les vitrages à étudier séparément ; Z Warp reste une piste de R&D différée (issue #94), pas une fonctionnalité implémentée. 
