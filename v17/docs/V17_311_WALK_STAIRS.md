# CSE WARP V17.311 : personnage en vue subjective, terrain, escaliers et obstacles

Branche indépendante `feature/v17-311-fps-walk-stairs-261111`, issue de V17.310. Cette évolution n'intervient pas dans l'éclairage.

- Marche à hauteur d'homme, caméra à 1,70 m au-dessus des pieds.
- Géométrie BVH de la scène utilisée comme source des collisions.
- Marche automatique jusqu'à 34 cm, descente jusqu'à 48 cm, gravité en l'absence de sol.
- Collision rayons multiples sur torse/tête, rayon de corps ~23 cm, glissement X/Z le long des murs.
- Case « Marcher au sol » (activée par défaut et mémorisée) ou vol libre conservé.
- ZQSD/flèches, souris bouton gauche, Maj rapide. Espace et Ctrl fonctionnent en vol libre.
- Test sans dépendance GPU : sol, 3 marches de 20 cm, descente et mur.

## Limites
Contrôleur par rayons, pas une capsule physique parfaite. Les marches et paliers doivent être présents dans le maillage. Les grandes scènes et escaliers de Pripiat seront à contrôler manuellement. Les objets animés et sauts ne sont pas encore gérés.
La table de GI précalculée par heure demeure un chantier séparé.
