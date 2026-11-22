# CSE WARP V17 — Full Benchmark

Le Full Benchmark rassemble dans une seule campagne les chemins demandés :

- **DirectX 11 Raster** : aperçu GPU et lecture des pixels, avec 1, 2 et 3 profondeurs de pipeline.
- **Vulkan Raster** : aperçu GPU et lecture des pixels, mesuré de façon synchrone.
- **CPU Ray Tracing PBR direct** : rendu CPU à 1 échantillon par pixel, sur une image de contrôle 320 × 180 avec le nombre de processeurs logiques disponibles.

DX11 et Vulkan reprennent la même scène, la même caméra et les résolutions 1366 × 768 et 1920 × 1080. La voie CPU utilise une résolution réduite pour garder la campagne utilisable ; ses temps sont présentés séparément et ne se comparent pas directement aux temps GPU. Le profil CPU multi-thread et le test Compute DX11 restent disponibles en option.

La campagne complète peut inclure les scènes fournies et deux caméras par scène. Elle conserve les temps de chargement, construction PBR, instanciation, BVH, ombres, les mesures par image, les percentiles P50/P95/P99, la dispersion, les compteurs GPU disponibles, les images de contrôle et les informations système.

## Lancer

Le package V17.600 n'installe pas d'interface de benchmark dans le moteur. Depuis une invite de commandes, lancer `LANCER_BENCHMARK.bat` avec le manifeste de scène, le dossier de sortie et les options souhaitées. Le benchmark tourne dans la console et ne démarre pas l'interface principale.

En ligne de commande :

```text
LANCER_BENCHMARK.bat <scene.json> <dossier-sortie> --full-benchmark
```

Le lanceur appelle `moteur3d_v17_campaign.exe`.

Ajouter `--all-scenes` pour toutes les scènes livrées et `--include-cpu` pour le profil multi-thread CPU et le diagnostic Compute DX11.

Chaque dossier de résultat contient `summary.json`, `campaign_results.csv`, `campaign_frames.csv`, `campaign_report.html` et les images PNG générées. Si Vulkan n’est pas disponible sur la machine, son cas est indiqué en échec avec le message du pilote ; DX11 et CPU continuent à être enregistrés.

## Lire les mesures

- Le **temps de construction du BVH** mesure la préparation de la structure d'accélération. Il ne mesure pas le temps nécessaire pour parcourir ce BVH pendant le rendu.
- Le **temps de rendu** comprend le travail du chemin de rendu mesuré. Il peut inclure des coûts CPU, GPU ou de lecture des pixels selon le backend ; comparer uniquement des cas ayant le même protocole.
- Les valeurs **P50/P95/P99** décrivent la distribution des images mesurées. Pour comparer des versions, regarder au minimum la médiane (P50), le P95 et la dispersion, pas une image isolée.
- Les temps de préparation (chargement, matériaux, instanciation et BVH) doivent rester séparés des durées par image. Une construction plus rapide ne garantit pas un rendu plus rapide, et inversement.

## Limites et protocole de comparaison

Les passes GPU de ce benchmark utilisent l’aperçu hors écran avec lecture des pixels. Elles comparent les moteurs de rendu, mais ne mesurent pas le swapchain DXGI/Vulkan natif, la présentation Qt ou le rafraîchissement de l’écran. Les mesures CPU prises autour de la soumission des commandes en présentation directe ne prouvent pas la fin d'exécution GPU.

La CI de V17.600 force WARP pour ses rendus DX11 : WARP est un adaptateur logiciel. Ces résultats valident des chemins fonctionnels et des régressions sur le runner Windows, mais ne prédisent pas les performances du Dell HD620 ou d'un autre GPU matériel. Une CI verte ne démontre donc pas un gain de vitesse BVH sur la machine cible.

Pour mesurer un gain BVH de façon exploitable :

1. Utiliser la même machine et le même état d'alimentation/thermique.
2. Garder la scène, la caméra, la résolution, le backend, le nombre d'échantillons et les options identiques.
3. Comparer les versions avant/après avec plusieurs passages ; noter les commits exacts et conserver tous les fichiers du dossier de résultats.
4. Comparer séparément le chargement, la construction BVH et les temps par image (P50/P95/P99). Ne pas mélanger une mesure WARP avec une mesure GPU matériel.
5. Répéter les passages dans le même protocole et vérifier les images produites pour s'assurer que les deux versions rendent le même contenu.

Une comparaison qui change à la fois le BVH, le backend, la scène ou les réglages ne permet pas d'attribuer le gain observé au BVH.
