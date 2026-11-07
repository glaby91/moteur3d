# Pripyat — assemblage de scène unifié, coordonnées métriques 1:1

**Date :** 7 novembre 2026  
**Branche :** `feature/pripyat-scene-assemble-scale-1to1-261107`  
**Format de livraison :** `scene_pripyat_central.bin` (scène V4 inchangée) + vues et documentation. **Pas de recompilation Qt** requise.

## 1. Ce qui est effectivement assemblé dans le fichier de scène

La branche d'intégration est fondée sur la dernière scène **Energetik / Polissya / GPU Compute** et NON sur une vieille branche qui remplacerait ces améliorations :

- Palais Energetik, hall, fresque, gymnase, sièges et accessoires existants.
- Hôtel Polissya orienté à 90°, façade, enseigne, terrasse et baies documentées par photos.
- Galerie courbe **sans raccordement** (départ latéral Energetik, arrivée devant Polissya), extrémités arrondies.
- Grande roue, nacelles et éléments du parc déjà présents.
- Arbres ramifiés, buissons, sols dégradés, lampadaires, mobilier existant.
- **Extension périphérique déjà présente** : routes/chemins OpenStreetMap et terrain SRTM échantillonné sur une grille de 100 m.
- Textures PBR, géométries et matériaux existants ; export avec `M3D_TEXTURE_SIZE=2048`, comme le paquet Windows précédent.
- Les 13 vues de caméra restent inchangées et sont incluses séparément pour le moteur Qt.

Le contenu de l'atlas (43 lieux vérifiés) **n'est pas** automatiquement de la géométrie : il sert de catalogue source.

## 2. Correction majeure : géographie 1:1

**Défaut d'origine :** la source OSM/SRTM a subi une contraction horizontale de **0,4882691** par rapport aux mètres réels lors d'une tentative d'adaptation aux centres provisoires de bâtiments.

Ce défaut affectait :
- rayon géographique 2 000 m → seulement ~976,5 unités moteur ;
- longueurs de rues converties en coordonnées X/Z pré-réduites ;
- largeurs des rues réduites dans la source ;
- altitudes relatives SRTM également multipliées par 0,4882691.

**Correction dans l'export :**
1. SRTM : la grille reste à pas 100 m, `world(E,N)` utilise une projection rigide orientée `theta` **sans coefficient de réduction**.
2. OSM : tous les sommets routiers déjà pré-réduits sont décompressés autour de l'ancrage `(-10;-31,1)` : `X = X0 + (Xold-X0) / 0,4882691`, identique en Z.
3. Les largeurs de chaussée utilisent `largeurOriginale / 0,4882691`.
4. Les dénivelés `SRTM - 113 m` restent désormais en **mètres**, avec l'aplanissement graduel conservé près du centre pour éviter une cassure des dalles.
5. Les routes de passage sont volontairement exclues du rectangle reconstruit du cœur urbain, afin de ne pas traverser les bâtiments/photo-objets.

| Mesure | Avant | Après correction |
|---|---:|---:|
| Rayon SRTM théorique 2 km | ~976,5 unités | **2 000 mètres** |
| Distance de 100 m sur carte | ~48,8 unités | **100 mètres** |
| Exemple de rue d'origine à 4,39 unités | ~4,39 unités | **~8,99 mètres** |
| Dénivelé brut de 5 m (hors zone aplanie) | ~2,44 unités | **5 mètres** |

### Limites restant à vérifier

**1:1 ne signifie pas cadastralement exact.** L'orientation `theta=0,21319471` et le point d'ancrage hérités de l'ancienne cartographie restent une estimation, tout comme l'emprise et l'écart exact entre Energetik et Polissya. Les sites sont confirmés, mais un véritable relevé OSM des polygones des bâtiments et des correspondances photo n'est pas encore intégré.

Le réseau via OSM (instantané 2026) ne doit pas être présenté comme une preuve du tracé exact de chaque rue en 1986. Altitudes SRTM (~100 m de pas dans l'export) : relief d'ensemble, pas détails de trottoirs.

## 3. Compatibilité et génération

L'exécutable Qt/Compute reste celui du [build Windows 296](https://github.com/glaby91/moteur3d/actions/runs/42400188810), qui lit déjà le format `scene.bin`.

Une nouvelle action `export-pripyat-scene-only.yml` exécute Node.js sur un runner Linux, génère les textures nécessaires et exporte **uniquement les données de scène** ; elle n'appelle pas `cmake`, `msbuild`, `windeployqt` et ne produit pas un nouvel EXE.

Commande native depuis `experiments/2026-10-30-cote-ete-gt/native_v14` :

```bash
M3D_SKIP_IMPORTED_CAR=1 M3D_TEXTURE_SIZE=2048 \
node export_scene.js source_v1314.html scene_pripyat_central.bin scene_pripyat_central.js
```

Dans l'application, utiliser **Scène → Ouvrir une scène…**, puis sélectionner `scene_pripyat_central.bin` du nouvel artefact. Conserver les autres composants de l'exécutable Windows 296.

## 4. Contrôles

- `test-pripyat-metric-1to1.mjs` : distances 100 m, 2 km, coordonnées OSM, largeur réelle, relief non compressé.
- `test-polissya-arcade.mjs` : hôtel tourné, galerie indépendante, virage et écart aux bâtiments.
- Autres vérifications Energetik / grande roue / fresque déjà utilisées par le build Windows.
- Validation de l'export par existence et taille du binaire ; le vrai rendu CPU/GPU doit encore être vérifié sur l'ordinateur utilisateur.

## 5. Pas inclus — ne pas annoncer comme terminé

La Forêt rousse de 1986 et la gare fluviale / bras mort sont **documentés mais pas encore modélisés**. Les secteurs urbains répertoriés dans l'Atlas ne sont pas tous reconstruits. Les routes 2026 et le relevé SRTM ne valident pas automatiquement une scène fidèle à 1986.
