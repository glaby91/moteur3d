# LumaTrace V17.604 — cache de rebond diffus à un rebond

## Portée

Cette évolution ajoute au module GPU LumaTrace un premier éclairage indirect diffus. Le rendu raster reste le chemin de présentation ; le calcul LumaTrace produit une radiance par triangle, réutilisée par le shader de composition.

## Calcul

- Un rayon d'ombre BVH est lancé vers chaque lumière depuis le centroïde du triangle. Il s'arrête dès qu'il trouve un obstacle.
- Quatre directions cosinus pondérées, déterministes par triangle, échantillonnent un rebond diffus vers une autre surface.
- À l'impact, le calcul réutilise les lumières et le test d'ombre existants, puis applique les albédos moyens des deux triangles.
- La sortie est conservée dans le buffer structuré GPU LumaTrace, avec une radiance par triangle. Ce n'est pas une lightmap UV : cela évite d'imposer des UV2 et des atlas à toutes les ressources. Le dispatch est réutilisé tant que la scène et les paramètres lumineux sont identiques.

## Choix de performance

Le résultat par triangle évite d'imposer des UV2 et des atlas à toutes les ressources de scène. Les quatre rayons sont calculés une fois par état lumineux, puis le rendu courant relit le buffer. Les rayons d'ombre gardent un parcours court avec sortie au premier obstacle ; seul le rayon de rebond recherche l'impact le plus proche.

## Limites connues

Ce premier palier fait un seul rebond diffus. Les albédos sont une approximation par matériau/triangle issue du mip moyen ; les variations de texture fine ne sont pas reproduites. Les objets analytiques et transparents restent hors du backend LumaTrace selon ses contraintes actuelles. Les sources mobiles invalident le cache global et déclenchent un nouveau dispatch ; le recalcul local autour d'une source mobile sera une évolution ultérieure. Les reflets spéculaires et miroirs ne sont pas couverts par ce rebond diffus.

## Validation à faire

La branche doit être compilée sous Windows avec le workflow LumaTrace avant intégration. Comparer le mode actuel et le mode un rebond sur la même scène : temps du premier dispatch, temps des images suivantes, stabilité du cache, fuite d'énergie, ombres portées et artefacts aux arêtes. Aucun résultat de compilation ou de rendu n'est revendiqué dans ce commit.
