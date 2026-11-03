# Bathysphère Rapture et navigation interactive — build 261103-0005

## Modifications

- Coque segmentée et rivetée, avec le grand hublot avant dégagé dans les deux couches de coque.
- Hublot avant élargi, cadre à plusieurs bagues et boulons visibles; hublots latéraux agrandis.
- Cabine ambrée approfondie, avec trois instruments, aiguilles, commandes et lampe visibles à travers le vitrage.
- Ouverture de `scene_rapture.bin` : cadrage rapproché orienté automatiquement vers la face avant de la bathysphère.
- Navigation : glisser gauche pour orbiter, glisser droit pour déplacer le point visé, molette pour zoomer, touches AZERTY Z/Q/S/D pour avancer, aller à gauche, reculer et aller à droite; R pour monter et X pour descendre.
- Pendant les mouvements, l’aperçu est calculé en 480 × 270 sans DOF ni AA contours. Après 450 ms sans mouvement, l’application revient à la qualité et à la résolution choisies.

## Vérification sur le paquet Windows

1. Ouvrir `scene_rapture.bin` et vérifier le cadrage de départ.
2. Orbiter autour du véhicule et inspecter l’ouverture avant et les deux hublots latéraux.
3. Maintenir une navigation : l’aperçu doit suivre la caméra.
4. Arrêter l’entrée caméra : le rendu complet doit remplacer l’aperçu après 450 ms.

Le workflow CI compile les cibles Qt Windows/Linux et réexporte les scènes. Les temps dépendent du processeur et de la résolution sélectionnée.

## Écart visuel constaté — 2026-11-03

La maquette artistique et le rendu du moteur ne correspondent pas. Une capture réelle du build 261103-0001 a été fournie le 3 novembre 2026. Elle montre une coque très proche de la caméra, coupée en haut et sur les côtés. Le hublot occupe une grande partie de l’image; la cabine et ses instruments ne sont pas clairement visibles et semblent masqués par une surface intérieure. Le résultat ne rejoint donc pas encore la maquette de référence.

La capture confirme que le build 261103-0001 a bien rendu la scène Rapture, mais elle révèle un échec de validation visuelle : le cadrage doit laisser voir la bathysphère entière et l’ouverture doit révéler la cabine. Le build 261103-0002 modifie les commandes clavier AZERTY seulement; il ne corrige pas encore ces défauts visuels.

### Suite de validation

- Ouvrir le paquet Windows 261103-0002 et charger `scene_rapture.bin`.
- Vérifier les commandes Z/Q/S/D, R et X.
- Reprendre une capture non retouchée du cadrage initial après correction du cadrage et de l’intérieur du hublot.
- Comparer la silhouette, les rivets, l’ouverture avant, la cabine éclairée et la taille du véhicule avec la maquette.

## Commandes caméra AZERTY — 2026-11-03

- `Z` : avancer; `S` : reculer.
- `Q` : se déplacer à gauche; `D` : se déplacer à droite.
- `R` : monter; `X` : descendre.

Le build est versionné en `261103-0002`. Le workflow Windows/Linux se relance sur cette modification pour produire les paquets correspondants.

## Correction géométrique et reflets — build 261103-0003

La capture réelle du build 261103-0001 montre une coque coupée par le cadrage, des fragments/surfaces qui semblent traverser le hublot et un reflet trop présent sur la coque. Le build 261103-0003 augmente la tessellation de la sphère, retire toute face de maillage dont les sommets ou points médians traversent la découpe du hublot, réduit la réflectivité de `wheelMetal` de 78 % à 30 % dans la scène Rapture et recule le cadrage de départ (distance 22 → 28). Ces ajustements restent à confirmer par une nouvelle capture réelle.

## Parcours vidéo rotatif — build 261103-0004

Les captures du mode vidéo du build 261103-0001 montrent des vues où la coque ou une surface sombre remplit presque toute l’image. En mode vidéo Rapture, le parcours applique maintenant une distance orbitale minimale de 18 unités afin de garder la caméra au-delà de la bathysphère et des parois proches. Cette modification doit être vérifiée avec une nouvelle boucle pré-calculée.

## Fixation des hublots — build 261103-0005

Retour utilisateur : le hublot avant semblait décollé de la sphère. Les bagues, boulons, attaches et éclairages du hublot avant suivent maintenant la profondeur de la surface sphérique; les bagues et le vitrage des hublots latéraux utilisent également leur position sur la coque. Une capture réelle doit confirmer le raccord visuel.
