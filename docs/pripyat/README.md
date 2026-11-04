# Pripiat — Palais de la Culture Energetik

Le développement de cette branche se concentre désormais sur **un seul bâtiment détaillé : le Palais de la Culture Energetik**. La place et l’hôtel Polissya restent en arrière-plan pour situer le bâtiment. Les autres scènes du moteur restent conservées sur GitHub.

## Références

- [Modèle photogrammétrique et présentation du Palais Energetik — Google Arts & Culture / Skeiron](https://artsandculture.google.com/story/palace-of-culture-energetik-skeiron/ggUh4kq6wkwB7w?hl=en)
- [Photos Wikimedia Commons — Palais de la Culture Energetik](https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat)
- [Photos Wikimedia Commons — bâtiments de Pripiat](https://commons.wikimedia.org/wiki/Category:Buildings_in_Pripyat)
- [Photos Wikimedia Commons — intérieurs](https://commons.wikimedia.org/wiki/Category:Building_interiors_in_Pripyat)
- [Plan de la ville sur Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Map_of_Pripyat.svg)

Les photos documentaires servent à vérifier la silhouette, les façades et l’état du bâtiment. Les images générées sont des repères de composition, pas des références historiques. Les plans et dimensions restent approximatifs et doivent être confrontés aux sources visuelles.

Voir [les références visuelles du bâtiment et la passe haute définition](ENERGETIK-REFERENCES-ET-QUALITE.md) pour les vues retenues, leur usage précis et l’état des améliorations.

## Démo visée

Le parcours automatique dure **1 min 30** :

1. approche extérieure du bâtiment sous un soleil matinal franc ;
2. entrée par le foyer vitré ;
3. découverte du foyer, de l’auditorium et des pièces modélisées ;
4. passage par les niveaux et la cage d’escalier ;
5. sortie sur le toit, avec une pause finale d’environ 11 secondes face au soleil.

Le moteur affiche les images à sa cadence de rendu. La durée est pilotée sur 90 secondes ; un rendu CPU lent peut réduire le nombre d’images affichées pendant la visite. Le tracé suit les passages ouverts reconstruits dans la scène. Les déplacements manuels et automatiques sont arrêtés par la première géométrie rencontrée, portes et ouvertures restant franchissables.

## Construction

Source : `experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`.

Le modèle répartit le travail sur des éléments visibles à l’échelle de la visite : volume long et bas, tour d’escalier, colonnade, façade vitrée, foyer traversant, salles, dalles d’étage, escalier, auditorium et toiture accessible. Cette passe ajoute du relief à la façade, un socle, des pilastres et corniches, le lettrage d’entrée, des murs à portes ouvertes, des peintures murales, des éclats de vitrage et des rampes d’escalier. La géométrie est reconstruite pour permettre la caméra intérieure ; ce n’est pas un relevé complet de chaque pièce historique.

L’exporteur produit les textures couleur de tous les matériaux utilisés par Energetik en 1024 × 1024 avec mipmaps jusqu’à 8 × 8. Il ajoute des cartes de normale et de rugosité pour le béton, l’enduit, les peintures murales, la pierre, l’asphalte, la brique, la rouille et les feuilles. Le mode **Haute définition — 2× interne** calcule en 2732 × 1536 puis réduit vers la sortie Vostro 1366 × 768 ; cela multiplie approximativement par quatre les pixels calculés et s’applique au rendu CPU. DirectX reste à la résolution de sortie pour préserver la cadence de l’aperçu.

L’éclairage démarre à 08:00 et une option activée par défaut ajoute quelques poussières visibles dans les rais lumineux. L’intensité des textures, la fidélité des volumes et le rendu DirectX doivent encore être validés sur l’exécutable Windows et comparés visuellement aux photos ; la reconstruction n’est pas une restitution photogrammétrique.

## Export et compilation

Depuis `experiments/2026-10-30-cote-ete-gt/native_v14` :

```bash
M3D_SKIP_IMPORTED_CAR=1 node export_scene.js source_v1314.html scene_pripyat_central.bin scene_pripyat_central.js
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Le workflow Windows exporte la scène et l’ajoute au paquet de l’application. Le fichier binaire est généré à la compilation ; la source JavaScript reste modifiable.

## Audit technique

Voir [l’audit complet du moteur V14 et de la scène de Pripiat](AUDIT-MOTEUR3D-PRIPYAT-261104-0001.md) pour les constats sur l’export, les ombres DirectX, le benchmark, la mémoire raster et le périmètre historique de la scène, ainsi que les corrections de cette révision.
