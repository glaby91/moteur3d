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

Deux pistes sont maintenant distinguées : la cache de feuilles de surface (exploratoire) et une représentation plus radicale par régions de visibilité. La piste prioritaire devient la seconde : remplacer la question « quel impact pour chaque pixel ? » par « où la surface visible change-t-elle ? ». Voir [la conception du rendu par frontières](CONCEPTION_RENDERING_PAR_FRONTIERES.md). Le prototype devra traiter explicitement :

1. un instantané cohérent de la scène, des matériaux, de l’éclairage et de la BVH ;
2. les paramètres de caméra candidats N+1/N+2 ;
3. la conservation ou le partage des données de scène entre workers, sans reconstruire inutilement la BVH ;
4. la conservation de l’image candidate et de ses données auxiliaires (profondeur, identifiant de surface, paramètres de pose) ;
5. la réutilisation des pixels de N par reprojection seulement lorsqu’ils restent valides ;
6. les trous, occultations et nouvelles surfaces visibles, qui exigent un recalcul ;
7. l’invalidation en cas de mouvement brusque, de modification de scène, de lumière ou de matériaux.

Une expérience antérieure de reprojection temporelle a produit des artefacts et a été désactivée. La reprise devra donc vérifier les occultations et la visibilité, comparer aux images de référence et privilégier une reconstruction graduelle plutôt qu’une réutilisation aveugle.

## Prochain jalon

Prototyper le rendu par événements de visibilité sur une scène réduite (sol + boîte, puis ellipse/cone) et mesurer si la construction des régions coûte moins que les rayons qu’elle évite. La cache de surfaces restera une piste auxiliaire si le test est concluant. La méthode est détaillée dans [CONCEPTION_RENDERING_PAR_FRONTIERES.md](CONCEPTION_RENDERING_PAR_FRONTIERES.md).

Ensuite, mesurer le **vrai coût et le vrai bénéfice** de la réutilisation temporelle :

- rendre N avec le moteur V9.2 ;
- préparer des candidates N+1/N+2 avec le même moteur et le même instantané de scène ;
- mettre en cache image et profondeur ;
- sélectionner la candidate correspondant à la pose demandée ;
- reprojeter les données encore valides et recalculer les zones non couvertes ;
- mesurer séparément le temps de préparation, le temps d’affichage de la candidate, le temps de correction, la qualité/erreur visuelle, l’impact sur N et la mémoire ;
- comparer 0, 1, 2… workers, puis répéter sur plusieurs machines avant de définir la configuration matérielle cible.

Le banc synthétique de cette archive reste utile comme mesure de saturation, mais ne doit pas servir de critère de sélection final.

## Notes de conception et rapport

- [Hypothèse de rendu par événements de visibilité](CONCEPTION_RENDERING_PAR_FRONTIERES.md) — piste prioritaire pour changer la représentation du calcul.
- [Diagnostic PDF des goulots T0](diagnostic_goulots_moteur3d_v92_T0_2026-10-29.pdf).
- [Simulation HTML de la carte de visibilité](moteur3d_v92_simulation_carte_visibilite.html) et [notice PDF T0](notice_simulation_carte_visibilite_V92_T0_2026-10-29.pdf). La section 06 simule maintenant la reconstruction couleur ; voir aussi la [notice PDF de reconstruction](notice_simulation_reconstruction_visibility_V92_T0_2026-10-29.pdf).
- [Enregistrement PDF des résultats de la première exécution](enregistrement_carte_visibilite_V92_T0_2026-10-29.pdf).
- [Piste secondaire des feuilles de surface](CONCEPTION_FEUILLES_SURFACE_PLIEES.md), conservée comme réflexion exploratoire.

## Référence

- Version moteur de départ : V9.2 sur `master`.
- Banc T0 : 29 octobre 2026.
- Document PDF du protocole T0 : conservé avec les livrables du projet.


## Diagnostic des goulots V9.2 à partir des captures moteur

Capture de diagnostic native communiquée : **1920 × 1200**, soit 2 304 000 rayons primaires ; **119 primitives mathématiques** plus le plan infini. Réglages comparés par Rodolphe : bilinéaire sur les trois mesures de précision.

