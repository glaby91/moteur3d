# Benchmark réel — GPU Compute D3D11 / Intel HD Graphics 620 (500 images)

**Date :** 7 novembre 2026  
**Origine :** capture d'écran du benchmark réalisée sur le PC de l'utilisateur.  
**Statut :** mesure utilisateur du GPU physique ; **ce n'est pas un test d'intégration CI Windows/WARP**.

## Configuration mesurée

| Paramètre | Valeur |
|---|---|
| Moteur | Moteur3D natif Windows x64, **Build 261107-0004** (benchmark à résolution native 1:1) |
| Backend | GPU Compute D3D11 — ray tracing HLSL expérimental, sans DXR |
| GPU testé | Intel HD Graphics 620 |
| Scène | `scene_pripyat_central.bin` (dossier de test local `296`) |
| Complexité indiquée par l'application | **92 886 objets ; 67 404 triangles** |
| Résolution de sortie et de calcul | **1366 × 768 pixels**, native 1:1 |
| Série | **500 images mesurées** (+ 1 image d'échauffement exclue) |
| Source des pourcentages | Compteurs **Windows WDDM GPU Engine**, moteur le plus actif et processus `Moteur3D` |
| Nombre d'échantillons valides | **223** |

## Résultats vérifiés sur la capture

| Mesure | Moyenne | Maximum |
|---|---:|---:|
| Charge GPU Windows, moteur GPU le plus actif | **88,9 %** | **93,5 %** |
| Charge GPU attribuée au processus Moteur3D | **88,9 %** | **93,5 %** |

Le panneau indique explicitement : `GPU Compute D3D11 : 500 images (1366×768)`.

### Interprétation

- Le backend HLSL sollicite effectivement fortement le GPU intégré sur cette scène et cette résolution.
- L'égalité des pourcentages « GPU le plus actif » et « attribué à Moteur3D » est **un résultat observé pour cette série**, pas une garantie générale d'absence d'autres charges GPU.
- Ces compteurs expriment l'**occupation d'un moteur GPU WDDM** : ils ne s'additionnent pas entre moteurs et ne donnent **ni la consommation de VRAM, ni la bande passante, ni la puissance de calcul restante**.
- Une occupation moyenne de 88,9 % n'implique pas que 11,1 % de performance peut être récupéré.
- Le nombre de FPS, le temps moyen/médian par image, les fréquences/consommations du GPU et la bande passante mémoire **ne sont pas présents sur la capture**. Ils ne sont donc pas revendiqués.
- Ces résultats concernent le rendu **GPU Compute expérimental**, et ne doivent pas être assimilés au ray tracing CPU hors ligne (effets et qualité encore différents).

## Historique et validité des comparaisons

Les séries précédentes sur 100 images étaient affectées par un héritage du facteur de réduction de l'**aperçu fluide**. Par exemple une sélection « 1280 × 720 » pouvait calculer `450×253` et « Full HD » `675×380`. Les pourcentages de ces séries **ne sont pas des benchmarks natifs** à la résolution sélectionnée.

La **Build 261107-0004** force désormais `renderW=outW` et `renderH=outH` dans le mode benchmark et refuse tout décalage interne. Les comparaisons futures doivent se fonder sur les dimensions **effectivement affichées** dans le rapport.

## Reproductibilité et suite des mesures

1. Décompresser [le paquet Windows GPU natif](https://github.com/glaby91/moteur3d/actions/runs/42411886443#artifacts).
2. Charger la même scène et choisir `Outils > Benchmark du moteur`.
3. Sélectionner `GPU Compute D3D11 — occupation GPU (%)`, `Résolution courante — natif 1:1`, `Images Compute = 500`.
4. Contrôler que le rapport indique **500 images (1366×768)** puis exporter le CSV WDDM lorsque disponible.
5. Pour comparer 1280 × 720, 1920 × 1080 et 3840 × 2160, relever **résolution effective, charge, nombre d'échantillons et durée/FPS**. Ne pas inférer des valeurs non mesurées.

**Provenance du code testé :** moteur Build 261107-0004, compilation Windows validée [run #42411694183](https://github.com/glaby91/moteur3d/actions/runs/42411694183), paquet complet [run #42411886443](https://github.com/glaby91/moteur3d/actions/runs/42411886443). Les mesures chiffrées proviennent de la **capture d'écran utilisateur**, pas des workflows de CI.
