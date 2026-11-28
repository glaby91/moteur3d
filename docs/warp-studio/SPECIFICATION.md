# CSE WARP Studio — Spécification fondatrice v1.0

**Statut :** conception fonctionnelle approuvée ; architecture à confronter au code existant.  
**Date :** 2026-11-28.  
**Périmètre :** éditeur autonome de scènes pour CSE WARP Engine V17. Ce document ne constitue pas une implémentation.

## 1. Vision et principes

Créer, modifier, valider, optimiser et publier une scène complète sans intervention dans le code C++ ou le JSON à la main. Cibles : architecture, industrie, reconstitutions historiques, visites interactives, simulation et rendu réaliste sur matériel modeste. Ne pas chercher à reproduire intégralement Unity ou Unreal.

- Un seul cœur moteur partagé avec le lecteur et les outils de benchmark ; aucun second moteur de rendu.
- Studio est un exécutable indépendant C++/Qt, pour Windows, Linux et macOS ; Android reste une cible de lecture et de test sur tablettes puissantes, non d'édition complète initialement.
- Scènes et assets séparés du code moteur ; révisions de moteur, d'éditeur, de schéma et de scène indépendantes.
- La conception graphique doit intégrer les coûts de calcul et les autorisations par objet dès la création.
- Développer par petites fonctionnalités autonomes, testables, réversibles ; préserver les workflows existants et les branches expérimentales de rendu.
- Les fonctionnalités proposées doivent être conservées dans la roadmap même si elles ne sont pas disponibles dans le premier MVP.

## 2. Architecture et dépendances

Cible logique : WARP Engine (runtime), WARP Studio (édition) et WARP Benchmark (validation) s'appuient sur WARP Core API. Ce dernier expose Scene Graph, Asset Manager, Material System, Physics, Lighting, Animation, Spatial Index, Render Manager, Diagnostics. Les backends DX11, Vulkan, GPU Compute, CPU PBR/GI, LumaTrace et Escargot sont accessibles selon les capacités effectivement disponibles ; Escargot/LumaTrace peuvent rester des modes internes et ne sont pas présumés des backends indépendants.

L'éditeur consomme une version identifiée du moteur (commit/tag exact), via une API stable, jamais des accès arbitraires aux données privées du renderer. Le format de scène et les assets restent compatibles avec le lecteur. Avant toute extraction de bibliothèque, inventorier les dépendances réelles du moteur et faire des adaptateurs progressifs. Ne pas imposer une réécriture générale.

Dépôts cibles :
- moteur3d : cœur, algorithmes, application runtime et benchmarks existants.
- warp-studio : interface, commandes d'édition, services, tests et packaging propres.
- dépôt de scènes existant : scènes, ressources et versions de scène.
- warp-assets et warp-sdk : éventuels dépôts dédiés à terme, sans obligation de création immédiate.

## 3. Règles impératives de développement

### 3.1 Fonctions simples et responsabilités
- Une fonction = une action identifiable et un seul niveau d'abstraction. Privilégier entrées/sorties explicites, noms verbaux, const-correctness et types forts.
- Décomposer les gros traitements en étapes indépendantes. Une fonction de 20 à 50 lignes et une complexité cyclomatique <= 10 sont des repères de revue, pas des quotas artificiels ; documenter les exceptions.
- Séparer orchestration, calcul pur, accès aux données, effets de bord, UI et appels GPU. Aucun calcul de géométrie, parsing ou I/O lourd dans un callback Qt.
- Éviter fonctions monolithiques, classes « god object », variables globales mutables, dépendances circulaires, copier-coller et booléens ambigus.
- Pas de fragmentation gratuite : extraire une fonction lorsqu'elle exprime un concept, réduit la complexité ou rend une étape testable.
- Gestion des ressources C++ par RAII, ownership explicite, durée de vie des objets Qt documentée, pas de pointeur brut propriétaire. Erreurs typées et diagnostics contextualisés.
- Interfaces de services petites, contrat documenté, injection de dépendances là où nécessaire ; pas d'architecture spéculative excessive.
- Préférer les calculs incrémentaux et caches invalidés par les changements réels. Mesurer avant d'optimiser.

