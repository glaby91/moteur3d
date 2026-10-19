# RayCastingEngine.bas — QB64-PE

**Branche :** `raycasting` · **Révision :** 1.0.0 · **Format :** 16:10.

Le programme calcule **un seul rayon primaire par pixel interne**, depuis une caméra mobile. Le rayon conserve la première intersection avec un **plan infini**, des **sphères** et des **boîtes analytiques**. Il ne rasterise aucun triangle.

- **320×200 interne par défaut** (64 000 rayons), agrandi en 640×400.
- Touche `H` : résolution **640×400 interne** (256 000 rayons).
- `A` / `D` : rotation ; `+` / `-` : distance ; `ESC` : quitter.
- Dégradé du ciel, atmosphère, matériaux procéduraux, lumière unique et légère ombre de contact simplifiée **sans second rayon**.

**Limites :** ni BVH, ni carte Z d'ombres, ni cônes et ellipsoïdes anisotropes, ni véritable pyramide RGB565 de la V9.2 HTML. Il s'agit d'une adaptation pédagogique du cœur ray casting et non d'un port complet.

**Validation :** source statiquement vérifiée ; compilateur QB64-PE non disponible ici. Compiler et lancer localement avant de promettre un exécutable vérifié.
