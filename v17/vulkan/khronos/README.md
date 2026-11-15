# CSE WARP V17.32 Vulkan — expérimentation optimisée HD 620

**Important : cette branche n'est pas master.** Le moteur DirectX 11 et la version de référence restent inchangés. Les gains sur le véritable Intel HD 620 exigent un essai utilisateur : GitHub Actions ne dispose pas de GPU Vulkan matériel.

## Modifications effectives

1. **Cache géométrie / PBR** : le moteur distingue `Scene::markGpuGeometryChanged()` (géométrie seule) de `markGpuMaterialsChanged()`. `markGpuContentChanged()` reste conservateur pour les anciens appels. Une modification géométrique explicite conserve les textures PBR en VRAM, leur résolution, leurs mipmaps et les descripteurs associés.
2. **Instances GPU** : lorsque la scène possède exclusivement des cubes opaques avec un seul matériau et une emprise au sol inchangée, une modification met à jour seulement les instances effectivement modifiées dans un buffer hôte mappé. Les cull-bounds sont recalculées. Pour toute géométrie plus complexe, le chemin mesh original reste disponible avec réutilisation des tableaux PBR.
3. **Deux images en vol** (Win32 direct swapchain) : pools de commandes, fences, queries, uniformes, descriptors, framebuffer, rendertargets et buffers transparents indépendants par image. Les fences sont attendues à la réutilisation d'un slot, et non après chaque présentation. Toute mutation de ressource partagée attend la fin du travail GPU en cours.
4. **Shader optimisé** : réutilisation du shader `raster_fast.frag.spv` existant. Évite les calculs solaires quand leur contribution est nulle et ignore précocement les points lumineux hors rayon ; le modèle PBR/normal/ombres/textures est conservé. Doit encore passer une comparaison visuelle sur HD 620.
5. **Mesures corrigées** : désactivation des écritures de console toutes les 120 images (cause identifiée de pics de latence), exécutable GUI Windows sans fenêtres DOS répétitives, rapport CSV tolérant aux lignes incomplètes, délai limite par essai, option d'affichage exact 1280x720.
6. **Régressions** : test de politique d'invalidation indépendant du GPU et test de géométrie déjà existant.

## Comparaison A/B en 720p

- Lancer `DIAGNOSTIC_ECRAN_NOIR.bat` d'abord si les images paraissent noires. Une compilation réussie ne prouve pas qu'une scène s'affiche correctement.
- Lancer `LANCER_COMPARAISON_AB.bat` pour deux campagnes comparables de 12 scénarios à 1280×720, 8 s par test. Les ZIP `*_baseline_*.zip` et `*_optimized_*.zip` sont produits séparément.
- Lancer `LANCER_KHRONOS_HD620.bat` pour la campagne optimisée complète : 12 scénarios × 3 répétitions × 20 s.
- Pour lancer une campagne particulière :
  `powershell -NoProfile -ExecutionPolicy Bypass -File RUN_KHRONOS_HD620.ps1 -Profile optimized -Seconds 12 -Repetitions 2`

## Paramètres de comparaison (avant le lancement du processus)

| Paramètre | Référence | Optimisé |
|---|---|---|
| `CSE_WARP_VK_OPT_INCREMENTAL` | `0` reconstruction globale | `1` cache PBR et instances |
| `CSE_WARP_VK_FRAMES_IN_FLIGHT` | `1` soumission synchrone | `2` double-slot |
| `CSE_WARP_VK_FAST_FRAGMENT` | `0` shader initial | `1` shader rapide |
| `CSE_WARP_VK_BENCH_BORDERLESS` | optionnel | `1` pour vrai 1280×720 |

Attention : les mesures GPU en mode 2 slots sont récoltées **avec un décalage maximal de deux images** ; elles ne correspondent pas toujours à l'image exacte de la même ligne CSV. Les temps de présentation logicielle ne sont pas les temps de scanout de l'écran. Les 40–45 FPS sur scène architecturale lourde ne sont pas démontrés par le test de cubes.

## Justification technique et sources Khronos

- https://docs.vulkan.org/samples/latest/samples/performance/descriptor_management/README.html — caching descriptor et regroupement des données dynamiques.
- https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/03_Frames_in_flight.html — duplication des ressources utilisées par le GPU et deux images en vol.
- https://docs.vulkan.org/samples/latest/samples/performance/wait_idle/README.html — éviter de vider le pipeline CPU/GPU.
- https://docs.vulkan.org/samples/latest/samples/performance/command_buffer_usage/README.html — réutiliser les command pools ; éviter de multiplier les threads sans besoin.
- https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html — barrières nécessaires mais ciblées.

**Décision d'intégration :** conserver ce travail sur la branche `experiment/v17-vulkan-hd620-optimized-261115` jusqu'à validation de l'image, absence d'erreurs pilotes et comparaison A/B sur la vraie scène lourde. Aucun merge vers master automatique.
