# V17.029 — L01 : anneaux concentriques / ombres solaires

Base exacte : V17.028, commit a9145fe4770ba64e15a534b803d5d73d52b1b8d6, branche feature/v17-dx11-r001 (Z Atmosphere 002 et DX11 R001 conservés). La V17.025 de master, commit eb8475c22d5985926abb962f7b088aea25c428d4, est une tentative antérieure sur base V17.024, à ne pas utiliser comme référence de validation de cette série.

## L01 — correctif candidat
DX11 Raster et LumaTrace partagent le shader HLSL ; Vulkan utilise l'équivalent GLSL. L'ancien PCF solaire utilisait neuf comparaisons avec un biais fixe de 0,095 m. Le nouveau PCF ajuste la profondeur du récepteur à chaque texel d'après le plan géométrique (dérivées de position monde avant branchement divergent, sans normal map). Biais résiduel 0,005 m + terme de précision, repli sur l'ancien biais aux angles rasants ou surfaces dégénérées. Pondération continue des neuf taps, aucun tap supplémentaire. Les options R001, Z Atmosphere, Z Reflect, scène et GI ne sont pas modifiées. Le calcul de radiance indirecte par triangle n'est pas modifié.

Test analytique simplifié précédent : 0/3000 fausses comparaisons sur un plan incliné contre 1190/3000 avec l'ancien biais. Ce test ne prouve ni la disparition du défaut sur la scène réelle ni un coût GPU nul. Compilation Windows et contrôles existants, puis comparaison V17.028/V17.029 à scène, caméra, heure et réglages identiques sur DX11, Vulkan et LumaTrace. Vérifier déplacement caméra au sol, ombres de contact, angles rasants et FPS HD620.

## L02 — NON RÉSOLUE / EN SUSPENS
Décomposition triangulaire de la lumière au pied des poteaux sur DX11, Vulkan et LumaTrace. Aucun correctif spécifique autorisé dans cette version. Observer si L01 atténue le défaut ; ne pas clôturer L02 sans validation indépendante.
