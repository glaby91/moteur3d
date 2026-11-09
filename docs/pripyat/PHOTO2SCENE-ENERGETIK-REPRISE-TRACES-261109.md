# Photo2Scene WARP — reprise des tracés du 9 novembre 2026

**Périmètre : Palais Energetik uniquement.** Les cinq captures aériennes/écran originales du 9 novembre et 17 photogrammes pertinents des vidéos de ce soir ont été repris en local. Les images d'hôtel, de restauration, de manèges ou de bâtiment non identifié ne sont pas assimilées au Palais.

## Correction de méthode

Les anciennes régions A–E étaient des **contours grossiers** : elles ne devaient pas servir de points de mesure. Nous utilisons maintenant les arêtes **effectivement visibles** et le calcul pur sur l'image. La procédure reproductible est dans `tools/photo2scene/edge_snap_review.py` :

1. Choisir un masque de travail restreint au bâtiment/à la pièce et noter sa résolution originale (certaines références sont des captures d'un lecteur vidéo).
2. Tracer quelques vrais segments de toiture, bandeau ou verticales sur les pixels originaux. Ne jamais tracer au-delà d'une occultation par des arbres.
3. Ajuster **localement** leurs offsets normaux sur les maxima de gradient Sobel (rayon de recherche ±7 px). Les extrémités restent des **estimations**, non des coins métriquement validés.
4. Calculer séparément des **candidats** OpenCV LSD dans le masque ; filtrer par longueur minimale et familles d'angles, ne pas transformer ces candidats automatiques en lignes architecturales certifiées.
5. Conserver chaque segment avec ses pixels source, le score de gradient, le statut de validation et les métadonnées de résolution. Aucun mètre, pose de caméra ou profondeur ne doit découler directement de ces traits.

## Bilan du premier traitement local

- **22 photos de travail** : 5 captures originales et 17 vues pertinentes de plusieurs vidéos documentant le Palais.
- **24 segments principaux** revus visuellement et ajustés sur les gradients, concentrés sur les quatre vues aériennes les plus lisibles.
- **958 candidats OpenCV LSD**, à vérifier : le grand nombre **n'est pas** une indication de 958 éléments architecturaux réels.
- Export : dossier photographique original recadré + overlay + détections candidates pour chaque vue, 2 planches-contact, un CSV des arêtes et un JSON référençant les images et les mesures. Dossier complet fourni dans la conversation, **pas republié sur GitHub** pour respecter les droits.

## États et restrictions

Les arêtes tracées manuellement sont des **segments de travail** : aucune pose de caméra validée et aucun relevé architectural coté. Les masques architecturaux ne doivent pas inclure le ciel/les arbres/les éléments d'interface YouTube. Les cas 1986 montrant des bâtiments non identifiés restent exclus de la scène du Palais.

La correction n'implique **aucun déplacement** de tour, des volumes du toit, des murs, de la fresque ni de l'aile arrière. Elle prépare les mesures indépendantes nécessaires à un futur recalage multivue.

## Fichiers complémentaires fournis dans la conversation

`Photo2Scene_WARP_Energetik_lignes_corrigees_soir_20261109.zip` : 22 vues corrigées + 22 crops originaux + 22 planches de candidats + 2 planches-contact + JSON/CSV + script d'exécution.
