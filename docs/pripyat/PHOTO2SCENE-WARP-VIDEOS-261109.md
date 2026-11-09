# Photo2Scene WARP — campagne vidéo Pripiat (9 novembre 2026)

**Statut : mesures 2D calculées + surface de référence OBJ/GLB, aucune profondeur métrique validée.**

## Matériaux exploités

Vidéos fournies par l'utilisateur et gardées localement. Aucun photogramme, fichier MP4 ou texture sous copyright n'est envoyé dans le dépôt public.

- `No comment. Припять. Июнь 1986 (колесо обозрения, во дворах).mp4` : à **120 / 124 / 128 s**, plan continu vers les bâtiments adjacents au parc d'attractions. L'authenticité de la datation « juin 1986 » reste à vérifier.
- `Film Tchernobyl - Pripyat 2009 Partie 22.mp4` : **295 s**, vue aérienne de l'arcade devant la place, Palais sur la droite ; **315 s**, façade oblique. Montage d'images, pas une caméra continue.
- Vidéo verticale fournie par l'utilisateur (15,3 s) : **3 / 5 / 11 / 13 s**, plafond à caissons et zone de scène. L'inscription « main hall » ne prouve pas qu'il s'agit du foyer avec fresque ; le volume semble correspondre à une salle de spectacle.

Le fichier `tools/photo2scene/data/pripyat/video-evidence-261109.json` contient tous les horodatages, régions d'intérêt et annotations dans **l'espace de l'image**.

## Calculs réellement effectués localement

Méthode : extraction OpenCV des images sources, détection LSD des segments de façade, ORB + rapport Lowe de 0,75, RANSAC homographie 3 px et matrice fondamentale 2 px. Les résultats sont **des diagnostics photogrammétriques**, non des distances mesurées au sol.

| Couple | ORB appariements | Homographie inliers | Fondamentale inliers | Diagnostic |
|---|---:|---:|---:|---|
| 120 → 124 s (historique) | 599 | 557 | 464 | Homographie dominante, profondeur indéterminée |
| 124 → 128 s (historique) | 696 | 602 | 503 | Homographie dominante, profondeur indéterminée |
| 3 → 5 s (salle) | 223 | 148 | 172 | Candidat multivue **non calibré** |
| 11 → 13 s (salle) | 266 | 153 | 168 | Candidat multivue **non calibré** |

Les 4/4 tests du prototype local ont réussi (reprojection projective, segments synthétiques, refus d'inférer de la profondeur métrique, vérification de l'export GLB). Le script intégré à GitHub dispose de trois tests synthétiques autonomes. Un candidat multivue ne garantit pas une triangulation fiable : il faut calibration de la caméra, trajectoire physique, sélection de points fixes et validation indépendante.

## Première géométrie exportée

- À 128 s : **quatre coins de baie vitrée** sélectionnés manuellement (provisoires) puis rectifiés par une homographie calculée ; patch de référence 1100×250 px. Le résultat est une texture de façade redressée, pas une reconstitution de détails disparus.
- La texture est placée sur un plan `OBJ`/`GLB` de largeur **1 unité sans échelle réelle** : destiné à l'analyse/recalage, non au remplacement des murs de la scène.
- À 295 s : suite de huit points de **l'arcade en pixels image**. Elle indique la courbe projetée, pas la longueur ni le rayon en mètres, et ne prouve pas de jonction physique avec Energetik.

## Exécution avec les vidéos locales

```bash
python -m pip install numpy opencv-python-headless Pillow trimesh
python tools/photo2scene/video_evidence.py \
  --root /dossier/videos \
  --config tools/photo2scene/data/pripyat/video-evidence-261109.json \
  --output /dossier/resultats
python -m unittest discover -s tools/photo2scene/tests -p 'test_video_evidence.py' -v
```

Les fichiers de sortie sont le rapport JSON, les photogrammes annotés, le patch rectifié et le plan de référence OBJ/GLB.

## Règles d'intégration dans la scène

1. **Ne pas remplacer** `M3D_ENERGETIK_ARCH`, l'arcade Polissya, les escaliers, la fresque ou les niveaux existants sans contraintes 3D étayées.
2. Ne pas utiliser les homographies dominantes des panoramiques historiques comme preuves de profondeur.
3. Ne pas identifier la salle verticale au foyer principal sans plan ou photo recoupée.
4. Garder les dimensions de projet-type 2С-06-9/70 comme **hypothèses comparatives**, non mesures du Palais.
5. Vérifier ensuite les résidus de reprojection sur des points de contrôle tenus à l'écart du solveur multivue et l'alignement avec les vues de la scène.

**Base de branche :** `feature/v16-modular-scene-dispatch`, qui contient déjà les solveurs `photo2scene.py`, `multiview_facade.py` et `three_view_front_plane.py`. La branche `feature/pripyat-polissya-arcade-261107` contient d'autres modifications récentes de la scène ; elles ne sont **pas fusionnées** ici. Le travail est isolé avant toute intégration 3D.
