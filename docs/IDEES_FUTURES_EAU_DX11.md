# Idée à étudier — eau plus vivante en DX11 et Vulkan

**Statut :** idée pour une révision ultérieure. Pas une demande de modification immédiate.  
**Référence visuelle :** scène piscine V17.523, captures utilisateur du 18 novembre 2026.

## Intention

Rapprocher l’eau des rendus raster **DirectX 11 et Vulkan** du relief, des reflets et des ombres appréciés dans le rendu compute, tout en conservant la rapidité et la netteté du raster. Le compute présente actuellement du bruit et un coût très élevé en exploration.

## Piste technique à évaluer

1. Shader de l’eau (HLSL côté DX11 et shader Vulkan selon l’architecture active) avec deux normales animées de faible amplitude, dans des directions et vitesses différentes.
2. Reflet d’environnement à faible coût (cubemap/probe), modulé par l’angle de vue avec Fresnel.
3. Si le budget GPU le permet, ombre raster via shadow map sur le bassin et son environnement.
4. Garder le même modèle visuel et les mêmes paramètres entre DX11 et Vulkan quand c’est possible, mais traiter séparément leurs ressources, synchronisations et présentation swapchain.
5. Ne pas réintroduire la GI progressive ou le ray tracing compute dans les chemins raster.

## Critères de comparaison

- Comparer DX11, Vulkan et compute à scène, caméra, résolution et paramètres identiques.
- Mesurer séparément chaque backend sur l’Intel HD 620 : qualité de l’eau, ombres/reflets, stabilité pendant le mouvement et temps GPU/frame-time.
- Vérifier que le relief gagné n’introduit ni grain, ni scintillement, ni régression de fluidité.
- Garder comme référence le DX11 direct actuel : la capture du 18 novembre indique un rendu à 2,7 ms, mais ce chiffre unique ne valide pas les performances soutenues.
- Ne pas considérer un résultat Vulkan comme validé avant d’avoir vérifié l’initialisation, le mouvement, la présentation directe et le comportement sur la machine cible.

**À reprendre après stabilisation des backends et du benchmark.**