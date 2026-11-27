# Z Escargot 001 / Debug 001 — branche de recherche

Base validée V17.031 build 4, scène V17.634. Aucun changement de version du moteur de production. **Ne pas compiler avant autorisation.** Z Reflect reste inchangé. Cette branche contient un laboratoire C++ relié aux données réelles LumaTrace (Runtime/Scene/UV/BVH4) ; ce n'est pas encore le backend temps réel natif. Aucun résultat de performance, rendu ou compilation revendiqué.

## Contrat de la version 001

- Empreinte écran conservatrice par objet optique, sans limite fixe de nombre de surfaces. Les budgets de BVH et de cellules bornent les travaux ; candidats non traités visibles dans les compteurs. Les objets sources peuvent être hors champ caméra.
- Respect des permissions existantes dans la déclaration de scène ; les surfaces courbes et les métaux ne sont pas éliminés par la politique de planéité de Z Reflect. L'identification précise de l'empreinte de chaque source est obtenue à partir des observations, rectangle local élargi d'un pas. Les sources invisibles entre échantillons peuvent être ratées.
- Grille fixe, ordre en spirale, aucun raffinement. Chaque requête BVH4 est comptée (primaire, continuation, secondaire), pas seulement les rayons de réflexion. La première version est CPU ; l'arbre LumaTrace existant et ses triangles sont lus sans deuxième BVH.
- Calques distincts par objet source, matériau et occurrence traversée ; ID objet 0 = absence de source. État Unknown distinct de Zero, Measured et Reconstructed. Une absence explicitement observée n'est jamais comblée. Trous : moyenne arithmétique des quatre observations originales les plus proches (ou moins si insuffisant). Zéros participent à la visibilité, jamais aux UV/profondeurs. Pas de propagation d'interpolations, pas de moyenne des identifiants. Les coutures UV sont approximativement protégées par un test de continuité ; une vraie identification des charts reste à faire.
- Reconstitution de tous les calques avant application des textures. Composition à chaque cellule **du plus éloigné au plus proche sur le trajet optique**, profondeur locale et alpha. Les objets opaques bloquent, les transparents composent. UV de texture source échantillonnés seulement en phase finale. Pas de caméra ni rasterisation secondaire.
- Réflexion et réfraction : modes de diagnostic séparés. Normale de shading barycentrique, Snell à une interface et rejet TIR côté transmission. Les interfaces d'entrée/sortie multiples ne sont pas encore réfractées une à une ; la direction reste rectiligne après la première interface. Aucune promesse de fidélité physique.
- Éclairage source de référence simplifié (albédo linéaire et proxy lumière/émission), pas le shader final LT. Pas encore de ciel en cas de rayon sans hit : zéro signifie absence de calque. La matrice finale est un diagnostic RGBA prémultiplié interne, exporté PNG en alpha droit. L'intégration native devra échantillonner les textures résidentes et respecter le récepteur raster exact (ID/profondeur) plutôt que mélanger des récepteurs transparents sur un seul écran.
- Debug 001 : escargot 3D avec coquille spirale, antennes, empreinte, trajectoire d'échantillonnage, rayons et positions de matrice, export OBJ de lignes avec groupes par objet/état. Aucun debug dans le BVH ; désactivé par défaut. Pas encore d'overlay interactif dans Qt/DX11.

## Organisation

`types.h` : contrats et états ; `bvh4.h` : traversée existante ; `footprint.h` : candidats/empreintes/budget ; `sampling.h` : spirale et parcours optique ; `matrix.h` : calques et reconstruction ; `composite.h` : transformation finale des UV et composition ; `runtime.h` : orchestration et métriques ; `debug.h` : géométrie de diagnostic. `v17/tools/escargot_lab.cpp` utilise le chargeur officiel. `v17/tests/escargot_smoke.cpp` contient tests à exécuter ultérieurement.

## Commandes futures (NE PAS exécuter avant accord)

Après configuration habituelle CMake V17, cible facultative `v17_escargot_lab` et `v17_escargot_smoke`. Exemple : `v17_escargot_lab scene.json v7_pool_fresco output 4096 reflection 31`. Dernier entier : bit 1 escargots, 2 empreintes, 4 spirales, 8 rayons, 16 points. Sans masque debug, aucun OBJ. Sorties : PNG optique et par récepteur, matrices CSV, métriques JSON, OBJ facultatif. Pas de rendu secondaire ; pas d'image finale du moteur natif. Choisir la caméra ID réelle du fichier.

## Comparaison et étapes suivantes

A = Z Reflect natif actuel ; B = Escargot ; C = hybride (uniquement si A/B le justifie). Mesurer à qualité égale, déplacement, stabilité, temps CPU/GPU, transfert, VRAM, rayons, surfaces traitées et erreurs de contours. Cas plans/courbes, inox, eau, verre superposé, petits objets, occultations, sources hors écran. Premier test de vérité : comparer transformation UV et masques à une référence par rayon exhaustive dans une fixture ; vérifier les erreurs aux coutures et petites empreintes. Tester profondeur locale qui inverse l'ordre de deux objets.

