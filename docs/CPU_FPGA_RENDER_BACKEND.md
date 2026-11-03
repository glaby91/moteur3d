# Architecture CPU logiciel et évolution vers Z80–FPGA

## Intention

Le moteur garde une implémentation logicielle comme référence visuelle et fonctionnelle. Les calculs 3D sont effectués par le processeur ; aucun pipeline de rendu 3D GPU n’est requis.

À terme, une carte associant un Z80 et un FPGA pourra accélérer certaines étapes. Le Z80 pilotera les commandes et les transferts ; le FPGA pourra traiter en parallèle les tâches choisies. Cette cible ne change pas le chemin d’exécution de référence CPU.

## État actuel : V13.15

La V13.15 introduit une première frontière logicielle de backend dans la page HTML :

- `beginFrame(frameDescriptor)` initialise une session de rendu ;
- `renderTile(frameSession)` exécute une tuile ;
- `finishFrame(frameSession)` clôt la session.

Le backend actif est `cpu-software-reference`. Il appelle le kernel JavaScript CPU existant et conserve la même scène, les mêmes intersections, le même shading et le même format d’image. L’interface indique le backend, le nombre de threads de rendu actifs et la valeur `navigator.hardwareConcurrency` annoncée par le navigateur.

**Cette étape n’ajoute pas encore de parallélisme et ne revendique aucun gain de vitesse.** Dans le navigateur, le moteur de rendu reste sur un seul thread JavaScript. `hardwareConcurrency` est une indication fournie par le navigateur ; ce n’est ni une détection complète du processeur ni une mesure de ses performances. Le calcul multithreadé natif sera une étape distincte.

## Contrat à stabiliser pour les futures backends

L’interface présente est un point de séparation initial. Avant de figer une ABI ou un format binaire pour la carte, définir et versionner les données échangées :

1. **Descripteur de frame** : résolution interne et finale, paramètres caméra, options qualité et numéro de frame.
2. **Paquet de scène** : types de primitive, bornes, paramètres géométriques, matériaux, textures et structure BVH, avec indices et formats numériques explicites.
3. **Tâche de rendu** : index de tuile ou plage de rayons, origine et dimensions.
4. **Résultat** : pixels, identifiants de surface/profondeurs si requis par les diagnostics, compteurs de travail et état de complétion.

Les buffers partagés et le mode de transfert devront être choisis selon la mémoire réelle de la carte. Ne pas imposer prématurément un format fixe aux données JavaScript courantes ; mesurer la taille et le coût de transfert du paquet avant d’arrêter l’interface FPGA.

## Feuille de route

### 1. Référence logicielle déterministe

- Conserver la version CPU comme référence pour l’image et les compteurs.
- Garder la V13.14 Full HD comme comparaison historique.
- Ajouter des scènes et images de référence ; comparer les différences pixel par pixel selon une tolérance publiée.

### 2. Exploitation du CPU dans un exécutable natif

- Porter le kernel de rayons et d’intersections dans un exécutable natif C++ ou Rust, sans rendu 3D GPU.
- Construire un ordonnanceur de tuiles et un pool de threads persistants.
- Détecter les capacités disponibles au lancement et sélectionner un chemin scalaire sûr ou des kernels vectorisés compatibles.
- Mesurer les gains par étape, par nombre de threads et par famille de calcul : BVH/AABB, intersections, textures et ombres.
- Conserver un chemin de secours portable pour les processeurs qui n’ont pas les mêmes extensions.

### 3. Accélération FPGA par étapes

- Commencer par une tâche déterministe et mesurable, probablement une étape de ray generation/intersection ou un traitement de pixels après analyse de profil.
- Comparer le coût du transfert de données avec le temps de calcul économisé ; une accélération n’est utile que si le résultat revient plus vite que le chemin CPU.
- Maintenir une implémentation CPU de chaque tâche accélérée pour la référence, les cartes non compatibles et le diagnostic.
- Faire piloter le chargement, la configuration et les lots de tâches par le processeur hôte/Z80 ; utiliser le FPGA pour les calculs massivement parallèles qui justifient le transfert.

## Mesures à garder comparables

Pour chaque backend, consigner la résolution de sortie **et** la résolution interne, les rayons exécutés, les tests géométriques, les tests AABB, les nœuds BVH parcourus, les texels, les accès d’ombre, les temps par étape et le temps total. Les millisecondes doivent préciser le processeur, le nombre de threads, le navigateur ou l’exécutable et les réglages de rendu.

La mesure utilisateur V13.14 en Full HD équilibré — 1 587 600 rayons, 4 429,6 ms — reste la base logicielle observée sur un poste donné ; elle n’est pas une promesse de performance sur une autre machine.
