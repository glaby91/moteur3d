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
| 5 — Contrôle/optimisation | Spirale bornée par rectangle, pas de grille hybride (8 essais locaux puis recherche binaire), tri sans stable_sort, CSV creux, métriques et debug des rayons, tests C++ ajoutés non lancés | 4096 grilles ; 30000 choix de pas identiques ; spirale 4096×1 : 16785409 → 4096 visites ; 259802 → 158138 évaluations de taille de grille sur l'échantillon ; la recherche binaire pure en demandait 343349 et a été rejetée |

### Limite quantifiée de la moyenne 4 voisins (passe 5)

Miroir plan z=0, caméra (0,0,2), source plane z=4 : intersection source (3x,3y,4). Grille 17×17, mesure tous les 4 pixels, 25 observations et 289 cellules. Les UV mesurés sont conservés exactement ; moyenne arithmétique des quatre voisins pour les trous : erreur UV euclidienne moyenne 0.06146, maximale 0.13975 (UV normalisés, référence analytique). Ce n'est pas une mesure sur la piscine. La moyenne n'est pas une interpolation affine exacte et peut produire du palier/décalage aux frontières. On la conserve conformément à la spécification 001 ; pas de raffinement automatique.

### États, mémoire et export

`samples.csv` contient les états de chaque échantillon et la complétude du trajet ; `matrices.csv` ne contient que les valeurs positives mesurées. Pour un calque absent de la seconde table : trajet complet = identifiant zéro explicite, trajet incomplet = inconnu. Pas de produit cartésien calques × points à écrire. Les matrices reconstruites ne servent jamais à alimenter une interpolation ultérieure. Les calques de trajets incomplets conservent le fallback de composition scalaire, avec approximation aux frontières. La couverture à quatre masques ne rajoute aucun rayon. La composition reste ordonnée par profondeur locale reconstruite ; pas d'affirmation d'exactitude pour les intersections complexes. Les données debug ne sont pas insérées dans la géométrie.

`preflight_pass5.mjs` reproduit les tests de la cinquième passe en JavaScript (aucune compilation C++). Le test C++ `v17_escargot_smoke` est enrichi (zéros, couverture, BVH invalide, spirale mince) mais volontairement non exécuté. Le laboratoire demeure CPU, sans shader natif LT, sans raffinement ni cache temporel ; budgets, vitesse et qualité à valider sur scène et matériel réels.

Contrôle final : les réflexions de parties concaves d'un même objet ne sont plus écartées par principe ; la réfraction mono-interface garde son approximation de sortie. Le choix de pas hybride est identique au choix ancien sur les 30 000 cas ; aucun gain temporel natif revendiqué.

## Cycle supplémentaire : optimisation / contrôle / simulation

### Opti
Les événements d'entrée/sortie des rectangles de calques découpent chaque ligne en intervalles semi-ouverts. La composition ignore les intervalles vides et ne parcourt que les calques actifs. Dans un intervalle inchangé, on réutilise le dernier pixel si le tuple **ordonné** des quatre observations originales et le statut exact/interpolé sont strictement identiques. Le cache est limité à l'intervalle, à l'image courante et aux trajets complets ; pas de cache approximatif, pas de rayon ajouté. Les lectures des textures sont toujours faites après reconstruction et classement par profondeur.

### Check
Les égalités à zéro, les échantillons exacts, les changements de calque, l'ordre des voisins (y compris égalités de distance), les chemins partiels et la profondeur sont traités séparément. Un zéro observé en dehors du rectangle d'un calque reste explicitement zéro. La réutilisation est désactivée dès que les ensembles d'observations diffèrent ou qu'un calque possède une observation positive partielle. Les valeurs RGB/alpha et les paramètres du récepteur sont conservés pour les pixels contributeurs. Dans un intervalle vide, les champs profondeur/poids inutilisés sont laissés à zéro ; les compteurs mesurent désormais les évaluations effectivement réalisées, pas le nombre logique de cellules reconstruites. Les tests C++ ajoutés comparent les sorties avec réutilisation active/inactive et protègent ces invariants ; ils n'ont pas été compilés ni exécutés.

### Simu
Script reproductible `v17/escargot/preflight_stencil.mjs` : modèle JavaScript indépendant, voisins par tri exhaustif, textures synthétiques, composition par profondeur et masques. Il reproduit les états et la logique de réutilisation, sans prétendre exécuter le C++ ni mesurer les FPS. 204 images synthétiques, 114054 pixels, 684324 composantes comparées (RGB prémultiplié, alpha, poids et profondeur utiles), erreur maximale 0. Les champs inutilisés à alpha zéro sont normalisés. Les gains comptent des opérations et ne tiennent pas compte du coût des événements/du cache.

