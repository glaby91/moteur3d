# Photo2Scene WARP — Palais Energetik : 13 poses contrôlées (10 novembre 2026)

## Nouvelle passe réelle sur la vidéo utilisateur

Le MP4 disponible est compressé à 640 × 360 pixels. Sur 17 captures entre 372,0 et 376,0 s, **4 sont rejetées** car le montage change de plan (372–372,25 et 375,75–376 s). Le plan continu conserve **13 positions de caméra**, réparties entre **10 vues d'entraînement** et **3 réservées au contrôle**.

Mesures : **330 correspondances SIFT**, 301 points compatibles avec la géométrie épipolaire, **264 points 3D relatifs** avec profondeur positive et parallaxe suffisante. Les 11 poses intermédiaires sont obtenues par PnP avec 20 % des correspondances tenues à l'écart de l'ajustement : sur **428 points contrôlés**, **420** ont une erreur inférieure à 3 px. Plusieurs faux appariements produisent de fortes erreurs RMS et ne doivent pas être masqués.

Les ROI du Palais sont des **polygones approximatifs**, et le traitement ne retire pas encore les arbres derrière la façade ni ne distingue automatiquement mousse et végétation.

Quatre plans candidats sont identifiés sur les points XYZ correspondants (RANSAC en unités relatives) : toiture **71/84**, façade **58/103**, volume haut **17/26**, bâtiment arrière **25/28**. Ce ne sont pas encore des surfaces métriques reconstruites.

## Code publié

- tools/photo2scene/prepare_energetik_13views.py : extraction de séquence, SIFT, matrice essentielle, contrôle de cheirality et PnP, split holdout, conversion des poses OpenCV vers OpenGL, export du dataset Nerfstudio.
- Les photos et vidéos fournies par l'utilisateur ne sont **pas envoyées** sur GitHub ; le pack complet existe dans la conversation, avec images, masques et fichiers de calibration.

Exécution avec le MP4 local :

    pip install numpy opencv-python-headless
    python tools/photo2scene/prepare_energetik_13views.py --video "/chemin/video.mp4" --output "./energetik-13poses" --focal 550

Fichiers créés : transforms.json avec train/val/test séparés, images/*.png, masks/*.png, baseline_sparse_nonmetric.npz, metrics.json.

## Vrai Gaussian Splatting (NON exécuté)

Ici PyTorch est installé uniquement en **CPU** ; pas de GPU NVIDIA CUDA et pas de Nerfstudio/gsplat. Les 9 095 splats initiaux ne sont PAS entraînés. Le nouveau dataset peut être utilisé sur une machine équipée correctement, après contrôle caméra/masques :

    ns-train splatfacto --data ./energetik-13poses nerfstudio-data --eval-mode filename

Sources documentation : https://docs.nerf.studio/nerfology/methods/splat.html et https://docs.nerf.studio/reference/cli/ns_train.html .

Le fichier de transforms est préparé suivant les conventions Nerfstudio, mais **n'a pas été validé par une exécution de Nerfstudio ici**.

## Garde-fous du projet

- Les **11,6 m** correspondent à une hauteur libre type d'auditorium, les **21 m** à une cote scénique à vérifier, sans addition artificielle des deux.
- La focale 550 px est supposée, la distorsion est inconnue et la distance entre caméras vaut une unité arbitraire : **pas de mesure en mètres**.
- Ne pas modifier la scène architecturale existante, l'emplacement de la tour, les toitures, les escaliers ou la fresque tant que les images et le modèle 3D ne sont pas recalés avec une mesure connue.
- Pour reconstruire réellement la façade et les murs cachés par les arbres, il faut de meilleures vues variées (idéalement le drone 4K source), un recalage photogrammétrique global et une reconstruction de surfaces distincte du 3DGS purement visuel.
