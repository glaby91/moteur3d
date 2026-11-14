# CSE WARP Beta X — V17 : objectifs du moteur 3D temps réel

**Version de cadrage : 1.0 — 10 novembre 2026**  
**Statut : prototype V17-1 implémenté ; raster DirectX11 en validation CI ; objectifs de qualité et de FPS restant à mesurer sur HD620**  
**Branche de travail :** `feature/v17-realtime-architecture-roadmap-261110`  
**Base conservée :** V16 `feature/v16-dell-native-1366x768-fhd-261110`. La V16 et ses autres PR restent intactes. Aucune fusion dans `master` autorisée implicitement.

## 1. La mission de V17

Faire passer CSE WARP Beta X d'un outil qui **calcule des images à la demande** à un **véritable moteur 3D temps réel**, avec caméra libre, déplacement continu, temps par image stable et qualité visuelle crédible.

**Deux objectifs indissociables :**
1. **Fluidité :** se déplacer dans le Pavillon 01, puis à Pripiat, sans devoir appuyer sur « Calculer l'image ».
2. **Qualité :** obtenir un environnement dense, détaillé, avec une lumière vivante, des contrastes crédibles et une atmosphère convaincante — références esthétiques : **S.T.A.L.K.E.R.: Shadow of Chernobyl** et surtout **S.T.A.L.K.E.R.: Call of Pripyat** (X-Ray 1.6).

**Point de diagnostic V16 :** les formes et objets sont globalement reconnaissables, mais l'image est trop **froide, plate, parfois granuleuse** ; les différents jeux de lumière n'ont pas encore donné la chaleur ni la profondeur attendues. Le problème ne se résout ni par une saturation orange globale ni par la seule augmentation du nombre de rayons.

## 2. Matériel, résolution et mesures de référence

- **Machine cible :** Dell Windows, **Intel HD Graphics 620**, pilote **31.0.101.2141**.
- **Résolution interactive principale : 1366 × 768** (1 049 088 pixels).
- **Full HD 1920 × 1080 conservée** (2 073 600 pixels) pour la comparaison et l'export ; les options QHD/4K restent disponibles.
- **V16 mesuré, GPU Compute HLSL, Full HD :** caméra 01 = **401,7 ms** ; caméra 02 = **441,0 ms** (recalcul avec ressources réutilisées).
- **V16 CPU direct FHD, caméra 02 :** 1 spp = **4 025,9 ms** ; 4 spp = **15 866,1 ms**.
- **V16 CPU GI FHD, caméra 02 :** 16 spp, 2 rebonds = **94 911,4 ms** ; 64 spp = rendu encore artificiellement granuleux, **durée inconnue** dans la capture.
- **IMPORTANT :** performances 1366 × 768 Intel HD620 **non encore mesurées**. Une estimation fondée sur le nombre de pixels n'est pas une mesure ; D3D11 WARP dans la CI n'est pas un benchmark GPU Intel.

### Cibles, pas promesses

| Niveau | Temps TOTAL par image | FPS indicatifs | Critère |
|---|---:|---:|---|
| Palier initial | ≤ 33,3 ms | ≥ 30 | Navigation stable |
| Cible principale | ≤ 25,0 ms | ≥ 40 | Navigation fluide à 1366 × 768 |
| Optionnel | ≤ 16,7 ms | ≥ 60 | Si la qualité et le matériel le permettent |

Évaluer les **temps affichage à affichage**, pas seulement les durées de Compute Dispatch ; publier FPS, médiane, P95 et P99 du temps par image, latence d'entrée, saccades et consommation mémoire. Ne jamais promettre 25 ms sans mesure sur le HD620 réel. Définir un profil 30 FPS si le profil 40 FPS n'est pas crédible.

## 3. Règles non négociables

1. **Le moteur CPU reste 100 % fonctionnel et indépendant du GPU** : ray tracing, path tracing GI, rebonds, ombres, verre, eau, réflexions, PBR et export PNG, sans DirectX/OpenGL obligatoire.
2. **Conserver l'ancien rendu GPU HLSL V16** comme mode comparatif/diagnostic pendant la transition.
3. **Données de scène communes** : géométrie, UV, normales, matériaux PBR, couleurs linéaires, textures/mipmaps, lumière, heure solaire, caméra, unités. Pas de géométrie truquée propre au GPU.
4. **Ne pas abandonner les assets et la scène de Pripiat**, ni dégrader la fidélité architecturale au profit du FPS.
5. La **V16 reste accessible** ; V17 se développe sur une branche séparée et par évolutions réversibles.
6. **Aucune implémentation, compilation, modification d'un autre chantier ou fusion sans accord explicite** du propriétaire du projet sur l'étape concernée.

## 4. Architecture GPU temps réel

```text
Scène V17 commune : objets / instances / PBR / UV / caméras / lumières
                           |
                      Cache des assets
                      /           \
           CPU 100 % logiciel   GPU temps réel
           (référence qualité)     |
                                 Rasterisation matérielle
                                 + vertex shader / Z-buffer
                                 + PBR (fragment/pixel shader)
                                 + carte d'ombres
                                 + GI efficace, réutilisable
                                 + reflets/eau/verre spécialisés
                                 + fog, tonemapping, AA
                                       |
                                  Image GPU native
                                       |
                               Affichage continu Qt
```

