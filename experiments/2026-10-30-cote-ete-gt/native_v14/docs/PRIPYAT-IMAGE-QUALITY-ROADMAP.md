# Feuille de route — qualité d’image de Pripiat

## Objectif

Approcher l’apparence des vraies photos de Pripiat abandonnée tout en conservant une exploration fluide de la scène. Les références photographiques réelles guident les couleurs, l’usure, les détails et la végétation. La première validation se fait sur un bâtiment et ses abords, avant extension aux autres secteurs.

## Ordre des évolutions

### 1. Cohérence des couleurs CPU / DirectX

- Lire les textures dans le bon espace (sRGB), éclairer en espace linéaire et appliquer la correction de gamma une seule fois en sortie.
- Comparer CPU et DirectX depuis la même caméra, avec la même lumière et une charte de couleurs neutres.
- Corriger d’abord les écarts de teinte, d’exposition et de contraste avant d’ajouter des effets.

**Critère de validation :** les surfaces neutres et les matériaux de référence gardent des couleurs proches dans les deux rendus.

### 2. Éclairage, ombres et particules atmosphériques

- Stabiliser la lumière du soleil et la lumière ambiante.
- Améliorer les ombres portées et ajouter une occlusion de contact légère pour asseoir les objets sur le sol.
- Garder une ambiance crédible selon l’heure et éviter les ombres bouchées ou les surfaces surexposées.
- Ajouter de fines poussières en suspension, surtout visibles dans les rayons de soleil à l’intérieur et près des ouvertures.
- Simuler leur diffusion par de petites particules translucides, discrètes et animées lentement, avec atténuation selon la distance et la lumière. Compléter par un léger voile atmosphérique dans les vues lointaines.
- Régler la densité selon le lieu : poussière dans les pièces abandonnées, particules plus rares dehors. Éviter l’aspect confettis et le bruit uniforme sur toute l’image; le grain photographique, s’il est ajouté, restera un effet séparé et léger.

### 3. Matériaux

- Faire évoluer le shader vers une réponse PBR cohérente : couleur, normale et rugosité réellement prises en compte.
- Vérifier l’échelle des UV afin que joints, briques, panneaux et feuilles gardent des dimensions plausibles.
- Prévoir 1K pour les matériaux courants et 2K seulement pour les façades et éléments vus de près.
- Étendre le format de scène et l’exporteur au-delà de la limite actuelle de 512 × 512, en conservant la lecture des scènes existantes et des mipmaps.

### 4. Géométrie et détails visibles

- Réserver les détails géométriques aux silhouettes et dégâts lisibles : vitres cassées, bords de béton, débris, végétation proche.
- Ajouter les traces fines (coulures, taches, peinture écaillée) dans les cartes ou sous forme de détails locaux.
- Prévoir des niveaux de détail selon la distance, sans changement visible aux limites des secteurs.

### 5. Finition d’image

- Améliorer l’anticrénelage et la stabilité des contours.
- Régler exposition, contraste et tonalité avant d’ajouter la brume.
- Garder profondeur de champ et effets cinématiques pour la séquence de visite; ils ne doivent pas masquer les défauts des matériaux.

## Première tranche de travail

Construire un bâtiment représentatif avec sa rue, ses matériaux usés et son éclairage. Capturer exactement le même point de vue en CPU et DirectX, puis comparer aux photos réelles disponibles. Une fois les couleurs et l’éclairage validés, augmenter la résolution des textures et étendre la méthode aux bâtiments suivants.

## Contraintes connues

Le moteur natif stocke actuellement les textures en RGB565 avec une pyramide de mipmaps plafonnée à 512 × 512. Le chemin DirectX 11 est un aperçu raster simplifié : il ne reproduit pas encore les effets du lancer de rayons CPU, notamment certaines ombres, réflexions, réfractions et la profondeur de champ. Ces écarts doivent être indiqués dans les diagnostics tant que les deux chemins ne sont pas équivalents.
