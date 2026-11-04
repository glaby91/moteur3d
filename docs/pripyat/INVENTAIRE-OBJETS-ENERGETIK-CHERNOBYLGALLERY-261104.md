# Inventaire photographique des objets — Palais Energetik

**État :** inventaire V1, fondé sur les 8 vues principales et les 92 miniatures de la galerie basse. Les miniatures ont été parcourues par séquences ; les objets décrits comme « confirmés » sont reconnaissables dans les vues observées ou nommés par la galerie. Les objets seulement déduits du texte ou de la fonction de la salle restent marqués comme à vérifier.

**Référence :** [The Chernobyl Gallery — Palace of Culture “Energetik”](https://www.chernobylgallery.com/galleries/palace-of-culture/). Les miniatures portent un filigrane du site : elles servent à l’identification et à la modélisation de référence, pas comme textures à redistribuer. Conserver les liens et vérifier les droits avant d’intégrer une image au projet.

## Arbre des objets repérés

```text
Energetik
├── 01. Enveloppe et accès
│   ├── Façade à travées et poteaux béton — confirmé
│   ├── Auvent / portique et coursive couverte — confirmé
│   ├── Fenêtres pleine hauteur, cadres déposés ou absents — confirmé par les vues et le texte
│   ├── Marches et paliers d’entrée — confirmé
│   └── Garde-corps / main courante manquants — confirmé par le texte
├── 02. Structure intérieure et circulations
│   ├── Couloirs et galeries avec baies ouvertes — confirmé
│   ├── Planchers, dalles et plafonds béton dégradés — confirmé
│   ├── Poteaux, poutres et retombées — confirmé
│   ├── Portes / ouvertures intérieures — confirmé, état variable
│   ├── Escaliers — signalés par le texte; formes à relever sur les vues dédiées
│   └── Arbres et végétation ayant pénétré les niveaux — confirmé
├── 03. Gymnase / salle sportive
│   ├── Grand sol sportif avec lignes peintes — confirmé
│   ├── Marquages et inscriptions murales — confirmés
│   ├── Bancs ou assises basses en périphérie — visibles sur une vue fournie
│   ├── Baies latérales et éclairage zénithal — confirmé
│   └── Équipements de sport (paniers, agrès, buts) — non validés par les miniatures consultées
├── 04. Salles, auditorium et théâtre
│   ├── Rangées de sièges / carcasses de sièges — confirmées par les photos déjà fournies
│   ├── Scène — confirmée par l’alt « Stage »
│   ├── Structure d’éclairage scénique — confirmée par l’alt « Lighting rig on stage »
│   ├── Salle de théâtre — confirmée par l’alt « Theatre »
│   ├── Plafond suspendu / éléments manquants — confirmé sur photos fournies
│   └── Rideaux, projecteurs individuels, pupitre — à relever sur originaux haute résolution
├── 05. Fresques, portraits et signalétique
│   ├── Portraits peints de dirigeants soviétiques — confirmés
│   ├── Fresques murales de figures et travailleurs — confirmées
│   ├── Panneaux / cartouches avec texte cyrillique — confirmés
│   ├── Slogan mural « DEAR COMRADES! » — confirmé par l’alt de la galerie
│   └── Panneaux de propagande sur supports extérieurs — confirmés
├── 06. Mobilier et objets abandonnés
│   ├── Assises / bancs — confirmés dans certaines salles
│   ├── Livres, journaux ou imprimés — confirmés par les photos de papier au sol
│   ├── Débris de mobilier et de plafond — confirmés
│   ├── Fragments de verre, bois, métal et plâtre — confirmés
│   ├── Matériel de discothèque « Edison 2 » — confirmé dans le texte et les photos d’illustration
│   └── Armoire, bibliothèque, luminaires autonomes — présence à confirmer photo par photo
├── 07. Sols et déchets
│   ├── Dalles et béton fissurés / ébréchés — confirmés
│   ├── Revêtement sportif peint, usé et couvert de débris — confirmé
│   ├── Éclats de verre — mentionnés dans le récit de visite
│   ├── Papiers et journaux — confirmés
│   └── Poussière, gravats, feuilles mortes et flaques — confirmés dans les vues d’intérieur
└── 08. Piscine et toiture
    ├── Piscine intérieure — confirmée par une vue principale, détails du bassin à relever
    ├── Charpente / toiture et éléments techniques — visibles dans la galerie
    └── Enseigne de toit et ossature métallique — confirmées par les références fournies antérieurement
```

## Tableau objets, modèle et matériaux cibles

Les identifiants ci-dessous renvoient au catalogue [PRIPYAT-MATERIALS-CATALOGUE.md](PRIPYAT-MATERIALS-CATALOGUE.md). La rugosité est une plage artistique initiale du catalogue, pas une mesure physique de la photographie. Aucun modèle photogrammétrique ni cote métrique ne peut être déduit des miniatures seules.

| Objet / élément | Présence et état observé | Modèle à prévoir | Matériaux et cartes à associer | Certitude |
|---|---|---|---|---|
| Poteaux, poutres, dalles et façades | Béton clair délavé, salissures, éclats, fissures; géométrie encore lisible | Modules béton séparés; bords cassés géométriques aux zones proches | `prp_concrete_panel`, `prp_concrete_raw`; albédo + normale + rugosité 0,78–0,97 | Élevée |
| Coursive / auvent | Longue portée couverte, piliers répétés, sol chargé de débris | Modules instanciés de poteau, dalle et plafond | béton; métal peint/rouillé pour pièces métalliques visibles | Élevée |
| Fenêtres et châssis | Grandes baies, vitrages absents ou brisés; menuiseries récupérées | Cadres en géométrie, panneaux vitrés manquants, éclats séparés au besoin | `prp_window_frame`; `prp_window_glass` seulement pour rares vitrages conservés; verre brisé en géométrie | Élevée |
| Main courante / garde-corps | Plusieurs portions absentes; texte indiquant que la main courante a été emportée | Segments complets là où visibles, attaches et extrémités de rupture | `prp_steel_painted`, `prp_steel_rusted`; rugosité 0,42–0,94 | Élevée pour l’absence; moyen pour détails restants |
| Escaliers, marches, paliers | Accès et circulations présents; marches usées et débris | Marches, contremarches et paliers en modules | `prp_concrete_raw`; dalles/pierre du foyer à séparer si vues le confirment | Moyenne |
| Couloir et plafonds | Baies ouvertes, plafond béton taché, panneaux/plâtre manquants | Couloir modulaire avec panneaux de plafond détachés en instances | béton, enduit `prp_plaster_pale`, métal de suspente | Élevée |
| Plafonniers rectangulaires | Visibles sur la photo du foyer fournie; trames lumineuses dans plusieurs salles | Boîtier, diffuseur et supports; variantes intacte / cassée / absente | boîtier métal peint; diffuseur verre/plastique opaque, roughness élevée | Élevée sur foyer; moyenne ailleurs |
| Gymnase et terrain peint | Salle vaste, lignes sportives, sol très usé, traces d’eau et débris | Grand volume, dalles de sol et lignes en texture/masque séparé | `prp_concrete_raw` ou revêtement sportif dédié; `prp_puddle_water` local | Élevée |
| Bancs périphériques | Assises basses le long d’un mur sur une photo fournie | Modules de banc; vérifier bois/métal sur les originaux | `prp_wood_weathered` et/ou `prp_steel_painted` | Moyenne |
| Sièges d’auditorium | Rangées de sièges avec assises manquantes / relevées sur références utilisateur | Siège par éléments: coque, piètement, assise et dossier séparés; instanciation par rangée | vinyle/peinture/bois à caler sur photo; métal sombre oxydé | Élevée pour la forme générale, matériaux à confirmer |
| Scène et gril d’éclairage | Scène et « lighting rig » nommés dans les légendes de galerie | Plateau, cadre de scène, poutres/rails et projecteurs séparés | bois peint, acier peint/rouillé, diffuseurs | Élevée pour scène et structure; faible pour petits accessoires |
| Fresques et portraits | Peintures murales, visages, dirigeants et propagande; peinture fortement altérée | Plans muraux subdivisés par panneaux; fresque comme albédo dédié, relief faible via normale | `prp_plaster_pale`; peinture pigmentée, rugosité 0,8–0,95 | Élevée |
| Signalétique et textes cyrilliques | Panneaux, slogans et lettrages visibles; inscriptions de lecture variable | Plaques ou décals séparés; conserver les caractères comme sur photo | `prp_sign_paint`; couleur + rugosité; pas de texte inventé | Élevée pour la présence, transcription à vérifier |
| Livres, journaux, feuilles | Imprimés au sol visibles dans les références; papier jauni et froissé | Feuillets, livres fermés/ouvertures simples en lots; niveaux de détail | papier mat (roughness ~0,9), albédo propre; scans lisibles seulement si droits et lisibilité validés | Élevée pour papiers; moyen pour livres identifiables |
| Gravats et débris | Bois, métal, plâtre, béton et verre mélangés au sol | Petits modules réutilisables + amas par zones, pas un simple decal | matériaux source correspondants; variation d’échelle et d’état | Élevée |
| Matériel disco Edison 2 | Légende et témoignage identifient des équipements de discothèque | Caisson électronique, cadrans/boutons, câbles; références détaillées à relever | métal peint, verre/plastique, étiquettes imprimées | Élevée pour présence; forme exacte à examiner en grand |
| Piscine | Une des huit vues principales est légendée piscine | Bassin, plages, margelles, eau stagnante ou bassin vide selon photo | carrelage `prp_tile_ceramic`, béton brut, eau `prp_puddle_water` | Élevée pour pièce; état détaillé à confirmer |
| Végétation intérieure | Bouleaux et végétation dans les étages; feuilles et branches au sol | Arbres en meshes distincts, racines/feuilles/débris en instances | `prp_bark_wet`, `prp_foliage`, `prp_soil_leaf_litter` | Élevée |
| Flaques et humidité | Sols humides et reflets visibles sur des vues de ruines | Surfaces d’eau localisées, masques d’humidité sur matériaux poreux | `prp_puddle_water`; masques d’humidité pour béton/bois | Moyenne à élevée selon salle |
| Enseigne de toiture / supports | Vue de structure de toit et enseigne sur références antérieures | Lettres séparées, support tubulaire et fixations | peinture usée + acier rouillé | Élevée sur références déjà fournies; hors miniatures basses |

## Registre photo de la page

La galerie basse expose 92 vignettes de la série `palace-of-culture-sub`; les noms ci-dessous reprennent le fichier original lié à chaque miniature. Ils servent d’identifiants stables pour annoter ensuite les objets par image. Les numéros des groupes sont des repères de navigation dans l’ordre affiché, pas des numéros de photo officiels.

| Groupe de navigation | Fichiers source | Objets dominants observés / à confirmer |
|---|---|---|
| Galerie basse 01–10 | `Palace-culture-4-2359-Edit-Edit.jpg`; `palace-culture-2356-Edit-2.jpg`; `Palace-culture-4-3849.jpg`; `palace-culture-2370-Edit-2.jpg`; `Palace-culture-4-2419.jpg`; `palace-culture-2416-2.jpg`; `Palace-culture-4-2361.jpg`; `Palace-culture-4-2429.jpg`; `Palace-culture-4-2431.jpg`; `Palace-culture-4-2434.jpg` | Extérieurs, coursives, salles ouvertes et débris; sous-classe par vue à annoter sur image pleine taille. |
| Galerie basse 11–25 | `Palace-culture-4-2439.jpg`; `Palace-culture-4-2442.jpg`; `palace-culture-2440.jpg`; `palace-culture-2472.jpg`; `Palace-culture-4-2407.jpg`; `Palace-culture-4-2408.jpg`; `Palace-culture-4-2411.jpg`; `Palace-culture-4-2446.jpg`; `Palace-culture-4-2473.jpg`; `Palace-culture-4-2475.jpg`; `Palace-culture-4-2480.jpg`; `Palace-culture-4-2478-Edit-Edit.jpg`; `Palace-culture-4-2483-Edit.jpg`; `Palace-culture-4-2489.jpg`; `Palace-culture-4-2492.jpg` | Série de vues du gymnase: lignes du terrain, grandes baies, plafond, bandeau mural et débris. Confirmation visuelle forte pour la salle; détails d’équipement sportif à contrôler. |
| Galerie basse 26–45 | `palace-culture-2477-Edit.jpg`; `Palace-culture-4-2493-2.jpg`; `Palace-culture-4-2495.jpg`; `Palace-culture-4-2496.jpg`; `Palace-culture-4-3852.jpg`; `Palace-culture-4-3854.jpg`; `Palace-culture-4-3858.jpg`; `DSC2367-Edit.jpg`; `c54-palace-culture-_DSC2383-4.jpg`; `palace-culture-2385.jpg`; `c61-palace-culture-_DSC2404-4.jpg`; `palace-culture-2357-Edit-3-2.jpg`; `palace-culture-2364-Edit.jpg`; `palace-culture-2366-Edit-Edit-2.jpg`; `palace-culture-2369-Edit.jpg`; `palace-culture-2372.jpg`; `palace-culture-2377-2.jpg`; `palace-culture-2378-2.jpg`; `palace-culture-2379.jpg`; `palace-culture-2380.jpg` | Passages entre salles, grandes ouvertures, intérieur abîmé, sol encombré; confirmer chaque attribution à partir des originaux. |
| Galerie basse 46–65 | `palace-culture-2381.jpg`; `palace-culture-2401.jpg`; `palace-culture-2402.jpg`; `palace-culture-2413.jpg`; `palace-culture-2415-2.jpg`; `palace-culture-2445.jpg`; `palace-culture-2447.jpg`; `palace-culture-2449.jpg`; `palace-culture-2450-2.jpg`; `palace-culture-3875.jpg`; `palace-culture-3878.jpg`; `Palace-culture-4-3876.jpg`; `palace-culture-2453.jpg`; `palace-culture-2456-2.jpg`; `Palace-culture-4-2462.jpg`; `palace-culture-2455.jpg`; `palace-culture-2457-2.jpg`; `palace-culture-2458.jpg`; `palace-culture-2467.jpg`; `Palace-culture-4-2465.jpg` | Couloirs et salles endommagés, mobilier/gravats et imprimés au sol; vues exactes à annoter en zoom. |
| Galerie basse 66–84 | `palace-culture-2469.jpg`; `palace-culture-2470-2.jpg`; `palace-culture-2471-2.jpg`; `palace-culture-2474.jpg`; `palace-culture-3855.jpg`; `palace-culture-3856.jpg`; `palace-culture-3857.jpg`; `Palace-culture-4-3870.jpg`; `palace-culture-2443.jpg`; `Palace-culture-4-3868.jpg`; `Palace-culture-4-3860.jpg`; `Palace-culture-4-3863.jpg`; `palace-culture-3859.jpg`; `palace-culture-3864.jpg`; `Palace-culture-4-3865-2.jpg`; `palace-culture-3865.jpg`; `Palace-culture-4-3845-Edit.jpg`; `Palace-culture-4-2552-Edit.jpg`; `Palace-culture-4-3839-Edit.jpg` | Panneaux et fresques, portraits politiques, environnement extérieur et végétation; distinguer les portraits muraux des affiches autonomes. |
| Galerie basse 85–92 | `Palace-culture-4-2392.jpg`; `Palace-culture-4-2388.jpg`; `Palace-culture-4-2389-Edit-Edit-Edit.jpg`; `Palace-culture-4-2542.jpg`; `Palace-culture-4-2394.jpg`; `Palace-culture-4-2399.jpg`; `Palace-culture-4-2390-2.jpg`; `c21-palace-culture-_DSC2386-4.jpg` | Fresques et portraits : figures humaines, compositions peintes, inscriptions cyrilliques et peinture écaillée; lecture détaillée des caractères à confirmer. |

Les 8 images de la galerie principale sont : `c29-Palace-culture-4-2545.jpg` (extérieur), `DSC2360-Edit.jpg` (intérieur), `c46-palace-culture-2452.jpg` (auditorium), `c15-Palace-culture-4-2406.jpg` (piscine), `DSC2487-Edit-Edit.jpg` (toiture), `c2-palace-culture-3880.jpg`, `DSC2418.jpg` (gymnase), `c70-Palace-culture-4-3861.jpg`. Les titres de cette galerie confirment explicitement extérieur, intérieur, auditorium, piscine, toit et gymnase.

## Mapping individuel — lot 1 (photos 01–32)

Les numéros suivent l’ordre des miniatures de la galerie basse. Les vues liées sont ouvertes en grand format (850 px côté long). « À confirmer » signale qu’un petit objet n’est pas identifiable avec assez de précision pour en faire un asset séparé.

| N° | Fichier photo | Pièce / objets identifiés | État et décision de modélisation | Matériaux probables | Confiance |
|---:|---|---|---|---|---|
| 01 | `Palace-culture-4-2359-Edit-Edit.jpg` | Façade principale, enseigne de toit, travées et poteaux | Parement dégradé; baies largement ouvertes; végétation au pied | `prp_concrete_panel`, `prp_steel_rusted`, `prp_foliage` | Élevée |
| 02 | `palace-culture-2356-Edit-2.jpg` | Façade d’angle, portique et coursive couverte | Vitrages absents, sol extérieur envahi de feuilles | béton, acier peint/rouillé, feuilles | Élevée |
| 03 | `Palace-culture-4-3849.jpg` | Escalier intérieur, rampe, panneau rouge de propagande | Panneau appuyé contre la rampe; escalier sombre et poussiéreux | béton brut, acier peint, `prp_sign_paint` | Élevée |
| 04 | `palace-culture-2370-Edit-2.jpg` | Salle traversante avec comptoirs/meubles bas et ouverture vers d’autres pièces | Plafond et parements cassés; mobilier incomplet; gravats au sol | béton, enduit, bois vieilli, gravats | Moyenne |
| 05 | `Palace-culture-4-2419.jpg` | Gymnase, terrain peint, grandes baies, plafond à caissons | Lignes sportives très effacées; fenêtres sans vitrage; débris | sol sportif peint, béton, métal, feuilles | Élevée |
| 06 | `palace-culture-2416-2.jpg` | Gymnase, bandeau mural avec lettrage cyrillique | Peinture murale écaillée; rails/éléments bas au pied du mur | enduit, peinture signalétique, métal rouillé | Élevée |
| 07 | `Palace-culture-4-2361.jpg` | Gymnase depuis l’extrémité, poteaux et baies latérales | Sol jonché de fragments; lumière latérale forte | béton, revêtement sportif, verre brisé, débris | Élevée |
| 08 | `Palace-culture-4-2429.jpg` | Gymnase, marquages du terrain, poutres/plafond | Usure et dépôts au sol; structure répétitive à modulariser | béton, peinture au sol, métal | Élevée |
| 09 | `Palace-culture-4-2431.jpg` | Gymnase, lignes de jeu et côté des baies | Eau/salissures et feuilles dans la salle; équipement fixe non lisible | revêtement sportif, béton humide, débris | Élevée |
| 10 | `Palace-culture-4-2434.jpg` | Vue en hauteur du gymnase et d’une plateforme/bordure longitudinale | Délimitation surélevée en bord de salle; vérifier si tribune ou passerelle sur une autre vue | béton, garde-corps métallique | Moyenne |
| 11 | `Palace-culture-4-2439.jpg` | Extrémité du gymnase, équipement de jeu fixe au fond (structure de panier probable) | Structures de sport très dégradées; identification de l’équipement à vérifier | métal rouillé, sol peint, béton | Moyenne |
| 12 | `Palace-culture-4-2442.jpg` | Gymnase, panneau vert mobile, slogan mural, ouverture de scène/alcôve | Panneau posé contre le mur; ne pas traiter comme mobilier permanent | panneau peint, béton, enduit | Élevée |
| 13 | `palace-culture-2440.jpg` | Gymnase et cadre métallique autoportant au centre du terrain | But/cadre d’exercice possible, usage non déterminable sur cette vue | acier rouillé, sol sportif | Moyenne |
| 14 | `palace-culture-2472.jpg` | Plaque verticale portant une consigne incendie en russe | Inscription rouge lisible, « При пожаре звонить 0-01 »; plaque montée sur mur clair | `prp_sign_paint`, enduit | Élevée |
| 15 | `Palace-culture-4-2407.jpg` | Bassin rectangulaire intérieur, margelles et baies latérales | Bassin vide, dépôts et végétation; modéliser volume du bassin avant l’eau | carrelage, béton brut, enduit, feuilles | Élevée |
| 16 | `Palace-culture-4-2408.jpg` | Piscine depuis l’extrémité, plages et bordures | Long bassin vide; débris et humidité; ouvertures sans vitrages | carrelage, béton, acier oxydé | Élevée |
| 17 | `Palace-culture-4-2411.jpg` | Petit bassin / cuve attenante à la piscine, échelle métallique | Eau absente; échelle en place; carreaux et margelles cassés | carrelage céramique, béton, acier rouillé | Élevée |
| 18 | `Palace-culture-4-2446.jpg` | Salle de rangement ou bibliothèque, rayonnages/armoires en bois | Meubles encore en place, panneaux manquants; contenu non lisible | `prp_wood_weathered`, enduit, béton | Élevée pour rayonnages; fonction à confirmer |
| 19 | `Palace-culture-4-2473.jpg` | Petite salle à baie, radiateur sous fenêtre, débris et pièces de mobilier | Intérieur vidé et sombre; éléments au sol à séparer en quelques modules | béton/enduit, métal peint, bois | Élevée |
| 20 | `Palace-culture-4-2475.jpg` | Façade latérale vue depuis un étage, fenêtres répétées | Cadres absents ou incomplets; végétation proche du bâtiment | béton, cadre métallique, feuillage | Élevée |
| 21 | `Palace-culture-4-2480.jpg` | Végétation haute et sol extérieur forestier depuis le toit | Bouleaux/arbres dans l’emprise; feuilles au sol | `prp_bark_wet`, `prp_foliage`, terre/feuilles | Élevée |
| 22 | `Palace-culture-4-2478-Edit-Edit.jpg` | Ossature de l’enseigne de toiture, lettres déposées/renversées | Treillis métallique exposé; peinture rouge/blanche écaillée | acier peint et rouillé, peinture d’enseigne | Élevée |
| 23 | `Palace-culture-4-2483-Edit.jpg` | Lettre monumentale renversée et mécanisme/support de l’enseigne | Grande lettre métallique inclinée; rivets et renforts visibles | acier rouillé, peinture rouge/blanche | Élevée |
| 24 | `Palace-culture-4-2489.jpg` | Enseigne cyrillique sur son treillis, vue côté arrière | Lettres volumétriques et structure en treillis; corrosion avancée | acier peint, rouille | Élevée |
| 25 | `Palace-culture-4-2492.jpg` | Enseigne de toit et vue de la ville | Plusieurs lettres subsistent; treillis, toiture plate et végétation lointaine | acier rouillé, béton/toiture bitume | Élevée |
| 26 | `palace-culture-2477-Edit.jpg` | Verso des grandes lettres d’enseigne, ossature porteuse | Lettres et montants arrière visibles; séparer lettres et treillis dans le modèle | acier peint écaillé, acier rouillé | Élevée |
| 27 | `Palace-culture-4-2493-2.jpg` | Toiture plate, parapet et vue vers le bâtiment voisin | Membrane/étanchéité dégradée, végétation poussant autour | `prp_roof_bitumen`, béton, feuillage | Élevée |
| 28 | `Palace-culture-4-2495.jpg` | Panorama de toiture, parapet, immeubles voisins et arbres | Toiture à grandes nappes; détails techniques trop petits pour identifier des équipements | bitume, béton, feuillage | Moyenne |
| 29 | `Palace-culture-4-2496.jpg` | Petit local intérieur avec pictogramme noir de visage / masque peint sur porte ou panneau | Peinture murale écaillée; pictogramme distinctif à conserver comme decal/texture dédiée | enduit, peinture mate, bois ou métal du support à confirmer | Élevée pour le pictogramme |
| 30 | `Palace-culture-4-3852.jpg` | Galerie intérieure, fresque colorée sur grand mur, baies, comptoirs/meubles bas | Fresque multicolore encore identifiable; plafond manquant et débris | enduit + texture de fresque dédiée, béton, bois | Élevée |
| 31 | `Palace-culture-4-3854.jpg` | Amas d’objets et déchets : papiers, tissus, fragments, petits objets mélangés | Ne pas modéliser chaque fragment; créer plusieurs amas réutilisables, papiers distincts au premier plan | papier, tissu, bois, plâtre, verre, métal | Élevée pour amas; faible pour objets individuels |
| 32 | `Palace-culture-4-3858.jpg` | Pièce sombre cadrée par une fenêtre sans châssis/vitrage, arbres dehors | Embrasure massive; intérieur presque vide; cadre et enduit fissurés | béton, enduit, verre absent, feuillage extérieur | Élevée |
| 33 | `DSC2367-Edit.jpg` | Cour arrière, façade latérale et arbres entre les bâtiments | Sol extérieur envahi; façades préfabriquées à fenêtres répétées | béton, cadres absents, végétation | Élevée |
| 34 | `c54-palace-culture-_DSC2383-4.jpg` | Petite salle ouverte sur une baie, fragments de vitrages au sol, morceau de mobilier | Châssis largement absent; gravats et verre au premier plan | béton, enduit, verre brisé, bois | Élevée |
| 35 | `palace-culture-2385.jpg` | Plafond avec grand motif circulaire/rosette en relief | Décor architectural central à modéliser en géométrie ou normal map selon distance; enduit qui se détache | béton/enduit peint, poussière | Élevée |
| 36 | `c61-palace-culture-_DSC2404-4.jpg` | Salle encombrée de tables/chaises ou éléments de mobilier renversés | Mobilier cassé empilé près d’une ouverture; identification chaise/table à garder générique | bois peint, acier, débris | Moyenne |
| 37 | `palace-culture-2357-Edit-3-2.jpg` | Portique extérieur, poteaux et dessous d’auvent | Travées répétées, façade abîmée et végétation qui gagne l’accès | béton, enduit, feuillage | Élevée |
| 38 | `palace-culture-2364-Edit.jpg` | Galerie extérieure couverte, grandes ouvertures, carreaux/dalles cassés | Longue circulation; gravats et végétation; manque des cadres de fenêtres | béton, carreaux, métal de châssis, feuilles | Élevée |
| 39 | `palace-culture-2366-Edit-Edit-2.jpg` | Vue de la cour et du bâtiment au travers des arbres | Façade répétitive, arbres et sol envahi; repère de composition extérieure | béton, feuillage, sol végétal | Élevée |
| 40 | `palace-culture-2369-Edit.jpg` | Salle longue à baies, bancs/assises périphériques et sol effondré | Plusieurs ouvertures vides; mobilier bas conservé le long du mur | béton, bois/métal de bancs à confirmer sur l’original, feuilles | Moyenne |
| 41 | `palace-culture-2372.jpg` | Grande salle ouverte soutenue par poteaux, sol avec flaques | Béton brut, infiltrations et eau stagnante; lignes de structure lisibles | béton, `prp_puddle_water`, rouille locale | Élevée |
| 42 | `palace-culture-2377-2.jpg` | Petite salle, fenêtre encadrée, plafonniers rectangulaires en grille | Plusieurs diffuseurs au plafond; intérieur vide et poussiéreux | béton/enduit, corps métal peint, diffuseurs opaques | Élevée |
| 43 | `palace-culture-2378-2.jpg` | Salle avec baie, table ou mobilier bas renversé, plafonniers | Gravats autour du mobilier; fenêtres absentes | bois, acier, enduit, verre brisé | Moyenne |
| 44 | `palace-culture-2379.jpg` | Amas de profilés, tôles et pièces métalliques de plafond ou mobilier | Éléments répétés en vrac; réutiliser en kit de débris métal plutôt que créer chaque pièce | métal rouillé, peinture écaillée | Moyenne |
| 45 | `palace-culture-2380.jpg` | Couloir avec portes latérales, plafonds et sol encombrés | Fuite de perspective; gravats en tas au fond; portes boisées ou peintes | béton/enduit, bois peint, poussière | Élevée |
| 46 | `palace-culture-2381.jpg` | Galerie longue côté fenêtres, parapet bas et amas de briques/débris | Baies sans vitrage; rupture localisée de maçonnerie et revêtement | béton, brique, verre, bois | Élevée |
| 47 | `palace-culture-2401.jpg` | Structure de mobilier en bois brisée et fragments de chaises/tables | Pieds, traverses et panneaux cassés; à traiter comme un petit amas modulaire | `prp_wood_weathered`, métal, plâtre | Élevée |
| 48 | `palace-culture-2402.jpg` | Autre amas de mobilier renversé, avec assises et cadres de bois | Meubles cassés, morceaux dispersés sur le sol; ne pas en déduire une armoire intacte | bois, tissu/vinyle possible, acier, gravats | Élevée |
| 49 | `palace-culture-2413.jpg` | Couloir/galerie vitrée, sol très encombré de débris et feuilles | Longue perspective; murs et plafond tachés; menuiseries absentes | béton/enduit, feuilles, verre, débris | Élevée |
| 50 | `palace-culture-2415-2.jpg` | Petite salle avec plusieurs sièges bas alignés près d’une fenêtre | Assises simples restantes; leur attribution (attente ou salle de réunion) n’est pas établie | bois ou métal peint, rembourrage à vérifier | Moyenne |
| 51 | `palace-culture-2445.jpg` | Feuille/journal en cyrillique parmi gravats et livres | Imprimé froissé identifiable comme papier; texte non retranscrire depuis cette vue | papier jauni, gravats, tissu | Élevée |
| 52 | `palace-culture-2447.jpg` | Couverture ou affiche imprimée avec deux personnages en rouge | Imprimé posé sur une table; conserver comme élément graphique distinct si l’image est lisible en original | papier, bois de table | Élevée |
| 53 | `palace-culture-2449.jpg` | Journaux et documents soviétiques, portrait imprimé et étoffe bleue | Papier et tissu posés dans une pièce; textes et identité des personnes non validés | papier, textile, poussière | Élevée |
| 54 | `palace-culture-2450-2.jpg` | Pile de journaux, livres et documents sur une surface de mobilier | Pages jaunies, couvertures colorées; objets individuels à instancier en faible nombre | papier/carton, bois, débris | Élevée |
| 55 | `palace-culture-3875.jpg` | Grande salle avec rangées d’armatures/sièges, structure de plafond et câblage suspendu | Salle d’assemblée ou auditorium probable; vérifier la fonction sur vue plus large et originaux | acier peint/rouillé, béton, tissu ou bois des assises à confirmer | Moyenne |
| 56 | `palace-culture-3878.jpg` | Grande pièce avec panneaux/armoires murales, table centrale et porte | Salle de club/réunion probable; objets de mobilier à modéliser séparément, sans inventer leur usage | bois peint, métal, béton | Moyenne |
| 57 | `Palace-culture-4-3876.jpg` | Salle basse avec affiches et documents aux murs, livres/papiers au sol | Nombreux éléments plats; affiches en décals, déchets en amas | enduit, papier, carton, bois | Élevée |
| 58 | `palace-culture-2453.jpg` | Salle avec rangées de grandes tables ou sièges renversés, plafond à caissons | Mobilier lourd brisé et accumulé; géométrie répétitive en modules | bois, acier, béton, poussière | Élevée |
| 59 | `palace-culture-2456-2.jpg` | Amas de mobilier avec caissons, tiroirs ou pupitres bas et pieds métalliques | Rangements/pupitres cassés; sous-types à distinguer à partir de détails | bois peint, métal rouillé | Moyenne |
| 60 | `Palace-culture-4-2462.jpg` | Série de photos/portraits accrochés ou exposés sur une cloison turquoise | Images encadrées ou panneaux d’exposition encore suspendus; chaque image = plan distinct | cadre bois/métal, verre terne, impression papier | Élevée |
| 61 | `palace-culture-2455.jpg` | Salle avec ouverture vers un espace de stockage et morceaux de mobilier | Seuil intérieur encombré; rayonnages ou panneaux en arrière-plan | béton/enduit, bois, papier et débris | Moyenne |
| 62 | `palace-culture-2457-2.jpg` | Panneaux avec portraits et reproductions photographiques abîmés | Deux grandes images sur support bleu; peinture et papier délavés | support peint, papier, cadre | Élevée |
| 63 | `palace-culture-2458.jpg` | Salle de rangement encombrée de meubles, cadres et panneaux illustrés | Objets entreposés/renversés; ne pas assimiler aux sièges d’auditorium sans preuve | bois, papier, métal, tissus | Élevée |
| 64 | `palace-culture-2467.jpg` | Grande galerie avec fenêtre, porte latérale et alignement de conduites/radiateurs ou bancs bas | Vaste circulation; déchets au sol; identifier les éléments bas comme radiateurs seulement après zoom | béton, métal peint, verre absent, feuilles | Moyenne |
| 65 | `Palace-culture-4-2465.jpg` | Grand amas de livres, cahiers, feuilles et couvertures colorées | Bibliothèque ou archives dispersées; géométrie en livres réutilisables + feuilles | papier/carton, poussière, gravats | Élevée |
| 66 | `palace-culture-2469.jpg` | Livres ouverts et fermés, couverture illustrée et document typographié | Étude de petites piles de livres; typographie à garder floue sauf référence lisible licenciée | papier, carton, encre mate | Élevée |
| 67 | `palace-culture-2470-2.jpg` | Livres illustrés, cahiers et pages ouvertes au sol | Papier déformé, couvertures colorées et poussière | papier/carton, débris | Élevée |
| 68 | `palace-culture-2471-2.jpg` | Journaux avec portrait imprimé, livres et photographies | Documents d’archives/documentaires; la photo imprimée ne prouve pas la présence d’un portrait peint dans cette pièce | papier jauni, encre, couverture | Élevée |
| 69 | `palace-culture-2474.jpg` | Table ou pupitre poussiéreux devant rideau/porte et mur peint | Surface de travail isolée; petit objet jaune sur la table, fonction non identifiée | bois peint, poussière, métal | Moyenne |
| 70 | `palace-culture-3855.jpg` | Livres, brochures, papiers et textile rouge entassés parmi les débris | Amas de bibliothèque/archives; plusieurs formats de livres et feuilles | papier, textile, gravats | Élevée |
| 71 | `palace-culture-3856.jpg` | Livre fermé isolé sur une table, couverture portant un titre cyrillique | Objet de premier plan à modéliser comme livre; titre translittéré à vérifier avant d’en faire un texte | papier/carton, table en bois peint | Élevée |
| 72 | `palace-culture-3857.jpg` | Livres et textiles mélangés à des déchets, papiers et petites boîtes | Amas d’objets divers; boîtes/tissus seulement des formes génériques à cette échelle | papier, tissu, carton, débris | Élevée |
| 73 | `Palace-culture-4-3870.jpg` | Salle d’accessoires/portraits : tableaux et portraits peints empilés avec toiles | Confirme un stock de décors et portraits de dirigeants; tableaux séparés, cadres/toiles variés | bois de châssis, toile peinte, papier, verre absent | Élevée |
| 74 | `palace-culture-2443.jpg` | Détail d’une fresque colorée à motifs géométriques et personnages | Pigments vifs malgré l’usure; capturer en texture de panneau mural avec relief léger | enduit peint, albédo de fresque, normale très faible | Élevée |
| 75 | `Palace-culture-4-3868.jpg` | Portrait peint d’un dirigeant soviétique en costume, tableau appuyé au mur | Toile/panneau autonome avec cadre; peinture salie mais lisible | toile peinte, bois, poussière | Élevée |
| 76 | `Palace-culture-4-3860.jpg` | Insigne circulaire rouge/blanc avec portrait et lettrage cyrillique | Panneau/affiche de propagande; lettrage « ВЛКСМ » visible à conserver exactement | peinture sur support, métal/bois à confirmer | Élevée |
| 77 | `Palace-culture-4-3863.jpg` | Portraits de dirigeants peints sur panneaux, grands formats empilés | Plusieurs tableaux et panneaux, dont visages en costume; supports inclinés | toile/panneau peint, bois, poussière | Élevée |
| 78 | `palace-culture-3859.jpg` | Pièce de stockage de tableaux, portraits et éléments de décor empilés | Confirme réserve de décors du théâtre; piles hétérogènes; modéliser 3–5 silhouettes distinctes | bois, toile, carton et pigments | Élevée |
| 79 | `palace-culture-3864.jpg` | Portrait officiel masculin peint, costume sombre et décoration de revers | Toile/panneau indépendant; bords et cadre à contrôler sur original | toile, bois, peinture mate | Élevée |
| 80 | `Palace-culture-4-3865-2.jpg` | Portrait en pied/buste d’un dirigeant soviétique avec décorations | Portrait officiel sur fond neutre; asset tableau séparé | toile peinte, châssis bois, encre/pigment | Élevée |
| 81 | `palace-culture-3865.jpg` | Autre portrait peint de dirigeant, appuyé contre un mur | Tableau vertical avec marques d’usure; ne pas réutiliser la même image que les autres portraits | toile/panneau peint, cadre/châssis bois | Élevée |
| 82 | `Palace-culture-4-3845-Edit.jpg` | Escaliers extérieurs et façade principale en arrière-plan | Gradins/marches en béton partiellement masqués par la végétation; vue utile aux accès | béton, terre/feuilles, feuillage | Élevée |
| 83 | `Palace-culture-4-2552-Edit.jpg` | Façade d’Energetik vue à travers les arbres | Travées et baies répétées, colonnes, arbres et sol envahi | béton, enduit, végétation | Élevée |
| 84 | `Palace-culture-4-3839-Edit.jpg` | Bâtiment annexe ou façade arrière basse sous une ligne d’arbres | Volume peu détaillé; attribuer au complexe seulement, fonction inconnue | béton préfabriqué, cadres de fenêtres absents, végétation | Moyenne |
| 85 | `Palace-culture-4-2392.jpg` | Panneau vertical extérieur avec portrait féminin et inscription russe | Signalétique peinte sur panneau métallique, fortement rouillé; texte partiellement lisible | tôle rouillée, peinture de signalétique | Élevée |
| 86 | `Palace-culture-4-2388.jpg` | Panneau vertical avec emblème circulaire dans une couronne et motif décoratif | Plaque peinte rouillée; modèle séparé de la fresque murale | tôle, peinture écaillée, corrosion | Élevée |
| 87 | `Palace-culture-4-2389-Edit-Edit-Edit.jpg` | Gros plan d’un médaillon circulaire dans une couronne de laurier | Relief ou médaillon peint presque effacé; normal map douce, géométrie peu profonde possible | béton/enduit peint, pigments ocres, rouille/coulures | Élevée |
| 88 | `Palace-culture-4-2542.jpg` | Fresque d’une travailleuse, date « 1928–1931 » au bas du panneau | Figure féminine frontale, peinture craquelée et décolorée; fresque plane | enduit peint, texture albédo dédiée + masque d’usure | Élevée |
| 89 | `Palace-culture-4-2394.jpg` | Fresque portrait d’une jeune femme souriante en tenue de travail | Portrait peint isolé, bord de panneau non visible | enduit, pigments rouges/bruns, peinture mate | Élevée |
| 90 | `Palace-culture-4-2399.jpg` | Fresque d’un homme au casque de travail, bouche ouverte, fond rouge | Peinture murale et fissures; garder le visage comme panneau unique, sans le transformer en personnage 3D | enduit peint, coulures, albédo dédié | Élevée |
| 91 | `Palace-culture-4-2390-2.jpg` | Fresque d’une travailleuse au premier plan et scène industrielle en arrière-plan | Grand panneau de propagande; peindre avec masque de décollement et taches | enduit peint, pigments, salissures | Élevée |
| 92 | `c21-palace-culture-_DSC2386-4.jpg` | Vue lointaine de Pripiat et grande roue à travers des arbres | Repère de contexte urbain; la grande roue est au loin, pas un objet intérieur d’Energetik | feuillage, ciel, silhouette métallique lointaine | Élevée |

## Mapping individuel — vues principales (H01–H08)

| N° | Fichier photo | Pièce / objets identifiés | État et décision de modélisation | Matériaux probables | Confiance |
|---|---|---|---|---|---|
| H01 | `c29-Palace-culture-4-2545.jpg` | Façade latérale longue, poteaux, travées ouvertes, enseigne au toit et arbres | Modules de façade et auvent; verre presque entièrement absent; ajouter végétation au pied | béton préfabriqué, acier peint/rouillé, feuilles | Élevée |
| H02 | `DSC2360-Edit.jpg` | Foyer monumental : escalier, palier, piliers, grande fresque colorée et plafonds en dalles | Volumes imbriqués, fresque sur panneau mural; dalles de parement manquantes; luminaires suspendus à relever | pierre/carrelage du foyer, béton, enduit peint, métal peint | Élevée |
| H03 | `c46-palace-culture-2452.jpg` | Auditorium/théâtre : rangées de sièges fortement dégradées, scène ou estrade, plafond et éclairages | Instancier les rangées, modéliser sièges comme coque/piètement/assise; plafond avec éléments manquants et structure visible | métal, bois ou vinyle des sièges à préciser par les références utilisateur, béton/enduit | Élevée |
| H04 | `c15-Palace-culture-4-2406.jpg` | Piscine : bassin vide allongé, plages, margelles, grandes fenêtres et plafond à caissons | Bassin à carreler et géométrie de gradins/bords; pas d’eau dans l’état photographié; vitrage manquant | carrelage céramique, béton, métal rouillé | Élevée |
| H05 | `DSC2487-Edit-Edit.jpg` | Toit et enseigne « Энергетик », grandes lettres métalliques, structure triangulée, vue urbaine | Reconstituer lettre par lettre et treillis de support; corrosion et panneaux manquants distincts | acier peint écaillé/rouillé, toiture bitume, végétation | Élevée |
| H06 | `c2-palace-culture-3880.jpg` | Salle vide ouverte sur grande baie, plafond et murs dégradés, eau/débris au sol, silhouette de mobilier à droite | Volumes simples, embrasure profonde, flaques localisées et débris en premier plan | béton/enduit, eau stagnante, métal/bois des restes de mobilier | Élevée |
| H07 | `DSC2418.jpg` | Gymnase : terrain peint, lignes sportives, slogan cyrillique, plafond à caissons et baies | Confirme les entrées 05–13; terrain sans panier clairement lisible dans cette vue | revêtement sportif, béton, enduit peint, métal | Élevée |
| H08 | `c70-Palace-culture-4-3861.jpg` | Panneau d’exposition incliné avec portraits/photos et documents imprimés, salle aux débris | Panneau autonome avec cases d’affichage; contenus imprimés séparés et peu contrastés; ne pas inventer de visages illisibles | support bois/métal, impressions papier, verre absent ou terne | Élevée |

## Statut du mapping

- **Parcours terminé pour cette page :** 92 photos de la galerie basse + 8 vues principales examinées en grand format. L’inventaire est associé aux noms de fichiers de la galerie et les objets sont regroupés dans l’arbre ci-dessus.
- **Confirmé par photo ou légende :** enveloppe béton, grandes baies absentes, coursives, gymnase, terrain peint, fresques et portraits, scène, structure d’éclairage scénique, auditorium, piscine vide, toiture, rayonnages, panneau d’exposition, livres, journaux, affiches et déchets visibles.
- **Partiellement confirmé :** sièges (références utilisateur antérieures), bancs, plafonniers, portes, flaques, essences de bois/métal des mobiliers. Les formes sont visibles mais les détails matière ou l’identification exacte exigent le fichier original agrandi.
- **À ne pas fabriquer sans nouvelle référence :** équipements sportifs non lisibles, armoires/bibliothèques, modèles précis de luminaires, textes cyrilliques illisibles, appareils électroniques détaillés hors équipement Edison 2.
- **Étape suivante pour passer au modèle :** valider avec les références utilisateur les dimensions des sièges et luminaires, examiner les vues originales pour les petits détails encore ambigus, puis établir dimensions, UV et matériaux glTF 2.0 metallic-roughness. Les cibles de rugosité de ce relevé ne sont pas des mesures calibrées.

## Réutilisation Unreal Engine

Garder les objets et matériaux séparés et nommés par identifiants stables; exporter ensuite en **glTF 2.0 / GLB** avec albédo, normale tangentielle et rugosité métallique, en conservant les liens d’origine des photos dans le manifeste de références. Les photographies de la galerie restent des références, pas des cartes de texture livrées. Le modèle de scène ne doit pas dépendre du format binaire privé du moteur natif.
