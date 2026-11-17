# Pistes Vulkan ciblées — CSE WARP V17.523

**Date :** 17 novembre 2026  
**Base examinée :** branche `codex/v17.522-vulkan-present-lifetime`  
**Nature :** revue statique du backend `v17/vulkan/m3d_vulkan.cpp`; aucune mesure ni compilation n'a été faite dans cet environnement. Les pistes ci-dessous sont donc des hypothèses à valider sur le Dell / Intel HD 620.

## État déjà présent dans la branche 522

- Le chemin natif Win32 utilise une swapchain Vulkan et présente les images sans readback CPU vers `QImage`.
- Un mode à deux images en vol existe avec cibles, UBO, requêtes de temps et fences séparés.
- Le cache de pipelines est déjà chargé/sauvegardé sous `%LOCALAPPDATA%\CSE-WARP\VulkanCache`, identifié par les informations du pilote. Les pipelines visibles dans ce backend sont construits au démarrage.
- Des timestamps GPU distinguent dessin et transfert ; le temps d'attente au retirement de frame est mesuré.
- Le rendu utilise encore les render passes Vulkan classiques, les barrières `vkCmdPipelineBarrier` et `vkQueueSubmit`. Le chemin direct copie l'image de rendu vers la swapchain par `vkCmdBlitImage`.

Ces points ne sont donc pas à réimplémenter comme des nouveautés.

## Priorités proposées

### P1 — Vérifier le coût CPU du tri des transparences

Dans la boucle de rendu, les centres des triangles transparents sont reprojetés dans le comparateur de `std::sort` à chaque comparaison, puis les sommets sont recopiés dans le buffer transparent. C'est une piste plausible pour la baisse de fluidité pendant le mouvement de caméra.

**Essai ciblé :** calculer une fois par frame la profondeur de chaque triangle, trier des paires profondeur/indice, puis remplir le buffer existant. Garder le même ordre de transparence et les mêmes données de sommets. Comparer les temps CPU de tri, de copie et le frametime p50/p95 sur une trajectoire caméra fixe, avant/après. Ne conserver le changement que si le benchmark montre un gain reproductible et aucune différence d'image.

### P1 — Exploiter les mesures déjà instrumentées avant de toucher aux files GPU

Faire une campagne identique caméra immobile / caméra en déplacement, et conserver les mesures CPU (préparation de scène, tri transparent, enregistrement), GPU dessin/transfert, attente au retirement et frametime. Cela déterminera si la limite est CPU, GPU ou synchronisation avant toute nouvelle optimisation.

### P2 — Synchronization2, comme migration de lisibilité et de vérification

Migrer ensemble les barrières et la soumission de la voie Vulkan vers `vkCmdPipelineBarrier2` / `vkQueueSubmit2`, en gardant exactement les dépendances et étapes actuelles. L'objectif est de rendre les dépendances plus explicites et plus faciles à auditer ; **aucun gain FPS n'est présumé**. Garder le chemin de rendu et l'image de référence inchangés.

### P3 — Dynamic Rendering, seulement si la maintenance des render passes devient un problème

La fonctionnalité réduit le besoin de gérer render passes et framebuffers et peut simplifier une refonte des passes. Elle ne garantit pas une accélération. À séparer en changement dédié avec captures d'image et mesures avant/après.

## À écarter pour cette passe

- **Cache de pipelines :** déjà en place ; ne pas le présenter comme une nouvelle optimisation.
- **Deux frames en vol :** déjà implémentées ; garder le comportement actuel jusqu'à ce que les mesures de retirement justifient un changement.
- **Files compute/transfert dédiées :** ne pas imposer une architecture à files multiples sans vérifier les familles de queues et mesurer le recouvrement réel sur HD 620.
- **Subgroup size control, inline uniform blocks et buffer device address :** fonctionnalités/options non nécessaires à la voie raster actuelle ; coût/portabilité à justifier par un profil avant adoption.
- **Maintenance4 :** ne concerne pas la destruction rapide des `VkShaderModule`. Les modules shaders peuvent déjà être détruits après création des pipelines ; Maintenance5 apporte une autre voie pour fournir les infos shader sans créer de module.

## Références

- Khronos, [Pipeline cache](https://docs.vulkan.org/guide/latest/pipeline_cache.html)
- Khronos, [Pipeline dynamic state](https://docs.vulkan.org/guide/latest/dynamic_state.html)
- Khronos, [Dynamic rendering sample](https://docs.vulkan.org/samples/latest/samples/extensions/dynamic_rendering/README.html)
- Khronos, [Queue families and multiple queues](https://docs.vulkan.org/guide/latest/queues.html)
- Khronos, [VK_KHR_maintenance4](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_maintenance4.html)
- Khronos, [vkDestroyShaderModule](https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyShaderModule.html)