### 3.2 Découpage des fichiers
Chaque module possède des interfaces publiques minimales, implémentations privées et tests. Les fichiers sont nommés par responsabilité. Le main ne fait que lancer l'application.

```text
warp-studio/
  CMakeLists.txt
  src/
    application/       # bootstrap, configuration, lifecycle
    ui/                # main window, docks, actions, layouts
    core_adapter/      # interface stable avec moteur
    scene/             # modèle documentaire, IDs, groupes
    commands/          # undo/redo, transactions
    viewport/          # caméra, picking, gizmos, overlays
    hierarchy/
    inspector/
    geometry/
    assets/
    materials/
    lighting/
    environment/
    physics/
    animation/
    optimization/
    validation/
    profiling/
    serialization/
    jobs/              # tâches annulables, progression
    publishing/
    plugins/
  tests/{unit,integration,regression,rendering}/
  resources/
  documentation/
  .github/workflows/
```

Exemple de découpage logique (noms indicatifs, API non existante) : moveSelectedObjects orchestre readSelection, validateTransform, computeTransform, buildTransformCommand, applyCommand ; ensuite le changement émet un ChangeSet et le moteur décide des invalidations GPU. L'UI ne modifie pas directement les buffers.

Dépendances : UI -> application/commands -> scene model/services -> core adapter -> engine. Les couches basses n'importent pas l'UI. Les commandes modifient le modèle, pas les widgets. La sérialisation est indépendante du viewport. Le modèle ne contient pas de handles GPU.

### 3.3 Concurrence et actualisation
UI sur le thread principal ; calculs CPU lourds et I/O dans des tâches annulables ; uploads GPU et ressources selon règles du backend. Publier résultats immuables ou appliquer transactions à un point sûr. Éviter attentes bloquantes, files sans limite et copies CPU/GPU inutiles. Grouper événements de glisser-déposer, invalider uniquement les ressources concernées, limiter rafraîchissement des vues inactives. Arrêt propre et récupération après erreur.

### 3.4 Qualité
CMake par cibles, tests automatisés, formatage et analyse statique, warnings examinés, documentation des interfaces et décisions architecturales. Chaque PR a un périmètre cohérent et des critères d'acceptation. Cinq passes avant intégration : audit du code ; tests fonctionnels ; intégrité ; graphisme/performance ; non-régression. Les résultats effectivement obtenus doivent être distingués des tests proposés ou non exécutés.

## 4. Interface
Fenêtres Qt ancrables, détachables, redimensionnables, multi-écrans, thèmes et configurations de travail enregistrées (architecture, matériaux, éclairage, optimisation, animation). Panneaux : viewport 3D, Scene Explorer, Inspector, Asset Browser, Material Editor, Lighting Editor, Environment Editor, Performance Analyzer, Console, Timeline, Layer Manager, Scene Validator. Recherche, filtres, raccourcis reconfigurables, menus contextuels, sélection synchronisée, actions annulables et état non enregistré visible.

Viewport : perspective, dessus, face, côté, filaire, grille, axes, échelle, cadrage de sélection, navigation libre/orbitale, une/deux/quatre vues. Limiter coût des vues multiples et employer les backends réellement compatibles. Mode édition et mode lecture/test séparés ; en quittant le test, retrouver l'état d'édition.

## 5. Édition et construction
Translation/rotation/échelle locale ou globale, pivots, sélection multiple, gizmos, magnétisme (grille/sommets/surfaces/objets), duplication simple/linéaire/circulaire, instances, groupes parent-enfant, alignement, mesures de distances/angles/surfaces, visibilité, verrouillage. Identifiants persistants ; cycles de parenté interdits.

Primitives paramétriques (cube, sphère, cylindre…), murs et ouvertures, fenêtres, portes, escaliers droits/tournants/hélicoïdaux, poteaux, poutres, dalles, plafonds, toitures, arcades, verrières, routes, trottoirs, bordures et terrains ; extrusion 2D, opérations booléennes, répétition automatique, plans/images de référence. Unité interne cible : mètre ; repère, angles et conventions de conversion à documenter selon le moteur réel. Les générateurs conservent leurs paramètres éditables et produisent des maillages déterministes. CSG complexe à terme.

