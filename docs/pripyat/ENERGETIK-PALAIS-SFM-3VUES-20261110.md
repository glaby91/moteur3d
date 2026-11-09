# Photo2Scene WARP — reconstruction multivue du Palais Energetik (10 novembre 2026)

## Réalisation effective

Nous avons repris les **22 références photographiques et vidéo** inventoriées le 9 novembre. Parmi elles, une séquence du drone d'un même plan offre une parallaxe utilisable entre **373,00 s, 374,25 s et 375,50 s**. Les autres vues, dont les façades recadrées, restent des références complémentaires d'orientation : elles ne sont pas introduites artificiellement comme photos d'une même caméra.

La reconstruction numérique a réellement été exécutée sur le MP4 transmis par l'utilisateur et les calculs/visualisations sont fournis dans la conversation : **243 points 3D relatifs** à la focale hypothétique de 550 pixels, avec textures RGB tirées de la vidéo. Les livrables comprennent un point cloud PLY et GLB, les correspondances pixels/XYZ, un visuel 3D, les trois images et un script autonome.

L'outil GitHub est `tools/photo2scene/sparse_drone_3view.py` et ses masques sont enregistrés dans `tools/photo2scene/data/pripyat/energetik-drone-3view-rois.json`. La vidéo **n'est pas publiée** dans le dépôt.

## Contrôle indépendant

- Image source : 640×360.
- Paire source 373→375,5 s : **340** correspondances SIFT.
- Modèle homographie : **243** inliers (71,5 %) ; il ne suffit pas à expliquer toute la scène.
- Modèle épipolaire à f=550 px : **305** inliers.
- Triangulation après filtrage (cheirality + parallaxe >= 0,8°) : **243** points.
- Parallaxe médiane : **2,084°**.
- Image intermédiaire 374,25 s : **42 points réservés** au test de la pose PnP, erreur RMS de reprojection **0,572 pixel** (médiane 0,273 px), avec sélection préalable des appariements réciproques.
- Analyse d'incertitude focale : 350, 450, 550, 650, 750 px. Les points et angles reconstruits changent avec la focale. À **450 px**, un faux appariement possible donne une erreur RMS globale de **41,265 px** bien que la médiane reste 0,313 px : cette anomalie doit rester signalée ; on ne présente donc pas la précision de reprojection comme une garantie générale.

Les erreurs de reprojection indiquent la cohérence des **points échantillonnés sous un modèle optique supposé**. Elles **ne prouvent pas** la fidélité géométrique de l'enveloppe, de la tour, ni des cotes en mètres.

## Frontière architecturale

Ces 243 points ne couvrent qu'une fraction du bâtiment visible (pans de toits, façade vitrée, volume technique, arrière). Ils ne permettent pas de reconstruire les parties cachées et ne sont pas un maillage de murs.

Aucune dimension métrique du Palais n'est prouvée par cette séquence. La translation de caméra est normalisée à **1 unité arbitraire** ; la focale et la distorsion DJI exactes sont inconnues ou peuvent avoir été modifiées par le montage vidéo.

Ne pas modifier automatiquement `M3D_ENERGETIK_ARCH`, le niveau de toiture, les cages d'escalier, la fresque ou le volume haut. Conserver l'arcade et le Polissya hors du travail.

## Exécution reproductible

```bash
pip install numpy opencv-python-headless trimesh
python tools/photo2scene/sparse_drone_3view.py \
  --video "/chemin/vers/video_pripyat.mp4" \
  --roi tools/photo2scene/data/pripyat/energetik-drone-3view-rois.json \
  --out /chemin/vers/resultats --focal 550
```

Le programme produit `validation.json`, `image_to_xyz.json`, un `.ply` et un `.glb` (nuage de points en coordonnées relatives).

La CI sur GitHub vérifie séparément le solveur avec une scène synthétique à trois caméras, sans reconstituer ni publier la vidéo privée de l'utilisateur.

## Ce qui manque pour remplacer la géométrie 3D de notre moteur

Pour aligner ce nuage à la scène il faut associer plusieurs **points 2D/3D réels** communs au modèle (corniches, sommets de la tour, pans de toiture), établir une focale et estimer l'échelle via dimensions indépendamment vérifiées. Une similitude 3D pondérée suivie de mesures sur des observations non utilisées permettra alors de décider quels volumes doivent être corrigés.

**Statut : exécution réelle de photogrammétrie sparse, sans intégration destructrice à la scène existante.**
