# CSE WARP Beta X — Moteur3D V16 Windows, Pavillon 01 et modules séparés

**Nom public du moteur : CSE WARP Beta X.** Les exécutables conservent temporairement leurs noms techniques `moteur3d_v16*.exe` pour préserver les scripts de build et les outils existants.

## Périmètre
- Windows x64 uniquement : MSVC 2022, Qt 6.8.3 et CMake.
- Pavillon 01 est la seule scène V16 active ; Pripiat sera converti séparément plus tard.
- Aucun lecteur de scène V3/V4/V5 dans la cible V16.

## Optimisations conservatrices (V16 — sans baisse de qualité)
- **GPU :** les scènes V16 construites par `buildRuntime()` portent un identifiant de contenu unique et une révision. Les changements de caméra ne rescannent plus les mipmaps PBR 2048² ; les modifications de matériaux dans Qt appellent `markGpuContentChanged()`. Les scènes non suivies continuent à utiliser l'empreinte intégrale, sans risque de cache périmé pour les appels historiques.
- **CPU :** une carte d'ombres calculée pour Pavillon 01 est réutilisée entre les caméras d'une même scène, tant que l'heure d'éclairage et la scène ne changent pas.
- Aucun échantillon supprimé, aucun changement de résolution, aucune réduction de la géométrie. La validation automatique vérifie la stricte identité des rendus GPU avant/après réutilisation du cache et l'invalidation en cas de modification de matériau.
- Les chiffres de performance sous WARP logiciel ne préjugent pas des gains sur le GPU matériel.

## Détail architectural — scène Pavillon 01 enrichie
- 311 instances (contre 172) et 26 matériaux PBR distincts (contre 11). Les nouveaux objets correspondent à de vrais détails : étanchéité de toiture, couvertines zinc, évacuations/ventilations, bardage bois à joints, soubassements minéraux, bandeaux et meneaux de fenêtres, seuils, terrasse en lames étroites, sols terrazzo et équipements intérieurs.
- Bois de terrasse, bardage, écorce et feuillages ont leurs matériaux distincts. Les textures secondaires utilisent des chaînes mipmap PBR de 512 à 1024 px afin de limiter le coût mémoire ; les matériaux d'origine conservent leur définition.
- La forme générale, les positions des caméras et la transparence vitrée de Pavillon 01 sont conservées.

## V16 — lumière naturelle, normal mapping et comparaison Full HD
- Soleil plus cohérent en orientation : l'azimut tourne avec l'heure, en plus de la hauteur. Ciel légèrement plus clair et moins saturé près de l'horizon, soleil de fin de journée plus chaud.
- Les deux moteurs utilisent la même heure solaire ; l'application possède désormais un sélecteur de 7 h à 19 h (pas de 15 minutes). Le cache des cartes d'ombres CPU est invalidé quand l'heure change.
- CPU direct : ambiance moins plate sur Pavillon 01, en conservant les ombres et le bump mapping existant. CPU GI : éclairement hémisphérique dépendant de l'orientation des normales (zénith plus froid, horizon plus neutre).
- GPU D3D11 Compute : couleurs du soleil et contribution du ciel diffus transmises au shader, sans rayon supplémentaire ni perte de définition. La lumière fait apparaître le relief des normal maps, sans faux grain d'image.
- Les matériaux procéduraux du bois, de la pierre, de l'enduit, du béton et de l'étanchéité ont des normales de hauteur cohérentes ou des fibres directionnelles. Ces effets ne modifient pas les silhouettes, contrairement à une vraie géométrie ou un déplacement parallax/occlusion.
- La CI teste les rendus en 1920×1080 à 14 h 45 et à 17 h pour le CPU et le GPU ; elle vérifie que le retour à 14 h 45 produit exactement la même image GPU. Les deux images « soleil rasant » sont incluses dans l'archive Windows.
- Ces modèles d'éclairage sont des approximations artistiques efficaces, **pas** une simulation atmosphérique physiquement calibrée ou une GI GPU multi-rebonds.

## V16.1–V16.3 — anti-bruit CPU et calcul progressif
- **V16.1 :** échantillonnage CPU direct configurable (1, 4 ou 16 échantillons/pixel) indépendant de la résolution native Full HD, QHD ou 4K. Débruitage bilatéral facultatif, désactivé par défaut en CPU direct pour préserver les détails des matériaux. Les textures procédurales restent déterministes, et leur grain n'est pas du bruit Monte-Carlo.
- **V16.2 :** vrai rendu GI progressif avec accumulateur HDR linéaire par pixel, échantillons uniques à chaque passe (séquence R2 et rotation par pixel), passes visibles 1/2/4/8/16/... jusqu'à la cible. Les pixels ne sont convertis en sRGB et tone-mappés qu'à l'affichage ; aucun PNG 8 bits n'est utilisé pour accumuler l'énergie.
- **V16.3 :** nombre d'échantillons GI réglable (4, 8, 16, 32, 64 spp) et profondeur de rebonds 2 ou 4. Débruitage bilatéral optionnel appliqué *après* la moyenne HDR et non dans l'accumulateur. Le changement de caméra, de matériau ou de réglage annule les passes obsolètes.
- **Tests :** accumulation progressive et accumulation monobloc doivent fournir des PNG strictement identiques à paramètres égaux (test algorithmique compact). Image de référence CI en **1920×1080, 4 spp GI et débruitée**, pas un rendu GI final 1 spp. Le GPU Compute direct demeure inchangé.
- Le débruitage bilatéral simple peut lisser de petits détails ; il n'est pas équivalent à un débruiteur guidé par normales/profondeur ou au débruitage IA.

