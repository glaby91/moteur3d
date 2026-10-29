# Hypothèse moteur — rendu par événements de visibilité

**Projet :** moteur 3D V9.2  
**Date :** 2026-10-29  
**État :** hypothèse de recherche, ni implémentée ni validée  
**Cible :** image 4K UHD à au moins 100 images/s (10 ms par image).

## Changement de représentation

Le moteur V9.2 construit actuellement l'image en demandant à chaque pixel : « quelle surface est la première rencontrée ? ». Une autre représentation consiste à demander : « où, dans l'écran, l'identité de la première surface change-t-elle ? »

L'idée est de construire une carte de visibilité formée par les frontières projetées des primitives et par les endroits où leur ordre de profondeur change. Ces frontières divisent l'écran en régions. À l'intérieur d'une région, la même primitive ou la même face reste visible.

Ce n'est pas un calcul de candidates N+1 ni une reprojection des pixels N. La géométrie projetée devient la structure qui génère l'image.

## Algorithme proposé

### 1. Compiler les primitives en événements d'écran

Pour chaque caméra, projeter les bornes BVH et produire les contours visibles des primitives analytiques. Pour les paires de primitives dont les bornes se recouvrent, chercher les courbes de silhouette ou les lieux où la profondeur des deux surfaces s'égale.

Le BVH filtre les paires possibles ; les primitives éloignées à l'écran ne participent pas à la construction des événements.

### 2. Construire les régions de visibilité

Assembler les courbes et événements en cellules d'écran. Dans chaque cellule, un rayon témoin ou une comparaison analytique désigne la surface gagnante. Les intersections exactes de référence restent disponibles pour les cellules ambiguës.

### 3. Produire les pixels à l'intérieur des cellules

Pour chaque pixel de sortie, connaître déjà la surface visible. Évaluer son point de surface et ses coordonnées de matériau à partir de la formule de cette primitive, sans refaire la recherche BVH du premier impact. L'éclairage statique peut être précalculé par surface ; brouillard et niveau mip restent évalués selon la nouvelle caméra lorsque nécessaire.

### 4. Mettre à jour lors du mouvement de caméra

Calculer les enveloppes projetées balayées par les primitives entre les deux poses. Les cellules hors de ces enveloppes gardent leur attribution de visibilité. Recalculer les contours et régions seulement là où une silhouette ou un ordre de profondeur peut changer.

Les workers éventuels calculent des groupes de contours, cellules ou tuiles ambiguës. Le rendu complet V9.2 sert de repli exact et de référence.

## Pourquoi cela pourrait battre le ray casting actuel

La capture V9.2 native donne environ 1,10 test géométrique par rayon, ce qui indique que la BVH est déjà efficace. Le poste dominant mesuré est plutôt l'éclairage/texture : environ 875 ms, contre 377 ms pour le tracé.

Le gain recherché ne vient donc pas d'une BVH plus profonde. Il vient d'un changement de représentation : ne pas refaire la décision de visibilité pour chaque pixel si cette décision est constante sur une région. La formule analytique de la surface visible peut ensuite être spécialisée et son apparence réutilisée.

Le coût de construction des frontières peut cependant dépasser le coût des rayons si la scène génère trop d'intersections de contours ou si les cellules sont minuscules. C'est la principale hypothèse à vérifier.

## Prototype falsifiable

Commencer par une scène réduite de primitives analytiques :

1. sol + boîte ;
2. ajouter ellipse et cône ;
3. réintroduire les 119 primitives et le BVH complet.

Comparer au même rendu V9.2 exact et enregistrer :

- temps de construction des contours et cellules ;
- temps de remplissage de l'image ;
- nombre de cellules, frontières et pixels ambigus ;
- nombre de rayons de repli ;
- erreur pixel/couleur face au rendu complet ;
- mémoire, latence caméra et temps de chaque image ;
- performance en pose fixe, mouvement doux et mouvement brusque.

La piste est abandonnée si la construction des frontières dépasse durablement le budget de 10 ms, si le nombre de cellules croît avec le nombre de pixels, ou si les cellules ambiguës couvrent presque toute l'image.

## Relation avec les autres pistes

La cache de feuilles de surface demeure une piste auxiliaire possible pour réutiliser matière ou éclairage. Elle ne définit pas l'algorithme principal. La première question est de vérifier si la carte de visibilité par régions supprime assez de calcul par pixel pour justifier son coût de construction.

## Originalité et statut

Cette méthode est une hypothèse construite à partir des primitives analytiques et du BVH de V9.2. Son caractère mondialement inédit n'est pas établi. L'objectif immédiat est la falsification par prototype, puis une recherche d'antériorité seulement si les résultats techniques sont prometteurs.