| Scénario | Cellules | Compositions avant | Après | Réutilisations | Intervalles vides (cellules) |
|---|---:|---:|---:|---:|---:|
| complete | 28553 | 28553 | 17252 | 11301 | 0 |
| zero | 16562 | 16359 | 12169 | 3828 | 565 |
| partial | 16897 | 16897 | 16365 | 532 | 0 |
| sparse | 28698 | 10123 | 2688 | 1887 | 24123 |
| ties | 16944 | 16944 | 11780 | 5164 | 0 |
| dense | 6144 | 6144 | 6144 | 0 | 0 |
| thin | 256 | 256 | 256 | 0 | 0 |
| Total | 114054 | 95276 | 66654 | 22712 | 24688 |

Recherches de voisins 95276 → 89366 ; visites de calques 479535 → 325497 ; évaluations de texture synthétiques 445220 → 294613. Aucun raffinement, aucune modification de Z Reflect ni du moteur de production. Une grille dense n'offre pas de réutilisation utile ; les trajets incomplets conservent le calcul normal. Le gain temporel réel reste à mesurer lors d'une compilation autorisée. La moyenne de quatre voisins et ses limites de précision restent inchangées.

## Premier benchmark A/B natif (expérimental)

A = LumaTrace preview + Z Reflect ; B = même raster LT avec atlas Escargot CPU transféré au pixel shader, zéro caméra secondaire y compris eau. B ne fait pas de rendu secondaire. Le shader applique la contribution uniquement si matériau et profondeur du récepteur correspondent (tolérance 2,5 %, minimum 3 cm), approximation susceptible de produire des lacunes ou erreurs aux contours. La couleur source reste un proxy albédo/soleil/émission, non la radiance complète LT. Premier comparatif fonctionnel, non à qualité égale ni une validation du calcul GPU des matrices. Le mode normal de l'application ne fournit pas d'atlas. Preview utilise une lecture GPU->CPU dans les deux branches.

Scène V17.634, quatre caméras si présentes, éclairage fixe, textures plafonnées 256, MSAA 1, warmup A/B, ordre alterné, CSV par image et PNG A/B/diff, P50/P95. Le temps B inclut calcul CPU et préparation/transfert des textures float4. CI Windows WARP 320×180, trois répétitions. Package local indépendant : glisser scene.json sur LANCER_ESCARGOT_AB.bat (1280×720, cinq répétitions, 4096 rayons par défaut, matériel réel). Arguments : scene, width, height, repeats, rays. Aucun résultat de compilation ou FPS affirmé avant vérification de CI.

Contrôles : B avec atlas vide (caméras également désactivées), nombre de pixels réellement modifiés par l'atlas Escargot, retour à A avec comparaison pixel exacte. Ces contrôles sont hors chronométrage.

## Correction après premier benchmark local

La réflectivité de base diélectrique (4 % dans le bridge) n'est plus une autorisation optique. Eau, miroir ou autorisation explicite admis ; en automatique transmission >= 10 % et rugosité nominale <= .25, ou métal >= .80 et rugosité <= .25. Pas de condition de planéité ni de plafond arbitraire de surfaces. Refus explicite prioritaire. Empreinte projetée minimale 16 pixels, seuil expérimental. Le catalogue statique, les propriétaires de primitives et la validation BVH4 sont préparés une seule fois par session immuable ; changement de révision impose reconstruction. Les temps préparation initiale, projection des candidats, admission, matrices, composition et total Escargot sont journalisés séparément. Aucun cache temporel d'image. Identifiant exact d'objet transmis dans les sommets LT opaques et transparents et contrôlé au shader, indépendamment des identifiants de radiance/plan Z Reflect. Profondeur reconstruite toujours approximative. Ajout de 4 octets par sommet dans cette branche, A et B utilisent le même format. Aucun résultat de performance réel revendiqué avant CI et bench local.

Composition : index spatial 2D exact pour ensembles d'observations clairsemés, parcours en anneaux borné au rectangle pour ensembles denses, maximum quatre travailleurs CPU par grande empreinte (au moins 65 536 pixels par travailleur), écritures sur lignes disjointes et réduction des compteurs. Aucun changement de moyenne ni raffinement. Le noyau GPU matriciel complet reste à faire après validation du comparatif.

Simulation indépendante de l'index 2D : 21400 requêtes top-4, 3497850 distances ponctuelles exhaustives contre 256522 dans l'arbre, 0 divergences. Il s'agit d'un compteur algorithmique (hors coût des boîtes), non d'une mesure native. Le test C++ sur 1024 cellules vérifiera aussi les distances et les égalités. Aucun changement de la scène ou de Z Reflect sur master.

