# Moteur 3D — antialiasing adaptatif

Statut : **première implémentation CPU, avec validation sur la scène native**.

## Principe actif

- Le rendu principal utilise la résolution sélectionnée; le SSAA 4× n’est plus proposé par l’interface.
- Le mode par défaut repère les pixels dont la luminance diffère nettement de celle des pixels nord, sud, est et ouest, puis mélange leur couleur avec la moyenne de ces quatre voisins.
- Un seuil de contraste protège les aplats et une grande partie des détails de texture. Le mode natif sans filtre et le FSAA 16× restent disponibles pour comparaison.
- Les mipmaps et le filtrage bilinéaire des textures restent indépendants de ce traitement d’image.

## Mesure initiale

Sur la scène V13.14, à 960 × 540 et 8 threads dans l’environnement de mesure, le rendu natif a pris environ 77 ms. Le post-traitement quatre voisins a pris une médiane d’environ 2 ms sur cinq passages. Ces résultats donnent le coût de cette machine et de cette définition, pas une promesse de temps sur tous les PC.

## Limites et vérification visuelle

Le filtre travaille sur les couleurs de l’image, sans tampon de profondeur ni identifiant d’objet. Il adoucit donc certaines transitions de texture contrastées proches d’une arête, et ne reconstitue pas exactement la couverture géométrique sous-pixel. Comparer les silhouettes, les lignes fines, les détails texturés et la scène en mouvement avec le mode natif et le FSAA 16× avant d’ajuster le seuil ou le mélange.