La version 001 est volontairement sans raffinement, cache temporel approximatif ni GPU annexe. Étape suivante après validation mathématique/compilation autorisée : calcul GPU de requêtes BVH4 avec calques compacts, composition dans raster LT natif, interrupteur A/B et overlay debug coloré. Puis seulement raffinement optionnel, réutilisation temporelle et comparaison hybride. La GTX 1050 Ti dédiée reste une plaisanterie/recherche facultative ; duplication des textures/BVH et synchronisation à mesurer.

Limites de ce commit : code rédigé/relu statiquement, non compilé, non exécuté sur scène. Le laboratoire ne remplace pas la méthode actuelle ni ne modifie l'interface. Les choix de budgets initiaux et proxy lumineux sont hypothèses d'expérimentation, pas paramètres validés.

## Revue en cinq passes — optimisations et simulations non compilées

Même branche de recherche, révisions 001 conservées (aucune publication moteur). Les vérifications JavaScript reproduisent les sous-algorithmes ; ce ne sont ni des tests C++ exécutés ni des mesures GPU.

| Passe | Changements | Vérification numérique |
|---|---|---|
| 1 — Géométrie | Validation BVH4 et indices, départ caméra au plan near réel, normale géométrique distincte de shading, tie-break par primitive, transmission primaire, eau explicite et biais secondaire réduit | 341 angles de projection near |
| 2 — Budget | Quota par point de la grille fixe, admission mémoire avant distribution, état complet/inconnu et compteurs | 16 320 scénarios de coûts synthétiques, pas de dépassement ni de privation des points restants |
| 3 — Matrices | Recherche exacte partagée des quatre voisins complets, fusion des observations positives partielles par calque, parcours dense ou creux | 21 600 requêtes top-4 et 21 600 fusions comparées au tri exhaustif ; scénario 12 calques : 280 800 recherches théoriques → 43 200 au maximum hors fusion partielle |
| 4 — Composition | Masques de couverture communs, union des silhouettes opaques, suppression des lectures de textures totalement occultées, LUT sRGB, filtre de calques par ligne | 24 000 compositions aléatoires : 299 964 → 111 366 évaluations de couleur, erreur numérique maximale 0 sur modèle de référence ; deux couvertures disjointes donnent alpha 1, au lieu de 0,75 |
| 5 — Contrôle/optimisation | Spirale bornée par rectangle, pas de grille par recherche binaire, tri sans stable_sort, CSV creux, métriques et debug des rayons, tests C++ ajoutés non lancés | 4096 grilles ; 30000 choix de pas identiques ; spirale 4096×1 : 16785409 → 4096 visites ; 259802 → 343349 évaluations de taille de grille sur l'échantillon |

### Limite quantifiée de la moyenne 4 voisins (passe 5)

Miroir plan z=0, caméra (0,0,2), source plane z=4 : intersection source (3x,3y,4). Grille 17×17, mesure tous les 4 pixels, 25 observations et 289 cellules. Les UV mesurés sont conservés exactement ; moyenne arithmétique des quatre voisins pour les trous : erreur UV euclidienne moyenne 0.06146, maximale 0.13975 (UV normalisés, référence analytique). Ce n'est pas une mesure sur la piscine. La moyenne n'est pas une interpolation affine exacte et peut produire du palier/décalage aux frontières. On la conserve conformément à la spécification 001 ; pas de raffinement automatique.

### États, mémoire et export

`samples.csv` contient les états de chaque échantillon et la complétude du trajet ; `matrices.csv` ne contient que les valeurs positives mesurées. Pour un calque absent de la seconde table : trajet complet = identifiant zéro explicite, trajet incomplet = inconnu. Pas de produit cartésien calques × points à écrire. Les matrices reconstruites ne servent jamais à alimenter une interpolation ultérieure. Les calques de trajets incomplets conservent le fallback de composition scalaire, avec approximation aux frontières. La couverture à quatre masques ne rajoute aucun rayon. La composition reste ordonnée par profondeur locale reconstruite ; pas d'affirmation d'exactitude pour les intersections complexes. Les données debug ne sont pas insérées dans la géométrie.

`preflight_pass5.mjs` reproduit les tests de la cinquième passe en JavaScript (aucune compilation C++). Le test C++ `v17_escargot_smoke` est enrichi (zéros, couverture, BVH invalide, spirale mince) mais volontairement non exécuté. Le laboratoire demeure CPU, sans shader natif LT, sans raffinement ni cache temporel ; budgets, vitesse et qualité à valider sur scène et matériel réels.
