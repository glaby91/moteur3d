# Audit de netteté — Pripyat / Energetik (2026-11-06)

## Première correction
Branche de base : feature/cpu-pbr-hdr-denoise-2026-11-05.
- Qualité native (100 %) sélectionnée par défaut dans l'application Qt.
- AA contours désactivé par défaut ; il peut être réactivé dans l'interface pour une comparaison A/B.
- SmoothPixmapTransform Qt activé uniquement quand l'image est redimensionnée. La fenêtre peut encore redimensionner l'image, donc ce n'est pas une garantie de pixel-perfect à toutes les tailles.

## Causes observées dans le code (non quantifiées)
- readSettings() peut rendre à 87,5 %, 75 %, 66,7 % ou 50 % puis appeler upscaleBilinear().
- navigationPreview réduit jusqu'à 1024x576 en D3D11 et 480x270 en CPU.
- antialiasEdges4Scalar mélange 45 % de la moyenne des quatre voisins sur les pixels sélectionnés par contraste.
- offlinePathTrace appelle systématiquement denoiseBilateral() (5x5, guidage par luminance uniquement).
- sampleMipChain utilise RGB565, un échantillonnage bilinéaire et un LOD arrondi.
- La vue Qt peut redimensionner la sortie avec interpolation.

## Protocole de vérification
1. Immobiliser la caméra dans le hall d'Energetik (fresque, encadrements, escaliers).
2. Sélectionner Natif 100 %, désactiver AA contours, DOF et mode navigation rapide. Vérifier que la fenêtre affiche sans agrandissement.
3. Capturer le rendu CPU et le rendu D3D11, avec résolution de sortie et caméra identiques.
4. Réactiver AA contours, comparer les détails des fissures et inscriptions.
5. Comparer ensuite le mode hors-ligne avec/sans débruitage (nécessite encore une option dédiée).
6. Vérifier la chaîne d'import des textures et les dimensions réelles des mipmaps avant de modifier la précision RGB565.
7. Mesurer la qualité (images comparatives, MTF ou gradients de contours) et le temps CPU/GPU.

## Limites
Les changements ne constituent pas une correction complète du flou. Aucun test visuel Windows ni compilation de cette branche n'est encore validé. Les chemins de rendu CPU et D3D11 ne sont pas visuellement équivalents.
