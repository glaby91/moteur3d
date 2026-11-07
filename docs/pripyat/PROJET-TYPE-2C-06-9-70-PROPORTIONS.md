# Projet-type 2С-06-9/70 — proportions et limites de transposition à Pripiat

Date de recherche : 2026-11-07. Sources d'origine en russe.

## Nouvelle référence architecturale

La documentation relative à **un chantier de rénovation d'un bâtiment de culture utilisant le même projet-type** indique (dimensions nominales en axes, non relevé de Pripiat) :

| Élément du projet-type | Valeur annoncée | Comparaison scène actuelle |
|---|---:|---|
| Emprise entre axes | **81,3 × 30,4 m** | blockout Energetik **81,0 × 24,6 m**. Profondeur manquante potentielle 5,8 m. Orientation du grand axe à vérifier |
| Nombre de niveaux | 4 dont 1 sous-sol | scène 3 niveaux hors sol, sans sous-sol intégré. Le bâtiment Pripiat est par ailleurs décrit « 2 étages » par PhotoBuildings : preuve de différences/ambiguïtés |
| Hall d'accueil / foyer | **540,6 m²** | foyer représenté sans métrage authentifié |
| Salle de spectacle | **24 × 18 m** ; 408,7 m² (valeur citée séparément) | notre volume auditorium est encore provisoire (environ 34 × 17 m) |
| Scène | **18,3 × 18 m** | scène actuelle représentée par estrade de profondeur ~3,2 m, et non une vraie cage de scène |
| Capacité de la salle | **720 spectateurs** | maquette de 12 rangées × 4 banques ×10 sièges = **480 sièges** |
| Hauteur libre au plafond de la salle | **11,6 m** | plafond de l'auditorium autour de 6,35 m dans la maquette ; **écart structurel non corrigé** |
| Cage de scène | **21 m** | maquette toit/tour max ~14 m ; **écart non corrigé** |
| Plan fonctionnel | foyer, parterre, scène, circulations, salles de danse et de cérémonie, locaux studio et techniques, sous-sol pour machinerie | distribution actuelle non mesurée |

Ces données sont tirées de la **description publiée d'une autre rénovation**, pas de plans d'exécution du bâtiment réellement construit à Pripiat. Les données de réhabilitation, équipements et niveaux ne peuvent donc pas être transposées telles quelles.

Sources :
- https://proekt.sx/tip/dk781-detail.html (description détaillée de rénovation 2с-06-9/70, dessins commerciaux)
- https://tipdoc.ru/catalog/14090/1640132/ (index des dessins en PDF, accès commercial)
- https://photobuildings.com/projects/2736/?lang=et (projet-type, 3 niveaux indiqués)
- https://photobuildings.com/object/80424/?lang=ru (Energetik de Pripiat, fiche bâtiment: 2 niveaux indiqués)

## Conclusion technique

L'ancien blockout ne correspond pas à l'emprise et aux volumes de la variante de projet-type détaillée. **Ne pas prétendre à une fidélité architecturale complète.** Il est nécessaire de reconfigurer longitudinalement les volumes salle + scène et d'établir l'emprise exacte de la variante Pripiat.

Une simple modification des dimensions du rectangle extérieur déformerait le raccordement avec les escaliers, fresque, scène et accès. Il faut d'abord :
1. Identifier, sur un plan de Pripiat, les points physiques du bâtiment et l'axe principal.
2. Identifier la variante construite du projet-type (modifications de plan et hauteur technique).
3. Relever une vraie coupe longitudinale scène–auditorium–foyer.
4. Corriger la topologie des volumes, puis seulement les sièges, les étages et la toiture.

## Ce qui est conservé sans perte lors de la mise aux cotes

La nouvelle géométrie photo-guidée du hall / arcades, le maillage texturé `energetikFresco`, les assises existantes et la palette PBR restent des **assets réutilisables**. Leur placement doit à terme dépendre des axes de plan, et non de positions isolées dans `makeScene()`.

## Qualification

- Tests d'intégrité/absence d'occlusion **≠** preuve de dimensions historiques.
- Les captures automatisées permettent le contrôle des volumes, pas une mesure sur photo.
- Les plans cotés détaillés disponibles à l'achat ne sont pas présents dans les fichiers utilisateur et **n'ont pas été consultés**.