## 6. Assets
Bibliothèque réutilisable indépendante de la scène, glisser-déposer, miniatures, recherche, tags, dépendances, instances, variantes de matériaux et prefabs. Import GLB/glTF/OBJ ; FBX à étudier. Textures PNG/JPEG/DDS/KTX2/TIFF ; HDR/EXR ; plans SVG/PNG et DXF à étudier ; audio WAV/OGG/MP3. Les formats exacts dépendent des importateurs et capacités validés. Formats natifs WARP Material et WARP Prefab à spécifier.

Pipeline d'import découpé : inspecter fichier, analyser dépendances, convertir repère/unités, classifier matériaux, contrôler maillage, préparer ressources, prévisualiser rapport, appliquer transaction. Contrôles : normales inversées, winding, sommets dupliqués, triangles dégénérés, Z-fighting, UV, textures absentes, orientation/échelle, surfaces vitrées, géométrie excessive, collisions simplifiées et LOD. Corrections proposées et réversibles ; ne pas fusionner automatiquement des sommets aux attributs incompatibles. Sources d'assets conservées ; chemins relatifs et licences/provenance documentées.

Instanciation : géométrie partagée, transformations indépendantes et variantes autorisées. Éviter 200 copies du même arbre ou carreau ; batching/instancing selon capacité renderer.

## 7. Matériaux PBR
Inspecteur et prévisualisation utilisant le moteur lorsque possible : couleur de base, rugosité, métallicité, opacité, IOR, émission, normal/bump, textures multiples, UV, couches et variantes. Presets béton, métal, verre, eau, céramique, émissif. Autorisations réflexion/réfraction/rebonds distinctes du matériau physique. Éditeur nodal à terme ; contrôle des limites par backend, fallback signalé. Toute modification est annulable et n'entraîne pas de reconstruction géométrique inutile.

## 8. Éclairage et environnement
Sources directionnelles (soleil/lune), ponctuelles, spots, rectangulaires, linéaires, surfaces émissives, ambiantes, profils IES à étudier. Intensité, température de couleur, portée, orientation, angle, ombres, priorité, fréquence d'actualisation et budget. Unités photométriques physiques si cohérentes avec le moteur. Afficher volumes d'influence, sources retenues/écartées, éclairage direct/indirect, ombres et sondes. Ciel, cycle jour/nuit, étoiles, météo, exposition ; prévisualiser un instant figé et transitions. Ne pas reconstruire la scène entière en changeant l'heure.

## 9. Politiques de rendu WARP
Paramétrage global, groupe, matériau, objet : Z-Reflect, caméras miroirs et réfraction, LumaTrace, Escargot, Z-Prepass, Z-Light Culling, BVH (inclusion/mise à jour), rebonds, ombres projetées/reçues, transparence, LOD, occlusion, texturing, priorité, comportement statique/dynamique.

Règle : une interdiction explicite applicable est contraignante ; une autorisation ne déclenche pas automatiquement un calcul. Le moteur décide de la sélection en fonction de visibilité, taille projetée, distance, importance, coût, capacité et budget. Les miroirs peuvent être choisis automatiquement. Les caméras de réfraction sont soumises à autorisation du créateur. Budget configurable, la cible historique de trois caméras et les essais à cinq ne sont pas des limites figées avant mesure. Afficher les raisons d'inclusion/exclusion, les coûts et les fallback. Les interdictions ne doivent pas être contournées par une option « forcée ».

## 10. Analyse des performances et optimisation
Modes de diagnostic : Geometry Complexity, Shader Complexity, Overdraw, Light Complexity, Reflection Cost, Refraction Cost, Shadow Cost, Bounce Coverage, BVH, LOD, normales, collisions, Z-buffer, occlusion, caméras supplémentaires. Distinguer estimation et mesure. Coûts objet/matériau/groupe lorsque mesurables ; analyse des draw calls, triangles, RAM/VRAM, caches, transferts et synchronisations.

Budget par image pour 30/45/60/120 FPS ; mesure CPU/GPU distincte avec temps de présentation, pipeline et recouvrement des passes. Éviter de sommer des tâches parallèles. Afficher médiane, p95/p99 frame time, 1% low, pics mémoire, conditions de test et résolution. Comparaison des modes DX11, Vulkan, Compute, CPU PBR/GI, LumaTrace, Escargot sur caméra/heure/résolution/exposition/scène identiques. CPU PBR/GI seulement comme référence après validation ; exporter images, différences, CSV/JSON, version du moteur/scène, GPU/driver et paramètres. Intégration du bench existant sans dupliquer ses mesures.

