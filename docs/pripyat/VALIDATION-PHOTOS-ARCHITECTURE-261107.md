# Pripiat — dossier de contrôle architectural et photographique
Date 2026-11-07. Objet : scène `scene_pripyat_central.js` du moteur V14.
**L'état de la scène est une reconstruction photoguidée, pas un relevé architectural.**

## Photos identifiées et contraintes observables
| Référence | Ce que la photo permet d'établir | Conséquence géométrique |
|---|---|---|
| [Timm Suess — hall, 26 mars 2009](https://commons.wikimedia.org/wiki/File:Palace_of_Culture_Lobby-pripyat.jpg), photo originale adaptée (couleur/contraste), CC BY-SA 2.0 | Mur peint haut continu, niveau bas porté par des piles ouvertes, grand escalier en extrémité, plafond plat au-dessus du bandeau peint | Conserver `m3dBuildFoyerMuralArcade`; aucune dalle traversante devant la fresque; une volée principale latérale |
| [IAEA/Dana Sacchetti — façade, 10 octobre 2010](https://commons.wikimedia.org/wiki/File:The_Palace_of_Culture_%E2%80%98Energetik%E2%80%99_on_Lenin_Square_in_Pripyat_(02710146).jpg), CC BY-SA 2.0 | Colonnade frontale, façade longiligne, soubassement et approche par les marches | Conserver les travées structurelles et l'escalier d'accès; comparer les nombres de travées à la photographie |
| [Wikimedia Commons — photos des intérieurs](https://commons.wikimedia.org/wiki/Category:Interiors_of_the_palace_of_culture_in_Prypyat) | Plusieurs pièces documentées séparément, notamment cinéma et salle sportive | Ne pas conclure à leur position relative sans plan |
| [Domophoto — projet-type 2С-06-9/70](https://domophoto.ru/building/80424/) | Type de bâtiment et **deux étages annoncés** pour le palais | Les trois altitudes de dalle de la scène ne prouvent pas trois étages historiques ; distinguer mezzanine, couverture et niveaux administratifs |
| [Reconstitution Unreal Engine de tiers](https://www.fab.com/listings/39bd11e2-0185-48e4-b75f-f2bf45d5baef) | Dimensions de référence secondaires : ~81.2×24.4 m, toit ~11.4 m, tour ~14 m | Valeurs plausibles pour le blockout; **pas de plan coté vérifié** |
| [Visite photographique — Mikhed](https://mikhed.com/en/world/2008-08-zone-pripyat.html) | Fresque, salle de spectacle, tour de scène et arche vers Polissya observées séparément | Garder des volumes distincts pour scène/théâtre, hall, arche et hôtel |

## Références complémentaires fournies (2026-11-07)
- [Dreamstime, photo éditoriale du palais Energetik, ID 317094627](https://fr.dreamstime.com/palais-culture-energetik-%C3%A0-pripyat-ville-abandonn%C3%A9e-zone-tchernobyl-ukraine-septembre-int%C3%A9rieur-du-chernobyl-d-image317094627) — lien transmis par l'utilisateur. **Page non consultable depuis l'outil de navigation** au moment de ce relevé : ne pas attribuer à cette photo un angle, des cotes ou des caractéristiques qui n'ont pas été contrôlés. Référence externe uniquement ; photo de banque d'images soumise à licence.
- [Wikimedia Commons — photographies des fresques du palais](https://commons.wikimedia.org/wiki/Category:Murals_in_the_palace_of_culture_in_Pripyat) — rechercher des vues homologues du bandeau peint, des piliers, de la galerie et de l'escalier.
- [Wikimedia Commons — salle de sport](https://commons.wikimedia.org/wiki/Category:Local_sports_hall,_Pripyat) — recherche de correspondances sur les traverses du plafond, les baies, les inscriptions et le parquet sportif.
- Photos transmises par l'utilisateur dans la conversation : `image(20261107-095642).png`, `image(20261107-095658).png` et `image(20261107-095926).png` illustrent la salle sportive, dont le parquet bleu délavé, zones rouges, lignes blanches/jaunes superposées, poutres à caissons, grandes baies latérales et enseigne cyrillique. **Ne pas confondre cette salle et le foyer à fresque** : volumes intérieurs distincts, localisation non confirmée par plan.

### Décisions de reconstruction
1. Garder l'arcade, l'escalier latéral et la fresque comme volume autonome du foyer ; aucun détail de gymnase ne doit écraser sa géométrie.
2. Prévoir pour la salle sportive un assemblage séparé : plancher peint à marquages multisports, murs vert d'eau écaillés, plafond béton à poutres ajourées, baies longues, radiateurs/tubulures et bandeau cyrillique.
3. Produire des matériaux procéduraux inspirés des références ou des textures dont les droits sont vérifiés ; **ne pas réutiliser directement** les images Dreamstime, leurs aperçus ou leurs filigranes comme atlas PBR.
4. Ne placer ni fixer définitivement les dimensions de la salle sportive sans emprise vérifiée sur un plan architectural.

## État de la réalisation
- **Implémenté** : emprise principale modulaire de 81×24.6 m (hypothèse), référence métrique, arcade basse ouverte sous la mosaïque, escalier latéral à dix-sept marches raccordé au premier palier, cages d'escaliers par niveaux, dalles ajourées, façade, coque, tour et matériaux conservés.
- **Contrôles automatiques** : volumes non inversés, dégagement devant la mosaïque, arcades ouvertes en partie basse, plusieurs lignes de visée vers la fresque, étages et paliers, hall auditorium, volumes creux du Polissya, porte d'accès et dix caméras.
- **Rendu de contrôle** : dix vues CPU en 640×360 générées par Actions, soumises à une vérification d'intégrité et de variance des pixels. Ce n'est PAS une comparaison pixel à pixel avec une photo d'origine.
- **Non certifié** : cotes exactes des pièces, orientation réelle du plan, arêtes de façade, positions des annexes, tracé des routes, nombre et usage des niveaux intermédiaires.

## Procédure d'acceptation
1. Faire correspondre un angle de caméra à une photo réelle, avec focale, hauteur et orientation explicites.
2. Relever la distance entre au moins trois repères physiques et les situer sur un plan réellement coté.
3. Remplacer les dimensions provisoires dans le référentiel commun.
4. Recalculer automatiquement les géométries et relancer la batterie de tests.
5. Examiner les dix captures CPU et vérifier les collisions en navigation.
6. Ne passer le statut **conforme historiquement** qu'après comparaison métrique et visuelle documentée.

Les images publiées restent chez leurs ayants droit ; le dépôt enregistre leurs liens et ne redistribue pas les photographies.
