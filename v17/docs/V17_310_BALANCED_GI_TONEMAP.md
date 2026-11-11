# CSE WARP V17.310 — lumière indirecte équilibrée / tone mapping indépendant

11 novembre 2026 · source dérivée de V17.309 · branche feature/v17-310-gi-energy-tone-261111.

## Problème observé dans la V17.309

Captures utilisateur à caméra et résolution identiques, 1366×768, 4× MSAA et 16× anisotropie. La version avec illumination hybride affichait des façades trop claires et des ombres manquant de profondeur ; une fois le mode désactivé, l'architecture reprenait ses contrastes d'origine. Temps affichés : 81,2 ms avec le mode et 76,9 ms sans, deux seules observations insuffisantes pour inférer un ratio de performance robuste.

Cause vérifiée dans le shader : accumulation de l'irradiance du ciel et du rebond **par-dessus** une contribution ambiante largement conservée, puis application systématique d'une courbe ACES suivie d'une puissance gamma 1/2.2. Le gamma éclaircissait toutes les demi-teintes sur la cible de rendu UNORM historique.

## Modifications implémentées

1. **Régulation de l'énergie :** remplacement de l'ambiance hémisphérique uniforme par un éclairage calculé à partir de la visibilité des sondes 3D. Les endroits ouverts conservent jusqu'à 92 % de l'intensité de référence ; les zones peu accessibles au ciel jusqu'à 48 %, plus une faible contribution indirecte locale. Le rebond tient compte de la couleur de la surface rencontrée et est atténué près du ciel ouvert.
2. **Couleur :** teinte du ciel normalisée pour conserver son énergie moyenne ; température solaire directe modifiée modestement selon l'heure et les ombres dynamiques déjà existantes.
3. **Traitement filmique indépendant :** option distincte et par défaut désactivée, même si l'éclairage hybride était déjà activé dans les préférences utilisateur. Elle n'applique que la compression des hautes lumières sur un framebuffer historique UNORM, **sans re-gamma**, et ne doit jamais rendre un canal plus clair.
4. **Interface :** deux cases indépendantes, mémorisées dans QSettings sous Raster/HybridLighting et Raster/FilmicTonemap. Le mode illumination indirecte reste activé par défaut pour les nouvelles installations, désactivable à tout instant. Les benchmarks moteur restent OFF par défaut pour comparer les valeurs historiques.
5. **Régression / simulation :** quatre états testés sous WARP (ancien, indirect seulement, indirect + filmique, filmique seul) avec répétabilité, absence d'écrasement lumineux global, contrôle par canal, variation 9 h/17 h 30 et retour exact en pixels au rendu historique après désactivation. Le pipeline archive trois images PNG réelles comme livrables comparatifs, dans lighting-ab/.

## Limitations

Il s'agit toujours d'un modèle **empirique de sondes 16×8×16** et d'une table horaire interpolée, non d'une illumination globale multi-rebonds de qualité DXR. Aucune affirmation de fidélité physique totale ni de gain de FPS. Les performances et couleurs sur Intel HD 620 restent à mesurer.

**Qualité conservée :** textures 2048 PBR, MSAA 4×, anisotropie 16×, géométrie, normal maps et cartes d'ombres. Aucun changement de Direct GPU Present.
