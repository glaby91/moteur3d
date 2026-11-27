# CSE WARP — feuille de route CSP 001 / Z-Reflect R001
Date : 27 novembre 2026. Référence : V17.016 build 772, scène V17.629, HD 620. Socle source : 1d3f169e6c15a9f009ef897cdef7e94f0aa2ac4d. Ce document est un registre de travail, pas une validation de compilation. Aucune compilation ou publication de scène autorisée à ce stade.

## Décisions de conception à conserver
- Adaptatif 0/1 OFF par défaut, option manuelle maintenue et mesurable indépendamment.
- Z-Reflect générique : propriétés optiques et géométrie, jamais une liste de noms d'objets comme règle finale. Réflexion != réfraction ; transmission traitée séparément.
- Précompiler hors ligne les données statiques dans le dépôt scène, après application de toutes les corrections, avec le même générateur que le moteur. Versionner formats, dépendances et empreintes. Repli local si absent, incompatible ou corrompu.
- Au chargement : caméras miroir dormantes, identifiants de buffers propres par plan ; allocation GPU différée et réutilisation contrôlée. Pas de calcul miroir tant que non nécessaire.
- Zones = présélection, jamais exclusion absolue : visibilité depuis une zone voisine et contributions lumineuses interzones conservées.
- Thread secondaire événementiel pour les données lumineuses CPU ; instantanés immuables versionnés, publication atomique, aucune attente sur le thread de rendu. Commandes GPU sur le thread propriétaire ; aucun accès concurrent aux données mutables du moteur.
- Développer par fonctions/module indépendants, sans régression des autres backends ni de la qualité. Aucun seuil agressif imposé avant mesure.
- Référence performance : [benchmark complet 29 caméras, 522 fenêtres](../../docs/benchmarks/2026-11-26-v17.016-build772-hd620/README.md). Cible HD 620 40–45 FPS en 720p ; aucune promesse de gain avant mesures.

## Autorité du créateur de scène sur Z-Reflect (nouvelle exigence)
Le tri automatique de CSP001 identifie globalement les candidats optiques et leurs plans, mais il ne décide pas seul de l'autorisation artistique de créer des caméras supplémentaires. Le créateur de scène peut autoriser ou interdire explicitement cette fonctionnalité par objet, avec éventuellement des valeurs héritées du matériau ou de la scène. Une interdiction explicite de l'objet ne doit jamais être annulée par l'heuristique du moteur. Une autorisation permet le calcul mais ne force pas une passe GPU lorsque la surface est invisible ou non compatible avec la méthode plane.

### Contrat de données proposé (à implémenter, noms provisoires)
- `planarReflection`: `disabled` (pas de caméra dédiée), `enabled` (autorisation sous réserve de compatibilité), `auto` (le créateur délègue explicitement la sélection au moteur).
- `visibleInPlanarReflections`: contrôle distinct permettant d'inclure/exclure un objet du contenu rendu par les caméras miroir, sans modifier sa visibilité dans la caméra principale. Valeur par défaut proposée : true.
- `refraction`: contrôle optique distinct si nécessaire ; désactiver la caméra miroir ne doit pas supprimer la transmission, la réfraction, les reflets PBR ordinaires, l'éclairage ou les ombres.

Priorité proposée : objet > matériau > politique de scène. En l'absence d'une décision explicite, inventaire automatique mais pas de nouvelle caméra dédiée ; conserver le comportement historique de l'eau pour les anciennes scènes sans politique déclarée. La migration vers une scène dotée d'une politique explicite doit respecter les refus de son auteur. Le choix par défaut pour les nouvelles scènes et les noms de champs sont des propositions, non un format déjà pris en charge.

