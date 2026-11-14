# CSE WARP V17 — Portage Raster Vulkan 1.3.204 minimum (expérimental)

**Branche :** `experiment/v17-vulkan-raster-backend-261114`. La V17 DirectX11 de `master` est conservée sans modification. Ce backend n'est **pas encore une réécriture fonctionnellement identique de tout DirectX11** ; c'est une première implémentation native suffisamment complète pour afficher la géométrie principale, puis mesurer les écarts de performance et de qualité.

## Compatibilité avec ton installation 1.3.204

Le minimum requis est désormais **Vulkan API 1.3.204**, aussi bien pour le chargeur que pour le GPU physique. Le SDK utilisé pour compiler doit lui aussi être en version 1.3.204 ou supérieure ; la CI utilise actuellement un SDK 1.4 compatible. L'installation Vulkan 1.3.204 signalée par l'utilisateur ne démontre pas à elle seule la version API du pilote graphique Intel HD620 ; le backend contrôle les deux indépendamment et signale une incompatibilité si le pilote expose une version inférieure. Les shaders SPIR-V sont compilés pour la cible Vulkan 1.3. Les fonctionnalités optionnelles restent soumises à vérification ; annoncer 1.3.204 ne les rend pas automatiquement disponibles. **Attention : la spécification Vulkan ignore le champ patch lors de `vkCreateInstance` ; notre rejet des versions <1.3.204 est volontairement plus strict que le minimum technique des fonctions Vulkan 1.3.** Un pilote exposant 1.3 avec un patch inférieur sera donc refusé.

La carte HD620 doit exposer une queue graphique Vulkan, les textures/attachments nécessaires, et une mémoire `HOST_VISIBLE` pour le buffer de lecture. L'exécutable signale les erreurs ; il n'effectue pas de repli silencieux CPU.

## Implémenté

- Instance et device Vulkan natifs, queue graphique, commande primaire, render pass, framebuffer offscreen couleur RGBA8 + Z-buffer, lecture de l'image pour Qt.
- GLSL/SPIR-V ciblant Vulkan 1.3, vertex/fragment, matériaux par couche, lighting PBR simplifié (normal/roughness, métal, fresnel, soleil, 16 lumières, ombres PCF 3×3).
- Géométrie issue des **mêmes définitions V17** : triangles, boîtes, boîtes arrondies, cônes, ellipsoïdes, capsules, terrain et UV.
- Conservation des sources instanciables pour les **boîtes** (une géométrie partagée + attributs par instance) et le **feuillage source** (maillage 12×6 + feuilles).
- Transparence géométrique avec tri CPU arrière→avant, test Z et pas d'écriture Z.
- Culling conservatif par batch pour les maillages et instances ; scène immuable conservée par identifiant/révision.
- Textures PBR groupées en image arrays, taille adaptative au budget de 128 Mio de textures GPU sur petites cartes, selon couches et limites du GPU. La résolution peut baisser sur les scènes à 186 matériaux : la fidélité 2048 n'est **pas** encore atteinte.
- Staging `HOST_CACHED` préféré, mapping persistant, un command pool réinitialisé par frame, GPU timers, séparation des temps de `vkQueueSubmit`, fence, conversion pixel.
- Backend sélectionnable dans l'application Qt en option 6, mode exploration/sky quand Vulkan est choisi. DX11 reste l'option par défaut.
- Test de triangulation sans GPU, smoke Vulkan sur triangle avec PNG, et benchmark A/B réel sur même scène et caméra en 1366×768, CSV/JSON/PNG.

## Limitations importantes

Ce premier port **ne reproduit pas encore à l'identique** le rendu DirectX11 de la V17. Les fonctions suivantes restent à porter ou à comparer : shaders matériels complets, Z-prepass optimisé, MSAA, anisotropie 16×, mipmaps complètes, LOD instancé dynamiques de tous les objets, moteurs météo et ciel via GPU, l'eau, probes GI, bloom/multires, les matériaux avancés 2048², textures animées, et les pics de synchronisation. Les shaders Vulkan utilisent une approximation PBR, les checksums entre DX11 et Vulkan ne devraient donc **pas** être égaux. Ne pas fusionner sur `master` avant validation des images de la piscine et de l'exploration sur la HD620.

