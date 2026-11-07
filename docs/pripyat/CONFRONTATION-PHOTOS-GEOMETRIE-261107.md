# Energetik — confrontation des volumes aux photos et au projet-type

Date de l'examen : 2026-11-07. **Statut : correspondances morphologiques établies, dimensions intérieures non levées.**

## Photographies primaires et correspondance dans la scène

| Photo et provenance | Ce qu'elle démontre | Géométrie actuelle | Vue de contrôle |
|---|---|---|---|
| [Palace of Culture Lobby-pripyat.jpg](https://commons.wikimedia.org/wiki/File:Palace_of_Culture_Lobby-pripyat.jpg) | Peinture colorée de grand format **au-dessus de baies ouvertes**, piliers massifs sous le mur peint, escalier latéral et plafond haut | `m3dBuildFoyerMuralArcade`, `m3dBuildFoyerStairFlights` et plancher évidé; fresque texturée existante conservée | 02 et 03 |
| [Murals of the palace of culture](https://commons.wikimedia.org/wiki/Category:Murals_in_the_palace_of_culture_in_Pripyat) | Vocabulaire artistique et éclat encore visible de la fresque, malgré l'usure | Matériau `energetikFresco`; ne pas remplacer par une couleur uniforme | 02 |
| [Intérieurs Energetik](https://commons.wikimedia.org/wiki/Category:Interiors_of_the_palace_of_culture_in_Prypyat) | Hiérarchie du hall, accès et volume des salles | Cloisons paramétrées; distribution encore provisoire | 03, 04, 05, 06 |
| [Palais Energetik](https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat) | Modénature, hauteur relative de la façade et de la tour | `m3dBuildEnergetikFacade`, shell et enseigne conservés | 01 et 10 |
| [Photographies du Polissya](https://commons.wikimedia.org/wiki/Category:Polissya_hotel_(Pripyat)) | Masse, fenêtres, étage supérieur et couronnement | `m3dBuildPolissya`, enveloppe creuse, ouvertures et terrasse | Vérification extérieure manuelle |

## Projet-type
[Domophoto — Energetik, projet 2С-06-9/70](https://domophoto.ru/building/80424/) décrit **deux étages**. Le découpage logiciel possède actuellement les altitudes `ground=0.38`, `firstSlab=3.25`, `secondSlab=6.35`, toiture `9.4` m : **ne pas appeler automatiquement ces trois plateformes trois étages historiques**. À confirmer avec plan de coupe, car des galeries/niveaux techniques peuvent être confondus avec des étages.

## Recoupement des archives du projet-type — conflit documentaire réel

- [PhotoBuildings — série 2С-06-9/70](https://photobuildings.com/projects/2736/) décrit **3 niveaux** dans le projet-type; [Domophoto — bâtiment de Pripiat](https://domophoto.ru/building/80424/) en indique **2**. Ce sont deux inventaires ayant potentiellement des conventions de comptage différentes.
- Une [notice de rénovation d'un bâtiment de la même famille](https://proekt.sx/tip/dk781-detail.html) donne, pour **ce projet de rénovation et non nécessairement le palais de Pripiat**, 81,3 × 30,4 m d'axes, une salle de spectacle 24 × 18 m et un ensemble sur 4 niveaux dont 1 sous-sol. **Ne pas importer ces mesures sans identifier le bâtiment exact et sa variante.**
- [Une reconstruction tierce](https://www.fab.com/listings/39bd11e2-0185-48e4-b75f-f2bf45d5baef) attribue à Pripiat 81,2 × 24,4 m et 11,4 m de hauteur : données de reconstruction, utiles comme ordre de grandeur, non preuve métrique indépendante.
- Le code actuel donne 81 × 24,6 m à l'emprise `M3D_PRIPYAT_BLOCKOUT.energetik` : cohérent en ordre de grandeur avec la reconstruction tierce, **non homologué**.
- La scène doit préserver une logique spatiale à plusieurs niveaux et un hall double hauteur **sans affirmer à tort** que chacun des niveaux dessinés correspond à un « étage » historique habité.

## Décisions de modélisation actuelles
1. Garder les textures PBR et les matériaux `stone`, `concrete`, `plaster`, `volcanicTuff`, `rust`, `glass`, `energetikFresco` préexistants.
2. Préserver un hall dégagé devant la fresque et des passages sous le mur peint; supprimer les grands prismes opaques qui coupent les lignes de vue.
3. Relier les paliers aux faces supérieures des dalles; contrôler la continuité des deux escaliers et le cheminement de la visite.
4. Conserver les ouvertures vraies dans les murs, pas uniquement de fausses vitres collées sur un bloc plein.
5. Ne pas redimensionner le Polissya ni la place à partir de simples repères GPS qui ne sont pas des coins relevés.
6. Les photos prises après abandon sont des preuves de forme, pas des relevés cotés : leur perspective ne donne pas une échelle unique sans étalonnage.

## Protocole d'inspection rendu
Exporter la scène, compiler Qt, exécuter `--snapshot 1..10` en CPU, comparer les angles d'architecture et noter les occlusions. La CI vérifie l'existence de 10 images 640 × 360 non uniformes. **Ce contrôle ne constitue PAS une homologation visuelle avec les photos historiques.** La vue 02 est prioritaire pour la fresque (haut + ouverture sous les piliers); la vue 03 doit montrer les marches et le palier.

## Validation qu'il manque
- Un **plan coté** ou un relevé mesurable des niveaux et des ouvertures.
- Des photographies appariées avec position approximative du photographe, fov, cotes de référence.
- Une comparaison visuelle assistée des images CI aux photos sources, non remplacée par les seuls tests de code.
- Géoréférencement par trois points physiques homologues; plan de la place et emprises Polissya/liaison.

Sans ces sources, l'étape finale doit rester intitulée **reconstruction cohérente et vérifiable**, non « reproduction historique exacte ».
