# CSE WARP — statut du correctif de transmission de l'eau

Date : 2026-11-25  
Branche : `fix/v17-water-transmission-parity-261125`

## Décision

Le correctif est **approuvé à titre provisoire**. L'estimation actuelle est d'environ **80 % de résolution**, à la louche. Ce chiffre est une appréciation de suivi, pas un taux calculé par le benchmark.

La validation complète de l'harmonisation reste **en suspens**. Le correctif reste isolé sur cette branche ; cette décision ne vaut pas fusion dans la branche officielle.

## Éléments du benchmark

- Les quatre campagnes ont abouti : **24 cas sur 24 marqués `ok`**.
- La variation de transmission modifie bien le rendu GPU, ce qui confirme que la valeur numérique est prise en compte.
- Entre la transmission de référence (0,985) et 0, la luminance moyenne appariée varie de **−2,33 % sur CPU**, **+6,48 % sur DX11**, **+9,71 % sur Vulkan** et **+4,49 % sur LumaTrace**.
- Sur les pixels opaques communs à la capture de référence, LumaTrace mesure **0,280** contre **0,239** pour DX11, soit environ **17 % de plus**. Cette différence globale n'est pas attribuée à ce correctif.
- Vulkan présente un écart à investiguer au passage de transmission **0,01 à 0** : sa variation par rapport à la référence passe de **+4,75 % à +9,71 %**. DX11 et LumaTrace restent presque stables sur ce passage.
- Les temps de rendu sont issus de deux images mesurées par cas ; ils donnent une indication et ne suffisent pas à établir la fluidité en déplacement.

## Suite à donner

1. Garder le correctif comme accepté provisoirement sur cette branche.
2. Ne pas considérer l'harmonisation lumineuse totale comme résolue.
3. Vérifier le chemin Vulkan à transmission nulle, notamment les conditions de seuil et le choix du chemin opaque/transparent.
4. Comparer les luminances dans la seule zone de l'eau avant toute modification générale d'exposition.
5. Pour mesurer le gain réel du patch, faire une comparaison avant/après avec le même protocole et les mêmes captures.

## Limite de conclusion

Le benchmark confirme que la campagne s'exécute et que les backends réagissent à la transmission. En l'absence d'une comparaison pré-patch appariée, il ne permet pas de calculer un pourcentage objectif de correction ni de démontrer à lui seul que 80 % du défaut est résolu.
