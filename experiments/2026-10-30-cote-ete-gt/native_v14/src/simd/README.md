# SIMD du moteur

Les noyaux AVX2 utilisent les instructions AVX2 et FMA3 sur les lots de rayons primaires, les intersections de triangles, l'éclairage par lots de lumières et le post-traitement. Le mode n'est activé que si le processeur expose AVX2 et FMA, et si le système d'exploitation sauvegarde correctement l'état étendu du processeur.

Le parcours BVH garde ses deux tests AABB en scalaire. L'ancien noyau AVX2 préparait six coordonnées, exécutait les opérations vectorielles puis réextrayait deux résultats scalaires pour chaque nœud visité; ce format ne correspondait pas au parcours actuel, qui traite un rayon à la fois. Un parcours BVH par paquets de rayons permettrait d'exploiter les voies AVX2 plus efficacement, mais demande une refonte distincte.

Les tests communiqués sur i5-7200U indiquaient que le mode AVX2 ralentissait la scène d'environ 30 %. Cette version ajoute FMA3 dans les calculs vectoriels utiles et retire le noyau AABB au format inadapté, dont le coût doit être isolé par un profilage avant d'attribuer précisément le ralentissement global. Le mode x64 standard reste sélectionné par défaut : le gain doit être mesuré sur la même scène et avec les mêmes paramètres avant de changer ce choix.
