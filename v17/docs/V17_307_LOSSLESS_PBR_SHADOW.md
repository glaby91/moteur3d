# CSE WARP V17.307 — optimisation sans perte de rendu

Branche de travail : `feature/v17-307-lossless-pbr-shadow-261111`
Référence : V17.306, campagne matérielle du 11 novembre 2026.
Aucune réduction du MSAA, de l'anisotropie, du LOD, du nombre de triangles,
des rebonds ni des résolutions PBR.

## 1. Cache permanent PBR (CPU + DirectX + Compute)

Dans `m3d_v16_bridge.cpp`, les résultats complets des mips couleur sRGB32,
normales RGB565 et rugosité RGB565 sont sauvegardés par matériau, y compris
tous les niveaux jusqu'à 1×1, dans le cache utilisateur `CSE-WARP/PBR/v1`.
La clé SHA-256 couvre l'identifiant de génération, la version Qt, le motif,
la couleur de base, la rugosité et la résolution. Le cache ne modifie aucun
pixel calculé : c'est une sérialisation des tableaux déjà générés.

- Fichier atomique via `QSaveFile` ; en-tête strict, longueur et SHA-256 des données.
- Entrée invalide, tronquée ou corrompue : recalcul intégral, puis réparation.
- Limite de taille disque de 4 Gio, éviction des fichiers les plus anciens.
- Désactivation de contrôle : `M3D_V17_DISABLE_PBR_CACHE=1`.
- Cache test isolé : `M3D_V17_PBR_CACHE_DIR=<dossier>`.
- La toute première ouverture peut prendre plus de temps (écriture des données).
  Le gain attendu porte sur les rechargements, **à mesurer sur Intel HD 620**.

## 2. Omission des transferts d'ombres identiques

Dans `m3d_directx11.cpp`, `UpdateSubresource` n'est plus lancé
quand le tampon `ShadowMap::d` est **strictement identique en mémoire**
(`memcmp`) à l'image effectivement envoyée précédemment.

Une simple comparaison de pointeurs ou de hash aurait risqué une image
d'ombres périmée. Le cache exact détecte les modifications en place,
et se vide automatiquement lors d'un changement de résolution GPU.

Désactivation A/B : `M3D_V17_DISABLE_SHADOW_UPLOAD_CACHE=1`.

## 3. Validation Windows et campagne utilisateur

Tests dédiés ajoutés :
- PBR série = parallèle, sans passer par le cache.
- Cache froid = cache chaud, égalité de chaque texel et de chaque mipmap.
- Fichier PBR corrompu = régénération à l'identique.
- Ombres GPU avec/sans cache, puis ombres modifiées et restaurées : image identique.
- Suite existante V17 : Raster WARP, CPU/Compute, 6 scènes, 1/2/3 buffers.

**À mesurer sur le Dell** : chargement froid/chaud du Pavillon, FPS/P95 de
24 conditions, mémoire pic, latence interaction et comparaison des images.
Ne pas annoncer de gains chiffrés avant benchmark réel. Tout pixel différent
doit bloquer la validation, indépendamment des FPS.