Grandes scènes : secteurs, étages, cellules, LOD/HLOD, imposteurs, frustum/occlusion culling, streaming, GPU instancing, index spatial, dirty flags, caches et compression. Hystérésis/transition des LOD et seuils de taille projetée pour éviter disparition/réapparition des détails. Chargement progressif et budgets mémoire. Ne pas accepter une optimisation qui introduit une régression visuelle non validée.

## 11. Physique, animations, interactions
Collisions boîte/sphère/capsule/mesh ; gravité, statique/dynamique, navigation, marche/saut/accroupissement, portes, ascenseurs, véhicules et trajectoires. Timeline et keyframes, séquences caméra, événements/trigger de zone, audio spatial, particules (pluie/neige/poussière/fumée), végétation animée. Système de comportements visuels par blocs à terme. Toute capacité runtime absente sera recensée et planifiée, non simulée comme déjà opérationnelle.

## 12. Format de scène, projets et versioning
Préserver le scene.json existant et SCENE_VERSION.json. Auditer chargeur, schéma, références, repère et migrations avant évolution. Format JSON lisible, documenté et versionné ; identifiants persistants, références indépendantes des noms, chemins relatifs, préserver les champs inconnus en aller-retour. Migrations explicites et testées ; sauvegarder de façon atomique, gérer erreurs et incompatibilités, ne pas écraser une source sans validation. Séparer données runtime et état UI. Export portable, contrôle dépendances et empreintes.

Exemple strictement illustratif d'une évolution (non compatible présumée avec le parseur actuel) :

```json
{
  "format": "WARP_SCENE",
  "schemaVersion": 2,
  "sceneVersion": "1.0.0",
  "name": "Pripyat Aquatic Complex",
  "units": "meters",
  "objects": [{
    "id": "glass-facade-001",
    "name": "Verriere principale",
    "type": "mesh",
    "asset": "assets/facade.glb",
    "transform": {"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]},
    "material": "glass_clear_01",
    "renderPolicy": {
      "reflection":"auto","refraction":"allowed","indirectLighting":true,
      "castShadow":true,"receiveShadow":true,"priority":80,"lod":"auto"
    },
    "physics":{"collision":true,"static":true}
  }]
}
```

Projet cible : scene.json, SCENE_VERSION.json, project.warp (métadonnées éditeur), assets/{meshes,prefabs,vegetation}, materials, textures, environments, animations, audio, lighting, benchmarks, thumbnails, validation. Convention exacte à valider. Les données générées et caches sont séparés et ignorés par Git lorsque pertinent.

## 13. Validation et publication
Validateur : fichiers/références/textures manquants, normales, maillage dégénéré, superpositions, objets flottants, collisions, ruptures géométriques, matériaux dupliqués, objets inutilisés, textures lourdes, anomalies lumineuses, compatibilité backend, budget graphique. Sévérité erreur/avertissement/info et lien vers objet, cadrage caméra, raison et correction proposée. Détection géométrique potentiellement ambiguë : ne pas corriger automatiquement une structure volontairement suspendue. Rapport JSON et lisible, contrôle avant publication, package avec manifest et checksums. Comparaison et historique Git ; ne publier ni écraser une scène distante sans action explicite.

## 14. Fiabilité
Undo/redo transactionnel, autosave configurable, récupération après crash, snapshots, sauvegarde atomique, mode sûr avec rendu simplifié, isolation des importateurs risqués, diagnostics horodatés, journal des erreurs, migration. Un glisser-déposer complet constitue une commande ; annuler restaure précisément l'état antérieur. Les tâches lourdes ne bloquent pas l'UI ; annulation et progression. Tester coupure de sauvegarde, asset absent, backend indisponible, scène invalide et reprise de travail.

## 15. Compatibilité et critères de performance
Windows/Linux/macOS : édition complète ciblée et compilation indépendante. DX11 Windows uniquement ; Vulkan Windows/Linux, macOS via couche de compatibilité à vérifier ; CPU PBR/GI et autres modes selon support validé. Android : lecture et benchmark ultérieurs. Le moteur publie ses capacités, l'éditeur désactive les contrôles non pris en charge et conserve leurs données.

