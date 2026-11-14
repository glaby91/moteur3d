# Catalogue Energetik — production qualité d’abord

## Référentiel

- Inventaire : 100 photos de référence, 20 familles d’objets et 22 IDs matériau `prp_*`.
- Les références servent à retrouver formes, placement et usure. Elles ne deviennent pas des textures redistribuables sans licence vérifiée.
- Les identifiants et paramètres du catalogue existant restent la source. Une valeur native `reflectivityPercent` ne devient jamais une métallicité.

## Priorité actuelle

1. **Travée de façade Energetik** — première pièce livrée : source native réutilisable, scène native isolée, GLB 2.0 avec cartes PBR et manifeste. Dimensions approximatives reprises du blockout existant; pas de prétention photogrammétrique.
2. **Siège / rangée d’auditorium** — première rangée livrée avec trois états documentés : assise manquante, relevée et abaissée; rail métallique commun et cartes PBR procédurales.
3. **Enseigne de toiture** — prochaine priorité, référence H05; lettres cyrilliques, treillis, peinture et corrosion à reprendre uniquement depuis la référence.

Les 17 autres familles restent dans le catalogue. Aucun remplissage par objets supposés.

## Première livraison

- Asset : `energetik_facade_bay` — 26 reliefs natifs, scène de 27 objets avec dalle témoin, 4 984 triangles GLB.
- Béton catalogue `prp_concrete_panel`, relié à l’ID natif existant `concrete` (réflectivité 2%). Métallicité GLB = 0; rugosité cible catalogue 0,78–0,94; moyenne de la carte ≈ 0,87.
- Cartes couleur, normale tangentielle, rugosité et masque glTF Metallic-Roughness intégrés. La normale est une approximation discrète dérivée de la luminance; elle n’est pas issue d’un scan.
- GLB 2.0 vérifié structurellement, avec trois images incorporées et orientation triangles/normales cohérente. Export natif effectué; import Unreal Editor et comparaison image finale à faire.
- Les corrections d’éclairage/PBR antérieurement mises en attente restent distinctes de cet asset et ne sont pas incluses dans son matériau.

## Deuxième livraison : rangée de sièges d’auditorium

L’asset `energetik_cinema_seat_row` conserve le même modèle de siège capitonné sur les trois emplacements. La rangée présente une assise absente à gauche, une assise relevée au centre et une assise abaissée à droite, conformément à la référence fournie. Un rail continu relie les trois places.

Le modèle est exporté comme scène native V14 et comme GLB 2.0 pour Unreal Engine. Le GLB regroupe quatre matériaux et incorpore les cartes de couleur, normale et métallique-rugosité. Les cartes sont procédurales et non photographiques. L’identification des matériaux reste approximative tant que les images sources détaillées n’ont pas permis de la confirmer. L’import Unreal et la comparaison visuelle dans le moteur Qt restent à effectuer.

Fiche asset : [ASSET-ENERGETIK-AUDITORIUM-SEAT-ROW.md](ASSET-ENERGETIK-AUDITORIUM-SEAT-ROW.md).

## Étapes suivantes

1. Contrôler les deux assets dans le moteur Qt et importer les GLB dans Unreal Engine; comparer aux références et ajuster l’échelle UV, la lumière et les reliefs.
2. Créer l’enseigne comme troisième pièce après lecture rapprochée de H05.
3. Revenir aux corrections d’éclairage du rendu après les assets prioritaires.

Branche : `feature/pripyat-catalog-assets-261105` — [ticket #4](https://github.com/glaby91/moteur3d/issues/4).

## Correction d’éclairage CPU hors-ligne

L’audit du rendu du siège a révélé que `reflectivityPercent` était converti en `metallic` dans le shader CPU PBR, alors que les scènes V4 ne définissent aucun canal métallique. Le correctif garde les matériaux dans le modèle diélectrique disponible : réflectivité pilote F0, et métallicité reste à zéro. Les normales de surface sont orientées contre le rayon avant l’éclairage et le calcul des rebonds utilise le même F0 que l’éclairage direct. Ce changement ne rend pas encore les surfaces métalliques physiques; cela demandera un paramètre metallic explicite dans le format.
