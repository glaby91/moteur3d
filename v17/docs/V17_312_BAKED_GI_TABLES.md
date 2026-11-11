# CSE WARP V17.312 — transport lumineux indirect précalculé dans une table horaire

**Objet :** remplacer le mode V17.310, qui ne faisait que teinter l'ambiance, par des contributions lumineuses calculées depuis les surfaces de la scène. La V17.311 et ses déplacements en vue à 1,70 m constituent la base de cette branche.

## Pipeline réel implémenté

1. Exécutable autonome `moteur3d_v17_gibake.exe`. Il recharge la scène et ses matériaux, construit le BVH CPU et calcule les irradiances indirectes à partir de rayons envoyés depuis un volume de sondes.
2. Lorsqu'un rayon touche une surface, le bake calcule sa couleur diffuse en **espace linéaire**, l'accès direct au soleil (avec un rayon d'ombre), le ciel (avec occultation), et éventuellement une deuxième contribution réémise par une autre surface. Les conditions solaires changent avec l'heure et sont **recalculées avant enregistrement**, et non recolorées artificiellement pendant l'affichage.
3. La sortie est une table binaire `scene.json.m3dgi` avec version, empreinte de géométrie/matériaux, dimensions du volume, heures, données RGBA FP32 et checksum FNV64 sur la charge utile. Les caches invalides/corrompus sont rejetés.
4. Le Raster DirectX 11 crée deux textures 3D correspondant aux heures encadrantes ; à chaque image il échantillonne ces données par interpolation spatiale trilinéaire et effectue une interpolation temporelle **sans lancer de rayons GI**. Le soleil direct et ses ombres restent dynamiques.
5. La table est réutilisée tant que son empreinte de scène correspond. Si aucune table n'existe, la case d'éclairage indirect retombe explicitement sur le modèle approximatif V17.310, afin que les tests de référence restent possibles. Le statut du rendu distingue les deux chemins.
6. L'interface offre un bouton **« Précalculer la table GI (ray tracing CPU) »**. Ce calcul a lieu dans un exécutable séparé : l'interface reste réactive ; au succès la table est rechargée au prochain rendu.

## Définitions de qualité

| Profil | Sondes | Rayons par sonde | Heures | Pas | Rebonds |
|---|---:|---:|---:|---:|---:|
| Normal (bake utilisateur) | 12×6×12 | 16 | 25 | 0,5 h | 2 |
| Court (benchmark GitHub WARP) | 4×3×4 | 8 | 5 | 2,5 h | 2 |

**Limites :** ce n'est pas du path tracing GI à haute qualité. Les volumes réguliers à faible résolution peuvent présenter des fuites lumineuses à travers les murs fins et perdre du détail dans un grand espace urbain. Les matériaux utilisent une estimation de leur albédo moyen, non un échantillonnage UV intégral de toutes les textures lors de chaque impact. L'empreinte de scène échantillonne le niveau de texture principal plutôt que chaque texel ; après édition de textures ou géométrie, **relancer le bake**. Les objets mobiles ne modifient pas cette illumination statique.

## Simulation / benchmark

- `v17_gi_table_precompute_and_checksum` vérifie : déterminisme du transport, différence entre heures, fichier binaire, empreinte après changement de matériau, corruption détectée.
- `v17_d3d11_raster_warp` produit un véritable rendu `.baked-gi.png`, vérifie le chargement de la table par le shader, le changement matin/soir et le retour exact au Raster classique.
- GitHub Actions exécute **trois scènes** (pavillon, matériaux, complexe civique), avec **3 bakes indépendants**, **7 relectures du cache**, **25 cycles de 200 interpolations** et enregistre les médianes en JSON/CSV. Ce sont de vrais temps CPU du runner Microsoft, pas des mesures d'Intel HD 620.
- La simulation totale WARP existante continue de vérifier 6 scènes, 72 configurations Raster, les rendus CPU/Compute, les PNG et toutes les régressions V16/V17.

## Réversibilité / intégrité

La qualité PBR 2048, les normal maps, les matériaux, le MSAA et l'AF ne sont pas réduits. La GI est optionnelle ; les rendus de référence avec hybrid OFF restent identiques. L'exécution Direct GPU Present ne fait pas partie de cette évolution. Chaque modification est versionnée par commit et archive de build.

## Suivi de validation GitHub

La PR doit rester en brouillon tant que la compilation Windows, les tests de cache GI et la simulation totale n'ont pas tous réussi. Les jeux de données de benchmark sont conservés dans l'artefact `Total-Simulation-WARP/gi-bench` avec les rapports JSON et le CSV des médianes de trois cuissons et sept relectures. Les images de validation comprennent la référence Raster, les sondes approximatives et la véritable table GI.
