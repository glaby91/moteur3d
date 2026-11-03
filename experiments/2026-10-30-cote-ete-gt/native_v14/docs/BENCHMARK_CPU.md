# Benchmark CPU du moteur natif

La fenêtre **Outils → Benchmark du moteur** sert à repérer la partie qui mérite une optimisation avant de modifier le moteur.

## Mesures

- **Rendu CPU** : temps du rendu des rayons, incluant parcours BVH, intersections, ombrage, textures, éclairage et réflexions.
- **Post-traitement** : redimensionnement et AA contours, selon les réglages actifs.
- **Construction des ombres** : passe unique d’initialisation, affichée séparément du temps par image.
- **Compteurs de charge** : rayons, tests d’objets BVH, nœuds BVH, tests AABB, impacts et lectures de texels.
- **Gain de parallélisme** : comparaison du temps total médian entre la plus petite et la plus grande configuration de threads sélectionnée.

Chaque configuration passe d’abord par un échauffement, exclu des résultats, puis par 1 à 7 répétitions. Le tableau utilise la médiane statistique (moyenne des deux valeurs centrales si le nombre de répétitions est pair). Les mesures se font dans un buffer séparé et ne remplacent pas l’image de la fenêtre principale. Les longues séries peuvent être annulées; les résultats déjà mesurés sont conservés.

## Lire le diagnostic

- Une part élevée de **post-traitement** dirige l’analyse vers le redimensionnement ou l’AA contours.
- Un **rendu CPU** dominant dirige l’analyse vers le parcours BVH, les intersections, l’ombrage, les textures et l’éclairage.
- Un gain faible quand le nombre de threads augmente indique qu’il faut chercher une charge sérielle, un coût de synchronisation ou une saturation mémoire avant d’ajouter des threads.
- Le ratio **tests BVH / rayons** permet de comparer le coût de recherche entre scènes et versions. Les compteurs seuls ne désignent pas automatiquement la fonction fautive : ils servent à orienter le prochain profilage ciblé.

L’export CSV conserve la scène, les paramètres, la résolution, le nombre d’échantillons, les mesures et les compteurs afin de comparer plusieurs exécutions.

## Limites actuelles

Le temps « rendu CPU » est encore agrégé : l’exécutable ne chronomètre pas séparément intersection, ombrage et échantillonnage de texture. Le diagnostic indique donc quelle grande étape attaquer; un profilage interne plus fin pourra ensuite départager ces sous-fonctions sans pénaliser toutes les mesures de référence.


Depuis le build 261103-0013, le benchmark transmet au rendu CPU le mode SIMD actuellement choisi dans l’interface. Les comparaisons de threads reflètent ainsi bien ce mode; pour comparer AVX2 et x64 standard, garder la même scène, la même résolution, les mêmes effets, le même nombre de threads et le même protocole de répétitions.


## Comparer les modes SIMD

Le mode AVX2 + FMA3 est sélectionné uniquement si le processeur et le système d'exploitation exposent les deux capacités. Le benchmark transmet le mode sélectionné au rendu. Pour comparer les modes, garder identiques la scène, la résolution, les effets, les threads et les répétitions, puis alterner les modes afin de limiter l'effet de la chauffe et de la variation de fréquence. Comparer les médianes, pas un seul rendu.

Dans le build 261103-0015, les calculs vectoriels de rayons, triangles, éclairage et post-traitement utilisent FMA3. Le test AABB des deux enfants BVH reste scalaire : la version AVX2 précédente vectorisait seulement deux boîtes et réextrayait aussitôt les valeurs, ce qui a un coût probable à chaque nœud, mais n'a pas été mesuré isolément. Le ralentissement global a été mesuré sur i5-7200U. Cette correction ne présume pas d'un gain; le résultat doit être confirmé par la comparaison sur le processeur cible.


## Environnement sous-marin de Rapture

Le build 261103-0016 donne à la scène Rapture un fond d'eau profonde bleu pétrole, indépendant de l'heure affichée, et ajoute des reliefs abyssaux lointains derrière la ville. Le ciel reste inchangé dans les autres scènes. Le profil est activé par la présence du matériau propre à Rapture, sans modifier le format des fichiers de scène.
