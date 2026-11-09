# Energetik — auditorium libre 11,6 m / volume haut 21 m (étude corrigée)

## Correction des référentiels verticaux

**L'auditorium et la cage de scène sont deux volumes différents. Il ne faut pas additionner leurs hauteurs.**

La description du projet-type `2С-06-9/70` (https://proekt.sx/tip/dk781-detail.html) donne :
- **11,6 m sous dalle** pour le grand auditorium ;
- **21 m pour le volume scénique à gril**, dont la référence verticale et l'altitude de la toiture extérieure ne sont pas entièrement clarifiées dans la notice descriptive.

Selon l'interprétation de travail exprimée par l'utilisateur le 10/11/2026, le modèle utilise **21 m comme hauteur maximale depuis le sol de référence** pour un futur volume scénique. Cela **ne prouve pas** que la hauteur extérieure de l'Energetik réellement construit à Pripiat était précisément 21 m. Cette cote ne doit être ni additionnée aux 11,6 m, ni appliquée à toutes les toitures.

## Géométrie exécutée dans le moteur

La scène conservait auparavant une salle à `floorY=0.38` avec un faux plafond vers `6.35` et une dalle supérieure vers `9.4`, limitant fortement le dégagement intérieur. La nouvelle géométrie impose :

| Référence | Y de travail (m du modèle) |
|---|---:|
| Sol fini de la salle | 0,38 |
| Hauteur libre (différence) | 11,60 |
| Sous-face plafond auditorium | 11,98 |
| Toiture au-dessus de la salle (dalle 0,30 m) | 12,28 |
| Maximum futur volume scénique (cible de travail) | 21,00 |

Modifications :
1. Les dalles intermédiaires des deux niveaux supérieurs sont désormais ouvertes dans l'emprise de l'auditorium ; les bandes de circulations adjacentes restent en place.
2. Un constructeur `m3dBuildEnergetikAuditoriumRoof` dessine de véritables murs supérieurs et une dalle physique à 11,98–12,28 m ; la dalle principale de 9,40 m est **découpée** autour de ce volume.
3. Les poutres/plafonniers du grand auditorium suivent le nouveau repère `hallTop` et ne restent plus figés vers 6 m.
4. Les escaliers, le foyer, la fresque, le gymnase, la galerie Polissya et les autres volumes ne sont pas déplacés par cette correction.
5. `M3D_ENERGETIK_ARCH.stage.highestExteriorStudyY=21` reste **une contrainte enregistrée** sans création d'un faux caisson de 21 m à une position non mesurée.

## Incertitudes restantes

Le dessin actuel de l'auditorium, ses proportions au sol et son emplacement de scène sont **des blockouts existants** qui ne coïncident pas nécessairement avec les 24 × 18 m du projet-type. La correction de hauteur à 11,6 m est distincte d'un recalage planimétrique ; elle ne valide pas les autres dimensions.

Pour positionner la cage de scène réellement visible sur le toit, il faut encore aligner les quatre coins de ce volume sur les photographies aériennes et confirmer les étages de référence. **Pas de déplacement automatique de cette tour** dans ce commit.

## Contrôles

`experiments/pripyat-survey/test-energetik-architecture.mjs` teste :
- `aud.ceilingUnderY - aud.floorY = 11.6` à précision flottante ;
- sous-face et sur-face de toiture, et absence de dalle dans la salle aux Y=6,40 et 10,5 ;
- indépendance du plafond et du maximum du volume scénique à 21 m.

Le workflow Windows `build-moteur3d-v14-qt.yml` compile la scène de travail, exécute les tests et rend ses 18 vues existantes. Une compilation réussie ne prouve pas une correspondance parfaite avec les photographies historiques.
