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
