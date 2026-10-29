# V9.2 — mesure N+1 et cible 4K/100 FPS

Date de référence : 2026-10-29  
Branche de travail : `experiment/v9.3-temporal-workers-2026-10-29`  
Base : branche `master`, commit V9.2 `ec7a9778f223a15548b0777d20ec24d2d3e5a475`.

Cette branche conserve V9.2 intacte et archive le banc Firefox utilisé pour la première mesure de concurrence. Elle prépare une expérimentation moteur réelle ; elle ne modifie pas encore le rendu V9.2.

## Objectif moteur

Cible retenue : **au moins 100 images/s en 4K**.

- 4K UHD : 3840 × 2160 = 8 294 400 pixels de sortie.
- 100 images/s : budget maximal de 10 ms par image.
- À un échantillon par pixel de sortie : 829,44 millions d’échantillons/s. Une résolution interne réduite avec reconstruction temporelle change ce nombre ; ce débit ne constitue donc pas une exigence fixe de rayons primaires.

Cette cible doit guider conjointement le choix de l’architecture du moteur et celui de la machine. Le PC de mesure actuel ne permet pas de fixer les performances de la machine finale.

## Banc archivé

Le fichier `moteur3d_v92_test_concurrence_workers.html` reprend le banc exécuté dans Firefox. Il mesure le temps du vrai rendu principal V9.2 pendant que des Web Workers exécutent une charge analytique synthétique de candidates N+1.

**Limite essentielle :** les workers ne rendent pas de vraies images V9.2, ne partagent pas la scène du moteur et ne conservent pas de buffers réutilisables. Le banc mesure une concurrence CPU, pas le gain de latence obtenu lorsqu’une vraie image N+1 calculée à l’avance est sélectionnée.

## Série Firefox communiquée le 29 octobre 2026

Réglages visibles : 16 candidates, charge spéculative élevée (96 objets), écart-type de pose faible. Machine : Intel Core i5-7200U, 2 cœurs physiques / 4 processeurs logiques. Le Gestionnaire des tâches a montré le processeur global à 99–100 % pendant les essais avec workers. Plusieurs processus Windows étaient actifs ; cette mesure globale ne permet pas d’attribuer toute la charge à Firefox.

| Workers N+1 | Temps image principale V9.2 | Écart sur 0 worker | Débit candidates N+1 | Erreur candidate |
|---:|---:|---:|---:|---:|
| 0 | 3 758,0 ms | +0,0 % | — | 1,13 σ |
| 1 | 4 428,0 ms | +17,8 % | 0,27 image/s | 0,75 σ |
| 2 | 4 904,0 ms | +30,5 % | 0,62 image/s | 0,91 σ |
| 3 | 5 733,0 ms | +52,6 % | 0,81 image/s | 0,52 σ |

### Lecture des résultats

- Sur cette machine, la concurrence avec les workers ralentit l’image principale, car ils partagent la capacité CPU disponible.
- Le débit de candidates augmente avec le nombre de workers, mais le coût payé par le rendu principal augmente aussi.
- Le passage de 2 à 3 workers augmente le débit N+1 d’environ 31 %, tandis que le rendu principal passe de 4 904 à 5 733 ms.
- Ces résultats ne démontrent pas que le calcul anticipé est inutile. Une candidate réellement calculée et mise en cache peut raccourcir le délai au moment où sa pose est demandée ; le travail est alors déplacé en amont.
- Les erreurs en σ sont celles du générateur de poses synthétique. Elles ne mesurent ni une qualité visuelle réelle ni le bénéfice d’une reconstruction d’image.

La série est indicative, issue d’une seule exécution communiquée par capture d’écran. Elle ne vaut pas comparaison de processeurs et ne prédit pas la machine cible.

## État de la conception

V9.2 est un ray caster CPU JavaScript : primitives mathématiques, BVH, textures RGB565 avec mipmaps, carte de profondeur pour l’unique lumière et un rayon caméra primaire par pixel calculé. Le banc T0 ne répartit pas ce vrai travail sur les workers.

L’anticipation N+1 doit exploiter les données inchangées de la scène N. Le futur prototype devra traiter explicitement :

1. un instantané cohérent de la scène, des matériaux, de l’éclairage et de la BVH ;
2. les paramètres de caméra candidats N+1/N+2 ;
3. la conservation ou le partage des données de scène entre workers, sans reconstruire inutilement la BVH ;
4. la conservation de l’image candidate et de ses données auxiliaires (profondeur, identifiant de surface, paramètres de pose) ;
5. la réutilisation des pixels de N par reprojection seulement lorsqu’ils restent valides ;
6. les trous, occultations et nouvelles surfaces visibles, qui exigent un recalcul ;
7. l’invalidation en cas de mouvement brusque, de modification de scène, de lumière ou de matériaux.

Une expérience antérieure de reprojection temporelle a produit des artefacts et a été désactivée. La reprise devra donc vérifier les occultations et la visibilité, comparer aux images de référence et privilégier une reconstruction graduelle plutôt qu’une réutilisation aveugle.

## Prochain jalon

Construire un prototype mesurant le **vrai coût et le vrai bénéfice** :

- rendre N avec le moteur V9.2 ;
- préparer des candidates N+1/N+2 avec le même moteur et le même instantané de scène ;
- mettre en cache image et profondeur ;
- sélectionner la candidate correspondant à la pose demandée ;
- reprojeter les données encore valides et recalculer les zones non couvertes ;
- mesurer séparément le temps de préparation, le temps d’affichage de la candidate, le temps de correction, la qualité/erreur visuelle, l’impact sur N et la mémoire ;
- comparer 0, 1, 2… workers, puis répéter sur plusieurs machines avant de définir la configuration matérielle cible.

Le banc synthétique de cette archive reste utile comme mesure de saturation, mais ne doit pas servir de critère de sélection final.

## Référence

- Version moteur de départ : V9.2 sur `master`.
- Banc T0 : 29 octobre 2026.
- Document PDF du protocole T0 : conservé avec les livrables du projet.
