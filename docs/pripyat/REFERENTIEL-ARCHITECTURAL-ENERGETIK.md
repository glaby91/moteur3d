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
| Mur support de la fresque | X=8.15 | hypothèse, dégagement de 3 cm derrière la texture |
| Plan image de la fresque | X=8.12 | hypothèse | 
| Bas du mur peint / arcade | Y=1.95 / 2.10 | hypothèse photo-guidée | 
| Face supérieure du premier palier | Y=3.49 | 3.25 + 0.24, géométriquement raccordée |

## Dépendances converties
- Planchers 1 et 2 et épaisseur des dalles.
- Dégagement de la galerie côté fresque.
- Géométrie et UV de la fresque conservés.
- Position de la dalle de palier du foyer.
- Départs des cloisons et hauteurs libres des pièces des trois niveaux.
- Départs des volumes des pièces supérieures.
- Hauteur de la tour.
- Mur peint porté par arcade de pierre, dégagements de passage et sous-face.
- Mur arrière réellement percé en partie centrale.
- Toit principal libéré de l'ancien grand volume plein.
- Connecteur Energetik–Polissya doté d'une arche piétonne.
- Terrasse et enseigne en caractères cyrilliques de l'hôtel Polissya (formes provisoires).

## Points encore non paramétriques / non validés
- Les deux escaliers ont maintenant des marches calculées à partir des altitudes de palier. La conformité historique des girons, largeurs et garde-corps requiert une coupe mesurée.
- Les ouvertures arrière, acrotères et arcade ont été dégagés; la conformité photo-métrique de façade, toiture et escalier extérieur reste à établir avec des points mesurés.
- Emprise de l'hôtel Polissya, connexion, routes, niveaux de terrain.
- Correspondance des pièces avec un plan de distribution authentifié.
- 10 captures de rendu CPU sont prévues via GitHub Actions pour vérifier les parcours; aucune comparaison photo appariée à métrique connue n'est encore certifiée.

## Critères d'acceptation avant qualification « conforme »
1. Plan à échelle et au moins trois points homologues pour la transformation plan → scène.
2. Une coupe de l'Energetik indiquant les niveaux, épaisseurs, escalier, hauteur libre du hall.
3. Plan du RDC et de l'étage permettant de placer la fresque et les ouvertures.
4. Contrôle automatique des collisions et intersections non désirées.
5. Export de scène, compilation Windows, captures CPU de contrôle et comparaison visuelle aux photos d'origine. Le backend DX11 actuel est un aperçu raster, pas un ray tracing GPU complet.

**Ne pas présenter l'état actuel comme une reconstruction historiquement conforme.**
