# Pistes Vulkan ciblées — CSE WARP V17.523

**Date :** 17 novembre 2026  
**Base examinée :** branche `codex/v17.522-vulkan-present-lifetime`  
**Nature :** revue statique du backend `v17/vulkan/m3d_vulkan.cpp`. Un micro-changement de tri est inclus dans cette branche, mais il reste à compiler et mesurer sur le Dell / Intel HD 620.

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

**Changement inclus dans cette branche :** une profondeur par triangle est calculée une fois par image, puis le tri compare les profondeurs pré-calculées. L'ordre de transparence visé et les sommets envoyés restent inchangés. Cela évite de refaire deux produits scalaires pour chaque comparaison du tri. Le gain n'est pas mesuré : compiler, comparer le temps CPU de tri/copie et le frametime p50/p95 sur la même trajectoire caméra, et vérifier l'image avant d'intégrer.

### P1 — Exploiter les mesures déjà instrumentées avant de toucher aux files GPU

Faire une campagne identique caméra immobile / caméra en déplacement, et conserver les mesures CPU (préparation de scène, tri transparent, enregistrement), GPU dessin/transfert, attente au retirement et frametime. Cela déterminera si la limite est CPU, GPU ou synchronisation avant toute nouvelle optimisation.

### P1 — Vérifier le plafond réel des lumières

Le raster Vulkan et le shader compute HLSL ont tous deux un plafond de 64 lumières. Ce nombre est une limite interne du moteur, pas une limite de Vulkan. Le benchmark A/B journalise maintenant les lumières générées, les lumières de scène, le nombre transmis au shader et le nombre ignoré. Ne pas augmenter le plafond avant de confirmer qu'il est atteint : les deux voies ont le même plafond, donc il n'explique pas à lui seul leur différence de tonalité.

### P1 — Rapprocher l'éclairage raster du compute sans perdre la netteté

Le retour visuel prioritaire est précis : le raster conserve mieux le détail des textures, mais paraît plus froid ; le compute donne une lumière indirecte plus crédible et plus chaude, avec des artefacts. La cible est de garder la netteté raster et de rapprocher l'éclairage diffus du compute, sans reprendre son bruit ni corriger l'écart en montant simplement l'exposition.

Le raster Vulkan sait déjà lire une table GI 3D, mais seulement si l'option hybride est active et si le cache est valide pour la signature exacte de la scène. En l'absence de cache valide, il reste sur le soleil, l'ambiance hémisphérique et les lumières directes ; le log l'indique. Première étape : faire le comparatif compute/raster sur la même caméra, même heure, même résolution et même exposition, puis confirmer que Vulkan charge bien la table GI correspondant à la scène. Si elle est absente ou rejetée, corriger ce chargement avant de retoucher le shader.

Le critère d'acceptation sera une lumière de sol et un rebond diffus plus chauds, des textures toujours aussi nettes, sans réintroduire les artefacts du compute ; mesurer le frametime avant/après sur HD 620. Si la table GI n'est pas assez stable, étudier une irradiance basse fréquence préfiltrée plutôt qu'un bruit de rayons par pixel.

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
