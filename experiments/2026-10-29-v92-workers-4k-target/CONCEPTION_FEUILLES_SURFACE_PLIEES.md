# Proposition de conception — feuilles de surface pliées

**Projet :** moteur 3D V9.2  
**Date :** 2026-10-29  
**État :** piste secondaire archivée, pas encore implémentée ni validée  
**Objectif :** réduire le coût des images successives vers une sortie 4K à au moins 100 images/s.

## Intuition

Cette piste est née d'une interprétation littérale de l'image du pliage évoquée dans la discussion. Rodolphe a précisé qu'il s'agissait d'une métaphore pour encourager une idée hors du cadre, et non d'une spécification de méthode. Elle est conservée comme hypothèse technique secondaire ; la recherche prioritaire est maintenant le rendu par événements de visibilité.

## Représentation

Pour un pixel calculé en N, V9.2 connaît déjà l'identifiant de primitive et la distance du premier impact. En conservant la caméra de N, on peut reconstruire le point 3D :

`X = origineCaméra_N + distance × directionRayon_N`

Ce point est stocké dans une feuille propre à sa surface :

`clé = (primitive, chart/sous-surface, coordonnées de surface u,v)`

La profondeur reste attachée au point pour résoudre les recouvrements lorsqu'il est reprojeté. La couleur est décomposée autant que possible en attributs réutilisables : matière/albédo et éclairage statique. Les termes qui dépendent du point de vue, comme le brouillard et l'empreinte mip, sont recalculés lorsque leur variation dépasse un seuil.

Il faut des charts distincts pour les faces de boîte, les coutures des ellipses et les parties latérales/caps des cônes. Le seul identifiant de primitive ne suffit pas à joindre correctement toutes les faces.

## Calcul d'une image N+1

1. Recevoir la nouvelle pose caméra et conserver l'instantané de scène, les matériaux, la lumière et la BVH.
2. Projeter les points des feuilles de surface vers l'écran de N+1 ; un test de profondeur décide quel point reste devant.
3. Réutiliser les données des points couverts par un chart valide.
4. Marquer les zones manquantes, les conflits de profondeur, les coutures et les changements de visibilité comme frontière de réparation.
5. Relancer le vrai ray caster V9.2 sur ces tuiles, puis ajouter les nouveaux points aux feuilles.
6. Rafraîchir progressivement les zones moins certaines selon un budget de temps et un seuil d'erreur visuelle.

Les workers éventuels traiteraient les tuiles de réparation ou des pages de surface ciblées. Ils ne calculeraient pas chacun une image complète redondante.

## Pourquoi cette piste vise les postes mesurés

Sur la capture native V9.2 de 1920 × 1200, éclairage/texels représente environ 875 ms (57,2 %) et le tracé BVH environ 377 ms (24,7 %). Le moteur a lu 5 889 928 texels pour 1 472 482 impacts, soit exactement quatre lectures par impact, cohérent avec le bilinéaire.

La feuille de surface vise à réutiliser des échantillons matière et éclairage lorsqu'un même point de surface réapparaît, et à éviter de refaire les intersections dans les régions dont la visibilité reste stable. Elle n'accélérera pas automatiquement tous les mouvements : les nouvelles zones visibles et les changements importants de pose nécessitent du ray tracing.

## Garde-fous

- Aucun mélange entre charts/surfaces différents.
- Test Z lors de la projection ; collisions de profondeur signalées, jamais moyennées aveuglément.
- Les coutures, silhouettes et zones occultées sont considérées incertaines et recalculées.
- Invalidation lors d'un changement de géométrie, matériau, texture, lumière ou version de scène.
- Le brouillard est recalculé avec la nouvelle distance caméra.
- Le mip est réévalué quand l'empreinte du rayon change suffisamment.
- Un rendu complet V9.2 reste la référence de comparaison.
- Une expérience antérieure de reprojection a produit des artefacts : cette proposition est donc expérimentale.

## Protocole de validation

Comparer, pour chaque pose N+1, la sortie reconstruite à un rendu complet de référence, avec les mêmes paramètres.

Mesures à enregistrer :

- temps de préparation de la feuille ;
- temps de projection/reconstruction et temps de correction ;
- proportion de pixels réutilisés, rééchantillonnés et relancés en rayons ;
- erreur de couleur et nombre d'artefacts aux silhouettes/occlusions ;
- mémoire utilisée par chart et par résolution ;
- délai input-to-display et temps de chaque image présentée ;
- résultats séparés en caméra immobile, déplacement doux, rotation, translation, mouvement brusque et changement d'éclairage.

Le premier prototype doit fonctionner en résolution réduite et afficher une carte des pixels réutilisés/corrigés. Le passage à 4K ne sera envisagé qu'après maîtrise des erreurs et du coût mémoire.

## Statut d'originalité

Il s'agit d'une proposition spécifique aux primitives analytiques, aux buffers d'impact et à la BVH de V9.2. Elle n'est pas présentée comme une invention mondiale déjà établie : une revendication de nouveauté demanderait une recherche d'antériorité distincte.