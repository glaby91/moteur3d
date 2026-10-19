# Moteur 3D — V6, stress-test ×10 du maillage

**19 octobre 2026.** Base : `experiments/2026-10-19-camera-z-v5/index.html`.

## Objectif

Multiplier réellement par dix le nombre de **triangles des objets** dans la même scène, puis relancer le moteur à deux Z-buffers, pour observer la croissance du coût de projection et de rasterisation.

## Géométrie réelle

| Élément | V5 | V6 ×10 |
| --- | ---: | ---: |
| Décor (murs, banc, arbre, rocailles, etc.) | 2 310 | **23 100** |
| Personnage animé | 100 | **1 000** |
| Total objets | 2 410 | **24 100** |
| Sol (deux triangles dédiés) | 2 | 2 |

La subdivision génère exactement 10 triangles coplanaires par triangle parent (3×3, soit 9 triangles, puis bissection d'un des 9). Chaque sous-face conserve la normale et la couleur de la face mère ; l'éclairage emploie son centroïde de référence pour minimiser les différences de teinte artificielles entre les nouveaux triangles. Le nombre annoncé de **24 100** ne comprend pas les deux triangles du sol.

Il s'agit d'un **test de charge de rasterisation**, pas d'une amélioration de la forme des objets : aucun détail supplémentaire n'a été ajouté aux contours, aux courbes ou aux volumes de collision.

## Simulateur

- Version ×10 sélectionnée **au lancement**.
- Résolution par défaut **320×240**, pour limiter le temps de calcul et les pauses. Possibilité de sélectionner 640×480 et 800×600.
- Menu « Maillage » avec ×1 et ×10, sans changement de scène.
- **Z-buffer caméra réel** et **ombre Z directionnelle interpolée unique V4** conservés.
- Conservation des deux anciens modes Canvas pour des comparaisons de rendu.
- Changement de densité : reconstruction complète des caches lumineux, de l'historique d'image et des prévisions de mouvement.
- Test d'aire automatique des sous-triangles et test de profondeur caméra au lancement.

## Tests effectués — simulateur JavaScript à API Canvas simulée

- Construction vérifiée : 23 100 triangles du décor et 1 000 du personnage.
- Conservation de surface sur un triangle de référence : erreur relative d'environ **2,46 × 10⁻¹⁶**.
- Occlusion synthétique : la face proche reste visible dans les deux ordres de dessin.
- Comparaison ×1 contre ×10 à 320×240 avec ombres V4 : **0 pixel de couleur différent sur 76 800** ; différence maximale de profondeur **0** dans ce test. Les détails du décor, la caméra et la lumière étaient identiques.
- Test en mouvement de quatre images à **640×480**, avec caméra, lumière et personnage déplacés : la rasterisation termine sur la scène de 24 100 triangles (24 062 triangles rasterisés à la pose testée, après clipping et rejet des triangles dégénérés).

Mesures indicatives (environnement V8 avec DOM/Canvas simulé, **pas des FPS Firefox ou Chrome**) :

| Conditions | Temps total simulé par image |
| --- | ---: |
| ×1, 320×240, état testé | ~156 ms |
| ×10, 320×240, état testé | ~529 ms |
| ×10, 640×480, après premier rendu | ~582 ms |

Ces durées incluent des surcoûts du simulateur et ne représentent pas la performance réelle du navigateur. **L'objectif du test est d'évaluer le coût géométrique, pas de promettre 60 FPS.** Le ralentissement à ×10 est important et demande une optimisation avant utilisation temps réel.

En test d'indépendance à l'ordre, l'inversion des faces du décor à ×10 redonne **0 pixel différent à 320×240** en désactivant la cache prédictive de luminosité indexée par face, afin de ne pas mélanger des caches correspondant à des indices différents. La visibilité Z n'est donc pas dépendante de l'ordre des primitives dans cette configuration.

## Points à résoudre ensuite

1. Accélérer la projection caméra (transformations de sommets partagés, frustum culling, binning 16×16).
2. Éviter les calculs lumineux identiques pour les 10 sous-faces coplanaires d'une même surface : mutualisation par primitive mathématique.
3. Optimiser les Z-buffers : rejeter rapidement les tuiles complètement occultées et rendre les ombres depuis la géométrie simplifiée quand elle est exactement équivalente.
4. Mesurer la cadence médiane et P95 dans un **véritable navigateur sur le poste utilisateur**, en distinguant calcul CPU, copie de framebuffer et présentation.
5. Ensuite, ajouter une vraie complexité de forme aux objets (surfaces courbes) au lieu de subdiviser des triangles plats.

## Anciennes références inchangées

- `../2026-10-19-camera-z-v5/`
- `../2026-10-19-shadow-stabilized-v4/`
