# Parité de luminosité V17

Cette suite compare les captures de **la même scène, caméra et résolution** dans les backends raster D3D11, Vulkan et LumaTrace. Elle lance le moteur de campagne déjà présent dans le package, puis mesure la luminance linéaire des PNG produits. Elle ne modifie ni le moteur, ni les matériaux, ni l'exposition.

## Lancer sur Windows

Copier `RUN_BENCHMARK_LUMINOSITE.ps1` et `manifest.json` dans le dossier du package V17, puis ouvrir PowerShell dans ce dossier :

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_BENCHMARK_LUMINOSITE.ps1
```

Le package doit déjà contenir `moteur3d_v17_campaign.exe` et `benchmark-civic\scene.json`. Le script n'installe ni ne télécharge le moteur. Pour choisir une scène ou régler la campagne :

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_BENCHMARK_LUMINOSITE.ps1 `
  -ScenePath ".\benchmark-civic\scene.json" -Rounds 4 -Frames 12 -Warmup 2
```

Les résultats vont dans `benchmarks-luminosite\<horodatage>\` : rapport CSV et JSON de luminance, plus le dossier complet de campagne avec PNG et résumé moteur. Le script sélectionne les captures dans `summary.json` et les apparie par scène, caméra et résolution.

## Lecture des mesures

- Mesure principale : moyenne de luminance Rec.709 après conversion sRGB vers lumière linéaire.
- Détail : image entière et grille normalisée 3 × 3, avec médiane, P95 et fractions proches du noir/blanc.
- Référence : capture D3D11 Raster à profondeur de lecture 1.
- Bandes de revue initiales : Vulkan ±5 % et LumaTrace ±10 % sur la moyenne globale. Un dépassement produit `REVIEW`, sans faire échouer ni corriger le moteur.
- Une différence LumaTrace peut refléter son modèle d'éclairage; ce rapport sert à repérer où et combien les rendus divergent, pas à imposer une égalité pixel à pixel.

## Portée actuelle

La première campagne réutilise les vues/caméras déjà définies par la scène V17. Elle ne fabrique pas encore de chart synthétique avec plages grises, matériaux isolés, lumière directe et rebonds séparés. Compute, CPU RT/GI et captures à résolution différente sont exclus de la comparaison de luminosité, car leurs charges ne sont pas équivalentes. Pour une mesure pertinente, lancer la campagne sur le GPU réel; le mode WARP valide le fonctionnement mais ne représente pas les performances matérielles.

La définition machine lisible est dans `manifest.json`.

Le vérificateur Windows de GitHub Actions exécute aussi `-SelfTest` : il compile l'analyseur et contrôle une capture PNG synthétique, sans lancer de benchmark matériel.
