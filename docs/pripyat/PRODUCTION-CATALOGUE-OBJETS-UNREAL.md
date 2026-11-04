# Production — catalogue d’objets Energetik

## Périmètre vérifié

- L’inventaire décrit **100 références photographiques** : 92 images de galerie et 8 vues principales.
- Il regroupe le travail en **20 familles d’objets** et référence **22 matériaux prp_***.
- Le siège cinéma existe comme module procédural de 80 primitives; la scène Energetik centrale contient déjà une reconstruction architecturale procédurale.
- L’inventaire détaille individuellement les photos 01–32 et H01–H08; les photos 33–92 sont regroupées par plages. Les attributions incertaines restent exclues.
- Le dépôt ne contient pas encore un pack GLB autonome des 20 familles. L’importeur glTF actuel ne conserve pas les cartes PBR glTF : il convertit seulement les triangles et remappe quelques noms de matériaux.

## Règles de production

1. Utiliser les identifiants et paramètres déjà définis dans les catalogues; ne pas transformer `reflectivityPercent` en métallicité.
2. Ajouter un nouveau paramètre matériau seulement si le format existant ne le décrit pas, et le documenter avant son utilisation.
3. Garder un identifiant stable, une unité métrique, un pivot et une échelle UV par objet.
4. Livrer une définition native réutilisable et un export glTF 2.0/GLB pour Unreal Engine à partir des mêmes données géométriques.
5. Les photos de référence guident la forme et l’usure; aucune photo n’est redistribuée comme texture sans licence vérifiée.
6. Garder les exclusions de l’inventaire : pas d’accessoires dont la fonction n’est pas confirmée.

## Livrables visés

- `assets/pripyat/catalogue-objects.json` : registre des familles, matériaux et états de validation.
- Modules d’objets réutilisables par famille, avec variantes paramétriques.
- Cartes PBR utilisant les définitions documentées : albédo, normale tangentielle, rugosité; ORM/metalness uniquement si une définition explicite existe.
- Paquet GLB et manifeste source/licence par famille, plus l’intégration native dans la scène Energetik.
- Vues de contrôle natives et vérification d’ouverture des GLB dans Unreal, en distinguant la compilation de la validation visuelle.

## Estimation sur la fenêtre de 6 h 30

Cette fenêtre suffit pour mettre en place le registre et le pipeline, produire une première passe modulaire des 20 familles et livrer un GLB de contrôle. Elle ne suffit pas à garantir une restitution photogrammétrique/photo-réaliste de chaque variante, la vérification des 100 vues image par image et une validation visuelle complète dans les deux moteurs. Les incertitudes de l’inventaire et les droits de chaque source restent à respecter.

## État de départ — 5 novembre 2026

| Domaine | État |
|---|---|
| Inventaire | 20 familles et 22 matériaux recensés; mapping détaillé incomplet pour les photos 33–92 |
| Géométrie native | Scène Energetik existante; siège cinéma réutilisable; pack modulaire manquant |
| Textures | Matériaux et cartes procédurales définis pour la scène; liaison stable aux identifiants prp_* à finaliser |
| Unreal | GLB 2.0 est le format cible; exporteur commun et intégration des cartes PBR à créer |
| Validation | Build seul insuffisant; contrôle de géométrie, matériaux et ouverture du GLB requis |

L’avancement courant est suivi dans [le ticket GitHub](https://github.com/glaby91/moteur3d/issues/4).