| Compteur | Valeur observée | Lecture |
|---|---:|---|
| Image complète | 1 529,3 ms | Temps de référence de cette capture, incluant les pauses de rafraîchissement navigateur |
| Intersections BVH | 377,3 ms | Environ 24,7 % du temps complet |
| Éclairage / texels | 875,2 ms | Environ 57,2 % ; premier poste mesuré |
| Temps hors boucles trace + shading | 276,8 ms | Environ 18,1 % : orchestration, préparation, présentation et pauses navigateur ; ce n’est pas une mesure isolée du Canvas |
| Tests géométriques | 2 524 416 | Environ 1,10 test par rayon : le rejet BVH est déjà efficace dans cette scène |
| Nœuds BVH parcourus | 129 128 | Environ 14,3 visites par tuile 16 × 16 à 1920 × 1200 |
| Intersections réussies | 1 472 482 | 63,9 % des rayons |
| Rayons ciel | 831 518 | 36,1 % des rayons |
| Texels lus | 5 889 928 | Quatre lectures par intersection, cohérent avec l’échantillonnage bilinéaire |
| Évaluations nuages | 83 475 | L’échantillonnage en grille évite une évaluation par rayon ciel |
| PCF complet / évité | 415 677 / 881 224 | La classification accélérée évite déjà de nombreuses évaluations d’ombre |

### Classement provisoire

1. **Éclairage et échantillonnage texture** : 875 ms, soit le poste dominant dans cette capture. Le chemin par pixel fait les calculs de matériau, texture, ombre et brouillard. Optimiser uniquement la BVH ne traitera pas la majorité du temps mesuré.
2. **Nombre de pixels/rayons** : le nombre de rayons reste directement lié à la résolution interne. Dans les captures de Rodolphe, le mode 75 % par axe lance 1 296 000 rayons et termine en 964,7 ms ; le mode 50 % lance 576 000 rayons et termine en 425,4 ms. La baisse de résolution interne est donc le levier déjà démontré le plus net, avec une reconstruction d’image à contrôler visuellement.
3. **Coût des ombres** : le PCF complet concerne 415 677 pixels. Son temps propre n’est pas séparé dans les compteurs ; il faut instrumenter ce poste avant de décider s’il faut réduire ou modifier la qualité d’ombre.
4. **Orchestration et présentation** : environ 277 ms ne sont pas couverts par les compteurs trace/shading. `finishMs` inclut explicitement les pauses entre rafraîchissements du navigateur. Il faut séparer temps CPU actif, attente `requestAnimationFrame`, copie `putImageData` et agrandissement de résolution.
5. **BVH** : elle ne ressort pas comme premier goulot sur cette scène : 377 ms face à 875 ms pour shading/texels, avec environ 1,1 test géométrique moyen par rayon. Elle reste à mesurer sur une scène plus complexe, mais ce n’est pas la priorité de la prochaine optimisation.

### Écart entre séries à résoudre

La capture de diagnostics indique 1 529,3 ms pour l’image native ; la série de concurrence du même jour affiche 3 758 ms sans worker. Ces valeurs diffèrent d’un facteur 2,46. Les paramètres exacts, l’échauffement, la charge Windows, le budget de rafraîchissement et les conditions d’exécution ne sont donc pas encore suffisamment contrôlés pour fusionner les temps absolus. Les pourcentages trace/shading ci-dessus viennent de la capture de diagnostics ; les écarts relatifs entre 0/1/2/3 workers viennent uniquement de la même série worker.

### Ordre d’optimisation retenu

- Instrumenter à part texture, ombres, brouillard, temps CPU actif, attente navigateur et présentation.
- Garder la BVH comme elle est jusqu’à preuve qu’elle devient dominante sur des scènes plus lourdes.
- Prototyper l’anticipation et la réutilisation N→N+1 avec l’image réelle, ses identifiants de surface et ses profondeurs.
- Mesurer le taux de pixels réutilisés, les trous/occlusions recalculés, le coût de correction et la qualité visuelle.
- Comparer les modes natif, 75 % et 50 % avec la même scène et les mêmes réglages ; ensuite seulement sélectionner l’architecture matérielle pour 4K/100 FPS.

Les temps et ratios de cette section sont des observations de captures, non des benchmarks reproductibles instrumentés sur la machine cible.


## Première exécution de la carte — capture Firefox

