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

## Statut du mapping

- **Confirmé par photo ou légende :** enveloppe béton, grandes baies absentes, coursives, gymnase, terrain peint, fresques et portraits, scène, structure d’éclairage scénique, auditorium, piscine, toiture, imprimés et déchets visibles.
- **Partiellement confirmé :** sièges (références utilisateur antérieures), bancs, plafonniers, portes, flaques, essences de bois/métal des mobiliers. Les formes sont visibles mais les détails matière ou l’identification exacte exigent le fichier original agrandi.
- **À ne pas fabriquer sans nouvelle référence :** équipements sportifs non lisibles, armoires/bibliothèques, modèles précis de luminaires, textes cyrilliques illisibles, appareils électroniques détaillés hors équipement Edison 2.
- **Étape suivante pour passer au modèle :** ouvrir les fichiers haute résolution un par un pour attribuer les sous-objets à leurs identifiants photo, choisir les vues mesurables, puis établir dimensions, UV et matériaux glTF 2.0 metallic-roughness. Les cibles de rugosité de ce relevé ne sont pas encore des textures calibrées.

## Réutilisation Unreal Engine

Garder les objets et matériaux séparés et nommés par identifiants stables; exporter ensuite en **glTF 2.0 / GLB** avec albédo, normale tangentielle et rugosité métallique, en conservant les liens d’origine des photos dans le manifeste de références. Les photographies de la galerie restent des références, pas des cartes de texture livrées. Le modèle de scène ne doit pas dépendre du format binaire privé du moteur natif.
