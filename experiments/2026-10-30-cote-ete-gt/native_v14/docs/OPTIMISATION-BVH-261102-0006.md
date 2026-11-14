# Optimisation BVH et intersections — build 261102-0006

## Mesure de départ

La capture du benchmark de la scène Rapture sert de référence avant modification : 207 661 objets, 148 203 triangles, Full HD, trois répétitions mesurées après échauffement.

| Threads | Rendu médian | Post | Total | Gain |
|---:|---:|---:|---:|---:|
| 1 | 20 990,6 ms | 150,7 ms | 21 144,6 ms | 1,00× |
| 2 | 11 248,9 ms | 85,8 ms | 11 327,4 ms | 1,87× |
| 4 | 6 052,8 ms | 49,8 ms | 6 102,6 ms | 3,46× |
| 8 | 5 932,3 ms | 50,9 ms | 5 983,2 ms | 3,53× |

La passe d’ombres, mesurée séparément, prend 1 744,2 ms. Le compteur indique 4 199 125 rayons et 35 555 427 tests d’objets BVH, soit environ 8,47 tests par rayon. Passer de quatre à huit threads ne réduit le temps total que d’environ 2 % sur cette machine.

## Changement appliqué

À l’ouverture de la scène, `loadScene` prépare pour chaque triangle ses deux arêtes et les constantes géométriques du calcul barycentrique. Ces valeurs sont conservées dans des champs déjà présents et inutilisés pour ce type d’objet. Le fichier V3/V4 et la taille mémoire de `Obj` restent inchangés.

Le parcours BVH réutilise les arêtes préparées pour les tests triangle. Le calcul de normale et l’interpolation UV réutilisent également les constantes barycentriques. Un triangle dégénéré ou construit sans préparation garde le chemin de calcul de secours.

Cette optimisation vise le volume élevé de tests d’intersection observé dans le benchmark. **Le gain après modification n’est pas encore mesuré** : la comparaison doit être relancée avec la même scène, résolution, qualité, caméra, heure et sélection de threads. Les temps d’ombres et l’ombrage des sources locales ne sont pas modifiés par ce changement.

## Suite de profilage

Comparer les médianes et compteurs du build 0006 au relevé de départ. Si le rendu CPU reste dominant, ajouter un chronométrage optionnel par sous-étape (intersection, ombrage, échantillonnage PBR et lumières) avant d’optimiser ces chemins. La passe d’ombres reste un coût séparé à 1,74 s dans la mesure de départ.