Le rendu reste **offscreen** pour compatibilité avec l'interface Qt existante, qui utilise des QImage. Il y a donc toujours une copie GPU→CPU ; un futur écran à surface Vulkan/Qt ou swapchain sera nécessaire pour tester les FPS sans cette lecture.

La V17 pleine DirectX11 reste la référence fonctionnelle et de performance.

## Comment tester sur le Dell

1. Télécharger l'archive du workflow GitHub final et extraire tous les fichiers.
2. Lancer `LANCER_AB_VULKAN_HD620.bat`, qui exécute le smoke GPU, puis le comparatif DX11/Vulkan sur une scène V17.
3. Envoyer `CSE_WARP_VULKAN_RASTER_RESULTS.zip`. Examiner les images `dx11.png` et `vulkan.png` ; comparer médianes et P95 en JSON.
4. Pour l'exploration, ouvrir `moteur3d_v17.exe`, puis sélectionner `V17 Vulkan Raster — expérimental` dans les options.

## Cinq passes de revue statique avant compilation finale

1. **API/architecture** : Scene/Camera/Stats conservées, backend séparé, version minimale 1.3.204 sur chargeur et GPU ; DX11 intact.
2. **Maillages/performance** : même triangulation, boîtes instanciées, frustum batches ; suppression des régressions évidentes sur les boîtes massives.
3. **Mémoire/PBR** : feuillage instancié, budget textures adaptatif, tableaux de textures et views compatibles même avec une couche, staging `HOST_CACHED`.
4. **Synchronisation/UI** : queues/fences, image barriers, stats GPU, exploration et watchdog Qt, invalidation sur heure solaire, compilation shaders SPIR-V.
5. **Validation et packaging** : tests de géométrie, rendu GPU smoke, benchmark A/B et README. Dernière étape = **une compilation Windows** sur la branche, puis correction explicite des anomalies éventuelles ; sans prétendre à la validation matérielle tant que le Dell n'a pas exécuté le package.

## Références Khronos

