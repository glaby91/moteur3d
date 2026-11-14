# V17 — première implémentation hybride : illumination indirecte spatiale / temporelle

**11 novembre 2026 — branche `feature/v17-308-hybrid-lighting-261111`.**

## Ce qui a réellement été ajouté

- **Volume de sondes spatiales à faible résolution (16 × 8 × 16)** : pour
  chaque cellule d'une scène, 8 rayons statiques vers différentes directions
  interrogent la BVH CPU ; leurs intersections fournissent une approximation
  de la visibilité du ciel et de la couleur des surfaces environnantes.
  Les coefficients sont envoyés dans une texture 3D GPU, échantillonnée
  en trilineaire à la position de chaque fragment DirectX 11.
- **Table temporelle de 25 états (7 h – 19 h, pas de 30 min)** : contributions
  de ciel, teintes de rebond et coloration solaire précalculées en CPU puis
  interpolées continûment à l'heure demandée. Les ombres solaires
  directionnelles existantes restent dynamiques.
- **Profil de sortie filmique HDR** appliqué après la lumière directe,
  les lumières locales, les reflets et les rebonds. Il s'agit d'un
  traitement expérimental visant à protéger les hautes lumières et
  donner du contraste aux demi-teintes ; sa calibration reste à faire
  sur le Dell Intel HD 620.
- **Commande Qt réversible** « Éclairage hybride : rebonds + HDR (bêta) »,
  mémorisée dans QSettings. Le mode est activé par défaut dans la nouvelle
  interface, mais le chemin moteur autonome / benchmark historique
  reste **désactivé par défaut**, afin de préserver la reproductibilité des
  images V17.306/307 sans option.
- **Tests Windows WARP** : homogénéité des rendus successifs, effet
  visuel réel, variation d'heure matin/soir et retour exact au Raster
  historique après désactivation.

## Limites clairement identifiées

Ce prototype **ne constitue pas une illumination globale précalculée
physiquement complète**. Les sondes utilisent un faible nombre de rayons
et une moyenne chromatique des matériaux et ne calculent pas de multiples
rebonds radiométriques ni l'occultation précise des petits détails. La
résolution spatiale fixe peut être insuffisante pour un immense paysage
urbain. Les changements de géométrie/ressources demandent de reconstruire
les sondes ; changer l'heure n'exige pas de reconstruire la géométrie.

Le GPU Intel HD 620 doit être testé : les premières images reconstruisent
les sondes CPU et le fragment shader effectue un échantillonnage 3D
supplémentaire. Les FPS ne sont **pas garantis**. Les couleurs et
l'exposition filmique ne sont pas encore réglées à partir de captures
calibrées de Pripiat.

## Intégrité, mesures et suite

- Aucune réduction de textures PBR 2048, de géométrie, d'ombres, de qualité
  MSAA ou de précision des normal maps.
- Conserver captures et benchmarks avant/après avec le **même SHA Git**
  et vérifier la fluidité (P50/P95/P99) sur HD 620.
- Étapes futures : augmenter la densité des sondes localement dans les
  bâtiments, développer une solution de baking hors ligne/cache persistant,
  calculer les transferts énergétiques plus rigoureusement et comparer à
  des références CPU GI/DXR.
- Le mode **Direct GPU Present** est un chantier distinct, suivi par
  [issue #25](https://github.com/glaby91/moteur3d/issues/25).
- La compression automatique des benchmarks est suivie dans
  [issue #24](https://github.com/glaby91/moteur3d/issues/24).

**Versioning :** chaque changement de code, scène et test dispose d'un
commit identifié ; seul le pipeline GitHub Actions ayant réellement
compilé et testé peut être annoncé comme build validée.