Réglage : image 1920 × 1200, tuile 16 × 16, tolérance profondeur 2 cm, référence complète de 2 304 000 rayons.

| Mesure | Résultat |
|---|---:|
| Tuiles stables | 6 527 / 9 000 |
| Tuiles frontière/incertaines | 2 473 / 9 000 |
| Pixels proposés stables | 72,5 % (1 670 912 pixels) |
| Faux stables contre la référence complète | 36 154 pixels |
| Erreur maximale de profondeur dans les tuiles stables | 0,3470 m |
| Budget théorique de rayons selon le modèle de la carte | 665 723 / 2 304 000 |
| Réduction théorique de rayons | 71,1 % |
| Durée affichée par la première simulation | 14,1 ms |

### Interprétation et correction de l’outil

La carte trouve de grandes régions uniformes, mais les 36 154 faux stables et l’écart maximal de 0,347 m montrent que le réglage 16 × 16 / 2 cm laisse passer des erreurs. La réduction de 71,1 % concerne un **compte théorique de rayons**, pas une accélération mesurée. Elle n’inclut ni la génération des régions, ni le remplissage/couleur, ni les artefacts visuels.

La première durée de 14,1 ms mélangeait classement des tuiles et validation exhaustive contre le rendu complet. Le simulateur a été corrigé pour afficher séparément ces deux durées ; la validation exhaustive sert à mesurer les faux stables et ne représente pas le coût du renderer envisagé.

Prochain essai conseillé : **8 × 8 et tolérance 5 mm**, puis comparer faux stables, surface stable et coût de classement. Une carte ne sera jugée exploitable qu’après mesure de l’erreur couleur, pas seulement profondeur.


## Deuxième exécution de la carte — capture Firefox

Réglage : image 1920 × 1200, tuile 8 × 8, tolérance profondeur 5 mm, référence complète de 2 304 000 rayons.

| Mesure | Essai 1 — 16 × 16 / 2 cm | Essai 2 — 8 × 8 / 5 mm |
|---|---:|---:|
| Tuiles stables | 6 527 / 9 000 | 17 657 / 36 000 |
| Tuiles frontière/incertaines | 2 473 / 9 000 | 18 343 / 36 000 |
| Pixels proposés stables | 1 670 912 (72,5 %) | 1 130 048 (49,0 %) |
| Faux stables vérifiés | 36 154 | 6 839 |
| Faux stables / pixels proposés | 2,16 % | 0,61 % |
| Erreur maximale de profondeur stable | 0,3470 m | 0,1031 m |
| Rayons estimés | 665 723 (71,1 % évités) | 1 262 237 (45,2 % évités) |
| Durée affichée | 14,1 ms | 19,0 puis 14,9 ms (2 exécutions) |

Les faux stables diminuent de 81,1 % et leur part dans la surface proposée passe de 2,16 % à 0,61 %. La surface proposée baisse de 72,5 % à 49,0 % ; le gain théorique de rayons évités passe de 71,1 % à 45,2 %. L’erreur maximale de profondeur reste de 0,1031 m, environ 20,6 fois la tolérance choisie.

Les deux exécutions de l’essai 2 affichent 19,0 ms puis 14,9 ms (moyenne observée : 16,95 ms ; écart : 4,1 ms). L’ancien chrono global ne sépare pas le classement du contrôle exhaustif ; ces temps ne décrivent donc pas le coût du renderer envisagé. La dernière version HTML sépare classement, validation exhaustive et durée totale ; relancer l’analyse avec cette version pour relever ces temps distinctement. La validation exhaustive est le contrôle contre l’image complète, pas le coût du renderer envisagé.

Ces résultats restent issus de captures ponctuelles. L’identité de la scène et des autres réglages doit être confirmée pour faire une comparaison stricte, et la qualité couleur/contours n’est pas encore mesurée. Voir [l’enregistrement PDF comparatif essai 2](enregistrement_carte_visibilite_V92_T0_2026-10-29_essai2.pdf).


## Prototype d’image reconstruite par carte de visibilité

La section 06 compare deux candidates à partir d’une carte calculée sur le rendu terminé :

