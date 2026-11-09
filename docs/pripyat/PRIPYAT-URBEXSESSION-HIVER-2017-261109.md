# Urbex Session — Pripiat en hiver et images avant 1986

**Entrée ajoutée à l'atlas Moteur3D le 9 novembre 2026.**

- **Source principale :** https://urbexsession.com/tchernobyl-pripyat/
- **Titre éditorial :** « Tchernobyl – Pripyat », Urbex Session, reportage en français.
- **Contexte de prise de vue :** l'auteur raconte avoir reçu une invitation le 27 décembre 2016, puis avoir visité la zone en hiver avec un groupe accompagné d'un guide. Le texte situe le voyage après décembre 2016 (il évoque 2017) ; la date exacte de chaque photo n'est pas fournie dans les données accessibles.
- **Crédit :** Urbex Session pour le reportage et ses prises de vue modernes ; attribution individuelle à contrôler. Les images de la rubrique « avant la catastrophe » sont des **archives distinctes** dont le photographe et la licence ne peuvent être attribués automatiquement à Urbex Session.
- **Droits :** liens et descriptions seulement ; aucune photo ni texture issue de ce reportage n'est recopiée dans le dépôt public sans autorisation.

## Fiches et textes pour l'atlas

### URB-PRP-01 — Ville de Pripiat, traversée hivernale
La rubrique « La ville de Pripyat fondée en 1970 » offre une série de vues hivernales de rues, bâtiments et espaces dégagés. L'auteur indique que la neige et l'absence relative de feuillage permettaient de distinguer les façades autrement dissimulées par la végétation. **Texte d'atlas :** « En hiver, Pripiat révèle ses volumes originaux derrière les arbres dénudés. La neige recouvre les voies et souligne les différences de niveau. Ces images de l'après-catastrophe servent avant tout à observer les formes bâties et non à restituer le sol et les espaces verts tels qu'ils étaient en 1985. »
**Consigne 3D :** études saisonnières et état abandonné, sans changer les dimensions des routes sur ce seul reportage.

### URB-ENE-02 — Palais de la Culture, vues d'exploration
La section « Le palais de la Culture » montre une série de photographies du palais sur la place Lénine. Le texte mentionne cinéma, théâtre, gymnase, bibliothèque et une piscine. Les espaces du gymnase et de l'auditorium sont documentés par d'autres sources, mais **l'existence et l'emplacement d'une piscine *dans* le palais ne sont pas établis ici** et ne doivent pas être ajoutés à la géométrie sans preuve. **Texte d'atlas :** « Sous les galeries dénudées du Palais Energetik, la structure en béton devient lisible à travers les vides laissés par les vitrages. Les volumes de l'ancienne institution culturelle sont désormais des repères architecturaux autant que mémoriels. »
**Consigne 3D :** comparer les travées et les avant-toits au mode `M3D_PRIPYAT_FACADE_PERIOD=abandoned` uniquement ; ne pas appliquer l'usure aux vitres de 1985/1986.

### URB-POL-03 — Hôtel Polissya, archives et état moderne
Deux types de documents apparaissent sous la rubrique Polissya : une série intitulée « L'hôtel Polissya avant la catastrophe », et d'autres vues récentes de l'hôtel. **Texte d'atlas :** « Le Polissya fait face au cœur de Pripiat et dialogue avec la structure courbe de la place. Les vues d'avant 1986 permettent de comprendre les volumes et les circulations historiques, tandis que les vues d'après abandon révèlent les parties abîmées et la végétation des abords. » La provenance des photos d'archives et l'orientation des vues doivent être vérifiées séparément.
**Consigne 3D :** comparer orientation, façade et auvent du Polissya à la galerie courbe indépendante déjà présente ; **pas de déplacement de l'hôtel ni création de raccordement géométrique sur ce seul texte**.

### URB-LUN-04 — Parc d'attractions et grande roue
La rubrique « Luna Park » contient de nombreuses images de la grande roue et des autos tamponneuses sous la neige, photographiées depuis différents angles. **Texte d'atlas :** « Les nacelles jaunes de la grande roue émergent d'un espace gagné par la végétation puis masqué par la neige. L'ossature métallique apparaît sans mouvement de manège, rappelant une inauguration jamais réalisée. »
**Consigne 3D :** utiliser les vues pour contrôler silhouettes, proportions et position relative des nacelles ; ne pas importer une couche de neige dans le décor habité par défaut.

### URB-PRP-05 — Albums « avant la catastrophe »
Le reportage regroupe des images d'archives de Pripiat dans une rubrique clairement distincte des photos du voyage et plusieurs rubriques « avant ». Ces documents sont susceptibles d'apporter davantage de repères pour les alignements des façades, avenues et enseignes, mais **leur datation, leurs crédits originaux et leurs liens d'image individuels restent à contrôler**. Aucune image particulière n'est considérée comme photographiquement vérifiée tant qu'elle n'a pas été ouverte et rapprochée d'un point de vue connu.

## Chronologie et niveau de preuve

- **Avant 26 avril 1986 :** photographies historiques intégrées à l'article, **photographes et dates exactes non identifiés**.
- **Hiver postérieur à décembre 2016 :** reportage Urbex Session, extérieur documenté en conditions hivernales. La date précise de prise de vue n'est pas attestée pour chaque fichier.
- **2026 :** ajout à notre atlas comme source à examiner ; **aucune dimension métrique déduite**.

## Confrontation avec la scène Moteur3D

| Élément de scène | Utilisation permise | Réserve |
|---|---|---|
| Façade vitrée d'Energetik | Comparer les hauteurs et travées des vues historiques aux portiques dégarnis modernes | Les faces observées ne sont pas toujours les mêmes ; cadrages à apparier |
| Salle de spectacle, gymnase et hall | Constituer une liste de contenus à vérifier sur d'autres photos | Le reportage mentionne les fonctions, il ne donne pas de plan |
| Polissya et galerie courbe | Référence de contexte et photographie en saison dégagée | Pas de preuve de jonction physique ni cote mesurée |
| Grande roue | Vérifier le rotor, les nacelles et la silhouette | Décor enneigé post-accident ≠ contexte 1985 |
| Végétation et voirie | Études saisonnières et distinction des époques | Ne pas conserver les broussailles post-1986 dans l'état habité |

**Décision code :** aucune modification de la géométrie ou des matériaux à partir de cette page seule. Les fiches de façade 1985/1986, les vues CPU 15–18 et les tests géométriques restent valides, sous réserve de validation d'un rendu visuel.

**Note sur l'accès :** le texte et ses rubriques sont indexés, mais les liens individuels et crédits de chacune des nombreuses images n'ont pas pu être vérifiés de façon exhaustive ; ne pas inventer de noms de fichiers ou de points de vue.
