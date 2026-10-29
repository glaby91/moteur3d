# Scène Manhattan dans le brouillard — Moteur V9.2

Variante autonome du simulateur V9.2 qui remplace le jardin par une rue urbaine inspirée de Manhattan et du survival horror brumeux.

## Construction dans le moteur

La scène est calculée par le vrai moteur CPU V9.2. Elle ne colle pas l’image de concept dans le Canvas. Elle utilise ses primitives analytiques existantes :

- 148 primitives : 96 boîtes, 37 cônes et 15 ellipsoïdes, plus le plan de sol infini ;
- façades, tours, fenêtres procédurales, chaussée, trottoirs, passages piétons, taxi, lampadaires, escaliers de secours, fourgon et sangsue ;
- matériaux urbains RGB565 avec pyramides mipmap ;
- BVH, carte de profondeur pour les ombres, brouillard, caméra et contrôles de résolution déjà présents dans V9.2.

La sangsue est une silhouette composée de primitives (torse voûté, membres allongés, griffes et tentacules faciaux). La direction reste alignée sur l’axe de l’avenue.

## Limites du rendu

Le moteur conserve sa lumière directionnelle unique et ses matériaux ne sont pas émissifs : les lampes sont représentées par des surfaces ambre, sans éclairage ponctuel réel. Les façades et le taxi sont des volumes simples avec textures procédurales, adaptés au ray casting actuel.

## Fichier

Ouvrir `moteur3d_v92_silent_hill_manhattan.html` localement dans Firefox ou Edge. Les réglages de caméra, résolution, filtre, ombres, mipmaps et brouillard restent interactifs.
