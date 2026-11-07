# Energetik — référentiel architectural de la scène
Version provisoire, 2026-11-07.

## Principe
La source de vérité est `M3D_ENERGETIK_ARCH` dans `scene_pripyat_central.js`.
Les cotes restent **hypothétiques** tant qu'elles ne sont pas homologuées sur un plan coté et des photographies de référence.

| Repère | Valeur scène (m) | Statut |
|---|---:|---|
| Sol de référence | 0 | convention |
| Sol fini RDC | 0.38 | hypothèse |
| Plancher 1 | 3.25 | hypothèse |
| Plancher 2 | 6.35 | hypothèse |
| Épaisseur dalle | 0.24 | hypothèse |
| Toiture principale | 9.4 | hypothèse |
| Tour | 14.0 | estimation |
| Fresque basse/haute | 2.00 / 6.30 | estimation |
| Fresque largeur | 9.50 | estimation |
| Mur porteur de la fresque | X=8.38 | hypothèse |
| Plan image de la fresque | X=8.12 | hypothèse |

## Dépendances converties
- Planchers 1 et 2 et épaisseur des dalles.
- Dégagement de la galerie côté fresque.
- Géométrie et UV de la fresque conservés.
- Position de la dalle de palier du foyer.
- Départs des cloisons et hauteurs libres des pièces des trois niveaux.
- Départs des volumes des pièces supérieures.
- Hauteur de la tour.

## Points encore non paramétriques / non validés
- Marches, garde-corps et paliers secondaires des deux escaliers; besoin d'une coupe mesurée.
- Toitures multiples, façades, baies, escalier extérieur et enveloppe du gymnase.
- Emprise de l'hôtel Polissya, connexion, routes, niveaux de terrain.
- Correspondance des pièces avec un plan de distribution authentifié.
- Validation visuelle par comparaison de photos avec cadrages appariés.

## Critères d'acceptation avant qualification « conforme »
1. Plan à échelle et au moins trois points homologues pour la transformation plan → scène.
2. Une coupe de l'Energetik indiquant les niveaux, épaisseurs, escalier, hauteur libre du hall.
3. Plan du RDC et de l'étage permettant de placer la fresque et les ouvertures.
4. Contrôle automatique des collisions et intersections non désirées.
5. Export de scène, compilation Windows et captures comparatives sur mêmes caméras CPU/GPU.

**Ne pas présenter l'état actuel comme une reconstruction historiquement conforme.**
