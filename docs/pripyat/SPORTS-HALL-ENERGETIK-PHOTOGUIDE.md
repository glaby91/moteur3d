# Salle multisports de l'Energetik — aile arrière intégrée (2026-11-07)

## Constat historique vérifié
La grande salle sportive se situe **dans le palais de la Culture Energetik**, dans son aile arrière. Les baies de la salle donnent sur le **parc d'attractions et la grande roue** derrière le palais, et l'accès s'effectue depuis un couloir intérieur, pas depuis un bâtiment isolé.

Références publiques :
1. [Abandoned Kansai, visite de l'Energetik, 2011](https://abandonedkansai.com/2011/01/12/zone-of-alienation-the-palace-of-culture-energetik/) : entrée dans le gymnase depuis un couloir, grande roue visible par la façade vitrée.
2. [Forgotten Chernobyl, 2019](https://www.forgottenchernobyl.net/chernobyl-then-and-now-part-7-just-married) : le palais comporte une salle sportive donnant sur le parc d'attractions.
3. [Reconstruction UE5 tierce](https://awesomeness3242.itch.io/palace-of-culture-energetik) : aile arrière du gymnase incluse ; les pavillons piscine / boxe se trouvent plus au nord et ne sont pas identiques au gymnase.
4. [Récit en russe sur la visite de Pripiat, 2015](https://www.recenzent.org.ua/review-poezka-v-zonu-otchezhdeniya-2015/) : salle orientée vers le parc, utilisée pour le handball féminin.
5. [Wikimedia Commons — catégorie Energetik](https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat) : salle de sport et autres locaux inclus dans le palais.

## Intégration moteur — corrigée
- Le club de sport **n'est plus** référencé comme un bâtiment indépendant du blockout urbain. Son emplacement est une sous-section `M3D_ENERGETIK_ARCH.sports`.
- L'aile arrière provisoire (`x=-47..-7`, `z=-68.9..-42.7`) **chevauche volontairement la profondeur du mur arrière** du bâtiment principal pour rendre le raccord physique. Ce ne sont pas des coordonnées relevées.
- Une vraie ouverture de circulation de ~2,55 m (X=-14,05..-11,5) est percée dans le mur arrière et dans le mur de l'aile sportive. La verrière précédemment en travers de ce passage est supprimée.
- Géométrie sportive reflétée par une transformation locale de l'axe Z : la façade de grandes baies donne maintenant vers `-Z`, c'est-à-dire l'arrière du palais / direction du parc dans le repère actuel.
- Le gymnase reste un volume haut distinct des pièces du foyer et du cinéma, mais il est **attaché à l'Energetik**.
- La fresque, les matériaux du cinéma, les objets de la place et les PBR antérieurs ne sont pas modifiés.
- Une caméra spécifique du gymnase remplace l'ancienne vue 06 de contrôle.

## Éléments photoguidés de la salle
- Parquet sportif à lamelles peint en bleu délavé, zones rouges et jaunes, lignes multisports blanches et jaunes; atlas UV spécifique sur deux triangles; générateur PBR albédo, normales et rugosité.
- Façade vitrée arrière composée de baies ouvertes et piliers porteurs.
- Poutres de toiture à caissons, murs bleu-vert écaillés, corrosion des équipements et bandeau cyrillique « СИЛЬНЫЕ СМЕЛЫЕ ЛОВКИЕ ».
- Galerie avec garde-corps, but de handball rouge et blanc, structures de basket, barres de gymnastique, radiateurs et petits débris.

## Vérification
Le script `experiments/pripyat-survey/test-energetik-sports-wing.mjs` contrôle le raccord, l'absence de cloison dans le passage, l'orientation de la façade vitrée, l'absence de boîtes inversées et les deux triangles du parquet.
Il est inclus dans la CI Windows.

## Ce qui reste incertain
**L'emprise exacte de l'aile**, la longueur, les hauteurs de caisson, la position précise du passage et les proportions de la galerie ne sont pas mesurées. Le témoignage visuel confirme la topologie (aile arrière + fenêtres vers parc), pas les dimensions numériques. Ne pas attribuer un statut « conforme au plan » sans plan coté.
