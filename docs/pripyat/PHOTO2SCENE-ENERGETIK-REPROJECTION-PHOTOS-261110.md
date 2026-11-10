# Photo2Scene WARP — projection réelle des photographies du Palais Energetik

**Exécution : 10 novembre 2026.** Projet Moteur3D / photo-projection géométrique CPU, sans CUDA, **Palais uniquement**.

## Travaux réellement exécutés sur le MP4 utilisateur

- Reprise de la séquence du drone et des **13 poses** du dataset multivue (en résolution originale disponible 640×360).
- **Six surfaces candidates** avec sommets calculés par intersection rayon caméra / plan 3D :
  1. toiture terrasse principale : plan RANSAC du nuage SfM ;
  2. façade vitrée : plan architectural orthogonal à la toiture, passant par l'arête visible ;
  3. retour latéral : idem, seconde arête de toiture ;
  4. paroi du volume haut : plan RANSAC de la zone haute (seulement 17 points parmi 26 candidats) ;
  5. toiture arrière : plan RANSAC des points arrière ;
  6. façade du volume arrière : plan déduit de l'arête de la toiture arrière.
- **Textures originales projetées** sur chacune des douze faces triangulées (deux triangles par quad), sans dessin artificiel ni complétion par générateur d'image.
- Export d'un **GLB texturé** et de cinq vues virtuelles indépendantes ; cinq comparaisons avec les photogrammes de drone.
- Sur les deux images dont les couleurs n'ont pas servi à texturer les faces :
  - 374,00 s : **22,60 dB PSNR** sur les **52 670 pixels** projetés appartenant au masque de Palais ;
  - 375,00 s : **20,46 dB PSNR** sur **58 521 pixels**.
- Ces images ont servi indirectement au calcul des poses SfM du pipeline précédent : ce ne sont **pas des contrôles totalement indépendants** de toute la chaîne, seulement de l'apparence projetée sur les panneaux.

**Ne pas présenter ce résultat comme un Palais reconstruit entièrement : la cage de scène et les façades dissimulées restent incomplètes.**

## Outil reproductible

Code : `tools/photo2scene/projective_photo_mesh.py` ; configuration des six plans : `tools/photo2scene/data/pripyat/energetik_photo_projection_planes_20261110.json`.

Dépendances : Python 3.11+, NumPy, OpenCV, Pillow, Trimesh.

```bash
python tools/photo2scene/projective_photo_mesh.py \
  --dataset "/dossier/Photo2Scene_WARP_Energetik_multiview_20261110" \
  --constraints "tools/photo2scene/data/pripyat/energetik_photo_projection_planes_20261110.json" \
  --output "./energetik_projected"
```

Fichiers de sortie : `energetik_photo_projected_nonmetric.glb`, `projection_*.png`, `view_*.png`, `projection_validation.json`.

Les photographies d'origine du Palais ne sont jamais ajoutées au dépôt GitHub public ; elles se trouvent dans les archives privées de cette conversation. Le script a besoin de leurs fichiers locaux pour reproduire exactement les textures.

## Limites et conditions avant intégration définitive dans Moteur3D

- **Échelle inconnue** : focale 550 pixels supposée, pas d'EXIF calibré ni de mètres/coordonnée SfM. Les 11,6 m de salle et 21 m du volume scénique ne sont pas additionnés ni utilisés ici pour inventer une mise à l'échelle.
- Certaines textures contiennent de la végétation au premier plan ; il faut des masques d'occlusion plus précis et une photo multi-source de chaque panneau.
- Le panneau haut et les parties cachées ne sont pas encore géométriquement raccordés en volume fermé. Ce GLB est un **ensemble de panneaux photo** ; il ne remplace pas les murs et plafonds de la scène Moteur3D.
- Les zones blanches des vues 3D correspondent volontairement à des surfaces absentes. Le solveur n'a pas été autorisé à les imaginer.
- Le renderer d'inspection trie les faces par profondeur médiane, ce qui est une approximation, pas un z-buffer par pixel. L'export GLB contient la vraie géométrie.
- Les plans sont construits à partir d'annotations manuelles vérifiables et de calculs sur les images ; ce n'est pas une reconstruction automatique de l'édifice complet.

**Test de validation à rechercher :** projection à des positions réelles non employées pour ajuster les caméras, puis cohérence des arêtes de toiture, du retour vitré et des volumes dans plusieurs photos. Avant cela, le projet reste sur une branche isolée et ne touche pas aux scènes historiques validées.

## Documents et résultats

Le pack fourni dans la conversation contient les six faces GLB, le script réellement exécuté sur les photos, la planche cinq vues, les images avant/après à 374 et 375 s et les coordonnées XYZ relatives des surfaces.

Branch : `feature/photo2scene-energetik-photo-projection-261110`.
