# Pripyat — contrôle géographique préliminaire

## Points de référence (WGS84, Wikimedia Commons)
| Repère | Latitude | Longitude | Source |
|---|---:|---:|---|
| Palais Energetik | 51°24′24,19″ N | 30°03′23,58″ E | https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat |
| Hôtel Polissya | 51°24′25,74″ N | 30°03′29,63″ E | https://commons.wikimedia.org/wiki/Category:Polissya_hotel_(Pripyat) |

Distance géodésique approximative **126,02 m** (environ 116,57 m est et 47,88 m nord).
Le centre des rectangles provisoires du moteur est séparé d'environ **61,50 m**.
**Ces deux valeurs ne sont pas directement comparables** : les points Wikimedia ne sont pas identifiés comme centres géométriques, et l'orientation des axes de la scène n'est pas encore géoréférencée. L'écart apparent est une alerte, **pas** une preuve que l'échelle doit être multipliée par deux.

## Conséquences
1. Ne pas figer les positions relatives Energetik/Polissya avec le blockout actuel.
2. Identifier les mêmes points physiques sur une vue aérienne ou un plan coté.
3. Calculer une transformation rigide (translation + rotation) depuis au moins deux points de contrôle; trois ou plus pour mesurer les résidus.
4. Comparer les distances de façade à façade, et non uniquement les points de repère.
5. Ne déplacer les bâtiments qu'avec leur géométrie intérieure et les éléments liés, pour éviter de désolidariser fresque, escaliers et collisions.

## État de l'implémentation
Le module `experiments/pripyat-survey/pripyat-survey.mjs` expose `geographicLandmarks`, `geodesicDistanceMetres`, `localTangentMetres` et `landmarkSeparationReport`. Les tests vérifient le calcul et empêchent d'annoncer les points comme comparables.

## Sources architecturales
- Projet-type Energetik **2С-06-9/70** : https://domophoto.ru/building/80424/
- Reconstruction tierce à **81,2 × 24,4 m**, hauteur **11,4 m** / tour **14 m** : https://www.fab.com/listings/39bd11e2-0185-48e4-b75f-f2bf45d5baef
- Esquisses conceptuelles de 1968 : https://www.forgottenchernobyl.net/architectural-sketches-of-pripyat

**Aucun plan cadastral coté n'a encore été exploité** : les dimensions de la place et les emprises des bâtiments restent provisoires.
