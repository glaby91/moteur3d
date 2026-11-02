# Contrôle visuel de la bathysphère Rapture — build 261102-0007

**Date :** 2 novembre 2026  
**Application :** Moteur 3D natif x64, build `261102-0007`  
**Branche :** `experiment/manhattan-silent-hill-2026-10-30`  
**Commit du build :** [`f2777c9`](https://github.com/glaby91/moteur3d/commit/f2777c9551b7af049f572658d74206077978a057)  
**Scène :** Rapture V4, `scene_rapture.bin`  
**Capture :** [capture-build-261102-0007.png](capture-build-261102-0007.png)  
**SHA-256 de la capture :** `59cbc2d9a88f38c050f11ef8ba1e625548a01041d2f175679b36e9bfd6f5858f`

## Résultat observé

La capture confirme une amélioration visible du bathyscaphe dans son environnement : sa coque est structurée, la passerelle et le quai le relient à la station, et l’éclairage de la scène donne une meilleure lecture de l’ensemble. Le retour utilisateur est positif : « c'est quand même mieux ».

À ce cadrage général, les hublots et l’intérieur ambré restent peu lisibles. La capture valide donc le progrès d’ensemble; une vue rapprochée est nécessaire pour confirmer visuellement la cabine, les instruments et la transparence des hublots.

## Paramètres et mesures visibles

- Résolution de sortie sélectionnée : 4K UHD, 3840 × 2160
- Threads CPU : 16
- Rendu : 22 732,4 ms
- Post-traitement : 186,1 ms
- Temps total affiché : 22 918,5 ms
- Objets : 226 942
- Triangles : 167 311
- Rayons : 17 542 173
- Impacts : 17 355 141
- Texels : 173 880 492

La capture affiche également 130 427 358 tests, 854 855 779 nœuds BVH parcourus et 1 645 420 217 tests AABB. Ces compteurs sont reproduits tels qu’affichés par l’interface.

## Référence de l’archive

Archive associée : `Moteur3D-Qt-windows-build-261102-0007.zip`. Elle contient l’exécutable Qt, `scene_rapture.bin`, `scene_rapture.js` et la documentation du build. L’archive documente aussi 119 899 nœuds dans le BVH exporté. Cette valeur décrit la structure de scène; elle est distincte du compteur de nœuds BVH parcourus affiché par le rendu.

## État

**Progrès visuel confirmé.** Conserver la scène séparée de l’exécutable. Pour la prochaine vérification, produire un cadrage rapproché de la bathysphère afin d’évaluer distinctement les ouvertures des hublots, le vitrage et l’intérieur éclairé.