- **Interpolation RGB** : les quatre couleurs de coin sont interpolées avec correction par la sonde centrale. L’image test a montré un lissage marqué des textures.
- **Patch de surface + shader V9.2** : trois coins définissent un plan local ; chaque pixel intersecte ce plan, puis recalcule UV, texture, éclairage et ombres par le shader du moteur.
- Dans les deux modes, les tuiles incertaines recopient les pixels V9.2 de référence. L’outil compare l’erreur couleur et produit une carte d’écart.

Le mode patch teste la qualité du modèle local et le coût de shading sans traversée BVH, avec identifiants et profondeurs de coins issus du rendu complet déjà calculé. Il **ne calcule pas encore une vraie image N+1**. Le repli exact est gratuit ; le temps affiché n’est pas un temps de rendu complet ni une mesure de gain FPS. Ces limites rendent la qualité globale optimiste.

La section 05 reste le contrôle géométrique de stabilité et l’estimation théorique du nombre de rayons. La version V9.2 du renderer est inchangée.


## Première capture du mode patch surface + shader

Capture Firefox de la section 06, image 640 × 400 :

| Indicateur | Résultat |
|---|---:|
| Tuiles stables avec plan local ajusté | 391 tuiles de surface |
| Pixels repris de la référence complète | 125 184 (48,9 %) |
| Surface reconstruite par patch | 130 816 pixels (51,1 %, calcul dérivé) |
| Erreur couleur moyenne, zones stables | 0,06 / 255 |
| Pixels stables avec écart supérieur à 8 | 0,0 % affiché |
| Temps reconstruction + comparaison CPU | 109,0 ms |

La carte d’écart est presque noire et la candidate conserve les textures. Le résultat visuel est très encourageant pour les tuiles stables. Il reste toutefois conditionné par les identifiants et profondeurs issus de l’image complète V9.2 ; près de la moitié de l’image est recopiée gratuitement depuis cette référence. Le temps de 109 ms ne mesure pas un rendu complet ni une vraie image N+1. Les paramètres de taille de tuile et de tolérance ne sont pas visibles sur la capture ; ils restent à confirmer avant répétition. Voir l’[enregistrement PDF T0](enregistrement_reconstruction_patch_visibility_V92_T0_2026-10-29.pdf).



## Essai N→N+1 par reprojection de visibilité

La section 07 du simulateur teste une candidate sans utiliser le rendu complet N+1 pendant sa construction :

1. conserver les identifiants, profondeurs et caméra de l’image N ;
2. déplacer la caméra et reprojeter les impacts de N avec un z-buffer ;
3. re-shader les surfaces reprojetées jugées sûres ;
4. retracer par BVH les trous, bords et zones douteuses avec les rayons de la nouvelle caméra ;
5. sauvegarder la candidate et son temps complet (projection, garde des bords, shading, rayons de correction) ;
6. seulement ensuite, calculer le rendu V9.2 complet et comparer les pixels, profondeurs/couleurs et temps.

Les bords sont écartés de la réutilisation avec une marge calculée selon le déplacement angulaire. Cette garde est une heuristique ; l’image complète postérieure sert à révéler les erreurs restantes, mais ne corrige pas la candidate.

### Mesure préliminaire, ancienne version avec oracle

Une première capture Edge en 640 × 400, mouvement 0,5° et tolérance 2 cm a donné 48,4 % (123 849) de pixels réutilisés, une erreur moyenne 0,84/255 et 151,0 ms de reprojection/validation. Cette ancienne version calculait d’abord le rendu complet N+1, puis s’en servait pour vérifier chaque pixel et recopier gratuitement les 51,6 % restants. Ces chiffres illustrent l’intérêt potentiel, mais ne sont pas la mesure du nouvel algorithme candidate-d’abord.

La nouvelle version vient d’être publiée et n’a pas encore été mesurée par l’utilisateur. Refaire la capture avec le fichier mis à jour ; relever taux de réutilisation, rayons BVH réellement retracés, erreur couleur sur les pixels réutilisés, temps candidat et temps de référence V9.2. Si le temps candidat baisse, répéter à plusieurs angles et résolutions avant de tirer une conclusion, notamment pour 4K/100 FPS.

Cette expérience ne modifie pas le renderer V9.2. Le candidat est construit avant l’image de contrôle ; la comparaison complète arrive après et ne participe pas à son temps mesuré.
