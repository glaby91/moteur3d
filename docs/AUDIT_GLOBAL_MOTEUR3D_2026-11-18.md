# Audit global du moteur 3D — 18 novembre 2026

## Synthèse et périmètre

Audit statique du dépôt public `glaby91/moteur3d`, branche par défaut `master`, HEAD observé `ce2b3d3b30b2db74f4e8c4c3ab7a676a6c550ffb`. Le dépôt annonce dans son README une base V17.493 (14 novembre), raster DirectX 11 comme moteur principal et CPU comme comparateur. La branche de travail V17.523 existe séparément (`codex/v17.523-vulkan-optimization-audit`, commit `100b2b23f7d09f07d70fce188ee485e12f032382)); ses changements ne sont donc pas dans master. Les observations d’usage et défauts rapportés par l’utilisateur sont présentés comme retours, pas comme défauts reproduits.

**Verdict : bêta de recherche fonctionnelle, pas moteur stable ni produit validé sur matériel.** Le dépôt conserve plusieurs générations (BASIC, navigateur/JS, C++/Qt natif, scènes et expériences). Le chemin V17 est un assemblage évolutif construit autour du moteur V16. Le principal écart est entre la cible « temps réel GPU direct » et le chemin réellement documenté : readback GPU vers CPU puis affichage Qt; la cible 25 ms/40 FPS n’est pas prouvée sur Intel HD 620.

**Limite importante :** aucun build, test, benchmark, exécution du moteur ou examen pixel par pixel des assets n’a été réalisé dans cet audit, conformément à la consigne. Il s’agit d’un examen documentaire et statique fondé sur le dépôt et les rapports de CI déjà consignés. Les affirmations qui demandent reproduction sont marquées comme telles.

## Architecture observée

- **Historique V1–V13 :** prototypes BASIC et HTML/JavaScript. Le README décrit V13.16 comme une démonstration Canvas/Web Workers, utile comme référence historique, pas comme moteur natif intégré.
- **V14/V16 natif :** C++17/Qt avec backend CPU ray tracing/path tracing, BVH, rendu par tuiles et, selon la documentation, backends raster/compute D3D11. Le portage natif conserve les scènes et les fonctions de la base V16.
- **V17 :** application Qt d’exploration temps réel réutilisant les moteurs V16. Les modes ne sont pas équivalents : CPU est le comparateur/référence et DirectX raster est présenté comme la voie principale. Vulkan est une branche d’expérimentation en cours, pas un backend dont la stabilité est établie dans master.
- **Pipeline et contenus :** code natif, scènes, ressources PBR, données audio, scripts de génération de scènes, expériences et nombreux workflows CI cohabitent dans un même dépôt. Cela conserve l’historique, mais rend la source de vérité et la reproductibilité moins immédiates.

## Constats classés

### Critique pour les objectifs de livraison

**C1 — La présentation DX11 n’est pas encore un chemin GPU→écran direct, selon la documentation V17.**  
`v17/README.md` décrit une lecture des pixels GPU vers le CPU avant affichage Qt, et indique que le rendu direct, le vsync et les 25 ms ne sont pas obtenus. Conséquence : transferts et synchronisations peuvent annuler une partie du bénéfice raster. Mesures WARP en CI ne peuvent pas valider le HD620. À vérifier sur la version et la machine cibles.

**C2 — La cible temps réel reste un objectif, pas un résultat établi.**  
Le README principal qualifie le moteur de bêta, précise que la cadence 25 ms n’est pas vérifiée sur HD620, et le README V17 indique que les temps WARP ne prédisent pas les performances matérielles. Il manque un relevé reproductible sur le PC utilisateur, avec scène, caméra, options et backend gelés.

### Élevé

**E1 — Master et branche V17.523 divergent.**  
Master est au socle V17.493 documenté le 14 novembre; une branche V17.523 est présente mais non fusionnée. L’audit de master ne certifie pas les correctifs Vulkan ni les révisions de scène de cette branche. Éviter de parler de « la dernière build » sans préciser le commit et la branche.

**E2 — Reproductibilité de la scène fragile.**  
`docs/V17_493_MASTER_AUDIT_2026-11-14.md` signale que le workflow principal télécharge la scène approuvée V17.444 depuis un artefact GitHub Actions antérieur (run 43512499128). Après expiration, une compilation depuis un clone propre peut ne plus reproduire le paquet livré. Le rapport recommande déjà une conservation pérenne des ressources et des checksums. Risque prioritaire, confirmé par une documentation de projet mais non réévalué contre le YAML actuel pendant cet audit.

**E3 — Assurance des tests en Release à renforcer.**  
Le rapport de promotion note que huit tests unitaires s’appuient sur 77 `assert()`, désactivables par `NDEBUG`. Les assertions seules ne constituent donc pas un garde-fou fiable si elles disparaissent en Release. Remplacer les vérifications essentielles par des assertions de test toujours actives et faire échouer explicitement le processus de test.

**E4 — Anomalies visuelles et fonctionnelles rapportées mais non reproduites.**  
Le suivi utilisateur récent mentionne points/bruit au sol en compute, différence de luminosité entre CPU et GPU, problème jour/nuit, perte d’image/affichage en Vulkan et baisse de fluidité lors du mouvement. Ce sont des symptômes importants à capturer dans la matrice de validation; cet audit ne peut conclure sur leur présence dans le HEAD master ni leur correction dans 523 sans exécution comparative.

### Moyen

**M1 — Pipeline Vulkan non qualifié pour le matériel cible.**  
Les noms de branches indiquent des travaux sur la durée de vie de présentation et l’optimisation Vulkan, et le dépôt conserve plusieurs expériences Khronos; cela prouve l’existence d’expérimentations, pas une compatibilité stable ni une présentation directe. Il faut distinguer init, chargement scène, rendu immobile, mouvement caméra, resize, fermeture et fallback.

**M2 — Parité visuelle entre backends non garantie.**  
Les documents de projet distinguent le rendu CPU complet du raster DX11 plus simple; l’audit 493 décrit une fresque piscine plus floue en CPU (LOD UV fixe et débruitage GI) que sous DX11. La parité image doit être spécifiée par métriques/tolérances utiles, sans imposer artificiellement l’identité de deux modèles d’éclairage différents.

**M3 — Chargement et mémoire: preuves insuffisantes.**  
Les scènes et artefacts mentionnés dépassent 100 Mo; la scène documentée comporte 186 matériaux, 183 ensembles PBR et 82 images. Aucun profil mémoire, pic pendant chargement, contrôle de libération répétée, ou gestion des échecs partiels n’a été reproduit ici. Les changements scène et fermeture doivent devenir des cas dédiés, car le projet a déjà consigné des risques de restauration/remplacement d’assets.

**M4 — Dette d’organisation du dépôt.**  
De nombreux workflows spécifiques à chaque itération, branches expérimentales et scènes/versionnements coexistent. Le rapport 493 dénombre 34 workflows historiques et relève l’absence d’une licence racine et des attributions d’assets à contrôler. Conserver l’historique, mais marquer clairement workflows actifs, expériences, ressources redistribuables et génération reproductible.

### Bas

**B1 — Le README principal est une bonne carte historique, mais trop vaste.**  
Il mêle état courant, anciennes versions, mesures utilisateur, prototypes et pistes. Cela rend difficile de savoir ce qui est compilé, livré ou seulement envisagé. Maintenir une page courte « état courant / commit testé / limitations / reproduire », puis relier les dossiers historiques.

**B2 — Contrôle géométrique de scène à systématiser.**  
Les retours récents signalent objets qui se croisent, enseigne derrière son support, support sans contact avec le sol, arbres et lampadaires mal placés. Ces défauts relèvent de la qualité des scènes autant que du moteur. Prévoir un diagnostic de collisions/intersections non déformables et d’ancrage des objets, activable en debug, puis revue visuelle multi-caméras.

## CPU, DirectX, Vulkan et compute — niveau de preuve

| Voie | État déclaré dans les documents | Risques ou limites à vérifier |
|---|---|---|
| CPU ray/path tracing | Référence complète/comparateur; BVH et tuiles parallèles hérités V16 | Performances non cibles; LOD/débruitage pouvant différer de DX11; tests de mémoire et de changement de scène |
| DirectX 11 raster | Voie temps réel principale du master | Readback vers CPU/Qt documenté; cadence HD620 non validée; éclairage et matériaux différents du CPU |
| D3D11 compute | Backend V16 conservé pour comparaison; symptômes de bruit signalés par l’utilisateur | Bruit/plages lumineuses à capturer; convergence et qualité sans réglage visuel non mesurées ici |
| Vulkan | Expérimental, branches hors master sur optimisation/presentation | Aucun verdict de stabilité/master; init, swapchain, synchronisation, transfert et cycle de vie à vérifier sur cible |

## Optimisations à privilégier sans sacrifier la qualité

1. Mesurer le pipeline avant optimisation : timings CPU/GPU séparés, temps readback/present, percentiles frame-time (p50/p95/p99), drops et mémoire, même trajectoire et même scène.
2. Priorité structurelle : viser la présentation native de la texture GPU dans Qt et éviter le readback; conserver un chemin de repli explicite si non supporté.
3. Charger les ressources de façon asynchrone et traçable; cache GPU par ressource avec libération déterministe au changement de scène et à la fermeture.
4. Pour le compute : isoler les causes des points/bruit par tests de contrôle (matériaux diffus, ombres, éclairage indirect, profondeur, caméra immobile), puis modifier seulement après mesures de qualité.
5. Construire une scène de référence versionnée et pérenne, checksums pour chaque asset, et manifester les options/caméras afin que les comparaisons ne changent pas en silence.
6. Détecter en mode debug les intersections solides, composants détachés, placements incohérents et matériaux/textures manquants. Ne pas exécuter ce coût dans le rendu utilisateur normal.

## Plan d’action

**P0 — Fixer la référence :** nom de branche + SHA pour 493/master et 523; comparer leurs chemins de rendu, changements Vulkan et chargement avant de publier un état « courant ». Sauvegarder les scènes/assets nécessaires au build, avec manifeste SHA-256 et provenance.

**P1 — Audit de compilation sans modifier le moteur (étape ultérieure autorisée) :** vérifier le workflow master et le dernier workflow 523, matrices de build, CTest Release, artefacts et durées. Ne pas considérer CI WARP comme test matériel.

**P2 — Campagne de validation utilisateur via un lanceur externe :** scène identique, caméras fixes, DX11/Vulkan/compute, temps immobile puis mouvement, changements de scène, fermeture/redémarrage; collecter JSON/CSV avec OS, version moteur, CPU, RAM, GPU/driver, résolution, backend, options et compteurs. Cette campagne est proposée; aucun benchmark n’a été lancé dans cet audit.

**P3 — Revue qualité visuelle/scène :** captures multi-vues pour bruit compute, jour/nuit, coupole approuvée, enseigne/support, arbres/lampadaires et intersections. Garder les critères distincts du throughput.

**P4 — Durcissement :** assertions Release toujours actives, gestion explicite des erreurs et ressources, tests changement de scène/fermeture/répétition, licence et provenance des assets avant diffusion publique.

## Sources internes examinées

- `README.md` (état annoncé V17.493, objectifs et historique).
- `v17/README.md` (architecture V17 et limitation readback DX11).
- `docs/V17_493_MASTER_AUDIT_2026-11-14.md` (résultats CI et risques de promotion).
- Arborescence GitHub master et liste des branches, y compris la branche V17.523.
- Retours utilisateur récents concernant Vulkan, compute, stabilité, géométrie et assets, traités comme symptômes à vérifier.

**Reproductibilité de cet audit :** GitHub API en lecture seule pour récupérer l’état et les documents. Aucun code source n’a été modifié, aucun build/test/benchmark lancé. Les numéros de lignes des fichiers sources ne sont pas cités car l’analyse a principalement porté sur les rapports et README; les liens du dépôt et le SHA de référence donnent le contexte vérifiable. Les constats de pipeline doivent être recontrôlés directement dans le code V17.523 avant toute correction.
