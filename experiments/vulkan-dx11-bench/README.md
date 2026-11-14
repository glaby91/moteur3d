## Campagne TURBO en une passe (14 novembre 2026)

**LANCER_BENCH_HD620.bat** effectue neuf tests à 1366x768, 32 itérations par pixel, 6 warmups et 30 répétitions par variante, puis crée **CSE_WARP_HD620_TURBO_RESULTS.zip** contenant les JSON, CSV et diagnostic. Il ne porte pas le Raster V17 vers Vulkan.

| Variante | Configuration | Ce qui change |
| --- | --- | --- |
| A baseline | DX11 + Vulkan legacy | Benchmark précédent, référence conservée |
| B cached | DX11 + Vulkan cached | Mémoire staging HOST_CACHED privilégiée, mapping persistant, commandes réutilisées |
| C direct | DX11 + Vulkan direct | Sortie shader directement accessible au CPU, sans staging |
| D cached_record | Vulkan cached + vk-record | Réenregistrement des commandes pour mesurer son coût |
| E/F cached_ring2/3 | Vulkan cached, 2/3 travaux | Ring de buffers, fences, timestamps et commandes |
| G direct_ring3 | Vulkan direct, 3 travaux | Accès direct et pipeline |
| H/I compute_only/ring3 | Vulkan compute, 1/3 travaux | Suppression de la lecture CPU par dispatch ; vérification checksum séparée |

**Précautions :** les temps totaux ne sont directement comparables qu'à politique de lecture et synchronisation équivalente. Dans les modes compute, la vérification du résultat se fait hors chronométrage ; les temps totaux n'incluent donc PAS la même opération que dans les modes avec lecture CPU. Avec plusieurs travaux en vol, consulter le débit global (pipeline_wall_ms et jobs/s), et pas uniquement la latence moyenne de chaque travail. Aucune de ces mesures ne représente les FPS du moteur Raster.

Les nouvelles colonnes CSV sont record_ms, submit_ms, wait_ms, invalidate_ms et cpu_copy_ms, en plus du temps GPU et du wall. Les attributs mémoire réellement sélectionnés HOST_CACHED, HOST_COHERENT et DEVICE_LOCAL sont imprimés dans les logs. Certains pilotes ne proposent pas de mémoire HOST_CACHED adaptée : c'est une propriété à vérifier, pas à inventer.

Aucun overclocking, réglage GPU risqué ni modification du pilote n'est réalisé par le benchmark.

Références officielles :
- https://docs.vulkan.org/spec/latest/chapters/memory.html
- https://docs.vulkan.org/samples/latest/samples/performance/command_buffer_usage/README.html
- https://docs.vulkan.org/guide/latest/profiling.html
- https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/03_Frames_in_flight.html

---

# CSE WARP — banc exploratoire Vulkan vs DirectX11 sur Intel HD620

**Statut : branche expérimentale isolée. Ne modifie pas V17, ses shaders de rendu, ni les scènes.**

## Ce que mesure réellement cette première étape

Comparaison **Compute Shader 5.0** entre **Vulkan 1.0** (GLSL→SPIR-V) et **Direct3D 11** (HLSL→DXBC) sur **le même GPU physique**, à résolution et charge de calcul identiques. Chaque pixel exécute les mêmes 32 itérations de calcul entier, écrit exactement un uint32 dans un buffer (aucune texture PBR, aucun triangle, aucune lumière). Les deux sorties sont comparées **bit à bit** par un checksum FNV-1a.

Ce microbenchmark permet de valider l'initialisation Vulkan, les buffers, le dispatch de shaders, les timestamps GPU, les synchronisations et le coût du chemin de lecture. **Il ne mesure PAS les FPS du moteur Raster V17, ni son Z-prepass, ni le coût des 700 k/1 M de triangles.** Un Vulkan plus rapide ici ne signifie pas que CSE WARP Raster serait plus rapide sans porter et comparer sa scène. Toute affirmation contraire serait incorrecte.

