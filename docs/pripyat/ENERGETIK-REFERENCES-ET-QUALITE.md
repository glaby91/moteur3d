# Energetik — références visuelles et passe haute définition

Cette passe resserre le travail sur le Palais de la Culture Energetik. Les références photographiques servent à corriger les grandes masses, les ouvertures et les traces d’abandon. Elles ne sont pas intégrées comme textures : l’usage de photos libres, leur licence et leur perspective doivent être contrôlés avant toute redistribution d’images dans l’application.

## Références retenues

| Source | Vue utile | Points à comparer dans le modèle |
| --- | --- | --- |
| [Relevé Skeiron — Google Arts & Culture](https://artsandculture.google.com/story/palace-of-culture-energetik-skeiron/ggUh4kq6wkwB7w?hl=en) | Photogrammétrie et vues autour du bâtiment | Silhouette, décrochements, proportions, détails de façade et état des surfaces. C’est le meilleur contrôle spatial repéré dans les sources publiques. |
| [Façade Energetik — Wikimedia Commons](https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat) | Vues extérieures de face et en biais | Travées ouvertes sans vitres, piliers, coursives, corniches, marches, enseigne et dégradation du parement. |
| [Hall d’entrée — Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Palace_of_Culture_Lobby-pripyat.jpg) | Vue intérieure du foyer | Escalier, mezzanine, revêtements minéraux, volume du hall et peintures murales. La fiche indique une licence CC BY-SA 2.0 ; vérifier l’attribution si l’image est redistribuée. |
| [Intérieurs Energetik — Wikimedia Commons](https://commons.wikimedia.org/wiki/Category:Building_interiors_in_Pripyat) | Salles et circulations accessibles | Usure, poussière, vitres cassées, contraste entre lumière extérieure et pièces sombres. |
| [Peintures murales — Wikimedia Commons](https://commons.wikimedia.org/wiki/Category:Murals_in_Palace_of_Culture_in_Prypyat) | Panneaux peints et fresques | Emplacement, teinte et dimensions des panneaux décoratifs à reconstruire. |

Les liens vers les catégories peuvent évoluer et regroupent des fichiers dont les licences varient. Contrôler la fiche de chaque photo avant de la réutiliser. Aucun fichier photo brut n’est ajouté au dépôt dans cette passe.

## Modifications de cette révision

- La façade gagne de l’épaisseur visuelle avec un socle, des pilastres, des chapiteaux, des corniches et des marches. Les travées de la façade restent ouvertes, comme sur les photos de référence, et ne reçoivent pas de panneaux de verre.
- Le lettrage de toiture, les panneaux muraux peints et les rampes rendent le foyer plus lisible à la visite ; les fragments de vitrage sont réservés aux ouvertures où les références montrent encore du verre.
- Les murs intérieurs comportent des passages alignés sur le parcours au lieu d’un couloir qui traverserait des cloisons pleines.
- Toutes les textures couleur d’Energetik sont exportées en 1024 × 1024. Enduit, fresque, béton, pierre, asphalte, brique, rouille et feuillage disposent aussi de cartes de normale et rugosité avec mipmaps.
- La première passe matériaux renforce les coulures sur le béton, les zones d’enduit écaillé, les pigments colorés mais usés de la fresque, les fibres du bois, les éclats de débris et les variations de rugosité du sol et du feuillage. Les couleurs restent procédurales et servent à donner du relief à la scène, pas à remplacer des textures photographiques calibrées.
- DirectX utilise maintenant les cartes de rugosité et de normales avec les mipmaps disponibles, afin de faire varier les reflets et le relief fin des surfaces. Le résultat doit encore être comparé au rendu CPU sur Windows ; les deux chemins n’ont pas encore une parité complète d’éclairage.
- La sortie Vostro reste 1366 × 768. Le préréglage haute définition CPU effectue un rendu interne à 2732 × 1536, puis un filtre de réduction 2×. Le GPU DirectX garde sa résolution de sortie native.
- L’heure par défaut est 08:00 ; la poussière discrète dans le hall est activée par défaut.

## Limites à garder visibles

La géométrie demeure une reconstruction procédurale primitive par primitive, et les textures sont générées par bruit et motifs : elles ne sont pas des scans photographiques calibrés. Le relevé Skeiron doit encore être contrôlé face à face sur les volumes et mesures. La nouvelle qualité d’image coûte environ quatre fois plus de pixels CPU qu’un rendu natif et peut être lente sur le Core i5-7200U. L’aperçu DirectX raster n’a pas encore la parité d’éclairage, de réflexion et de réfraction avec le rendu CPU ; le réglage SSAA 2× lui est donc désactivé fonctionnellement.

La visite guidée et les déplacements manuels sont freinés au contact des primitives opaques ; les ouvertures ajoutées au modèle peuvent être traversées. Une vérification interactive sur Windows reste nécessaire pour confirmer chaque porte, chaque palier et chaque transition de niveau.