## Escargot — reconstruction GPU compacte après le deuxième benchmark

B utilise les observations BVH4 CPU (budget inchangé) sans composer les pixels sur le CPU. Paquet structuré float4 t13 : table par identifiant exact de récepteur, tuiles 32x32, listes de calques locales et observations originales positives/zéro. Une borne conservatrice distance min/max conserve le top-4 exact à tout pixel d'une tuile, y compris les égalités. Les calques partiels ajoutent leurs observations positives aux observations complètes. Le shader sélectionne ses quatre voisins, moyenne, masque, trie les calques en profondeur et échantillonne les textures GPU résidentes (y compris tier artwork), sans atlas CPU plein écran ni caméra secondaire. Limite de sécurité 64 calques par tuile et 4M enregistrements float4 ; dépassement signalé comme échec, jamais tronqué. Les rayons restent CPU et le coût de fabrication/transfert du paquet est mesuré. Aucun raffinement.

Récepteur : ID objet et matériau exacts, plus cohérence normale/plan tangent local, au lieu du rejet de profondeur moyenne à 2,5 %. Ce test est une approximation pour objets courbes ; validation visuelle requise. Source : si les triangles effectivement observés d'un calque ont une carte UV affine coplanaire cohérente, et quatre voisins positifs compatibles, intersection analytique du rayon réfléchi par le fragment avec ce plan, sans rayon BVH supplémentaire. Transformation continue limitée à l'enveloppe UV des voisins avec petite marge ; sinon moyenne 4 voisins. Les mesures exactes et zéros restent protégés. L'éclairage proxy est conservé. C = capture non chronométrée de l'ancien pipeline CPU pour comparer les défauts ; A = Z Reflect inchangé ; B = nouveau chemin GPU. Le test négatif utilise un paquet vide. Les compteurs packet_bytes/tiles/layer_lists/observations/affine_layers remplacent les compteurs d'atlas plein écran pour B. Les performances et la disparition du damier ne sont pas présumées avant exécution réelle.

Optimisation du paquet : les trajets complets partagent les quatre voisins déjà cherchés pour le récepteur ; les enregistrements de calque ne dupliquent pas les coordonnées/ID de ces échantillons (2 float4 au lieu de 3). Les trajets partiels conservent leur recherche indépendante. Simulation JS indépendante : 50000 sélections top-4 identiques au tri exhaustif, 502 cas de zéro exact, 3601700 observations candidates exhaustives contre 2903500 conservées dans les listes locales. Miroir plan 17x17 (25 mesures) : erreur UV moyenne 0.061462 avec moyenne brute, 0.000000 avec transformation géométrique et garde d'enveloppe. Ce modèle ne constitue ni une exécution HLSL ni une preuve de qualité sur la scène réelle.

## Cycle après benchmark GPU 004206

Budget 4096 et grille fixe inchangés. Travail BVH réparti entre au plus quatre travailleurs par défaut (option 1 à 8), chaque worker possède sa pile, aucune revalidation du BVH ni mutation de la scène. Quotas et ordre interne de spirale par empreinte conservés ; réduction des compteurs après jointure. Les divisions de slabs sont remplacées par réciproques précalculées par requête et marges conservatrices, intersections triangles exactes conservées. Paquet : suppression de la double shortlist sur trajets complets, pas de copie des IDs communs, table temporaire de pointeurs par calque limitée à 262144 entrées par surface, sinon recherche map. Pas de changement volontaire de format ni d'échantillons.

Contours GPU : seuls les masques de couverture sont pondérés par la distance, avec support décroissant (distance au dernier voisin + 1 pixel carré). UV, profondeur source et normales gardent leur moyenne originale ; les mesures et absences exactes demeurent prioritaires. C'est un adoucissement approximatif, pas une interpolation exacte des silhouettes ; égalités/surfaces complexes à vérifier. Verre : ajout du rebond diffus LT existant, échantillonné au point source approximatif, modulé par l'albédo et le facteur diffus ; aucun boost arbitraire du Fresnel, pas de nouveau rayon ni boucle de lampes. Le résultat peut rester faible si la couverture est insuffisante. Eau et A/C gardent leurs conventions de lumière antérieures.

Benchmark : A/B chronométrés avec recalcul du paquet à chaque image ; C CPU sérialisé, puis mesure séparée de réutilisation à pose fixe, incluant encore upload/render/readback. Comparaison exacte des pixels B parallèle et paquet issu du parcours série ; pas de gain statique présenté comme un gain caméra mobile. Portage BVH sur GPU et cache invalidé sur déplacement/changement de géométrie restent à étudier. Pas de raffinement, pas de fusion master. Compilation et qualité à valider.
