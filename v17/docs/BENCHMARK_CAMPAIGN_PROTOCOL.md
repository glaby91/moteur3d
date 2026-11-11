# Campagne de benchmark V17 — méthode maintenue

Une unique compilation Windows contient l’application, `moteur3d_v17_benchmark.exe` (smoke/diagnostic) et `moteur3d_v17_campaign.exe` (campagne répétée), accessible par **Outils → Benchmark**.

En mode campagne par défaut : **6 séries × 12 images**, 2 images d’échauffement par série, 1/2/3 buffers en ordre tournant ; deux caméras, 1366×768 et Full HD, plusieurs scènes sélectionnables. Les profils de qualité graphiques sont transférés depuis l’UI au processus de benchmark. On peut augmenter à 8 × 24 pour confirmer une tendance.

Les fichiers créés sont `summary.json`, `campaign_results.csv`, `campaign_frames.csv`, `campaign_report.html` et des PNG. Les statistiques sont mesurées hors présentation Qt ; FPS de pipeline ≠ FPS écran. Le test GPU WARP de la CI est un test fonctionnel, **pas** une prévision des performances sur matériel.

Le runner affiche les phases `chargement`, `construction`, `ombres`, `mémoire`, `CPU/Compute`. Les résultats des étapes achevées sont exportés progressivement pour conserver les observations en cas d’arrêt.

Commandes manuelles :

```powershell
.\moteur3d_v17_campaign.exe ".\benchmark-civic\scene-extreme.json" ".\benchmarks" `
  --all-scenes --include-cpu --rounds 8 --frames 24 --warmup 3 `
  --raster-msaa 4 --raster-anisotropy 16 --raster-lod-percent 100 `
  --raster-culling 1 --raster-lod-enabled 1
```

À lire avant toute optimisation : [Bilan Dell du 10 novembre 2026](BENCHMARK_CAMPAIGN_2026-11-10_DELL.md).
