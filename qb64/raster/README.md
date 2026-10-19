# RasterEngine.bas — QB64-PE

**Branche :** `raster` · **Révision :** 1.0.1 · **Format :** 16:10.

À ouvrir dans **QB64 Phoenix Edition**.

C'est un port **fonctionnel simplifié** du moteur raster V8.1 : des triangles de sol, de mur, d'arbre et de banc sont projetés, rasterisés par coordonnées barycentriques et ordonnés au pixel par un vrai **Z-buffer**. Les UV sont corrigés par perspective et les matières sont produites de façon procédurale.

- **320×200 interne par défaut**, agrandi en 640×400.
- Touche `H` : basculer en **640×400 natif**.
- `A` / `D` : pivoter autour de la scène.
- `+` / `-` : rapprochement / éloignement.
- `ESC` : fermer le programme.

**Limites :** ce port ne reproduit pas tous les triangles, l'interface du navigateur, les mipmaps RGB565 4096² ni les cartes d'ombres V8.1. Le raster Z par pixel, la perspective et la géométrie triangulaire sont en revanche présents. Le clipping des triangles au plan proche est volontairement absent et les triangles traversant le plan proche sont rejetés.

**Validation :** source statiquement vérifiée ; compilateur QB64-PE non installé dans cet environnement. Une compilation et un essai graphique sont à réaliser sur QB64-PE avant de la considérer comme une version validée.
