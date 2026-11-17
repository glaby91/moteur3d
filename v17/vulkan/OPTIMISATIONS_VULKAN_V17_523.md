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

### P0 — Mesurer et supprimer les gels du cycle jour/nuit

Le code UI a deux opérations susceptibles de produire les pauses observées, quel que soit le moteur :

- Le cycle 24 h / 6 min actualise le sélecteur d'heure toutes les 1,5 s. Son signal valueChanged demande alors un nouveau rendu complet.
- Le rendu raster recalcule la shadow map CPU à des paliers d'heure simulée. Le pas vaut 0,25 h pour Vulkan, soit environ toutes les 3,75 s ; DX11 raster utilise déjà 1 h, soit environ toutes les 15 s. Le compute reçoit lui aussi des demandes de rendu à chaque tick, alors que certaines vues prennent plusieurs secondes à calculer.

Ce sont des causes probables à confirmer par mesures. Ne pas ralentir le cycle ni retirer les ombres à l'aveugle.

**Deux niveaux de test sont nécessaires.**

1. **Test fonctionnel de 6 minutes par moteur** : il couvre une fois le cycle complet jour/nuit et vérifie que les mises à jour de lumière et d'ombres passent bien aux heures attendues. Une variante de 2 minutes sert au diagnostic rapide, mais ne valide pas le cycle complet.
2. **Test d'endurance de 30 minutes par moteur** : cinq cycles jour/nuit, après échauffement. Cela aide à révéler les gels intermittents, l'accumulation de demandes, une dérive mémoire ou une dégradation progressive. Lancer DX11 et Vulkan séparément, avec la vraie fenêtre directe, la même scène, caméra fixe, résolution et qualité ; cela représente environ une heure de mesure pour les deux moteurs.

Le test de 6 minutes valide le cycle, pas la stabilité longue durée. Pour une validation candidate de livraison, une session de 60 minutes par moteur peut compléter l'endurance, surtout après toute modification de synchronisation ou de gestion mémoire. Le benchmark A/B hors écran existant ne suffit pas : il mesure une autre voie de présentation.

Enregistrer en CSV, à chaque frame : heure simulée, intervalle entre présentations, durée de rendu, temps de construction de shadow map et son palier, temps GPU, attente de fence, soumission, nombre de demandes coalescées et backend. Le rapport JSON donne p50/p95/p99/max, FPS, pauses >100/250/500/1000 ms, reconstructions d'ombres et leurs heures, ainsi que la version, la scène, le GPU, le pilote, la résolution et les réglages. Suivre aussi le heartbeat UI, la mémoire du processus et les erreurs de périphérique pendant l'endurance. Capturer des images aux mêmes heures simulées pour vérifier que la stabilisation n'a pas changé l'éclairage.

**Correction intégrée à la 17.523 :** pendant le cycle accéléré, DX11 et Vulkan réutilisent chacun la shadow map CPU sur des tranches d'une heure simulée, contre un quart d'heure auparavant pour Vulkan. Les valeurs du soleil et des lumières restent calculées à l'heure courante ; seul le rafraîchissement de la shadow map est moins fréquent. Un test GPU-free vérifie les pas et les tranches. Ce changement peut rendre les ombres moins à jour entre deux reconstructions : la campagne doit donc aussi vérifier leur comportement visuel.

Le menu **Outils → Campagne jour/nuit — DX11/Vulkan direct** propose 6, 30 et 60 minutes. Il démarre après 10 secondes d'échauffement, garde la caméra fixe et exporte un CSV détaillé ainsi qu'un JSON récapitulatif dans Documents/CSE WARP/Benchmarks. Le CSV enregistre les intervalles entre présentations, durée de rendu, heure simulée, reconstruction/palier d'ombre et temps GPU disponibles. Le JSON rapporte notamment les pauses au-dessus de 100/250/500/1000 ms, la résolution, le backend, la scène et la version.

Le test de 6 minutes couvre un cycle complet ; 30 minutes donne une première endurance (cinq cycles). Une heure par backend reste la validation recommandée avant livraison. L'intégration continue vérifie la compilation et les tests automatiques, mais elle ne remplace pas l'essai réel sur Intel HD 620 : la campagne doit y être exécutée séparément en DX11 puis Vulkan, aux mêmes réglages.

Cette passe ne modifie ni les valeurs d'éclairage, ni les shaders/lumières du compute pour rapprocher les rendus. Le rendu Compute de la capture reste hors périmètre de la 17.523.

### P1 — Vérifier le plafond réel des lumières

Le raster Vulkan et le shader compute HLSL ont tous deux un plafond de 64 lumières. Ce nombre est une limite interne du moteur, pas une limite de Vulkan. Le benchmark A/B journalise maintenant les lumières générées, les lumières de scène, le nombre transmis au shader et le nombre ignoré. Ne pas augmenter le plafond avant de confirmer qu'il est atteint : les deux voies ont le même plafond, donc il n'explique pas à lui seul leur différence de tonalité.

### Éclairage — hors périmètre de la 17.523

Aucune modification de couleur, d'intensité, de shader ou de stratégie de lumière n'est incluse dans cette version. L'objectif ici est la stabilité de la simulation jour/nuit et la mesure comparative DX11/Vulkan ; le rapprochement avec le rendu Compute sera traité séparément après cette validation.

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
