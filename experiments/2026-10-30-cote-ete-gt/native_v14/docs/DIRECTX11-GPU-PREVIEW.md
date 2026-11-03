# Aperçu GPU Direct3D 11

Le build 261103-0024 ajoute un backend Direct3D 11 facultatif à l’application Qt Windows.

- **CPU — ray tracing complet** reste le mode initial et la référence visuelle.
- **GPU — DirectX 11 (aperçu rapide)** rasterise la géométrie, les textures et le sol sur le GPU matériel détecté par Direct3D 11.
- La sortie initiale est **1366 × 768**, adaptée à la résolution native du Vostro. Les résolutions supérieures restent accessibles dans la liste.
- L’aperçu GPU utilise un éclairage simplifié. Il ne calcule pas les ombres par rayons, la profondeur de champ, ni les réflexions et la réfraction du rendu CPU. Les formes analytiques sont converties en maillages de preview.
- Si le périphérique, les shaders, les buffers ou les cibles de rendu ne peuvent pas être créés, l’application affiche la raison dans les diagnostics et revient au rendu CPU complet.
- Le mode GPU sert à parcourir rapidement la scène et à comparer les temps. Il ne doit pas être comparé au CPU comme une image de qualité identique.

Le backend utilise la rasterisation Direct3D 11, compatible avec les GPU Windows qui prennent en charge D3D11, dont Intel HD Graphics 620. Le chemin ne requiert pas DirectX Raytracing (DXR).