Objectifs initiaux (non mesurés) : UI fluide à 60 Hz si possible ; viewport Intel HD 620 30–45 FPS en 720p avec profil adapté ; GPU récent 60 FPS en 1080p, cible matérielle à préciser ; sélection < 50 ms ; modification simple immédiatement visible ; CPU au repos minimal ; imports non bloquants ; reconstruction BVH partielle si possible ; changement matériau/lumière ciblé ; stabilité mémoire sur test prolongé. Tests avec scène et profil fixes, mesures distinctes de l'UI et du runtime.

## 16. Roadmap et critères de sortie
| Version | Périmètre | Critère principal |
|---|---|---|
| 0.0 | Audit code, formats, build, API et risques | Cartographie vérifiée, décision d'intégration et tests de référence |
| 0.1 | Fondations, Qt, viewport, lecture scènes | Ouvrir scène existante sans la modifier |
| 0.2 | Sélection, gizmos, hiérarchie, sauvegarde, undo/redo | Éditer puis recharger sans perdre de données |
| 0.3 | Construction architecturale, primitives, snapping | Construire un bâtiment simple sans coder |
| 0.4 | Import GLB, assets, instances, PBR | Importer, contrôler et réutiliser des assets |
| 0.5 | Éclairage, ciel, cycle jour/nuit, sondes | Modifier et tester éclairage sans régression |
| 0.6 | Politiques WARP, Z-Reflect, LumaTrace, Escargot | Respecter autorisations et budgets sur modes compatibles |
| 0.7 | LOD, BVH, culling, streaming, profilage | Bench reproductible et optimisation mesurée |
| 0.8 | Physique, animations, audio, événements | Mode test interactif et édition temporelle |
| 0.9 | Validation, récupération, publication, multiplateforme | Tests de panne, migrations, packaging et non-régression |
| 1.0 | Version stable, documentation | Scène complète construite, vérifiée et publiée sans modifier le code |

Les jalons ne sont pas des délais. Les fonctionnalités avancées (CSG, node editor, visual scripting, SDK, etc.) peuvent être livrées progressivement selon dépendances ; leur report doit être documenté.

## 17. Recette et non-régression
Scène de référence : complexe aquatique de Pripyat (architecture, verrières, piscine, mosaïques, arbres, véhicules, lampadaires, spots, ascenseur, cycle jour/nuit). Figer révision et données de test. Ajouter scènes synthétiques pour isoler chaque fonctionnalité. Tester les anomalies connues : normales inversées, surfaces vitrées mal classées, objets flottants, Z-fighting, détails fins instables, reflets/réfraction et artefacts d'éclairage.

Tests unitaires des calculs purs, commandes, transformations, sérialisation et migrations ; tests d'intégration chargement/édition/sauvegarde/relecture moteur ; tests graphiques comparatifs avec seuils documentés ; tests de performances et de stabilité. Comparaison sémantique du JSON et préservation champs inconnus. CI Studio séparée, aucun changement aux workflows runtime sans revue dédiée. Les cinq passes de contrôle sont exigées pour chaque fonctionnalité majeure.

## 18. Écosystème cible
WARP Engine (runtime), WARP Studio (édition), WARP Benchmark (mesure), WARP Assets (bibliothèque), WARP SDK (extensions), WARP Player (lecture multiplateforme). Les noms et frontières de packaging sont à confirmer pendant l'audit.

## 19. Première action technique
Avant implémentation : inventorier le moteur actuel, points d'entrée Qt, gestion fenêtre et renderer, chargement scene.json, ownership des ressources, accès aux objets, caméras, données PBR, mécanisme de benchmark et workflows. Proposer une API minimale de lecture et de commandes d'édition. Identifier ce qui est déjà réutilisable, ce qui nécessite un adaptateur, et les fonctions manquantes. Commencer par un viewport en lecture seule ; ensuite ajouter une seule commande de transformation avec tests et sauvegarde compatible. Pas de réécriture générale, pas de modification des backends expérimentaux.

**État de ce document :** spécification et règles de développement uniquement. Aucun résultat de test, compilation, mesure ou fonctionnalité nouvelle n'est revendiqué.
