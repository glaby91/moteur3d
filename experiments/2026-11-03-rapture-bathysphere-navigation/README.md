# Bathysphère Rapture et navigation interactive — build 261103-0001

## Modifications

- Coque segmentée et rivetée, avec le grand hublot avant dégagé dans les deux couches de coque.
- Hublot avant élargi, cadre à plusieurs bagues et boulons visibles; hublots latéraux agrandis.
- Cabine ambrée approfondie, avec trois instruments, aiguilles, commandes et lampe visibles à travers le vitrage.
- Ouverture de `scene_rapture.bin` : cadrage rapproché orienté automatiquement vers la face avant de la bathysphère.
- Navigation : glisser gauche pour orbiter, glisser droit pour déplacer le point visé, molette pour zoomer, touches WASD pour se déplacer et R/F pour monter/descendre.
- Pendant les mouvements, l’aperçu est calculé en 480 × 270 sans DOF ni AA contours. Après 450 ms sans mouvement, l’application revient à la qualité et à la résolution choisies.

## Vérification sur le paquet Windows

1. Ouvrir `scene_rapture.bin` et vérifier le cadrage de départ.
2. Orbiter autour du véhicule et inspecter l’ouverture avant et les deux hublots latéraux.
3. Maintenir une navigation : l’aperçu doit suivre la caméra.
4. Arrêter l’entrée caméra : le rendu complet doit remplacer l’aperçu après 450 ms.

Le workflow CI compile les cibles Qt Windows/Linux et réexporte les scènes. Les temps dépendent du processeur et de la résolution sélectionnée.

## Écart visuel constaté — 2026-11-03

La maquette artistique de la bathysphère et la capture réelle de l’application ne correspondent pas. La maquette montre une coque détaillée, un grand hublot ouvert sur une cabine lumineuse et un véhicule proche de la caméra. La capture disponible du build 261102-0007 montre une sphère lisse, vue de loin; elle précède les changements décrits ci-dessus et ne valide donc pas le rendu du build 261103-0001.

Le build 261103-0001 a été compilé sous Windows et Linux, mais aucune capture de son rendu réel n’a encore été vérifiée. L’intégration visuelle reste à confirmer.

### Vérification à faire

- Ouvrir le paquet Windows 261103-0001 et charger `scene_rapture.bin`.
- Enregistrer une capture non retouchée avec le cadrage initial.
- Comparer la silhouette de la coque, l’ouverture avant, la visibilité de la cabine et la taille du véhicule dans l’image.
- Corriger la géométrie ou le cadrage si ces éléments ne sont pas visibles comme prévu, puis joindre une nouvelle capture réelle au suivi.
