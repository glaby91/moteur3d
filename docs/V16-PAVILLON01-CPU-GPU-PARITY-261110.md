# CSE WARP V16 — qualité GPU D3D11 / CPU GI (Pavillon 01)

## Objectif et captures de référence

La priorité du 10 novembre 2026 est de **rapprocher réellement les images finales du GPU et du CPU**, pas seulement d'accélérer le GPU. Les captures transmises par l'utilisateur sont **toutes des rendus terminés**.

Sur les deux captures originales Full HD : écart moyen des couleurs absolu d'environ 19,09 niveaux RGB / 255 ; régions toiture : 35,38/255 ; façade/vitrages : 21,16/255 ; bassin : 15,61/255. Ces chiffres n'incluent pas d'optimisation des différences de bruit et ne représentent pas un score de vérité photographique.

## Divergences identifiées dans le code V16 initial

- CPU GI : `offlinePbrShade` en lumière linéaire, BRDF GGX/Smith/Schlick et ciel hémisphérique ; accumulateur HDR `packHdr` ACES puis sRGB ; réflexions/réfraction et deux rebonds en path tracing.
- GPU ancien : albedo sRGB éclairé directement, spéculaire de type Blinn-Phong simplifié, clamp avant sortie, lumière ambiante arbitraire, **aucun point light JSON** dans le shader. Un rayon direct par pixel avec ombres dures, verre traversé en ligne droite. Pas de GI deux rebonds.

## Modifications effectivement codées

1. HLSL : conversion de l'albedo et des lampes en **linéaire**, modèle direct GGX/Schlick, hémisphère du CPU, sortie ACES/sRGB **une seule fois** en fin de pipeline.
2. C++ D3D11 : upload et invalidation des lumières ponctuelles de la scène, F0 diélectrique distinct du métal, paramètres du soleil identiques au CPU.
3. HLSL : estimation d'un **rebond diffus** via `M3D_V16_GPU_GI_SAMPLES` rayons hémisphériques cosinus, traçage BVH et éclairage secondaire GGX direct. Ce n'est pas une solution 3DGS ni un GI récursif à deux rebonds exact comme le CPU.
4. `v16/tests/gpu_warp_smoke.cpp` contrôle la déterminisme, le calcul avec rebond activé ou désactivé, les changements de soleil et l'invalidation du cache.
5. `v16/tools/compare_cpu_gpu.py` compare les PNG finaux CPU/GPU sans correction d'exposition, produit `cpu_gpu_comparison.json`, `difference_x3.png` et `cpu_gpu_triptych.jpg`. Les artefacts Windows contiennent les PNG sources et les rapports.

## Option de qualité

- `M3D_V16_GPU_GI_SAMPLES=0` : aucun rebond GPU (comparaison directe / performance).
- `M3D_V16_GPU_GI_SAMPLES=1` : un rayon secondaire par pixel.
- **Valeur par défaut : 2** rayons secondaires par pixel (compromis qualité/performance).
- `M3D_V16_GPU_GI_SAMPLES=4` : qualité supérieure au coût GPU accru.

Cette option est accessible par variable d'environnement Windows avant de démarrer l'exécutable. Les tests CI utilisent **D3D11 WARP** (émulation CPU du GPU), pas l'Intel HD 620 réel. Les temps relevés par l'utilisateur sur sa machine restent la référence des performances matérielles.

## Validation

- Même scène Pavillon 01, première caméra, 1920×1080, 14,75 h dans les tests.
- CPU GI **4 spp** dans le CI (l'utilisateur utilise **16 spp** dans l'interface) contre GPU 1 échantillon primaire + 0 à 4 rayons secondaires.
- Ne pas prétendre à l'équivalence parfaite entre deux algorithmes différents : les réflexions et la réfraction ne suivent pas encore le CPU, les ombres WARP restent à comparer, le contenu de la variance de 16 spp reste non capturé par le test.
- Ne pas conclure qu'une différence visuelle est réduite uniquement parce que le GPU est plus clair : PSNR, MAE et détail matière doivent être vérifiés sur les sorties **après exécution Windows**.
- Les éditions de matériaux et les lumières invalidant le cache GPU sont testées.

Les changements ne touchent ni l'architecture de Pripiat, ni les objets du Pavillon.