## Éclairage global CPU — mode facultatif
- Le moteur disposait déjà d'un path tracer CPU avec rebonds indirects et encodage HDR. La V16 propose désormais dans la liste des backends : « CPU Éclairage global — 2 rebonds, 4 échantillons » et « CPU Éclairage global — 4 rebonds, 16 échantillons ».
- Ces modes sont **CPU uniquement** et plus coûteux que l'aperçu direct. Le GPU D3D11 Compute conserve son mode PBR simplifié rapide ; aucun calcul GI GPU n'est revendiqué.
- Les tests Windows conservent CPU/GPU Full HD et ajoutent une image GI Full HD distincte. Le test GI technique emploie 1 échantillon pour couvrir la voie d'exécution, les préréglages d'utilisation en emploient 4 ou 16.

## Architecture livrée
- `moteur3d_v16_core.dll` : moteur C++ (ray tracing CPU, aperçu Compute D3D11, BVH, matériaux).
- `moteur3d_v16.exe` : visualisation Qt6, catalogue des matériaux, explorateur et six raccourcis caméra **1 à 6**.
- `moteur3d_v16_benchui.exe` : **fenêtre autonome** de configuration des benchmarks.
- `moteur3d_v16_benchmark.exe` : calculateur de benchmark indépendant, sans fenêtre GUI.

L'interface et les benchmarks se compilent séparément de la DLL. Les modifications de l'interface ne nécessitent pas de recompiler le cœur tant que son code et son ABI ne changent pas.

## Scène compacte
- Maison : environ 18 × 12 m ; sol : **30 × 26 m** (centre en z=1), environ 5 à 6 m autour.
- Terrasse rapprochée et bassin resserré pour tenir dans les limites du terrain.
- **3 arbres** (12 instances tronc/houppier), **6 massifs**, et accès en dalles.
- **6 caméras** (façade complète, jardin, entrée, salon, étage, aérienne) accessibles par boutons et touches 1 à 6.
- Les anciennes sphères de feuillage restent des primitives : l'objectif de qualité visuelle n'est pas encore atteint.

## Benchmark complet — exports automatiques
Dans le programme principal : **Outils > Benchmark complet…**.
La fenêtre de benchmark propose un dossier de destination et les modes complet, rapide, ou CPU seul.

Mode complet :
- CPU ray tracing : 6 caméras × 3 résolutions (1920×1080, 2560×1440, 3840×2160) × 5 réglages de threads (1/2/4/8/16).
- CPU qualité : 6 caméras × 1920×1080, 4 échantillons/pixel.
- GPU D3D11 Compute : 6 caméras × 1920×1080, textures PBR simplifiées ; marqué `skipped` si indisponible.
- Dossier horodaté `Pavillon01-V16-AAAAmmJJ_HHMMSS_mmm`.
- Exports `results.csv`, `summary.json`, `system.json`, `progress.jsonl` et `images/*.png`.
- Les essais GPU sont des aperçus géométriques ; **aucune parité PBR CPU/GPU n'est revendiquée**.

Le test rapide automatique de GitHub Actions vérifie un parcours CPU et la production de CSV, JSON et PNG. Le mode complet n'est pas exécuté en CI.

## Construction incrémentale
Depuis la racine du dépôt :
```powershell
.\v16\build-windows.ps1 -Target ui
.\v16\build-windows.ps1 -Target engine
.\v16\build-windows.ps1 -Target benchmark
.\v16\build-windows.ps1 -Target benchmark-ui
.\v16\build-windows.ps1 -Target all
```
La compilation utilise un seul cache `build-v16` : un changement de source GUI seul ne provoque pas la recompilation des fichiers moteur.

## Tests et limitations
- Build Windows, tests de chargement, rendu CPU et export rapide benchmark **validés sur GitHub Actions n°85**.
- L'application utilise le Full HD (1920×1080) par défaut, avec options Full HD (4 échantillons CPU) et 4K. Tous les PNG de référence CPU, GPU et benchmark sont en Full HD minimum ; la CI contrôle les dimensions réelles des fichiers.
- La génération d'un PNG n'est pas une validation de qualité architecturale.
- Les cartes PBR sont encore majoritairement procédurales ; les images PBR externes et le rendu GPU PBR cohérent restent à faire.
- L'exécutable Windows n'a pas été manipulé manuellement sur un PC physique dans cette session.

## Liens
- Branche : https://github.com/glaby91/moteur3d/tree/feature/v16-modular-scene-dispatch
- Workflow Windows : https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v16.yml
