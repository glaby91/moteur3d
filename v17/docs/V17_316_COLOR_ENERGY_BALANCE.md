# CSE WARP V17.316 — correction colorimétrique à la source

## Constat utilisateur
Les images du complexe civique V17.311 sont à la fois trop claires et ternes : béton et vitrage uniformément gris, nuances des matériaux écrasées malgré des reflets convaincants et des collisions FPS fonctionnelles. Ce défaut touche **tout l'éclairage**, pas seulement le verre ou la GI.

## Causes dans le code
- **Budget lumineux :** ancien shader `diffuseBase=.30`, `diffuseDay=.44`, `diffuseDirect=.56`; le béton recevait ~0,74 d'ambiance avant soleil direct (~0,56 supplémentaire selon orientation), sans adaptation à la structure de la scène.
- **Reflets non métalliques :** l'ancienne couleur de spéculaire par défaut était bleu acier (178,202,214)/255, environ 70–84 % avant multiplication par le terme de Brillance. Pour un matériau diélectrique tel que le béton, une valeur neutre proche de 4 % est plus appropriée. Le vitrage transparent conserve une contribution de réflexion plus élevée.
- **Fond de ciel :** RGB sRGB d'authoring envoyé sans décodage comme entrée **linéaire** d'une cible D3D11 `SRGB RTV`, d'où un encodage supplémentaire éclaircissant le fond.
- **Traitement final :** les valeurs lumineuses élevées pouvaient être écrêtées à 8 bits, diminuant les différences locales. Les saturations artificielles ou une baisse uniforme du brightness ne résoudraient pas ces erreurs.

## Mode de correction V17.316
La case **Couleurs fidèles et lumière équilibrée**, activée par défaut dans l'éditeur Qt mais désactivée dans l'API moteur pour conserver les benchmarks historiques, applique :
1. Un éclairage ambiant réduit et indépendant du soleil direct, évalué en lumière **linéaire** ; supprime le soleil direct dans les zones réellement couvertes selon la carte d'ombre.
2. Une réponse spéculaire diélectrique neutre à ~4 % pour le béton et les autres matériaux non métalliques, adaptée à la transparence pour préserver les reflets du verre et aux paramètres PBR pour les métaux.
3. Une lumière des sources locales multipliée par l'albédo du matériau, et non ajoutée sous forme de halo gris uniforme.
4. Le décodage sRGB du ciel avant la conversion automatique du render target sRGB.
5. Une compression douce des **hautes lumières sur la luminance**, sans gamma supplémentaire et sans filtre de saturation ni perte de teinte. Le réglage `Exposition linéaire` (0,65–1,45, défaut 1,00) est indépendant de la GI. Il permet un réglage sur l'Intel HD620 sans recompiler.

**Intégrité :** aucune perte de PBR 2048, normal maps, MSAA 8x, anisotropie 16x, géométrie ni collisions ; mode historique entièrement réversible et contrôlé par test pixel à pixel. La nouvelle lumière n'est pas une garantie d'exactitude photométrique et nécessite une appréciation visuelle sur le vrai complexe civique.

## Benchmark / comparaison
- L'exécutable WARP de test calcule 2 rendus identiques dans chaque mode, 3 variantes (nouvelle lumière par défaut, exposition 0,85 et 1,15), puis restaure les pixels historiques bit-à-bit.
- L'archive de simulation totale comprend 4 PNG supplémentaires, et `color-metrics.csv` avec luminance moyenne et chroma normalisé.
- Les campagnes Raster/CPU/Compute V17 ainsi que les benchmarks **tables GI 864 sondes ×25 heures** restent inchangés, pour écarter les régressions.
- La validation visuelle finale nécessitera une capture sur l'Intel HD 620, à la même caméra et à l'option GI constante avant/après.
