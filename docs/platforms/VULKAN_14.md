# Stratégie Vulkan 1.4 — compatibilité et optimisation commune

**Audit du 28 novembre 2026 — V17.031 / master `2036130d88e9337c6e6aab057c6a63b71fe43a37`.** Document de conception uniquement. Aucun changement de version, shader, réglage ou moteur. Les estimations de potentiel sont des hypothèses à mesurer.

## Constat dans le code

- [Politique de version](https://github.com/glaby91/moteur3d/blob/2036130d88e9337c6e6aab057c6a63b71fe43a37/v17/vulkan/m3d_vulkan_version.h) : minimum 1.3.204 ; test de frontière 1.3.203/204, 1.4.0 et variant non standard.
- [Backend Vulkan](https://github.com/glaby91/moteur3d/blob/2036130d88e9337c6e6aab057c6a63b71fe43a37/v17/vulkan/m3d_vulkan.cpp) : configure() demande 1.3.204, vérifie loader et périphérique indépendamment ; sélection GPU/queue graphique, refuse CPU sauf opt-in CI ; active l'anisotropie si disponible. Utilise encore render passes, barrières classiques, descripteurs ordinaires, vkQueueSubmit et fences. Le chemin Win32 natif existe ; l'initialisation surface propre au moteur hors Windows n'est pas portée.
- [Adaptateur](https://github.com/glaby91/moteur3d/blob/2036130d88e9337c6e6aab057c6a63b71fe43a37/v17/moteur3d_v17_render_vulkan.cpp) : tentative directe puis repli Vulkan QImage explicite ; aucune substitution silencieuse CPU. Vérifier l'intégration Qt et la durée de vie des surfaces avant d'étendre ce chemin.
- Build 11 Mac : loader 1.4.363, GPU 1.3.357. Build 11 Linux : loader 1.3.275, périphérique logiciel 1.4.318. Ni l'un ni l'autre ne constitue une exécution complète de l'API 1.4 actuellement demandée par le moteur.

## Plan par étapes — aucune activation automatique avant validation

| Priorité | Piste | Conditions et repli | Indicateurs |
|---|---|---|---|
| P0 | Inventaire capacités et négociation version | Choisir la version d'instance autorisée par loader ; vérifier version physique et capacités effectives, extensions instance/périphérique, features2/properties2, formats, files et limites ; en 1.3 utiliser uniquement les extensions présentes, en 1.4 utiliser les fonctions core activées. En cas d'absence garder le chemin 1.3.204. | JSON loader/device/driver/API négociée/features activées, validation layers, tests 1.3 et 1.4 |
| P1 | Présentation native (WSI) | Surfaces Qt/Cocoa, X11 et Wayland, extensions surface/swapchain, queue présente, formats, composite alpha, resize, DPI, out-of-date/suboptimal, surface/device lost, destruction ordonnée. Pas une nouveauté 1.4. Garder QImage disponible et explicite. | Zéro readback image sur WSI, p50/p95/p99 FPS et latence, stabilité resize/minimize |
| P1 | Host image copy | Core 1.4 facultatif ou EXT sur 1.3 : vérifier feature, usage HOST_TRANSFER, formats, layouts autorisés, alignements et synchronisation ; comparer staging/queue de transfert, préserver mips, couleurs et formats. Sur GPU discret transfert asynchrone peut être meilleur. | Temps froid, RAM/VRAM de pointe, débit upload, hashes de pixels |
| P2 | Push descriptors / maintenance6 | Vérifier pushDescriptor et maxPushDescriptors, layout/ressources ; réserver aux petites mises à jour fréquentes, conserver pools pour arrays PBR. Maintenance5/6 simplifient API mais n'impliquent pas de gain FPS. | Temps CPU de préparation, nombre de mises à jour, validation |
| P2 | Synchronization2 / timeline semaphores | Déjà possibles en Vulkan 1.3 via features ; remplacer progressivement les attentes globales hors chemin critique, isoler ressources et descripteurs par frame ; aucune réutilisation avant fence/timeline. | Temps d'attente, latence, 2 frames en vol sans artefacts, test churn scène |
| P3 | Dynamic rendering local read | Core 1.4 ou extension 1.3 si supportée. Vérifier limites depth/stencil/MSAA et compatibilité formats. Le moteur utilise encore vkCmdBeginRenderPass ; étudier rendu dynamique 1.3 d'abord. Conserver Z-prepass, profondeur, transparence et ordre ; évaluer local read uniquement pour passes réellement éligibles. | Bande passante, encodeurs Metal, GPU ms, rendu bit-exact si attendu |
| P3 | Index uint8 / load-store NONE / map_memory2 | Tester petits maillages avec limites d'index, aucune perte de géométrie ; NONE seulement si contenu réellement inutile ; map_memory2 pour gestion, pas de gain garanti. | VRAM, mémoire de transfert, conformité, performances |
| P4 | Cache pipeline, indirect draws, allocator | Déjà possibles avant 1.4. Réduire cold-start, appels de dessin sans modifier ordre/culling ; tester mémoire persistante selon cohérence et heaps. | Taille cache, temps froid, appels de dessin, pic VRAM |

## Protocole de promotion

1. Même SHA moteur, scène vérifiée, caméra/parcours déterministe, heure, formats, exposition, Z-prepass, MSAA 1/4, textures, résolution 720p/FHD. Référence 1.3.204 et options désactivées.
2. Mesurer séparément chargement froid, fixe et mouvement, CPU submit, fence/GPU timestamps lorsque disponibles, readback, présentation native, mémoire, p50/p95/p99. Au moins 3 répétitions entrelacées ; sessions de stabilité 30 min, changements de scène et fenêtre. Documenter GPU/driver réels.
3. Comparer PNG/pixels sur même GPU et paramètres. Pour transformations supposées neutres : identité exigée, sinon diagnostic précis ; comparaison inter-GPU tolérance documentée et contrôle visuel, pas de promesse bit-exact universelle.
4. Validation Khronos (y compris synchronisation), aucun crash ou erreur nouvelle, fallback testé sur absence de feature et loader 1.3. Une erreur device-lost doit interrompre la soumission et proposer une récupération contrôlée, pas être masquée.
5. Rejeter toute combinaison plus lente ou visuellement différente ; n'activer aucune option par défaut sans décision et benchmark sur matériel cible. Windows, Linux et Mac doivent passer leurs tests. Le HD620 reste une cible de compatibilité.

## Références externes
- [Khronos — Vulkan 1.4](https://docs.vulkan.org/refpages/latest/refpages/source/VK_VERSION_1_4.html)
- [Khronos — VkPhysicalDeviceVulkan14Features](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceVulkan14Features.html)
- [Khronos — porting guide](https://docs.vulkan.org/guide/latest/versions.html)
- [Khronos — Host image copy](https://docs.vulkan.org/samples/latest/samples/extensions/host_image_copy/README.html)
- [MoltenVK — portability et installation](https://github.com/KhronosGroup/MoltenVK)
