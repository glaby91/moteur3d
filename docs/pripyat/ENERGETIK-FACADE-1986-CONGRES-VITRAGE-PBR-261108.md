# Energetik — reconstruction de la façade habitée de 1986 (lot du 8 novembre 2026)

**Périmètre :** façade principale vitrée du Palais de la culture Energetik, entrée monumentale et décor temporaire du XXVIIe Congrès du PCUS.  
**Décision éditoriale et logicielle :** l'état **1986 avant l'évacuation** est la configuration par défaut du moteur. L'état abandonné demeure disponible en variante.  
**Nature de la reconstruction :** géométrie de travail **photo-guidée** ; aucune cote cadastrale certifiée ni reconstitution photogrammétrique exhaustive.

## Photographies de référence — avec textes et limitations

### ENE-86-01 — Façade en couleur, XXVIIe Congrès, douze portraits
Photographie d'archive **fournie par l'utilisateur dans la conversation le 8 novembre 2026** (`image(20261108-170936).png`). La vue presque frontale montre une ligne continue de grands vitrages, une décoration rouge à draperies verticales en partie centrale, un panneau sombre portant `XXVII / СЪЕЗДУ / КПСС`, et douze portraits officiels alignés au niveau du bandeau horizontal. Les cheminements et plantations sont visibles en bas.

**Texte pour l'atlas :** « En 1986, la façade vitrée du Palais Energetik servit également de toile de fond aux manifestations politiques de la ville. Pour le XXVIIe Congrès du Parti communiste de l'Union soviétique, une imposante installation rouge encadra un panneau de lettres blanches, tandis qu'une série de douze portraits politiques fut accrochée en rangée devant les baies inférieures. Les panneaux étaient temporaires ; l'architecture en verre et béton leur servait de support. »

**Inscription :** le panneau comporte les mentions `XXVII`, `СЪЕЗДУ`, `КПСС`, et des lignes supplémentaires correspondant à `ДОСТОЙНУЮ ВСТРЕЧУ`. L'exacte taille de caractères, le détail des portraits et leur identification doivent être contrôlés sur un original HD. Ne pas prétendre que les portraits procéduraux sont authentiques.

### ENE-85-02 — Photographie d'hiver, 1945–1985
Photographie noir et blanc fournie dans le chat le 8 novembre 2026 (`image(20261108-171410).png`). Elle montre une affiche commémorative centrale portant les **chiffres 1945–1985**, au-dessus de marches enneigées et de la façade vitrée. La partie supérieure comporte une forme évoquant `XXXX` (quarante en chiffres romains stylisés).

**Texte pour l'atlas :** « À l'occasion du quarantième anniversaire de la victoire de 1945, la façade reçut un affichage monumental à fond sombre. Sa présence montre combien le palais pouvait changer de visage au gré des commémorations, tout en conservant ses grandes travées vitrées. »

### ENE-86-03 — Élévation d'ensemble et bâtiment voisin
Vue large fournie le 8 novembre 2026 (`image(20261108-171203).png`) : aile du Palais, façade vitrée centrale, décor rouge et grande affiche verticale distincte plus à gauche. La grande roue apparaît derrière l'extrémité orientale. La photo sert à repérer **les proportions relatives**, les accès, le bandeau, les deux ailes et la perspective de place. L'affiche verticale gauche ne doit **pas** être confondue avec le décor central du congrès.

### ENE-EXIST-04 — Architecture après abandon
Photographie du portique ouvert fournie le 8 novembre 2026 (`image(20261108-152617).png`) : poteaux très élancés, plancher profond, garde-corps et bandes de façade dépourvus de vitres à cet état. Cette image n'est **pas la façade de 1986** ; elle sert aux volumes de la structure et aux matériaux du bâtiment.

**Crédits photographiques :** photos fournies par l'utilisateur, auteurs originaux et licences inconnus. Conserver les liens et ne pas téléverser les pixels dans le dépôt public sans permission.

## État du code réellement intégré

Fichier source : `experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`  
Export et PBR : `experiments/2026-10-30-cote-ete-gt/native_v14/export_scene.js`

### Paramètres
- `M3D_PRIPYAT_FACADE_PERIOD=pre1986` : **par défaut** ; vitrage historique de la façade haute et des parties basses ; ouvertures centrales permettant de traverser l'entrée à la caméra.
- `M3D_PRIPYAT_FACADE_PERIOD=abandoned` : état antérieur du projet avec vitrages absents, signes de dégradation et ancien accès.
- `M3D_ENERGETIK_FACADE_DISPLAY=congress1986` : **par défaut** ; draperies rouges, panneau central sombre et douze panneaux de portraits.
- `M3D_ENERGETIK_FACADE_DISPLAY=victory1985` : panneau central 1945–1985.
- `M3D_ENERGETIK_FACADE_DISPLAY=soviet_emblem` : bloc descriptif provisoire pour l'emblème soviétique/СССР, **non certifié photographiquement**.
- `M3D_ENERGETIK_FACADE_DISPLAY=none` : façade libre de décor temporaire.

