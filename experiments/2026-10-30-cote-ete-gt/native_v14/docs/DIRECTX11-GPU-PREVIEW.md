# Aperçu GPU Direct3D 11

Le build 261103-0026 ajoute un backend Direct3D 11 facultatif à l’application Qt Windows.

- **CPU — ray tracing complet** reste le mode initial et la référence visuelle.
- **GPU — DirectX 11 (affichage temps réel)** rasterise la géométrie, les textures et le sol sur le GPU matériel détecté par Direct3D 11. Pendant les déplacements, l’image est recalculée toutes les 50 ms au maximum, à une résolution de preview plafonnée à 1024 × 576 ; à l’arrêt, le rendu sélectionné est recalculé à la résolution de sortie.
- La sortie initiale est **1366 × 768**, adaptée à la résolution native du Vostro. Les résolutions supérieures restent accessibles dans la liste.
- L’aperçu GPU utilise un éclairage simplifié. Il ne calcule pas les ombres par rayons, la profondeur de champ, ni les réflexions et la réfraction du rendu CPU. Les formes analytiques sont converties en maillages de preview.
- Si le périphérique, les shaders, les buffers ou les cibles de rendu ne peuvent pas être créés, l’application affiche la raison dans les diagnostics et revient au rendu CPU complet.
- Le mode GPU sert à parcourir la scène en temps réel. Il ne doit pas être comparé au CPU comme une image de qualité identique.

Le backend utilise la rasterisation Direct3D 11, compatible avec les GPU Windows qui prennent en charge D3D11, dont Intel HD Graphics 620. Le chemin ne requiert pas DirectX Raytracing (DXR).
