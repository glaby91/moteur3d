# Architecture éclairage multi-sources — feuille de route

## Contraintes

La simulation actuelle rend **une seule source directionnelle Z-bufferisée**.
Les nombres ci-dessous sont des objectifs de capacité, **pas des performances mesurées**.

| Fonction | Capacité cible | Révision du 19/10/2026 |
| --- | ---: | ---: |
| Lumières actives | 32 | 1 |
| Lumières influençant une tuile 16×16 | 8 | 1 |
| Sources à ombres dynamiques | 4 | 1 |
| Shadow-map par directionnelle | 1 | 1 |
| Spotlight (carte perspective) | 1 par source | Pas implémenté |
| Ponctuelle omnidirectionnelle | 6 faces de profondeur / source | Pas implémenté |
| Z-buffer caméra | 1 écran (tuilé) | Pas implémenté |

## Contrat futur d'une source

Champs proposés : `id`, `type` (directional/spot/point), `enabled`, `castsShadows`,
`color`, `intensity`, `position`, `direction`, `range`, `shadowResolution`,
`shadowBias`, `shadowVersion`.

Le tableau actuel `lightingScene.lights` possède une source directionnelle; les propriétés
de couleur, intensité et atténuation des autres sources ne sont PAS encore interprétées.

## Étapes de développement

1. Registre complet + gestion de plusieurs contributions d'éclairage par matériau.
2. Sélection de 8 sources par tuile selon influence maximale estimée et visibilité du volume d'influence.
3. Gestion de quatre shadow-maps dynamiques maximales, autres sources uniquement sans ombres dynamiques.
4. Directionnelle : carte Z orthographique. Spot : perspective Z et cône. Ponctuelle : cube-map 6 faces avec sélection des faces nécessaires.
5. Cache par lumière et invalidation incrémentale : distinguer lumière qui change, occluder qui change et région de réception des ombres.
6. Ajout d'une carte de profondeur **caméra** pour régler les occlusions des faces géométriques croisées.
7. Mesurer nombre de comparaisons Z, écritures/lectures mémoire, médiane/p95 de la cadence et budget FPGA avant de fixer les capacités finales.

## Points de vigilance

- L'ombre d'un objet mobile peut s'étendre hors de ses tuiles écran; invalider les seules
  tuiles du personnage serait incorrect.
- Une lumière directionnelle n'est pas un point : une unique carte 2D orthographique ne
  fournit PAS un éclairage ponctuel omnidirectionnel.
- Le filtrage PCF, le MSAA et les ombres géométriques n'ont pas le même coût.
- Les seules estimations d'opérations JavaScript ne valent pas chronométrage FPGA.
