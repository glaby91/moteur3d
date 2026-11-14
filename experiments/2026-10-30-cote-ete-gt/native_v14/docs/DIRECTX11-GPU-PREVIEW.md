# Aperçu GPU Direct3D 11

Le build 261103-0028 ajoute un backend Direct3D 11 facultatif à l’application Qt Windows.

- **CPU — ray tracing complet** reste le mode initial et la référence visuelle.
- **GPU — DirectX 11 (affichage temps réel)** rasterise la géométrie, les textures et le sol sur le GPU matériel détecté par Direct3D 11. Pendant les déplacements, l’image est recalculée toutes les 50 ms au maximum, à une résolution de preview plafonnée à 1024 × 576 ; à l’arrêt, le rendu sélectionné est recalculé à la résolution de sortie.
- La sortie initiale est **1366 × 768**, adaptée à la résolution native du Vostro. Les résolutions supérieures restent accessibles dans la liste.
- L’aperçu GPU utilise un éclairage raster simplifié, avec les lumières ponctuelles, les matériaux émissifs, les cartes de normales et de rugosité, leurs mipmaps, les reflets spéculaires et une transparence triée. Les ellipsoïdes, capsules et boîtes arrondies sont convertis en maillages plus fidèles, et les textures des boîtes suivent leur projection dans la scène et leurs canaux rouge/bleu sont transmis dans l’ordre attendu par Direct3D. Il ne calcule pas les ombres par rayons, la profondeur de champ, ni les réflexions et la réfraction complètes du rendu CPU.
- Si le périphérique, les shaders, les buffers ou les cibles de rendu ne peuvent pas être créés, l’application affiche la raison dans les diagnostics et revient au rendu CPU complet.
- Le mode GPU sert à parcourir la scène en temps réel. Il ne doit pas être comparé au CPU comme une image de qualité identique.

Le backend utilise la rasterisation Direct3D 11, compatible avec les GPU Windows qui prennent en charge D3D11, dont Intel HD Graphics 620. Il demande le MSAA 4× si le format couleur et le tampon de profondeur l’acceptent, puis tente 2× avant le repli sans MSAA. Les textures couleur s’échantillonnent en sRGB et la cible de rendu encode la sortie en sRGB; les cartes normales et rugosité restent linéaires. Le sampler utilise jusqu’à 16× d’anisotropie, selon le niveau pris en charge par le périphérique. Le chemin ne requiert pas DirectX Raytracing (DXR).

La compilation et le calcul du nombre de niveaux MSAA sont intégrés, mais le changement demande encore une validation visuelle sur Windows et sur un GPU réellement compatible. En particulier, les scènes natives conservent leur référence CPU; l’aperçu raster Direct3D reste un chemin de navigation avec des différences d’éclairage et de transparence.
