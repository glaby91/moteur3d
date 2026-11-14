# CSE WARP V16 — Pavillon 01 — GPU GI adaptative (10 novembre 2026)

## Décision
L'objectif reste la qualité de la Build 336 **avec un rendu GPU beaucoup plus rapide**, sans CUDA, DXR ni modification de la scène. La mesure fournie par l'utilisateur sur Intel HD 620 pour le **deuxième rendu final Full HD** est **670,9 ms**. Objectif indicatif : 300–400 ms, non validé sur le matériel réel.

## Implémentation effective
- Entrée HLSL `CSIndirect` : seuls les rayons secondaires coûteux sont calculés à **50 % de la largeur et 50 % de la hauteur**, soit **25 % des pixels**. 2 rayons hémisphériques secondaires par échantillon par défaut ; 0–4 configurables.
- Deux textures GPU intermédiaires `RGBA16F` de 960×540 en Full HD : irradiance indirecte **non multipliée par l'albedo**, normales géométriques et profondeur de réception. Environ 8,3 Mo au total. Le métal et les fenêtres transmettrices suivent les règles existantes, la géométrie est inchangée.
- Entrée `CSMain` Full HD : calcule les ombres/direct, PBR, vitrage et eau à résolution native ; réutilise l'irradiance à l'aide de 4 voisins pondérés selon leurs normales et profondeurs. Aux discontinuités de géométrie, calcul réel local plutôt que mélange de surfaces sans rapport.
- Le cache est réutilisé **uniquement** si scène/révision, shader, caméra, résolution, heure solaire et nombre de rayons restent identiques. Modification de scène/viewport/shader invalide la carte.
- Repli sur le rendu full-res GI lorsque le GPU ne sait pas créer les textures half-res.
- Aucun changement CPU GI ni aux scènes/objets/textures Pavillon ou Pripiat.

## Commandes de comparaison
- `M3D_V16_GPU_GI_ADAPTIVE=1` : **nouveau mode par défaut**.
- `M3D_V16_GPU_GI_ADAPTIVE=0` : mode de référence, 2 rayons secondaires par pixel Full HD.
- `M3D_V16_GPU_GI_SAMPLES=0` : pas de GI secondaire, direct seul.
- `M3D_V16_GPU_GI_SAMPLES=1,2,4` : niveaux de qualité, défaut 2.

## Mesures factuelles — Windows Server 2022 sous D3D11 WARP (logiciel)
La comparaison porte sur le même moteur et la même scène, avec textures CPU/GPU à résolution de base de 512 px.

| Mesure | Build 336 (résolution GI native) | GI adaptative |
| --- | ---: | ---: |
| WARP Full HD, rendu à chaud | 5 068 ms | 2 513 ms (Build 341, run 42786951904) |
| Écart moyen RGB face au CPU GI 4 spp | 12,409/255 | 12,369/255 |
| PSNR face au CPU GI 4 spp | 19,692 dB | 19,712 dB |
| Test A/B WARP, 320×180, GI full vs adaptative | 265 ms full | 162 ms adaptative |
| Test A/B, écart moyen adaptatif/full à 320×180 | — | **0,776/255** |

Les mesures WARP démontrent la diminution du coût logiciel de la passe GI sans perte mesurée notable au test, **mais ne prédisent pas** directement la durée sur le GPU Intel HD 620 de l'utilisateur. Les timings incluent la synchronisation lecture image et peuvent varier avec l'environnement. Le premier appel cold WARP est **plus lent** avec les deux shaders et le cache à initialiser ; ne pas extrapoler les timings de warm-up.

## Vérifications et limites
- Pipeline Windows : compilation C++ et des deux points d'entrée HLSL SM5.0, rendu CPU, rendu GPU WARP Full HD, cache, heure solaire, édition de matériaux, scènes Pavillon et atelier.
- Test A/B plein GI/adaptatif en petite résolution et restitution exacte après changement de résolution. L'intégrité des caches est primordiale.
- **Limites visuelles** : grandes ombres angulaires du shader Build 336 et forte réflexion du bassin déjà connues. La reconstruction de GI ne les corrige pas automatiquement. Une vraie équivalence au path tracing CPU à 2 rebonds n'est pas atteinte.
- **Risque matériel** : pression sur les registres HLSL et disponibilité des UAV FP16 selon GPU/pilote. Mode full-res de secours prévu.
- Prochaine mesure déterminante : lancer le package sur le même Intel HD 620, même résolution Full HD, caméra 01, heure 14,75h et relever le **deuxième** temps GPU pour comparer directement à 670,9 ms. Ne pas annoncer 300–400 ms comme obtenu avant cette mesure.

## Liens
- PR développement : https://github.com/glaby91/moteur3d/pull/18
- CI finale validée : https://github.com/glaby91/moteur3d/actions/runs/42786951904
- Base Build 336 : https://github.com/glaby91/moteur3d/actions/runs/42737363678