- [Versions et versioning instance / device](https://docs.vulkan.org/guide/latest/versions.html)
- [Gestion mémoire](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [Utilisation des command buffers](https://docs.vulkan.org/samples/latest/samples/performance/command_buffer_usage/README.html)
- [Barrières et synchronisation](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html)

Les optimisations multithread secondaires, timeline semaphores et trois frames in flight n'ont **pas** été ajoutées arbitrairement : elles peuvent améliorer le débit mais aggraver la latence sur la HD620 ; elles demandent une mesure avec notre Raster.

## Diagnostic CI Windows (ajout novembre 2026)

Le workflow exécute les tests GPU-indépendants avec les chemins Qt, Vulkan SDK et moteur V16 visibles. Les codes de sortie Windows sont enregistrés en décimal et hexadécimal ; les contrôles de maillage signalent leurs valeurs en erreur. Si la compilation réussit mais qu'un smoke échoue, le ZIP de diagnostic est quand même publié (répertoire `diagnostics/`), puis le workflow reste **en échec**. Un statut CI vert ne démontre toujours pas la validité des rendus GPU sur l'Intel HD620, qui doit être testée séparément.

Le test `v17_vulkan_mesh_smoke` vérifie uniquement la géométrie CPU du convertisseur de maillage. Il est compilé **sans dépendance au moteur partagé V16** pour éviter une fausse panne de chargement DLL sur les runners Windows. Il utilise un matériau de sol local déterministe ; cela ne remplace ni le test de chargement du moteur partagé ni les tests du GPU Vulkan. La publication CI utilise `actions/upload-artifact@v6` (runtime Node.js 24).

## Optimisation fragment shader (profil A/B, 14 novembre 2026)

Le profil par défaut reste **baseline**. Un profil **optimized** sélectionnable par la variable d'environnement `CSE_WARP_VK_FAST_FRAGMENT=1` compile le même GLSL avec `-DCSE_WARP_VK_FAST_FRAGMENT=1` dans `raster_fast.frag.spv`. Sur les surfaces tournées à l'opposé du soleil, il saute le filtrage d'ombre PCF 3×3 et le spéculaire solaire (contribution nulle). Pour les points lumineux hors rayon, il élimine `sqrt`, normalisation et calcul d'atténuation. L'intérieur du rayon utilise la même loi d'atténuation (multiplication à la place de `pow(x,2)`), à d'éventuels arrondis flottants près.

**Vérification matérielle obligatoire :** lancer `LANCER_OPTIMISATION_VULKAN_HD620.bat`. Ce programme effectue quatre campagnes de 40 images dans l'ordre baseline–optimized–optimized–baseline, avec deux passages d'échauffement à chaque campagne, même scène, même trajectoire et même résolution. Il produit `CSE_WARP_VULKAN_OPTIMISATION_HD620.zip` contenant JSON, images et logs. Ne pas annoncer un gain avant comparaison des médianes, P95, temps GPU et écarts visuels sur le Dell. Le benchmark alterné comprend aussi un rendu DX11 par frame, ce qui sert de référence mais n'établit pas la parité de qualité.

Les améliorations écartées à ce stade : multiplier les command buffers et ajouter plusieurs frames GPU en vol dans le chemin QImage synchrone (sans refonte API Qt asynchrone, cela ne supprime pas le readback nécessaire), réallouer les descriptors par image (ils sont déjà persistants) ou réduire textures/résolution pour fausser le comparatif.

Sources Khronos : https://docs.vulkan.org/samples/latest/samples/performance/command_buffer_usage/README.html ; https://docs.vulkan.org/samples/latest/samples/performance/descriptor_management/README.html ; https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html .

Les quatre campagnes sont sauvegardées comme quatre répertoires distincts dans l'archive, pour conserver les quatre fichiers `ab_results.json` et leurs captures sans conflit de nom.

## Pipeline GPU vers Qt : BGRA8 natif (expérience A/B)

Le chemin historique rend dans `VK_FORMAT_R8G8B8A8_UNORM`, puis le CPU permute les canaux pour `QImage::Format_ARGB32`. Le nouveau chemin **opt-in** `CSE_WARP_VK_BGRA_READBACK=1` rend directement dans `VK_FORMAT_B8G8R8A8_UNORM`, dont l'ordre des octets correspond au QImage Windows little-endian : le CPU ne fait plus de permutation, seulement une copie avec alpha forcé opaque comme dans le chemin historique. Si la carte n'annonce pas `COLOR_ATTACHMENT`, `COLOR_ATTACHMENT_BLEND` et `TRANSFER_SRC` pour ce format, le moteur signale une erreur claire ; le chemin de référence (variable absente ou égale à 0) reste intact.

**Validation :** exécuter `LANCER_PIPELINE_VULKAN_HD620.bat`. Il force le **même shader optimisé** dans les quatre passes ABBA et ne change que le format/copie : RGBA A, BGRA A, BGRA B, RGBA B, chacune avec 40 images + échauffement en 1366x768. Les fichiers JSON contiennent `vulkan_readback_profile`, `vulkan_pixel_convert_ms` et maintenant une empreinte `vulkan_frame_hash` **par image**, afin de vérifier les 40 positions, pas seulement celle du premier PNG. Le CPU-only `v17_vulkan_readback_smoke` contrôle les canaux couleur sur le runner CI ; la performance réelle et la compatibilité GPU doivent être confirmées sur l'Intel HD620.

Le rendu Qt reste synchronisé avec une copie GPU→CPU. Ce changement n'ajoute pas artificiellement plusieurs frames en vol ; pour supprimer cette copie il faudrait une interface Qt utilisant une texture Vulkan/swapchain, à concevoir séparément. Références Khronos : https://docs.vulkan.org/spec/latest/chapters/formats.html et https://docs.vulkan.org/guide/latest/formats.html.

## Qualité de texture par matériau : moteur automatique, descripteur prioritaire

Ce mécanisme est **générique**, sans référence au nom, à la position, à la distance ou à la géométrie d'une scène particulière. Dans une ressource `materials/*.json` (ou, par défaut, dans le `textureSet` correspondant), chaque matériau peut déclarer :

```json
"textureDetail": {
  "mode": "preserve",
  "minResolution": 2048
}
```

- `mode: "auto"` ou champ absent : le moteur décide librement de la résolution selon son budget GPU, comportement historique inchangé.
- `mode: "preserve"` : le matériau est placé dans un **ensemble de textures haute résolution séparé**. `minResolution` (puissance de deux entre 64 et 2048) indique la qualité minimale attendue ; en l'absence de valeur, on utilise la résolution déclarée du textureSet. La qualité reste maintenue quelle que soit la distance de caméra.
- La politique de l'entrée matériau prime sur celle du textureSet. Aucun identifiant de scène (fresque, piscine, etc.) n'est codé dans le moteur.
- Si la résolution source, les capacités GPU ou le budget protégé de 256 MiB ne permettent pas d'honorer la demande, **le moteur refuse explicitement le rendu plutôt que de réduire silencieusement la qualité**. Les autres matériaux restent soumis au moteur automatique.

À noter : le backend Vulkan actuel ne charge qu'un niveau de mip dans ses images GPU, pour les deux catégories. Cette mise à niveau protège surtout la **résolution effective** face au budget des textures multiples. Un véritable streaming trilineaire/LOD gradué reste un chantier séparé ; la politique `preserve` devra être respectée lors de son introduction. Les autres paramètres de qualité GPU/DX11 sont inchangés.

## Optimisations générales supplémentaires : réemploi et cache de pipelines

- **Tri transparent sans allocations récurrentes :** le tableau d'indices est conservé entre frames, réinitialisé avec l'ordre original à chaque frame, puis trié exactement avec le même comparateur. Ni l'ordre de dessin, ni les textures, ni les effets ne changent.
- **Cache Vulkan persistant de compilation de pipelines :** écrit dans `%LOCALAPPDATA%/CSE-WARP/VulkanCache/`. La clé et l'entête prennent en compte fabricant, modèle, version du pilote, UUID interne et version d'entête Vulkan. Toute donnée incohérente est rejetée et recompilée automatiquement. Le cache ne contient **aucune donnée de scène** et ne modifie jamais les paramètres des shaders, du Z-buffer, de la transparence ou du MSAA. Il réduit potentiellement le coût du démarrage/chargement des pipelines, **pas le temps GPU de chaque image**.
- L'optimisation optionnelle BGRA et le profil fragment restent désactivés par défaut tant que des campagnes A/B qualité ne les ont pas validés sur plusieurs scènes.

Les frames en vol et un Z-prepass ne sont pas automatiquement activés ici : sur la présentation Qt actuellement synchrone, une file GPU asynchrone impose une gestion explicite des images/latences, et un Z-prepass doit respecter transparence et surfaces coplanaires. Les ajouter sans mesures et sans tests de parité pourrait dégrader le rendu ; ils restent des travaux expérimentaux distincts et génériques.

## Z-prepass Vulkan : activé de façon permanente

Le pipeline Vulkan exécute systématiquement son **Z-prepass opaque** (shader vertex uniquement, écritures couleur désactivées), puis le rendu couleur PBR et enfin les surfaces transparentes une seule fois. La création des trois pipelines de profondeur se fait à l'initialisation. Aucun benchmark A/B ni contrôle adaptatif ne se déroule au démarrage ou par frame. Il n'existe **plus de variable d'environnement** pour modifier ce comportement.

Sur la campagne Intel HD620 de 160 images (Pavillon 01, 40 positions répétées), la médiane est passée d'environ 24,52 ms à 14,49 ms, soit environ +69,3 % de débit théorique. Les 40 empreintes images étaient identiques sur cette scène de test. Cela **ne garantit pas** la parité sur des scènes ou des géométries non testées.

L'ancien lanceur de comparaison on/off `LANCER_ZPREPASS_VULKAN_HD620.bat` a été remplacé, sous le même nom pour compatibilité, par une **validation du Z-prepass permanent** : smoke GPU (2 draw calls sur un triangle) puis 40 images et empreintes sous le backend actuel. Il produit `CSE_WARP_VULKAN_ZPREPASS_DEFAULT_HD620.zip`. La métadonnée JSON `vulkan_depth_prepass` vaut toujours `true` et `vulkan_depth_prepass_policy` vaut `always_on_no_runtime_comparison`. Les anciens ZIP sont les archives historiques du comparatif on/off.

## État d'intégration de la dernière V17 / master

**Ne pas confondre validation de l'optimisation Z-prepass et parité complète du backend Vulkan.** Vulkan fonctionne sur l'Intel HD620 et la CI Windows teste la compilation, le chargeur API, la géométrie et les canaux couleur. La branche expérimentale ajoute déjà Vulkan comme choix distinct dans la fenêtre Qt V17, et la branche master conserve DirectX11 comme rendu de référence.

La fusion vers master n'est **pas encore approuvée** : les sorties Vulkan et DirectX11 ne sont pas visuellement équivalentes, et les fonctionnalités PBR avancées, eau, ciel, MSAA, mipmaps et filtrage/LOD ne sont pas toutes portées. Notre politique est de ne pas régresser en qualité par rapport à DirectX11. Les tests GPU réels restent nécessaires : eau, transparence, fresques haute résolution, textures protégées, changements de scène et exploration. La présence du Z-prepass ne règle pas ces écarts.

## Finition des matériaux PBR — mipmaps GPU natives

Le backend peut générer **une chaîne de mipmaps jusqu’à 1×1**, dans la limite du budget GPU et sans réduire la résolution du niveau 0, à partir du niveau 0 avec `vkCmdBlitImage`, une seule fois pendant le chargement des textures, et active le filtrage trilineaire pour les matériaux automatiques. Il contrôle `BLIT_SRC`, `BLIT_DST` et `SAMPLED_IMAGE_FILTER_LINEAR` sur les deux formats GPU avant de le faire. En cas d'incompatibilité, il conserve le chemin mono-niveau, avec un message explicite dans le log. Le moteur fixe d’abord la résolution de base comme avant, puis alloue autant de mipmaps supplémentaires que le budget le permet. **Le niveau de détail principal n’est jamais sacrifié pour ajouter des mipmaps** ; aucun changement de géométrie ni de résolution de rendu.

Les matériaux déclarés `textureDetail.mode = preserve` utilisent un sampler séparé, limité au **LOD 0**, pour ne jamais dégrader les fresques ou panneaux protégés à distance. Cette règle s'applique à tout matériau, sans reconnaître sa scène. Le Z-prepass Vulkan reste **permanent**. Le test `v17_vulkan_mipmap_smoke` vérifie les dimensions de la pyramide ; le rendu GPU et la comparaison qualitative avec DX11 doivent toujours être exécutés sur HD620.

## Finition matériaux : émission de lumière déclarative et diagnostic comparatif

Tout matériau peut désormais déclarer `"emission": {"color": [1.0, 0.8, 0.4], "intensity": 2.0}` dans son `materials/*.json`. Cette donnée générique prime sur les anciens identifiants de lampe/néon utilisés par compatibilité. Une émission explicitement nulle désactive aussi l'ancien traitement, quelle que soit l'identité du matériau. Uniquement les entrées valides sont acceptées (couleur 0–1, intensité 0–100), et l'ancien rendu reste inchangé si le champ est absent. Aucun code supplémentaire n'est appliqué à un décor spécifique.

Le benchmark A/B génère maintenant `dx11_last.png`, `vulkan_last.png` et `vulkan_vs_dx11_last_diff.png` (carte rouge des différences par pixel), en plus du diagnostic `last_frame_rgb_mae` et du pourcentage de pixels dont au moins un canal diffère de plus de 30. Ces calculs se font **après la campagne mesurée** et n'augmentent pas le temps déclaré de chaque frame. Ceci permet de concentrer les corrections Vulkan sur des écarts mesurables au lieu d'estimations visuelles.

### Garantie de non-régression du niveau de détail principal

Le calcul du budget **ne diminue pas** la résolution du niveau 0. Si la mémoire disponible ne permet que deux niveaux, le sampler dispose de deux mips et non d'une chaîne complète. Si seul le niveau 0 tient dans le budget, le filtrage reste celui de l'ancienne version. Cette règle s'applique également aux matériaux non protégés et est vérifiée par le smoke des budgets partiels.
