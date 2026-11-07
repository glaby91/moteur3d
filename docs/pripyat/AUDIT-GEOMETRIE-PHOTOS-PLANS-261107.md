# Audit global de géométrie — Pripiat / Energetik / Polissya
Date : 2026-11-07. Statut : **audit statique du code et des références documentées, PAS validation photogrammétrique**.

## Références retrouvées
- Carte de repères utilisateur `carte_reperes_pripyat.html` : références photographiques Wikimedia de la façade Energetik et du Polissya, plan du centre `Pripyat_center.png`, repères GPS et fonds OSM/Esri. La carte précise que les repères GPS ne sont pas des coins de bâtiments.
- Montage utilisateur `Montage du Palais de la Culture Energetik abandonné.png` : collage illustratif avec façade, hall, mosaïque, auditorium, gymnase, boxe, piscine et bibliothèque. **Montage de reconstitution, non preuve de leur emplacement réel ni plan coté.**
- `docs/pripyat/GEORREFERENCEMENT-POINTS-DE-CONTROLE-261107.md` : coordonnées publiées et écart non résolu entre centres provisoires et points GPS.
- `docs/pripyat/RECONSTRUCTION-MODULAIRE-261107.md` : emprises provisoires et sources.
- `scene_pripyat_views.json` : dix caméras reproductibles.

## Constat de code (scène native)
1. **Place / rues : non validées.** La place et les immeubles de contexte restent des volumes paramétriques, sans contours relevés sur plan.
2. **Polissya : non validé.** Le volume modulaire ne découle pas encore d'un plan coté ni d'une correspondance façade/photo; nombre d'étages et implantation à confirmer.
3. **Liaison : non validée.** `m3dBuildConnector` est un parallélépipède de X=30.2 à 40.2, Z=-24 à -18.6, sans arche mesurée.
4. **Energetik : enveloppe contradictoire à reprendre.** `m3dBuildEnergetikShell` construit des murs périphériques épais, des blocs de toiture et une tour; façade, entrée et toiture sont ensuite détaillées par plusieurs fonctions distinctes. Aucun jeu de plans de coupe n'établit les niveaux, ouvertures et hauteurs.
5. **Hall / fresque : dimensions internes estimées.** Fresque à X=8.12, Z=-41.55 à -32.05, Y=2.00 à 6.30 (9.5 × 4.3 m); mur derrière X=8.38. Le plancher à Y=3.25 a été partiellement évidé; le plancher à Y=6.35 se trouve juste au-dessus du haut de la fresque. Cela supprime un masquage évident, mais ne prouve ni la vraie hauteur du hall ni la position historique de la fresque.
6. **Escalier du foyer : incohérence géométrique potentielle.** Les marches montent de Y=0.48 à ~3.24 et avancent vers Z=-33; le palier est Y=3.25. Il faut vérifier le recouvrement de la dernière marche et le passage libre vers le palier après suppression de la bande de mezzanine.
7. **Cage d'escalier : non validée.** Marches de 11 × 0.255 m par volée, paliers artificiels; contrôle de continuité verticale et des collisions à effectuer.
8. **Auditorium et autres salles : disposition non vérifiée.** Les locaux représentés dans le montage (gymnase, piscine, bibliothèque, boxe) ne constituent pas un plan de distribution; ne pas les placer arbitrairement.
9. **Caméra fresque : point cible [8.12,4.15,-36.8], distance 7 m, angle -90°.** Vérifier l'origine réelle de la caméra et les masquages en rendu, pas seulement la cible.

## Risques de régression
- Les éléments d'intérieur sont encore écrits dans un même `makeScene()` avec coordonnées absolues. Déplacer l'enveloppe seule désolidariserait fresque, escalier, collision et caméras.
- Les ouvertures sont construites par assemblage de boîtes : un volume « porte » ne perce pas automatiquement les autres volumes qui le recouvrent.
- Les tests géométriques statiques vérifient quelques invariants, pas la visibilité finale des surfaces ni l'absence d'intersections de tous les triangles.

## Plan d'exécution priorisé
**P0 — preuve** : retrouver le plan source à échelle exploitable et les photos d'origine (avec provenance et point de vue), identifier au moins 3 points homologues sur plan / terrain, documenter chaque cote (confirmée, estimée, inconnue).
**P1 — repère** : définir une transformation unique plan → scène; produire polygones de la place, Energetik, Polissya et liaison avec résidus géométriques.
**P2 — architecture** : reconstruire Energetik par sections (socle, ailes, hall double hauteur, auditorium, toiture, tour) et percements réels; conserver la fresque et ses UV.
**P3 — intérieur** : comparer plan d'étage et coupes, vérifier marches/paliers, portes, hauteurs libres, garde-corps et parcours caméra; éviter d'inventer la distribution.
**P4 — validation** : vues photographiques appariées avec mêmes cadrages et focales, tests d'occlusion et collisions, export BIN, build Windows et captures CPU/GPU comparables.

## Règle de qualité
Aucune cote de reconstitution ou image de synthèse ne doit être qualifiée d'historiquement exacte sans mesure correspondante. La fidélité visuelle doit être jugée sur des photographies identifiées, non sur un collage illustratif.