Les deux variables doivent être définies **au moment de l'export de scène** : ce sont des options de génération et non encore un sélecteur temps réel dans Qt.

### Matériaux PBR
- `congressRed`, `congressCrimson` : étoffe/bâche peinte à plis matérialisés, couleur rouge et variation de pigment, surface peu réfléchissante.
- `congressCharcoal` : fond sombre du panneau central et de la variante 1945–1985.
- `congressIvory` : lettres peintes claires ; graphie russe, non traduction sur la façade.
- `congressFrame` : support/cadre des portraits.
- `congressGold` : accents des représentations plus anciennes, non datées.
- `propagandaPortraitAtlas` : atlas UV **4 colonnes × 3 lignes** avec douze **représentations procédurales temporaires et non nominatives**. Ce ne sont **PAS** les portraits photographiques des dirigeants, leur licence/identité n'étant pas établie pour la diffusion. La géométrie des douze panneaux est réelle ; deux triangles texturés par panneau.

Tous ces matériaux sont déclarés au moteur avec couleur, normale et rugosité à mipmaps. La peinture PBR ne prétend pas être un relevé réel des matériaux originaux ; elle vise un rendu plausible à la distance des vues historiques.

### Géométrie
- Vitrage haut dans les travées, avec traverses et montants métalliques.
- Baies vitrées basses latérales ; entrée centrale non obstruée.
- Six larges marches sur 25,6 m de largeur de travail, palier supérieur et garde-corps métalliques. **Dimensions estimées**.
- En 1986 : absence de cicatrices en façade et de balustrades cassées dans le lot concerné.
- Enseigne supérieure `ДВОРЕЦ КУЛЬТУРЫ` conservée en objets géométriques, avec l'enseigne `ЭНЕРГЕТИК` déjà présente.

## Correction indépendante galerie devant le Polissya

La référence nommée **IMG-022 est RETIRÉE/EXCLUE du registre des sources fiables** : **ne pas utiliser cette image pour les plans, le profil ou la courbure de la galerie**. Dans le modèle, la galerie garde une courbe en plan et sa toiture possède désormais une **légère courbure transversale (faîtage bombé)**. La valeur `roofCrownRise=0.22 m` est un réglage visuel provisoire, pas un relevé. La géométrie réelle devra être confrontée à des photographies fiables provenant d'autres points de vue. Les deux extrémités restent libres et l'auvent d'entrée de l'hôtel reste distinct.

## Contrôles de qualité et suites
- `node experiments/pripyat-survey/test-energetik-facade-1986.mjs` vérifie les douze panneaux, leurs UV, les matières PBR référencées, les vitrages, les marches et les variantes.
- `node experiments/pripyat-survey/test-polissya-arcade.mjs` interdit une couverture de galerie plate en coupe.
- GitHub Actions : `Pripyat survey geometry tests` pour la syntaxe et les invariants ; `Build Moteur 3D Qt — Windows` pour export PBR, compilation, paquet et captures CPU si le workflow atteint les dernières étapes.
- **Non encore attesté :** correspondance photométrique et perspective exactes avec chaque cliché ; véritable texture de portrait d'archive ; qualité visuelle sur rendu CPU/GPU ; façades de toutes les autres ailes en état complet 1986 ; réfection de l'ensemble du décor paysager.

La priorité suivante consiste à placer un appareil photo virtuel dans l'axe de **ENE-86-01** et mesurer l'écart entre les panneaux, les impostes, les hauteurs de garde-corps et les lignes de fuite.

## Sources annexes
- [Galerie The Chernobyl Gallery — Palace of Culture](https://www.chernobylgallery.com/galleries/palace-of-culture/) pour les objets et images d'intérieurs.
- [Chernobyl Visit — avant/après](https://chernobyl-visit.com/chernobyl-diaries/pripyat-city-before-the-chernobyl-disaster-and-after/).
- [Wikimedia Commons — portraits conservés à Energetik](https://commons.wikimedia.org/wiki/Category:Portraits_in_the_palace_of_culture_in_Prypyat) : cette collection d'objets conservés dans des réserves n'établit pas automatiquement leur ordre sur la façade de 1986.