CSP001 conserve le diagnostic des candidats autorisés/refusés, la provenance de la décision, les coefficients optiques et les associations de plans. Une surface refusée ne reçoit pas de slot caméra/buffer dédié. Si plusieurs surfaces partagent un plan, une surface interdite ne doit pas échantillonner le buffer partagé même lorsqu'une autre surface l'utilise. La table doit distinguer les destinataires d'un reflet des objets pouvant apparaître dans ce reflet. Les changements de politique invalident les tables et caches concernés, y compris au repli local. Les exclusions de contenu des passes miroir sont des choix artistiques explicites, pas une optimisation automatique.

## Budget de trois caméras miroir simultanées (décision auteur, 27/11/2026)
Limiter à trois plans de réflexion sélectionnés simultanément pour le champ de la caméra principale. Il s'agit des caméras miroir supplémentaires Z-Reflect, pas des rayons de réfraction ni de la caméra principale. La sélection des miroirs est automatique parmi les surfaces autorisées par la politique auteur R10. Aucun passage GPU si aucun plan autorisé n'est potentiellement visible. Les surfaces coplanaires compatibles partagent un slot. Les objets hors zone mais visibles restent candidats.

Proposition de classement à valider : agréger par plan la surface projetée visible et la contribution spéculaire estimée (Fresnel selon l'angle, rugosité, F0 métallique/diélectrique, éventuellement luminance du reflet), avec priorité aux vrais miroirs nets et grandes surfaces réfléchissantes ; ne pas éliminer un plan uniquement parce que son éclairage local est faible. Ordre déterministe en cas d'égalité. Hystérésis et maintien du plan sélectionné si les scores sont voisins, sans bloquer un nouveau plan nettement prioritaire. Le classement doit respecter les interdictions explicites ; ENABLED autorise, ne réserve pas un slot. Les autres plans utilisent le rendu PBR/sondes de secours (pas de texture miroir ancienne projetée incorrectement). Les caches valides réduisent les nouveaux rendus mais ne permettent pas d'excéder le plafond de trois plans sélectionnés. Un cache dormant supplémentaire peut subsister dans un pool borné sans être échantillonné. Mesurer la qualité des transitions entre plans et les cas de plus de trois candidats ; un reflet exact pour tous les plans visibles n'est pas garanti avec ce budget.

## Réduction drastique du catalogue Z-Reflect (proposition à valider)
Le diagnostic V17.605 (archive source de V17.629, pas le ZIP final) a recensé 1083 objets candidats sur 5260, dont 536 montants métalliques et 206 éléments en acier peint. Ce tri optique est trop permissif pour constituer un registre de caméras. Distinguer inventaire brut, surfaces autorisées, plans éligibles et trois plans actifs au maximum.
- DISABLED : jamais de slot ; ENABLED : autorisation explicite, géométrie compatible et sélection par budget ; AUTO : critères restrictifs, pas d'activation par métallicité ou IOR seuls. Les vrais miroirs sont reconnus via une propriété optique explicite, pas par nom d'objet.
- Seuils de départ proposés pour AUTO (non validés) : surface plane continue >= 1 m² et rugosité réelle <= 0,15 ; eau, vitrage, miroir ou finition fortement polie. Une surface explicitement autorisée peut déroger aux seuils de taille et de rugosité, mais pas à la compatibilité géométrique ni au budget. Ne pas confondre roughnessScale et rugosité réelle.
- Regrouper seulement les surfaces coplanaires compatibles ; distinguer continuité physique et partage de texture. Ne pas additionner arbitrairement des milliers de petits éléments décoratifs disjoints pour franchir le seuil de surface. Exclure automatiquement montants, acier peint, petites pièces courbes ou décoratives du registre de caméras sauf autorisation explicite compatible.
- À l'exécution : seuil initial proposé de 2 % de surface projetée pour les plans AUTO (mesurer et ajuster, hystérésis), puis classement Fresnel/contribution et sélection de 0 à 3 plans. Préserver les surfaces explicitement autorisées et les vrais miroirs significatifs dans le classement. Repli PBR pour les autres.
- Objectif proposé : quelques grandes familles optiques par zone, pas des centaines de slots. Ne pas annoncer de nombre final avant analyse de V17.629 et clustering réel. Mesurer séparément les comptages à chaque filtre ; vérifier la qualité et les transitions. Les seuils ne sont pas des exigences définitives.
Travaux à rattacher à R01, R02, R08 et R10, sans créer de nouvelle tâche ni changer le code à ce stade.

## Essai R001 catalogue révision 2 — sans plafond actif
Le plafond futur de trois est conservé dans la politique, mais non appliqué dans le diagnostic actuel. Nouveau filtre sur la rugosité nominale source (pas une mesure des mips), surface minimale et géométrie de plaque mince : épaisseur <= 0,15 m, <= 15 % de la plus petite dimension latérale, au moins 90 % de l'aire totale parallèle au plan dominant. Ces trois seuils géométriques supplémentaires sont des choix expérimentaux. Chaque face large admissible est enregistrée, seule la face supérieure de l'eau est retenue ; les faces avant/arrière des vitrages restent distinctes. Les surfaces réellement planes de moins de 1 m² peuvent être explicitement autorisées. Les objets courbes restent en PBR. Regroupement par plan et côté, tolérance 1 mm, recherche des cases de quantification voisines pour éviter la duplication des plans à une frontière d’arrondi. Diagnostic par caméra 1280x720 : faces côté observateur, triangles découpés par frustum et somme des aires projetées, seuil 2 % sans occlusion, sans plafond. Les objets sans décision auteur sont analysés hypothétiquement ; pas d'autorisation effective de nouvelles caméras. Aucun shader multi-plan, buffer GPU ou comportement runtime nouveau : R03–R09 restent ouverts. Contrôle auteur partiel au catalogue ; politique complète R10 reste ouverte.
 
Résultats chiffrés et limites : [simulation du catalogue restrictif](CSP001_ZREFLECT_FILTER_SIMULATION.md).

## État du socle (code écrit, pas compilé)
| ID | Fonction | État réel |
|---|---|---|
| S01 | BVH2, ordre des primitives, BVH4 dans un sidecar binaire | Implémenté en source |
| S02 | Lecture transactionnelle, SHA-256, validation structurelle, fallback, M3D_CSP001=0 | Implémenté en source |
| S03 | CLI cse_scene_precompiler et raccord optionnel au ZIP scène | Implémenté en source, pas de workflow de production |
| S04 | Rejet hors champ du miroir horizontal existant | Implémenté en source ; pas de culling par occlusion |
| S05 | Inventaire optique restrictif révision 2, plans, identifiants dormants, simulation des vues sans plafond | Prototype diagnostic non consommé par le rendu |
| S06 | Smoke test C++ roundtrip/corruption/visibilité | Écrit, non exécuté |
Les simulations indépendantes (100 000 AABB, 10 scénarios BVH) ne valident pas le C++ compilé ni la scène réelle. Aucun fichier de tables pour V17.629 n'a encore été produit.

## Registre des travaux restants
Priorités proposées P0 = validation indispensable, P1 = étape suivante, P2 = optimisation ultérieure. Tous les éléments ci-dessous restent ouverts ; leur ordre est une proposition technique à valider.

| ID | Priorité | Travail restant | Dépendances | Critère de clôture |
|---|---|---|---|---|
| V01 | P0 | Compiler le socle et le CLI avec Qt/MSVC ; exécuter smoke tests, contrôler exports DLL et plugins JPEG | S01–S06 | Build et tests réels OK ; aucune erreur de chargement |
| V02 | P0 | Générer les sidecars sur V17.629 et comparer cache hit / miss / corruption ; profiler lecture, hash, validation, reconstruction et pic RAM | V01 | Parité géométrie/BVH et rendu ; mesures de démarrage et taille, repli fiable |
| V03 | P0 | Ajouter un interrupteur A/B miroir historique / frustum, télémétrie GPU isolée et comparaison d'image | V01 | Mesurer passe miroir seule, principale, FPS/P95 ; aucune disparition de reflet |
| C01 | P1 | Intégrer automatiquement CSP dans le workflow scène après patches, pin du commit moteur, manifeste et révision de package reconnue par updater | V01–V02 | ZIP reproductible, installation et mise à jour testées ; ancienne scène toujours chargeable |
| C02 | P1 | Précalcul Geometry Prep : sommets, indices, attributs, variantes layout/LOD ; éviter double stockage inutile | V02 | Parité octet/attributs et image, taille/temps/RAM mesurés |
| C03 | P1 | Précalcul mapping objet→triangle compact, ordre compact, albédo représentatif et bornes des groupes de 256 triangles | V02 | Même IDs/sentinelles, textures et albédo compatibles, classification 0/1 identique |
| C04 | P1 | Tables spatiales et dépendances statiques Z-Light : zones, volumes, lampes par groupe, relations entre zones | V02 | Aucun rejet de source contributrice ; intégration aux modules existants |
| C05 | P2 | Précalcul disposition des sondes, dépendances statiques, éventuellement cache d'éclairage à heures discrètes | C04 | Parité de positions ; interpolation seulement si qualité jour/nuit validée |
| C06 | P2 | Étudier précalcul de géométrie triangulée et PBR/mips compatibles afin de réduire aussi les étapes encore réalisées au démarrage | V02, C02 | Gain net temps téléchargement+chargement+RAM, pas de perte de détail |
| R01 | P1 | Catalogue de production : vraie rugosité (texture/valeur), IOR, F0 RGB métallique/diélectrique, transmission, normales, emplacements, bornes, association objet/plan/zone | S05 | Pas d'objet arbitrairement exclu sur simple réflectivité de face ; données validées |
| R10 | P1 | Contrôle auteur par objet/matériau/scène : autoriser, interdire ou déléguer les caméras miroir ; distinguer participation au rendu miroir et réfraction | R01 | Refus respectés par précompilation, fallback, regroupement coplanaire et shader ; compatibilité scènes anciennes ; tests de politique et d'héritage |
| R02 | P1 | Clustering robuste de plans quelconques, normales et offsets, tolérances, surfaces coplanaires, triangles uniques, surfaces courbes exclues de la méthode plane sauf approximation validée | R01 | Plans horizontaux/verticaux/inclinés corrects ; pas de mélange de plans distincts |
| R03 | P1 | Consommation runtime du catalogue précompilé, validation de dépendances et fallback local | R01–R02, R10, C01 | Table réelle chargée et utilisée, absence/corruption sans crash ni reflet perdu |
| R04 | P1 | Caméras miroir généralisées : réflexion position/orientation de caméra principale sur chaque plan, clipping par plan, projection et échantillonnage cohérents | R03 | Tests géométriques et images sur eau, vitre, miroir vertical et plan incliné |
| R05 | P1 | Gestion des buffers GPU par plan, allocation différée, pool borné, recyclage, invalidation au changement de scène/device | R04 | Pas de fuite, pas de buffer périmé, budget mémoire HD 620 vérifié |
| R06 | P1 | Shader multi-plan : sélectionner le bon reflet par surface ; appliquer Fresnel, rugosité, métal et transmission dans le matériau, pas dans la texture commune | R04–R05 | Reflets indépendants corrects, coplanaires mutualisés, eau existante préservée |
| R07 | P1 | Z-Reflect Spatial/Frustum : sélection conservatrice par zone et caméra, tous formats d'écran, surfaces visibles depuis zones voisines | R03–R06 | Tests limites frustum et caméra en mouvement ; zéro faux rejet dans cas de référence |
| R08 | P1 | Z-Reflect Depth/Optical : sélection automatique des trois meilleurs plans autorisés au maximum, contribution projetée et Fresnel, résolution adaptative, hystérésis ; occlusion fiable en optimisation ultérieure | R07, R10, V03 | 0 à 3 plans sélectionnés et au plus 3 passes miroir par image ; test de 0/1/2/3/4+ candidats, coplanaires, interdictions, transitions stables et repli PBR ; coût GPU et qualité comparés |
| L01 | P1 | Table lumineuse statique : soleil/ciel, lampes et dépendances de zones/plans ; ne pas assimiler zone du miroir à zone des objets reflétés | C04, R03 | Dépendances conservatrices, validation lampes extérieures/intérieures et objets réfléchis |
| L02 | P1 | Gestionnaire lumière dynamique CPU : événements heure/soleil/ciel/lampe/intensité/couleur, recalcul ciblé, versions et snapshots immuables | L01 | Mise à jour cohérente sans accès concurrent aux globals ni blocage du rendu |
| L03 | P1 | Thread secondaire : file coalescente bornée, annulation au changement de scène, cadence/priorité mesurées, publication atomique, repli conservatif | L02 | Tests transitions et reload, aucune course ni deadlock ; coût CPU mesuré sur 2c/4t |
| R09 | P1 | Z-Reflect Cache : états DORMANT/CACHED/DIRTY/ACTIVE, clé caméra/plan/éclairage/géométrie/matériaux/résolution, mise à jour lorsque nécessaire | R05–R07, L02–L03 | Caméra immobile + scène stable réutilise image ; mouvement ou lumière invalide ; aucune image périmée |
| L04 | P2 | Mutualiser les données lumineuses entre Z-Reflect, Z-Light, sondes et adaptatif 0/1 ; séparer clés radiance locale/sondes | C03–C05, L03 | Invalidation correcte, test A/B/C (OFF / compute seul / compute+raster) |
| V04 | P0 avant release | Validation intégrée, campagne 29 caméras, scènes sans eau/avec plusieurs plans, jour/nuit, autres backends, fallback | Étapes livrées | Qualité CPU GI de référence, images comparées, FPS/P95, temps démarrage, RAM/VRAM, taille ZIP ; aucune régression non acceptée |

## Informations à tracer
- Format indépendant CSP/optique, générateur, SHA du moteur et de la scène, empreintes par dépendance, taille brute/compressée, compteurs d'objets/plans/triangles.
- Cache hit/miss et motif, temps de génération/lecture/hash/validation, allocations et pics mémoire. Cap actuel BVH 128 Mio : ne pas l'augmenter sans mesures.
- Caméras candidates/visibles/dormantes, rendues/réutilisées, raisons d'invalidation, temps GPU par passe, nombre de draw calls, cadence/coût thread secondaire.
- Les 522 fenêtres du benchmark historique ne sont pas indépendantes ; reproduire conditions et ordre ON/OFF, relever fréquences/températures si possible. VSync OFF. Tester séparément les changements avant campagne finale.

## Ordre de livraison proposé
1. V01–V03 : valider le socle sans ajouter d'autres fonctionnalités.
2. C01–C04, R01, R10, R02–R03 : tables précompilées et lues, politique auteur appliquée avant le rendu multi-plan, sans changement optique non validé.
3. R04–R08 : multi-plan avec buffers, rendu, sélection conservatrice et plafond de trois plans ; occlusion avancée reportée.
4. L01–L03 et R09 : tables lumineuses dynamiques, thread séparé, cache cohérent.
5. Occlusion avancée R08, C05–C06, L04 et V04 : optimisations mesurées puis qualification finale.

À chaque incrément : mettre à jour les IDs, statut, commit, preuve de test et résultat mesuré. Ne jamais marquer « validé » sur une simulation seule. Garder les changements isolés ; aucune fusion ni compilation implicite. La scène préparatoire est sur [feature/csp001-scene-precompile](https://github.com/glaby91/moteur3d-scene/tree/feature/csp001-scene-precompile) (commit 7a7d0aa3346cb097fd4fb7c6a4100a931b3dbee8).
