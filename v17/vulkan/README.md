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
