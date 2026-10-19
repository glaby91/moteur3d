# V8 — RGB565, UV perspective et mipmaps 8×8 à 4096×4096

**Date : 19 octobre 2026** · Démonstrateur HTML autonome, expérimental. Toutes les versions V7 et précédentes sont conservées.

## Implémentation effectuée

- **Textures réelles** en tableaux `Uint16Array` RGB565, entièrement produites en JavaScript sans CDN ni fichiers d'image externes.
- Matériaux procéduraux : **écorce** (tronc et branches), **feuillage**, **herbe au sol**, **pierre** (murs, blocs, allée) et **bois du banc**.
- Pour **l'écorce**, génération d'une véritable image **4096 × 4096 RGB565**, puis réduction successive à **2048, 1024, 512, 256, 128, 64, 32, 16 et 8**.
- Les autres matériaux utilisent une pyramide plus économique : **256, 128, 64, 32, 16, 8**.
- Première texture d'écorce de secours en 256² disponible immédiatement ; la 4096² est **construite par tranches via requestAnimationFrame** pour réduire les pauses de génération, puis remplace la texture provisoire lorsque les dix niveaux sont complets.
- **Coordonnées UV générées en espace de scène** (projection planaire selon la normale dominante ; projection XZ pour le sol).
- Pour les surfaces coupées par le plan proche caméra, les UV sont calculés sur les sommets **après clipping**, ce qui préserve leur continuité.
- À chaque pixel qui gagne le Z caméra : **interpolation UV perspective-correcte (`u/z`, `v/z`, `1/z`)** ; calcul des **dérivées UV par rapport aux pixels de l'écran** pour estimer l'empreinte de texture et choisir le mip.
- Modes de filtrage disponibles : **plus proche**, **bilinéaire**, **trilinéaire** (défaut).
- Mode diagnostic **Afficher les niveaux mip par couleur** ; choix **Forcer mip** de 8² à 4096², limité au maximum réellement présent pour chaque matériau.
- Curseur **Distance caméra de 4 à 22 m** permettant de tester réellement les passages de niveaux selon la distance.
- Visualisation côte à côte des **dix niveaux de la pyramide d'écorce** (les niveaux non encore générés sont temporairement sombres).
- Profilage de la quantité de pixels texturés, des lectures de texels, du mip le plus utilisé et de la RAM RGB565.
- **Textures ON/OFF** pour comparer sans modifier la géométrie ni le Z-buffer.
- Les changements de filtres et de modes de visualisation n'invalident pas les cartes Z lumineuses ; seules les options de lumière le font.

## Mémoire

Une pyramide d'écorce 4096² à 8² en RGB565 occupe exactement **44 739 200 octets**, soit **42,67 Mio**. Les autres pyramides 256² utilisent environ 0,17 Mio par matériau. Un tampon temporaire 4096² est présent pendant la construction.

Ne pas confondre les **16 bits du texel** et le tampon écran **RGBA 32 bits** de la démonstration. Le système n'est pas encore un moteur de texturage FPGA.

## Tests exécutés

**En JavaScript V8 avec DOM/Canvas simulés (ce n'est pas une mesure de FPS Firefox ou Chrome) :**

1. Génération intégrale **4096² → 8²** : dix niveaux vérifiés, tailles et mémoire vérifiées.
2. Échantillonnage trilineaire avec empreintes de 1 à 512 texels/pixel : niveaux choisis successivement **4096, 2048, 1024, 512, 256, 128, 64, 32, 16 et 8**.
3. Forçage explicite de 4096² : vérifié.
4. Test Z sur deux triangles superposés dans les deux ordres : réussi.
5. Rendu complet à 320 × 240, textures OFF puis ON : **53 685 pixels couleur différents** mais **aucun changement d'une valeur du tampon Z**.
6. Avec les textures provisoires 256² à 320 × 240, le mip le plus utilisé évolue avec la distance caméra : **128² à 4 m ; 64² à 8,4 m ; 32² à 22 m**.
7. Sur cette image à 8,4 m : **95 546 pixels texturés** pour environ **747 884 lectures de texels** (somme des consultations bilinéaires/trilinéaires comptabilisées).
8. Le temps CPU simulé augmente sensiblement avec le filtrage trilinéaire et les 28 416 triangles V7.

## Limites connues

- **Rendu logiciel coûteux** : démarrage volontaire à **320 × 240** avec choix 640 × 480 / 800 × 600.
- La création progressive du 4096² peut durer de nombreuses images si le rendu CPU est déjà lent ; la texture de secours reste visible entre-temps.
- Dans la V8, **l'arbre en écorce** est l'exemple 4096² ; les autres matériaux sont limités à 256².
- Le filtrage anisotrope n'est pas implanté ; les dérivées UV tiennent déjà compte de l'obliquité, mais l'échantillonnage utilise un mip isotrope.
- Coordonnées UV planaires générées automatiquement ; des raccords visibles peuvent subsister sur les formes complexes. Pour une qualité artistique définitive, un vrai dépliage UV par objet sera nécessaire.
- Les ombres de la lumière Z stabilisée et le Z-buffer caméra restent actifs ; les textures ne déplacent ni sommet ni profondeur.
- Le texturage n'est activé **que dans le rendu V8 « Z caméra »**. Les anciens modes Canvas V4, conservés pour comparaison, ne sont pas texturés.
- Le rendu visuel interactif et les performances n'ont **pas** été vérifiés dans un véritable navigateur par ces tests automatisés.

## Prochaines optimisations

- Binning par tuile caméra 16×16 et rejet précoce des triangles entièrement masqués.
- Cache d'UV et de gradients pour les faces coplanaires regroupées.
- Gestion anisotrope limitée pour sols et murs vus en angle rasant.
- Choix de taille et budget de mémoire par matériau sur l'architecture FPGA.