- Même GPU : sélection de l'adaptateur D3D11 par VendorID/DeviceID PCI retournés par Vulkan. Refus si introuvable ; ne jamais comparer une iGPU à une autre carte sans le signaler.
- GPU : timestamps Vulkan et D3D11 centrés sur le **dispatch compute**, sans copier les pixels. 5 warmups, 30 mesures réelles. Médiane et P95.
- CPU wall : de la soumission au retour en mémoire CPU (y compris copie et synchronisation). Ce n'est **pas un temps GPU**.
- Les tests sont intercalés dans les deux ordres (DX/VK, VK/DX) pour limiter le biais thermique.
- Sorties : `.json` et `.csv` avec les échantillons individuels, adaptation matérielle détectée, résolution et checksum.
- Un shader GLSL Vulkan et un shader HLSL D3D11 utilisent le même algorithme uint32 déterministe ; les compilateurs peuvent produire des instructions différentes. Les résultats caractérisent donc **l'implémentation du pilote + compilateur + API**, pas un prétendu coût pur des API.

## Sur ton Dell Windows 10 / Intel HD620

1. Dans la page **GitHub Actions** du workflow `CSE WARP V17 EXPERIMENT - Vulkan vs D3D11 Compute A-B`, télécharger l'artefact Windows x64 généré. Extraire les fichiers ensemble.
2. Mettre à jour le pilote Intel et vérifier que `vulkan-1.dll` et le pilote Vulkan Intel sont présents (aucun SDK nécessaire pour exécuter le ZIP). Le backend n'utilise pas DXR.
3. Brancher le Dell sur secteur, fermer les applications GPU et conserver la résolution native.
4. Lancer **`LANCER_BENCH_HD620.bat`**. Il exécute un diagnostic puis l'A/B sur 1366×768, warmup 6, 30 échantillons par API, 32 itérations/pixel.
5. Envoyer **`bench_hd620.json`**, **`bench_hd620.csv`** et **`vulkan_dx11_probe.txt`** pour analyse. Conserver le ZIP et la version de pilote Intel.

Lancement manuel :

```bat
cse_warp_vk_dx11_bench.exe --probe
cse_warp_vk_dx11_bench.exe --api both --width 1366 --height 768 --loops 32 --warmup 6 --repeat 30 --output bench_hd620
```

### Contrôles techniques isolés en CI

Le workflow compile le programme et le SPIR-V sous Windows ; il lance D3D11 en **WARP logiciel** et Vulkan en **SwiftShader logiciel** pour vérifier que les deux chemins fonctionnent et donnent le même checksum. **Les temps du runner GitHub ne sont pas comparables** : ce ne sont pas deux API sur le même GPU réel. Aucune conclusion FPS ne découle de cette CI.

Pour faire une vraie comparaison du **Raster**, phase 2 requise : importer une même tranche représentative de la scène approuvée V17, mêmes triangles indexés, textures PBR 2048, ombres, réglages MSAA, même Z-prepass et même résolution ; produire des images comparables et mesurer GPU Draw / transfert / FPS écran. Cette phase ne fait PAS partie de ce microbenchmark rapide.

## Compilation de développement

Windows 10/11, Visual Studio 2022 C++ x64, CMake >=3.21 et Vulkan SDK LunarG **pour compiler seulement** (exécutable autonome avec chargeur Vulkan fourni par le pilote). Le build compile `benchmark.comp` avec `glslangValidator`; HLSL est compilé à l'exécution par `d3dcompiler_47.dll` (Windows standard).

```powershell
cmake -S experiments/vulkan-dx11-bench -B build-vulkan-dx11 -G "Visual Studio 17 2022" -A x64
cmake --build build-vulkan-dx11 --config Release
```

En cas d'échec du lancement, le message mentionne l'étape Vulkan/D3D11 concernée. Un GPU sans Vulkan compute ou sans pilote fonctionnel est déclaré indisponible ; pas de bascule silencieuse vers un autre GPU.