**Règle de calcul :** en exploration, ne plus utiliser un rayon BVH primaire par pixel pour les surfaces opaques. La rasterisation produit les surfaces visibles. Les rayons secondaires GPU deviennent un **complément ciblé**, pas le coût de base de chaque pixel.

### Deux backends potentiels

- **DirectX 11 / HLSL : premier candidat Windows.** Réutiliser de `native_v14/src/m3d_directx11.cpp` : vertex/pixel shaders, rasterisation, Z-buffer, tableaux de textures PBR, mipmaps, MSAA, shadow mapping, alpha blending, géométrie GPU. **Ne pas reprendre tel quel son ancien modèle lumineux simplifié.**
- **OpenGL 4.3+ / GLSL : second candidat**, pour l'intégration Qt via `QOpenGLWidget` et Linux. Tester la version/les extensions exposées réellement par le pilote HD620. Évaluer les deux sur **la même scène et la même qualité** avant un choix définitif.
- Éviter de créer deux moteurs GPU sans abstractions communes. On peut partager les ressources/passes/règles PBR tout en écrivant les shaders nécessaires à chaque API.
- **Aucun DXR matériel nécessaire.** L'absence de DXR n'empêche pas le rendu moderne. Les jeux X-Ray historiques utilisent éclairage différé, ombres, AO, shaders et posttraitements sans lancer de rayons matériels.

### Interaction et boucle de rendu

- Caméra libre : déplacement continu WASD/flèches, souris, vitesse réglable, collisions optionnelles ; six caméras prédéfinies conservées.
- Boucle d'images avec cadence régulière, scène précalculée/cachée, chargement non bloquant et contrôle utilisateur réactif.
- Présenter une **texture GPU directement** lorsque possible, au lieu d'un transfert synchrone `GPU → mémoire CPU → QImage` à chaque image.
- Export de capture PNG et rendu CPU haute qualité continuent d'exister mais sont des commandes distinctes.

## 5. Objectif visuel : finesse et « chaleur » de la scène

**Référence de qualité visuelle : ambiance de S.T.A.L.K.E.R.: Call of Pripyat**, sans copier ses assets, ses shaders ou ses niveaux. On vise la profondeur, la cohérence et les détails d'un jeu temps réel de cette génération, pas un photoréalisme path tracé à 64 spp en mouvement.

### Problèmes V16 à résoudre

- Objets correctement représentés mais **formes parfois trop propres/simplifiées** ; textures uniformes, manque de relief local et de micro-détails.
- Surfaces blanches très **plates et froides**, peu de distinction entre zones ensoleillées et indirectement éclairées.
- Intérieurs insuffisamment chaleureux et mauvaise perception du **contraste intérieur / extérieur**.
- Ombres parfois trop anguleuses, incohérentes ou dépourvues d'occlusion de contact.
- Vitres trop bleu-noir, reflets insuffisamment convaincants ; bassin peu réaliste.
- CPU GI encore granuleux à 64 spp : filtre bilatéral de prévisualisation trop simple ; le seul nombre de spp ne garantit pas une atmosphère réaliste.

### Techniques à évaluer, dans cet ordre

1. **Colorimétrie physique et artistique contrôlée** : travailler en espace linéaire ; distinctement **soleil plus chaud** et **ciel indirect plus frais** selon heure/météo ; balance des blancs et exposition cohérentes ; courbe ACES/sRGB vérifiée CPU/GPU ; **éviter un filtre jaune/orange uniforme**.
2. **Éclairage différé ou forward+**, direct PBR GGX, lumières locales chaudes, éventuel éclairage emissif, atténuation cohérente ; paramétrage jour/nuit/intérieur/extérieur.
3. **Contact shadows + SSAO/GTAO** abordables : lier objets au terrain, cadres aux murs, arbres au sol ; ne pas noircir artificiellement toutes les crevasses.
4. **GI statique/partiellement précalculée** pour murs/plafonds/sols : lightmaps ou sondes irradiance, éventuellement calculées depuis le moteur CPU, avec invalidation quand la lumière ou la scène change.
5. **Matériaux et détails** : textures albedo, normal/bump, roughness cohérentes, tiling UV, mipmaps, détails de façade/boiseries/sol, decals, relief visuel ; renforcer le maillage/LOD seulement là où la silhouette le justifie.
6. **Reflets spécialisés** : reflet planaire du bassin si pertinent, cubemaps/sondes d'environnement, Fresnel et transparence du verre. Rayons ou SSR seulement si le coût mesuré le justifie.
7. **Atmosphère mesurée** : brouillard léger de profondeur, lumière rasante, shafts/god-rays à faible coût, bloom très modéré, color grading cohérent. Ne pas masquer les défauts par du flou.
8. **Anti-aliasing + qualité temporelle** : limiter le bruit, l'instabilité/flickering et le ghosting pendant le mouvement.
9. **Optimisations géométriques** : frustum/occlusion culling, instancing des arbres, LOD, texture compression, regroupement de matériaux, buffers persistants, qualité/effets adaptatifs.

