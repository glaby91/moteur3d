
# CSE WARP V17 — consignes de la prochaine version

Date : 11 novembre 2026.
**État : spécification uniquement, aucun développement demandé maintenant.**

## 1. Mode « Direct GPU Present » — architecture future

L'objectif est un affichage plus fluide en remplaçant pour la navigation
la chaîne GPU → staging → Map(READ) → CPU → QImage → Qt par une présentation
GPU directe dans une fenêtre native, au moyen d'une **swap chain DXGI Flip Model**.
Le principe évoque le framebuffer des cartes 3dfx Voodoo 4/5 / VSA-100,
mais les ordinateurs modernes utilisent normalement la composition Windows :
ne pas garantir un scan-out DirectFlip/Independent Flip sans mesure.

### Architecture prévue

- Conserver les moteurs Raster D3D11, CPU ray tracing, Compute et tous les
  matériaux/scènes et shaders existants.
- Intégrer une fenêtre de rendu native Win32 (HWND) dans l'interface Qt 6.
  Utiliser DXGI_SWAP_EFFECT_FLIP_DISCARD ou un mode compatible.
- Calculer le MSAA 4× dans une surface multisample puis effectuer un
  ResolveSubresource sur GPU vers le buffer mono-échantillon de la swap chain
  avant Present. Ne pas utiliser Map(READ) pour les images de navigation.
- Conserver les chemins actuels de lecture CPU pour PNG, mesures, captures,
  tests de référence, rendu CPU et diagnostics.
- Gérer VSync, cadence de Present, attente d'images en vol, rafraîchissements
  caméra/entrée, DPI, redimensionnement, minimisation, Alt-Tab, fenêtres
  superposées, écrans multiples et récupération après panne du GPU.
- Proposer « Direct GPU Present » comme mode réversible à côté du
  mode « Raster compatible ». Prévoir repli automatique si non compatible.
- Tester DirectFlip et Independent Flip de façon conditionnelle ; DWM peut
  continuer de composer la fenêtre et ce n'est pas un échec.

### Contraintes non négociables

- Aucun sacrifice de finesse ou de fidélité visuelle : textures PBR jusqu'à
  2048 px et mipmaps, normal maps, rugosité, métallicité, transparence,
  géométrie complète, soleil, ombres, éclairages et reflets inchangés.
- MSAA 4×, anisotropie 16×, LOD 100 %, sans résolution dynamique imposée.
- Ne pas réinventer le moteur ni reconstruire les scènes de Pripiat.
- Conserver les fonctionnalités d'export, benchmark et ray tracing CPU.

### Essais de validation indispensables

1. Vérifier la fidélité des pixels **avant Present** par A/B avec le Raster
   existant, puis caractériser séparément la colorimétrie du bureau Windows.
2. Mesurer le GPU Draw, Resolve, Present, FPS réellement affichés,
   latence entrée-à-écran et P50/P95/P99 des images.
3. Mesurer sur le Dell Intel HD 620 et une machine GPU moderne, à plusieurs
   résolutions et sur les scènes extrême, Pavillon et Pripiat.
4. Tester les déplacements AZERTY/flèches, souris, changements de caméra,
   fermeture, bascule plein écran / fenêtre et répétition des séquences.
5. Retour arrière immédiat si les images ou la stabilité se dégradent.

## 2. Versioning strict de toute modification

**Chaque modification doit être versionnée, sans exception**, même mineure :
code, corrections, shaders, scènes, objets/meshes, textures/assets, paramètres,
scripts GitHub Actions, benchmark, tests, consignes et documentation.

Règles :
1. Une trace Git obligatoire : branche de travail, commit(s) descriptifs,
   SHA et fichiers modifiés. Les expérimentations passent par PR et ne
   modifient pas silencieusement la version de référence.
2. Tout nouveau binaire Windows porte sa propre identité CSE WARP V17.<build>
   liée au numéro réel GitHub Actions, à la révision Git et à la date.
   Ne jamais réutiliser le numéro d'une build pour un binaire différent.
3. Maintenir un journal de changements explicite. Distinguer « demandé »,
   « codé », « compilé », « testé sur WARP », « validé sur matériel »,
   « publié ». Aucun résultat ne doit être présenté comme validé sans preuve.
4. Associer les tests à leur version exacte : captures PNG, résultats CSV/JSON,
   rapport HTML, ZIP automatique des benchmarks et références de parité.
5. Garder la dernière version stable téléchargeable et une méthode de repli.
   Ne pas fusionner une modification qui dégrade le rendu sans accord.
6. Les présentes consignes sont elles-mêmes conservées dans un commit Git.
   Leur enregistrement **n'autorise pas leur implémentation immédiate**.

## 3. Références et suivi

- Issue Direct GPU et versioning : https://github.com/glaby91/moteur3d/issues/25
- Issue compression automatique de benchmark : https://github.com/glaby91/moteur3d/issues/24
- Documentation DXGI : https://learn.microsoft.com/fr-fr/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model
