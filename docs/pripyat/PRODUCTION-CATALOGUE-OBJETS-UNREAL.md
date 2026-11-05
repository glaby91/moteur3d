# Production prioritaire — assets photoréalistes Energetik

## Inventaire source

- 100 références photographiques : 92 de galerie et 8 vues principales.
- 20 familles documentées; 22 IDs matériau `prp_*`.
- Mapping photo par photo détaillé pour 01–32 et H01–H08; les références 33–92 sont regroupées par plages.
- Le siège cinéma existe déjà en 80 primitives; l’architecture principale existe en blockout procédural. Les assets ne sont pas encore livrés en GLB autonome.

## Priorité décidée

Produire moins d’objets et pousser leur fidélité visuelle. Les objets suivants passent en premier :

1. **Travée de façade Energetik** — références 01/H01; béton, relief, UV et usure.
2. **Siège/rangée d’auditorium** — référence H03 et photos fournies; coque, revêtement, coutures et structure.
3. **Enseigne de toiture** — référence H05; lettres cyrilliques, treillis, peinture écaillée et corrosion.

Le reste des 20 familles reste au catalogue en attente. Aucun détail ambigu ne sera inventé.

## Matériaux et fidélité

- Réutiliser les IDs, cartes de rugosité et normales définis dans `PRIPYAT-MATERIALS-CATALOGUE.md` et les paramètres de scène existants.
- Ne jamais convertir `reflectivityPercent` en métallicité. Le format de scène ne fournit pas ce paramètre PBR.
- Ajouter ou réinterpréter un paramètre seulement après avoir identifié sa définition et sa source.
- Les photos protégées servent à la modélisation; elles ne deviennent pas des textures redistribuables sans licence vérifiée.
- Les textures doivent être raccordées aux matériaux natifs et inclues en PBR dans les GLB destinés à Unreal.

## Livrables sur la fenêtre de 6 h 30

Objectif réaliste : les trois assets prioritaires en première passe haute fidélité, réutilisation de leur géométrie dans le moteur natif et GLB 2.0 pour Unreal, avec manifeste des sources et matériaux. Cette fenêtre ne garantit pas une restitution photogrammétrique complète ni la validation sur toutes les configurations Unreal.

## Vérification

- Générer la scène native sans modifier ni supprimer les objets existants.
- Contrôler l’échelle métrique, UV, normales, orientation et matériaux.
- Compiler le moteur natif sous Windows.
- Valider le GLB 2.0 et son ouverture/import dans Unreal séparément du succès de compilation.
- Garder les captures de comparaison photo/rendu et les performances comme éléments de validation.

## Suivi

Branche de travail : `feature/pripyat-catalog-assets-261105`.
Ticket : [Créer le catalogue d’objets Energetik natif + Unreal](https://github.com/glaby91/moteur3d/issues/4).

Les corrections PBR et d’éclairage du rendu CPU évoquées après le build `261105-0001` restent au backlog et seront traitées après les assets si le temps le permet.