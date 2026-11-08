# Pripiat — Palais de la Culture Energetik

## Suivi des travaux différés

- [Panorama de l'hôtel Polissya : baies, toiture inclinée, carrelage, briques et état d'abandon — non prioritaire](BACKLOG-POLISSYA-PANORAMA.md). Priorité actuelle : colonnade courbe, ses quatre marches et l'arrivée près de l'avancée de l'hôtel.

## Benchmark GPU sur Intel HD Graphics 620 — 7 novembre 2026

Mesure sur matériel réel du ray tracing **Compute D3D11 sans DXR** : **500 images natives 1366 × 768**, **223 échantillons WDDM**, **88,9 % d'occupation GPU moyenne** et **93,5 % maximale**. Scène : 92 886 objets et 67 404 triangles. Les FPS ne sont pas mesurés.

- [Rapport du benchmark et limites des mesures](BENCHMARK-GPU-HD620-500-261107.md)
- [Résultats JSON exploitables par les prochains audits](BENCHMARK-GPU-HD620-500-261107.json)
- [Version Windows Build 261107-0004 + scène Pripyat](https://github.com/glaby91/moteur3d/actions/runs/42411886443#artifacts)

Les anciennes séries 100 images ayant hérité de la réduction d'aperçu ne sont pas comparables à résolution sélectionnée identique. **Aucune modification du shader ou de la scène dans cet enregistrement de résultats.**


Le développement de cette branche se concentre désormais sur **un seul bâtiment détaillé : le Palais de la Culture Energetik**. La place et l’hôtel Polissya restent en arrière-plan pour situer le bâtiment. Les autres scènes du moteur restent conservées sur GitHub.

## Références

- [Modèle photogrammétrique et présentation du Palais Energetik — Google Arts & Culture / Skeiron](https://artsandculture.google.com/story/palace-of-culture-energetik-skeiron/ggUh4kq6wkwB7w?hl=en)
- [Photos Wikimedia Commons — Palais de la Culture Energetik](https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat)
- [Photos Wikimedia Commons — bâtiments de Pripiat](https://commons.wikimedia.org/wiki/Category:Buildings_in_Pripyat)
- [Photos Wikimedia Commons — intérieurs](https://commons.wikimedia.org/wiki/Category:Building_interiors_in_Pripyat)
- [Plan de la ville sur Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Map_of_Pripyat.svg)
- [Palais de la culture Energetik — Wikipédia (français)](https://fr.wikipedia.org/wiki/Palais_de_la_culture_Energetik) — repère général sur les fonctions et l’état du bâtiment; l’article signale lui-même que ses sources sont incomplètes.
- [Photographies de Pripiat — Getty Images](https://www.gettyimages.fr/photos/pripyat) — vues d’ensemble et d’environnement urbain; les images restent des références visuelles et ne sont pas intégrées comme textures.

Les photos documentaires servent à vérifier la silhouette, les façades et l’état du bâtiment. Les images générées sont des repères de composition, pas des références historiques. Les plans et dimensions restent approximatifs et doivent être confrontés aux sources visuelles.

Voir [les références visuelles du bâtiment et la passe haute définition](ENERGETIK-REFERENCES-ET-QUALITE.md) pour les vues retenues, leur usage précis et l’état des améliorations.

Le cahier des exigences, les niveaux communiqués, les références prioritaires et la validation restante sont suivis dans [Cahier de reconstruction Energetik](CAHIER-ENERGETIK-PASSE-261104.md).

## Fresque du hall d’escalier

La fresque du foyer est désormais une mosaïque panoramique lumineuse, mappée directement dans la scène native sur le mur côté hall près de l’escalier. Le quad de 9,5 × 4,3 m est placé en X=8,12, Y=2,00, Z=−41,55 m, face −X. La texture sRGB 2048 × 930 conserve le ratio estimé de l’œuvre; l’export natif construit des mipmaps RGB888. Un GLB autonome est fourni pour Unreal.

La composition est une reconstruction générée à partir des quatre vues fournies; les pertes sans référence sont interprétées. Les dimensions et l’emplacement suivent le blockout et restent à valider visuellement. Voir [la fiche de l’asset](../../experiments/2026-10-30-cote-ete-gt/native_v14/docs/ASSET-ENERGETIK-FRESQUE-ESCALIER.md).

## Démo visée

Le parcours automatique dure **1 min 30** :

1. approche extérieure du bâtiment sous un soleil matinal franc ;
2. entrée par les travées ouvertes de la façade ;
3. découverte du foyer, de l’auditorium et des pièces modélisées ;
4. passage par les niveaux et la cage d’escalier ;
5. sortie sur le toit, avec une pause finale d’environ 11 secondes face au soleil.

Le moteur affiche les images à sa cadence de rendu. La durée est pilotée sur 90 secondes ; un rendu CPU lent peut réduire le nombre d’images affichées pendant la visite. Le tracé suit les passages ouverts reconstruits dans la scène. Les déplacements manuels et automatiques sont arrêtés par la première géométrie rencontrée, portes et ouvertures restant franchissables.

## Construction

Source : `experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`.

Le modèle répartit le travail sur des éléments visibles à l’échelle de la visite : volume long et bas, tour d’escalier, colonnade, façade ouverte sans vitres, foyer traversant, salles, dalles d’étage, escalier, auditorium et toiture accessible. La façade doit montrer ses grandes travées ouvertes, ses piliers et ses coursives ; les photos fournies ne montrent pas de vitrages sur cette façade. Le parcours intérieur s’appuie sur la répartition communiquée : stand de tir, réserves et archives au sous-sol ; foyer, théâtre/concert, cinéma et aile sportive au rez-de-chaussée ; mezzanine, danse/discothèque Edison-2 et bibliothèque au premier étage ; salles de clubs et accès à la terrasse au niveau supérieur. Les positions exactes des pièces et les connexions non visibles dans les photos restent des interprétations. La géométrie est reconstruite pour permettre la caméra intérieure ; ce n’est pas un relevé complet de chaque pièce historique.

L’exporteur produit les textures couleur de tous les matériaux utilisés par Energetik en 1024 × 1024 avec mipmaps jusqu’à 8 × 8. Il ajoute des cartes de normale et de rugosité pour le béton, l’enduit, les peintures murales, la pierre, l’asphalte, la brique, la rouille et les feuilles. Le mode **Haute définition — 2× interne** calcule en 2732 × 1536 puis réduit vers la sortie Vostro 1366 × 768 ; cela multiplie approximativement par quatre les pixels calculés et s’applique au rendu CPU. DirectX reste à la résolution de sortie pour préserver la cadence de l’aperçu.

L’éclairage démarre à 08:00 et une option activée par défaut ajoute quelques poussières visibles dans les rais lumineux. Les textures procédurales ont des couches d’usure, coulures, éclats de peinture, variations de rugosité et pigments délavés pour la fresque. Le mode DirectX échantillonne désormais les cartes de rugosité et de normales avec les mipmaps disponibles. Ces changements doivent encore être validés sur l’exécutable Windows et comparés visuellement aux photos ; la reconstruction n’est pas une restitution photogrammétrique.

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
