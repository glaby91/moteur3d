# Confrontation photos → scène — 8 novembre 2026

**Projet :** Moteur3D / Pripiat, branche `feature/pripyat-polissya-arcade-261107`.  
**Statut :** correction géométrique photo-guidée implémentée et régression automatique ajoutée ; **pas de certification photogrammétrique ni validation d'une capture appariée**.  
**Lot traité :** Hôtel Polissya, galerie courbe indépendante, façade extérieure du Palais Energetik.  
**Références photographiques utilisateur :** trois photographies déposées le 08/11/2026 (Energetik de face, grand portique de l'Energetik après abandon, avenue résidentielle avec poussette). Les fichiers originaux demeurent dans la conversation, **ils n'ont pas été copiés dans le dépôt**.

## Photographies étudiées, légendes et textes

### PR-ENE-1986-01 — Palais Energetik en hiver, place habitée

Vue historique en noir et blanc : une foule se rassemble sur la place enneigée devant le Palais de la culture. La façade régulière est composée de grands panneaux vitrés entre des montants étroits et des corniches rectilignes. La grande enseigne de toiture `ДВОРЕЦ КУЛЬТУРЫ ЭНЕРГЕТИК` marque le bâtiment. Une affiche verticale pend au centre et des travées de la façade latérale sont visibles à droite. La photographie autorise la restitution de la **trame de vitrage** et de la **forme du bandeau**, mais ne fournit ni longueur métrique ni date précise. L'auteur et la date exacte du cliché restent à identifier.

**Textes attestables :** inscription de toiture à conserver en caractères cyrilliques ; le contenu de l'affiche verticale n'est pas entièrement lisible : **ne pas inventer** de slogan ni d'affiche.

**Décision 3D :** conserver le mode moderne à travées ouvertes, ajouter une étude **activable** de vitrages hauts et montants métalliques. Cette option ne signifie pas que les autres éléments de la scène (usure, végétation, sols) ont été restaurés dans leur état de 1986.

### PR-ENE-RUINE-02 — Grande galerie ouverte de l'Energetik, vue rapprochée

Vue contemporaine en couleur et contre-plongée : une succession de hauts piliers de béton porte la corniche supérieure et laisse apparaître le dessous du plafond, les garde-corps et le rez-de-chaussée en retrait. Plusieurs cimes d'arbres et buissons occupent désormais les terrasses et les abords. Le titre en toiture est partiellement visible, depuis un angle différent de la photo d'hiver.

**Texte pour l'atlas :** « Les grands portiques d'Energetik, aujourd'hui dégarnis de leurs vitres sur plusieurs faces, révèlent le dessin de la structure. Les colonnes fines, les balustrades et le plafond de la galerie présentent une profondeur qu'un simple mur texturé ne saurait restituer. La végétation apporte l'indice le plus spectaculaire du temps écoulé depuis la fermeture du bâtiment. »

**Limite importante :** il n'est **pas démontré** que la vue d'hiver et cette vue rapprochée montrent exactement la même face et les mêmes baies. Leur angle relatif doit être établi par plans et points homologues avant de comparer pixel à pixel ou de conclure à la disparition de tous les vitrages du cliché historique.

### PR-RUE-ARCH-03 — Avenue résidentielle de Pripiat et scène de vie

Photographie couleur d'époque montrant une femme auprès d'une poussette rouge au premier plan. Derrière : trottoir généreux, bande gazonnée, plates-bandes fleuries, large chaussée, lampadaires régulièrement espacés, alignements serrés de peupliers et immeubles collectifs de hauteur moyenne. Cette photographie documente l'échelle humaine et l'aménagement paysager d'une ville entretenue.

**Texte pour l'atlas :** « Avant de devenir une ville abandonnée, Pripiat présentait les traits d'une cité résidentielle jeune : rues propres, parterres plantés, hautes rangées d'arbres et bâtiments espacés. Une scène quotidienne avec une poussette replace les dimensions des avenues à la mesure de leurs habitants. Cette normalité est une référence essentielle pour représenter un environnement urbain vivant plutôt qu'un décor exclusivement post-catastrophe. »

**Limites :** rue, date et auteur de l'image non identifiés ; **ne pas positionner cette avenue dans la scène à partir de la seule photo**. Les vêtements et poussette ne constituent pas des mesures de taille calibrées.

## Comparaison code — état avant/après du 8 novembre

| Élément | Avant | Correction intégrée | Statut |
|---|---|---|---|
| Polissya — auvent d'entrée | 10 m de longueur locale, projection 2,25 m | 26 m de longueur locale, projection 3,55 m, poteaux et fascia continus | Valeurs estimées sur photos |
| Galerie courbe indépendante | bout devant hôtel à X=78,1 m, largeur 3,70 m | point d'arrivée à X=75,3 m, largeur inchangée, toiture courbe + deux bouts arrondis | Modèle corrigé |
| Passage entre les deux toitures | chevauchement possible d'environ 0,30 m au niveau de leur projection X | dégagement calculé ~1,30 m entre bord externe auvent et bord de galerie à l'arrivée | Propre au plan du moteur, non mesuré sur terrain |
| Energetik, vitrage | galerie supérieure ouverte par défaut | **option** `M3D_PRIPYAT_FACADE_PERIOD=pre1986` ajoutant 10 grandes baies vitrées géométriques avec montants | Étude visuelle, pas restitution globale 1986 |
| Enseigne du palais | 9 lettres `ЭНЕРГЕТИК` déjà construites en néon | ajout d'une phrase géométrique `ДВОРЕЦ КУЛЬТУРЫ` distincte | Position et taille de police estimées |

**Source modifiée :** `experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`.

**Contrôle de régression :** `node experiments/pripyat-survey/test-photo-references-261108.mjs` ; vérifie la séparation, les dimensions de l'auvent, l'option des vitres et la présence des lettres. Ajout au workflow GitHub Actions `build-moteur3d-v14-qt.yml`. Les tests géométriques ne remplacent ni rendu photo calé ni validation humaine.

## Géométrie — confrontation au tracé de la place

Le code conserve l'arcade **courbe et autonome** (deux extrémités libres, toiture arrondie, poteaux en double file). Son orientation reste définie par le croquis et les photographies fournies antérieurement. Le nouvel auvent de l'hôtel suit la **façade** sans la remplacer par un mur plein et laisse une petite voie de circulation devant ses colonnes. Il ne doit PAS rejoindre l'arcade. La route latérale proche du Polissya doit être relevée sur une carte/topographie ; cette modification n'en déduit pas une largeur inventée.

### Points de contrôle proposés pour comparaisons d'images

- **POL-01 :** extrémité libre de l'arcade et distance au premier poteau de l'auvent de l'hôtel.
- **POL-02 :** bord extérieur de la couverture rectiligne de l'auvent, longueur sur toute sa façade.
- **POL-03 :** relation auvent / avancée / fenêtres / entrée piétonne.
- **ENE-01 :** rythme des grandes baies vitrées et pilastres, sur photo d'archive.
- **ENE-02 :** corniche et profondeur du portique moderne, depuis une caméra recalée.
- **ENE-03 :** position du texte `ДВОРЕЦ КУЛЬТУРЫ` par rapport à `ЭНЕРГЕТИК`.
- **RUE-01 :** avenue ancienne : arbres et trottoirs à reconstituer seulement après identification géographique.

## Références externes

1. [Chernobyl Visit — Pripiat avant et après](https://chernobyl-visit.com/chernobyl-diaries/pripyat-city-before-the-chernobyl-disaster-and-after/) : images de l'hôtel et d'Energetik.
2. [Atlas Obscura — Abandoned City of Prypiat](https://www.atlasobscura.com/places/abandoned-city-of-pripyat) : galerie, place et hôtel.
3. [Forgotten Chernobyl — Main Square](https://www.forgottenchernobyl.net/pripyat-then-and-now-part-1-main-square) : vues croisées de la place.
4. [Map Modernism — Energetik](https://modernism.in.ua/prypiat-cultute/) : vues photographiques historiques datées dont 1985 et vue plus récente.
5. [Domophoto — Energetik](https://domophoto.ru/building/80424/) : projet-type et description de la signalétique.

**Droits :** les images fournies dans le chat sont utilisées pour la comparaison documentaire, non transférées automatiquement vers GitHub. Les photos d'autres éditeurs sont liées et créditées ; toute reproduction dans l'atlas public impose de vérifier les droits.

## Livrables complémentaires

Atlas éditorial `Pripyat_Atlas_photographique_2026-11-08.docx` et `.pdf` : version illustrée des trois images transmises dans le chat, schéma de recalage géométrique du code et catalogue de sources. Les figures calculées à partir du code sont indiquées comme telles, **pas des captures de rendu du moteur**.
