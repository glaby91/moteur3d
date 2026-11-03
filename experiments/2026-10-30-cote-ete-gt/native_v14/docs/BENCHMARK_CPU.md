# Benchmark CPU du moteur natif

La fenêtre **Outils → Benchmark du moteur** sert à repérer la partie qui mérite une optimisation avant de modifier le moteur.

## Mesures

- **Rendu CPU** : temps du rendu des rayons, incluant parcours BVH, intersections, ombrage, textures, éclairage et réflexions.
- **Post-traitement** : redimensionnement et AA contours, selon les réglages actifs.
- **Construction des ombres** : passe unique d’initialisation, affichée séparément du temps par image.
- **Compteurs de charge** : rayons, tests d’objets BVH, nœuds BVH, tests AABB, impacts et lectures de texels.
- **Gain de parallélisme** : comparaison du temps total médian entre la plus petite et la plus grande configuration de threads sélectionnée.
- **Comparaison SIMD** : choix du mode courant, x64 standard, AVX2 + FMA3 ou comparaison complète des deux; chaque mode suit exactement la même série de threads.
- **Barres de débit relatif** : chaque barre indique le facteur de performance par rapport au x64 standard avec le plus petit nombre de threads mesuré. La colonne « Gain SIMD » compare AVX2 + FMA3 au x64 standard à nombre de threads identique.

Chaque configuration passe d’abord par un échauffement, exclu des résultats, puis par 1 à 7 répétitions. Le tableau utilise la médiane statistique (moyenne des deux valeurs centrales si le nombre de répétitions est pair). Les mesures se font dans un buffer séparé et ne remplacent pas l’image de la fenêtre principale. Les longues séries peuvent être annulées; les résultats déjà mesurés sont conservés.

## Lire le diagnostic

- Une part élevée de **post-traitement** dirige l’analyse vers le redimensionnement ou l’AA contours.
- Un **rendu CPU** dominant dirige l’analyse vers le parcours BVH, les intersections, l’ombrage, les textures et l’éclairage.
- Un gain faible quand le nombre de threads augmente indique qu’il faut chercher une charge sérielle, un coût de synchronisation ou une saturation mémoire avant d’ajouter des threads.
- Le ratio **tests BVH / rayons** permet de comparer le coût de recherche entre scènes et versions. Les compteurs seuls ne désignent pas automatiquement la fonction fautive : ils servent à orienter le prochain profilage ciblé.

L’export CSV conserve la scène, le build, le système et l’architecture, le nombre de processeurs logiques, la disponibilité AVX2/FMA3, le mode choisi, le nombre de répétitions, les réglages, la résolution, les mesures, le débit relatif et les compteurs.

## Limites actuelles

Le temps « rendu CPU » est encore agrégé : l’exécutable ne chronomètre pas séparément intersection, ombrage et échantillonnage de texture. Le diagnostic indique donc quelle grande étape attaquer; un profilage interne plus fin pourra ensuite départager ces sous-fonctions sans pénaliser toutes les mesures de référence.


Le benchmark transmet au rendu le mode sélectionné et peut lancer x64 standard et AVX2 + FMA3 dans une seule série. Les résultats sont affichés en valeurs et en barres relatives; les médianes de rendu, de post-traitement et le temps de construction des ombres restent distincts.


## Comparer les modes SIMD

Le mode AVX2 + FMA3 est sélectionné uniquement si le processeur et le système d'exploitation exposent les deux capacités. Le benchmark transmet le mode sélectionné au rendu. Pour comparer les modes, garder identiques la scène, la résolution, les effets, les threads et les répétitions, puis alterner les modes afin de limiter l'effet de la chauffe et de la variation de fréquence. Comparer les médianes, pas un seul rendu.

Dans le build 261103-0015, les calculs vectoriels de rayons, triangles, éclairage et post-traitement utilisent FMA3. Le test AABB des deux enfants BVH reste scalaire : la version AVX2 précédente vectorisait seulement deux boîtes et réextrayait aussitôt les valeurs, ce qui a un coût probable à chaque nœud, mais n'a pas été mesuré isolément. Le ralentissement global a été mesuré sur i5-7200U. Cette correction ne présume pas d'un gain; le résultat doit être confirmé par la comparaison sur le processeur cible.


## Environnement sous-marin de Rapture

Le build 261103-0016 donne à la scène Rapture un fond d'eau profonde bleu pétrole, indépendant de l'heure affichée, et ajoute des reliefs abyssaux lointains derrière la ville. Le ciel reste inchangé dans les autres scènes. Le profil est activé par la présence du matériau propre à Rapture, sans modifier le format des fichiers de scène.


## Diagnostiquer l’activité AVX2

La comparaison complète présente, pour chaque configuration, les paquets de 8 rayons générés, les paquets de 8 triangles effectivement traités par le noyau AVX2, les échantillons de texture interpolés par ce noyau et les blocs de 8 lumières traités. Le CSV conserve les mêmes compteurs.

Ces nombres indiquent si le chemin vectorisé est réellement emprunté; ils ne mesurent pas à eux seuls le temps économisé. Par exemple, un nombre faible de paquets triangulaires signale que la structure des feuilles BVH et le mélange des primitives limitent ce noyau. Le benchmark alterne désormais le mode qui passe en premier entre les configurations de threads et d’un lancement à l’autre, afin de réduire le biais thermique et la variation de fréquence.


## Build 261103-0021 — rejet anticipé des boîtes AABB

Le CSV du build 0020 montre que le rendu reste dominant et que le noyau SIMD d’intersection des triangles ne reçoit aucun paquet. Le compteur AABB est élevé. Le build 0021 teste donc une optimisation indépendante du SIMD : l’intersection rayon-boîte met à jour l’intervalle de distance au fil des axes et abandonne immédiatement le test lorsqu’un axe rend la boîte impossible. Cela évite de calculer les intervalles des axes restants pour les boîtes rejetées.

Cette modification conserve l’ordre des calculs par axe, les limites du rayon et le traitement des directions parallèles. Elle ne change ni les données de scène ni le résultat géométrique attendu. **Aucun gain n’est présumé** : le benchmark comparatif x64/AVX2 du même poste doit vérifier les temps de rendu et les compteurs.
