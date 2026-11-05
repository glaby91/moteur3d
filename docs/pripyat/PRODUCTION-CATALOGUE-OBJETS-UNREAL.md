# Catalogue Energetik — production qualité d’abord

## Référentiel

- Inventaire : 100 photos de référence, 20 familles d’objets et 22 IDs matériau `prp_*`.
- Les références servent à retrouver formes, placement et usure. Elles ne deviennent pas des textures redistribuables sans licence vérifiée.
- Les identifiants et paramètres du catalogue existant restent la source. Une valeur native `reflectivityPercent` ne devient jamais une métallicité.

## Priorité actuelle

1. **Travée de façade Energetik** — première pièce livrée : source native réutilisable, scène native isolée, GLB 2.0 avec cartes PBR et manifeste. Dimensions approximatives reprises du blockout existant; pas de prétention photogrammétrique.
2. **Siège / rangée d’auditorium** — reprendre la pièce existante en commençant par corriger ses matériaux et détails, en suivant H03 et les photos fournies.
3. **Enseigne de toiture** — référence H05; lettres cyrilliques, treillis, peinture et corrosion à reprendre uniquement depuis la référence.

Les 17 autres familles restent dans le catalogue. Aucun remplissage par objets supposés.

## Première livraison

- Asset : `energetik_facade_bay` — 26 reliefs natifs, scène de 27 objets avec dalle témoin, 4 984 triangles GLB.
- Béton catalogue `prp_concrete_panel`, relié à l’ID natif existant `concrete` (réflectivité 2%). Métallicité GLB = 0; rugosité cible catalogue 0,78–0,94; moyenne de la carte ≈ 0,87.
- Cartes couleur, normale tangentielle, rugosité et masque glTF Metallic-Roughness intégrés. La normale est une approximation discrète dérivée de la luminance; elle n’est pas issue d’un scan.
- GLB 2.0 vérifié structurellement, avec trois images incorporées et orientation triangles/normales cohérente. Export natif effectué; import Unreal Editor et comparaison image finale à faire.
- Les corrections d’éclairage/PBR antérieurement mises en attente restent distinctes de cet asset et ne sont pas incluses dans son matériau.

## Étapes suivantes

1. Contrôler la travée dans le moteur Qt et l’importer dans Unreal Engine; comparer aux références et ajuster échelle UV, lumière et relief.
2. Reprendre le siège avec les mêmes contrôles et éviter les brillances métalliques constatées dans l’image précédente.
3. Créer l’enseigne comme troisième pièce seulement après lecture rapprochée de H05.
4. Revenir aux corrections d’éclairage du rendu après les actifs prioritaires.

Branche : `feature/pripyat-catalog-assets-261105` — [ticket #4](https://github.com/glaby91/moteur3d/issues/4).
