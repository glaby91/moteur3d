# Matériaux Pripiat réutilisables — CSE WARP V17

Cette bibliothèque conserve les **22 identifiants stables** `prp_*` présents dans le catalogue historique :
`experiments/2026-10-30-cote-ete-gt/native_v14/docs/PRIPYAT-MATERIALS-CATALOGUE.md`.
Son fichier central est `catalogue.json`, avec pour chaque matériau le nom, la couleur
de base, la rugosité, la métallicité, la transmission, l'indice et un `textureSet`.

## Fidélité de l'extraction

- **Identifiants d'origine :** conservés exactement, sans renommer les 22 matériaux.
- **Paramètres et recettes V17 :** conversions artistiques documentées, issues des plages et
  indications de Pripiat (non des pixels des fichiers PBR historiques).
- **Cartes générées dans V17 :** albédo, normale, rugosité et mipmaps via les recettes
  procédurales du moteur. Ce sont des alternatives **compatibles** et reproductibles,
  **pas une copie binaire** des cartes historiques.
- **PBR natif réellement existant dans le dépôt :**
  `native_v14/assets/pripyat/textures/prp_concrete_panel.m3dpbr.gz` et son
  `prp_concrete_panel_source.jpg`. Le pipeline Windows les joint **sans modification** sous
  `material-library/pripyat-v17/originaux/` pour réutilisation future. Le chargeur
  JSON V17 ne sait pas encore lire automatiquement ce format `.m3dpbr.gz`.
- Le pack conserve également les assets existants `energetik_facade_bay.glb`,
  `energetik_fresco_mosaic.glb`, l'albédo JPEG de la fresque et leurs manifestes
  sous `material-library/pripyat-v17/hero-assets/`. Ces fichiers originaux
  **ne sont pas utilisés** dans le bâtiment fictif et ne sont pas encore lus
  automatiquement par le chargeur JSON V17.
- Les autres variantes `prp_*` décrites dans le catalogue sont principalement
  des prescriptions de matériau, non 21 fichiers PBR binaires indépendants.
- La fresque historique et les photographies documentaires restent distinctes :
  aucune photographie n'est incluse dans les cartes de cette scène fictive.

## Réutiliser dans une nouvelle scène V17

1. Copier `catalogue.json` comme source canonique, ou relancer
   `scenes/v16/benchmark-civic/generate.cjs`.
2. Les deux fichiers produits `materials/pripyat-reuse.json` et
   `pbr/pripyat-reuse-<profil>.json` sont directement acceptés par le moteur V16/V17.
3. Les déclarer dans le `scene.json` du projet **à l'intérieur de son répertoire racine** ;
   le chargeur refuse les ressources qui sortent du répertoire de la scène.
4. Plusieurs milliers d'objets peuvent partager **une seule entrée de matériau** sans
   multiplier la génération PBR par instance.
5. Pour un import fidèle des vraies cartes natives, prévoir plus tard un décodeur
   `.m3dpbr.gz` avec contrôles des canaux, de l'orientation, du gamma et des mipmaps.
   Cette tâche n'est pas masquée derrière le simple nom de matériau.

## Sécurité mémoire

Le rasteriseur DX11 crée des tableaux de textures à résolution **commune maximale** :
charger un seul matériau 2048 peut augmenter le coût GPU de tous les matériaux.
Le benchmark limite donc ses 22 jeux de textures à **512 px en léger et 1024 px
en lourd/extrême** par défaut. Les recettes d'origine jusqu'à 2048 px sont
conservées dans `catalogue.json` ; on ne force pas 2K sur l'Intel HD 620.
