# Photo2Scene WARP — Gaussian Splatting du Palais Energetik

**10 novembre 2026 — prototype expérimental en reconstruction photogrammétrique réelle.**

## Résultats locaux sur les vidéos de l'utilisateur

La séquence drone à 373,00 / 374,25 / 375,50 s a été traitée dans le périmètre du seul Palais Energetik. Le traitement CPU produit **9 095 gaussiennes colorées**, contre les 243 correspondances SfM éparses de l'essai précédent.

Procédure réellement exécutée :

1. Localisation du Palais sur les trois images (masques de travail).
2. Estimation SIFT/E/R/t et pose PnP sur la vue intermédiaire. Focale **hypothétique 550 pixels** (pas un EXIF mesuré).
3. Flot optique dense DIS aller/retour et filtrage des pixels incohérents ; épipolaire et profondeur positive.
4. Triangulation de positions 3D **relatives**, puis création de gaussiennes (centre, couleur RGB, échelle XYZ, rotation et opacité).
5. Export PLY avec les champs 3DGS standard, format SPLAT et visualisations par compositing CPU de gaussiennes.
6. Mesures de reprojection et comparaison avec une image à 374,75 s non utilisée pour la densification. Cette image contribue toutefois à l'estimation de sa propre pose de contrôle : cette comparaison **n'est pas** une validation indépendante de la géométrie absolue.

Sur la vidéo de contrôle :
- **9 095 gaussiennes initialisées** ;
- **0,18 pixel d'erreur médiane** sur l'image intermédiaire utilisée dans le filtrage ; cette erreur ne démontre pas l'exactitude des formes du Palais ;
- **PSNR de 20,15 dB** sur les pixels couverts d'une image de contrôle à 374,75 s ;
- environ **78 % de la zone du Palais visible** couverte sur cette image ; les parties manquantes ne sont pas inventées.

**Ce résultat n'est PAS un modèle 3D Gaussian Splatting entraîné de bout en bout.** L'environnement de calcul actuel n'a pas de GPU CUDA. Nous avons initialisé des splats depuis des mesures d'images et effectué le rendu CPU ; aucun ajustement photométrique complet des covariances, opacités ou couleurs sur toutes les images n'a été revendiqué.

## Utiliser le pipeline GitHub

```bash
pip install numpy opencv-python-headless
python tools/photo2scene/gaussian_from_drone_cpu.py \
 --video "/chemin/vers/drone.mp4" \
 --config tools/photo2scene/data/pripyat/energetik-drone-3view-rois.json \
 --out "/chemin/vers/resultats" --focal 550 --stride 2
```

Le script dans ce dépôt génère un **PLY gaussien et un .splat** depuis le fichier vidéo local. Le pack de la conversation ajoute un moteur de rendu CPU, des images comparatives et les fichiers issus de l'exécution initiale. Les images et vidéos de l'utilisateur **ne sont pas ajoutées à GitHub**.

## Restrictions architecturales

- **Aucune échelle en mètres**, même si le moteur contient les hypothèses 11,6 m (auditorium) et 21 m (volume scénique).
- Les faces derrière les arbres ou hors champ sont absentes ; les vues extrapolées sont incertaines, les fenêtres peuvent se déformer.
- Les différentes surfaces du Palais ne sont pas encore isolées proprement en murs, verrières, toitures et cage de scène.
- Aucune transformation destructive de `M3D_ENERGETIK_ARCH`. Le Gaussian PLY est une représentation de photo indépendante de notre ray tracer.
- Prochaine amélioration : davantage de vues du drone avec pose d'appareil consolidée, calibration caméra, segmentation dynamique végétation, puis **entraînement GPU 3DGS** et vérification face à des vues réservées au contrôle.

## Formats

Le PLY inclut positions XYZ, normales initiales, couleurs en coefficients SH d'ordre 0, champs de SH restants à zéro, opacité en logit, échelles logarithmiques et quaternion. Les données sont compatibles avec les conventions des visualiseurs Gaussian Splatting qui acceptent cette forme de PLY ; ce n'est pas un maillage OBJ. Le format SPLAT 32 octets/gaussienne fournit une alternative.

Références : <https://docs.nerf.studio/nerfology/methods/splat> et <https://developer.apple.com/documentation/visionos/rendering-gaussian-splats-with-realitykit>.