**Différence de scènes :** Pavillon 01 exige des matériaux contemporains et un éclairage chaleureux naturel ; Pripiat exige fidélité architecturale, usure et atmosphère post-abandon. Les réglages artistiques ne doivent pas être codés en dur globalement : presets par scène / météo / heure.

### Validation visuelle explicite

Pour **la même caméra, même heure et même météo**, comparer :
- Vue extérieure en plein soleil ; vue rasante de fin d'après-midi ; entrée dans une pièce ; vue nocturne avec lumières locales.
- Profondeur et netteté des ouvertures, cadres, pierres, végétation, textures et petits objets.
- Température du soleil vs ombre du ciel ; lumière indirecte perceptible ; surfaces non brûlées et noirs non bouchés.
- Ombres de contact sans halos, verre lisible et reflets plausibles, eau cohérente.
- Image **stable en mouvement**, sans fourmillement, scintillement excessif, bruit ni traînées.
- Comparaison CPU/GPU au-delà de mesures automatiques MAE/PSNR (qui ne mesurent pas l'ambiance perçue).

Les références visuelles **S.T.A.L.K.E.R.** servent d'**objectif artistique et d'inspiration technique**, non de garantie que la HD620 atteindra la même densité visuelle avec 40 FPS.

## 6. Benchmark et critères de validation

### Environnement
- **Dell HD620 natif 1366 × 768** ; répéter en **FHD 1920 × 1080**.
- Caméra 01 et caméra 02 du Pavillon 01 ; ajouter un parcours de caméra continu et une scène extérieure plus dense (Pripiat à terme).
- Lumière et textures strictement identiques CPU/DirectX/OpenGL pour les comparaisons physiques ; profils de qualité annoncés séparément.
- 2 à 3 mesures après réchauffement, test sur 30–60 s, noter température/limitation thermique si disponible.
- Conserver captures PNG, réglages, compteur FPS, distribution P50/P95/P99, temps CPU, temps GPU, charge mémoire et commit exact.

### Étapes et portes de validation (aucun ordre de développement implicite)

| Étape | Contenu | Validation demandée |
|---|---|---|
| V17-0 : cadrage | Ce document, choix des objectifs et non-régressions | Accord sur architecture et qualité |
| V17-1 : mesures de base | V16 natif/FHD, CPU/GPU, analyse de coûts/rendements | Résultats Intel réels |
| V17-2 : prototype raster | Rendu rasterisé PBR sur scène V16, comparé avec CPU | Image exacte et temps documentés |
| V17-3 : affichage temps réel | Boucle, navigation libre, image GPU affichée sans readback bloquant | Déplacement continu et latence acceptable |
| V17-4 : éclairage et détails | PBR, soleil/ciel, contact shadows, GI efficace, textures, eau/verre | Références visuelles validées |
| V17-5 : optimisation HD620 | Instancing, culling, LOD, temporalité, profiler, dyn-res si utile | 30 puis 40 FPS **mesurés**, sans artefacts rédhibitoires |
| V17-6 : OpenGL | Backend comparatif et Linux si pertinent | Parité visuelle/fonctionnelle et résultats mesurés |
| V17-7 : consolidation | CPU indépendant, sauvegarde/export, QA, scènes, Windows/Linux selon backend | Build reproductible + audit complet |

La V17 est validée **seulement si le mouvement est fluide**, si les images ne se dégradent pas gravement et si le mode CPU complet est toujours opérationnel. Un benchmark fixe rapide ne suffit pas.

## 7. Références techniques et graphiques

- S.T.A.L.K.E.R.: Call of Pripyat (X-Ray 1.6, 2009/2010), contemporaneous analysis: https://www.pcgameshardware.de/DirectX-Software-255525/Tests/Stalker-Call-of-Pripyat-DirectX-11-im-Test-gegen-DirectX-10-699416/
- Historical DirectX 11 features and rendering: https://www.jeuxvideo.com/news/2009/00037444-images-directx-11-de-s-t-a-l-k-e-r-call-of-pripyat.htm
- Intel API compatibility: https://www.intel.com/content/www/us/en/support/articles/000005524/graphics.html

Ces sources illustrent qu'un rendu de jeu crédible peut reposer sur **deferred lighting, SSAO, bump/parallax, effets de surface humide, ombres, tonemapping, fog et shaders**, avec DX11 ou antérieur, **sans DXR**. Leurs performances sur du matériel historique ne sont **pas extrapolables** directement à la HD620.

## 8. Protocole de changement

La conception V17-1 a été approuvée et un premier exécutable de navigation a été développé. Cela **n'autorise pas de fusion automatique dans master** ni ne valide les performances de la HD620.
Avant chaque phase : présenter l'impact, les fichiers à modifier, les risques, le benchmark attendu et les possibilités de retour arrière. Attendre une instruction explicite (« exécute », « code », etc.) pour les changements de code, compilations, branches supplémentaires ou fusion.
