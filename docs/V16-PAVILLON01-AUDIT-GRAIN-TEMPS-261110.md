# CSE WARP Beta X V16 — constats CPU GI à 64 spp et temps de rendu (2026-11-10)

## Constats terrain
Sur la caméra 02, Pavillon 01, Full HD 1920×1080, 14,75 h :
- CPU direct à 1 spp : 4 025,9 ms.
- CPU direct à 4 spp : 15 866,1 ms.
- CPU GI deux rebonds, 16 spp avec débruitage : 94 911,4 ms.
- CPU GI deux rebonds à 64 spp : l'image reste perçue comme granuleuse et artificielle ; **temps final non fourni car la durée disparaît de l'interface**.
- GPU D3D11 Compute GI adaptative : 401,7 ms caméra 01, 441,0 ms caméra 02 après deux calculs (Intel intégré utilisateur).

## Diagnostic de code confirmé
1. `moteur3d_v16_qt.cpp` affichait le résultat via `statusBar()->showMessage(...,15000)` : durée auto-effacée après **15 s**. Corrigé sur cette branche : statut final conservé + widget permanent « Dernier rendu : N ms (M s) » à droite, qui résiste aux messages temporaires. L'ancien résultat reste visible même si un rendu ultérieur échoue.
2. L'image CPU GI `renderGiAccumulatePass` accumule des valeurs HDR linéaires et n'applique ACES/sRGB qu'en preview : comportement correct, **ne pas** simplement augmenter les spp pour supprimer un défaut de modèle.
3. Le `denoiseBilateral()` actuel est un filtre 5×5 sur image **8 bits déjà tonemappée** ; il n'utilise ni normales, ni profondeur, ni albedo, ni variance. Cela peut laisser du grain sur des aplats et ne conserve pas de façon robuste les détails.
4. `traceOfflineColor()` bascule vers `traceColor()` sur les matériaux avec transmission : la méthode d'éclairage des vitrages diffère du chemin GI opaque ; pas un traçage physiquement cohérent multi-rebonds.
5. Matériaux procéduraux et éclairage de la scène limitent aussi la plausibilité. La fidélité photoréaliste ne découle pas du seul nombre de rayons.

## Suites prioritaires (non codées dans ce correctif)
- Denoiser guidé par `albedo + normal + depth` sur l'accumulation linéaire (A-trous/variance) avec comparatifs de détails, surtout sur les huisseries.
- Modèle verre : Fresnel/transmission/ior cohérent pour le CPU GI et GPU, sans bascule CPU directe arbitraire.
- Vérifier cohérence des textures PBR, éclairage, ombres, intensité relative du ciel et du soleil.
- Benchmark à scène/caméra identiques en mode CPU 16/64 spp, GPU, et mémorisation de la durée complète en fin de rendu.

Ne pas présenter l'ajout de spp comme une résolution des problèmes de modélisation, d'éclairage ou de posttraitement.
