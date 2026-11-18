# CSE WARP V17 — Full Benchmark

Le Full Benchmark rassemble dans une seule campagne les chemins demandés :

- **DirectX 11 Raster** : aperçu GPU et lecture des pixels, avec 1, 2 et 3 profondeurs de pipeline.
- **Vulkan Raster** : aperçu GPU et lecture des pixels, mesuré de façon synchrone.
- **CPU Ray Tracing PBR direct** : rendu CPU à 1 échantillon par pixel, sur une image de contrôle 320 × 180 avec le nombre de processeurs logiques disponibles.

DX11 et Vulkan reprennent la même scène, la même caméra et les résolutions 1366 × 768 et 1920 × 1080. La voie CPU utilise une résolution réduite pour garder la campagne utilisable; ses temps sont présentés séparément et ne se comparent pas directement aux temps GPU. Le profil CPU multi-thread et le test Compute DX11 restent disponibles en option.

La campagne complète peut inclure les scènes fournies et deux caméras par scène. Elle conserve les temps de chargement, construction PBR, instanciation, BVH, ombres, les mesures par image, les percentiles P50/P95/P99, la dispersion, les compteurs GPU disponibles, les images de contrôle et les informations système.

## Lancer

Ouvrir `moteur3d_v17_benchui.exe`, laisser coché **Full Benchmark : DirectX 11 Raster + Vulkan Raster + CPU Ray Tracing direct**, régler séries/images/échauffement, puis lancer.

En ligne de commande :

```text
moteur3d_v17_campaign.exe <scene.json> <dossier-sortie> --full-benchmark
```

Ajouter `--all-scenes` pour toutes les scènes livrées et `--include-cpu` pour le profil multi-thread CPU et le diagnostic Compute DX11.

Chaque dossier de résultat contient `summary.json`, `campaign_results.csv`, `campaign_frames.csv`, `campaign_report.html` et les images PNG générées. Si Vulkan n’est pas disponible sur la machine, son cas est indiqué en échec avec le message du pilote; DX11 et CPU continuent à être enregistrés.

**Limite de mesure :** les passes GPU de ce benchmark utilisent l’aperçu hors écran avec lecture des pixels. Elles comparent les moteurs de rendu, mais ne mesurent pas le swapchain DXGI/Vulkan natif, la présentation Qt ou le rafraîchissement de l’écran.
