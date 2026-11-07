# Pripiat / Energetik — reconstruction photographique et géométrique

**Date : 2026-11-07.** Branche : `feature/pripyat-square-survey-measurements-261107`.
État : **version de travail photo-guidée, non certifiée conforme à des plans cotés**.

## Sources vérifiées
- [Skeiron — relevé patrimonial Energetik](https://artsandculture.google.com/story/ggUh4kq6wkwB7w) : enveloppe et historique.
- [Wikimedia, Lobby-pripyat](https://commons.wikimedia.org/wiki/File:Palace_of_Culture_Lobby-pripyat.jpg) : le mur peint du hall repose sur des piliers avec des passages ouverts en partie basse ; grand escalier et mezzanine.
- [Wikimedia, fresques de l'Energetik](https://commons.wikimedia.org/wiki/Category:Murals_in_the_palace_of_culture_in_Pripyat) : dessins, retours et dégradations.
- [Wikimedia, façade Energetik](https://commons.wikimedia.org/wiki/File:Pripyat_-_Palace_of_culture.jpg) : façade répétitive, volumes ouverts, enseigne et revêtement clair.
- [Photo Polissya](https://www.wolniej.com/wymarle-miasto-prypec-dzien-drugi) : travées de fenêtres, couronnement et enseigne.
- [Photobuildings 2С-06-9/70](https://photobuildings.com/object/80424/?lang=ru) : mention de **deux étages**. Notre scène compte encore trois niveaux de dalles localement : point de désaccord important à résoudre sur un plan de coupe.
- [Carte utilisateur des repères](https://commons.wikimedia.org/wiki/File:Pripyat_center.png) ; nos coordonnées scène restent provisoires.
- [Projet de reconstruction tiers](https://awesomeness3242.itch.io/palace-of-culture-energetik) : 81.2 × 24.4 m et hauteur 11.4 m publiées comme reconstruction, et non cote issue d'un plan.

## Réalisé dans la scène
1. **Métrique** : référentiel `M3D_ENERGETIK_ARCH` pour sol, planchers, toiture, hauteur tour, position du mur peint, escalier et palier.
2. **Hall** : galerie au premier étage dégagée devant la fresque, arrière peint positionné 3 cm derrière sa texture, arcades/piliers indépendants au rez-de-chaussée, accès vers les pièces maintenus.
3. **Escaliers** : foyer avec marches calculées jusqu'à la **face supérieure** du palier ; cage est à 3 volées et galeries de retour connectées aux niveaux, au lieu des anciennes marches de 1.42 m.
4. **Enveloppe** : mur arrière composé de vrais percements alignés aux vitrages existants ; ancienne masse pleine de toiture remplacée par des acrotères et couvertines, toit parcourable.
5. **Lien Energetik–Polissya** : ouverture piétonne en arche segmentée au lieu d'une masse opaque.
6. **Polissya** : silhouette avec terrasse et portiques de toiture, cannelures de façade, enseigne `ГОТЕЛЬ ПОЛІССЯ` en matériau rouillé. Dimensions, étages et implantation toujours indicatifs.
7. **Matériaux** : palette et textures déjà présentes conservées (concrete, plaster, stone, rust, metal, volcanicTuff, glass, windowdark, energetikFresco, etc.).
8. **Caméra 02** : cadrage plus reculé pour montrer fresque entière et base des arcades.
9. **Contrôles** : `test-mosaic-clearance.mjs` et `test-energetik-architecture.mjs` contrôlent géométries réellement construites, trous d'arcades, ouvertures arrière, toit, arche et escaliers.
10. **Validation graphique automatisable** : `moteur3d_qt.exe --snapshot <vue> <fichier.png>` calcule un PNG 640×360 par rendu CPU ; workflow Windows vise les vues 02, 03 et 09.

## Limites et points ouverts
- **Pas de plan architectural coté authentifié** disponible parmi les ressources exploitées. Les volumes ne peuvent pas être qualifiés d'exacts au centimètre près.
- **Contradiction à lever** : deux étages selon base architecturale, trois niveaux de plancher dans la scène. Le second plancher peut correspondre à des zones techniques/partielles, mais cela doit être documenté par une coupe.
- **Référence géographique** : repères GPS Energetik/Polissya et coordonnées locales ne sont pas homologues ; ne pas étirer arbitrairement les routes.
- **Salle, bibliothèque, gymnase, boxe, piscine et annexes** : l'implantation réelle exige un plan de distribution ; aucun nouvel emplacement fantaisiste n'a été imposé.
- **Rendus** : les contrôles mathématiques ne garantissent pas la fidélité visuelle ; examiner les PNG produits par Windows Actions et comparer à cadrages rapprochés.
- **Rendu GPU** : cette refonte porte sur la géométrie, pas sur un backend DXR/compute ray tracing.

## Critères de validation
1. `node --check experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`
2. `node experiments/pripyat-survey/test-pripyat-survey.mjs`
3. `node experiments/pripyat-survey/test-mosaic-clearance.mjs`
4. `node experiments/pripyat-survey/test-energetik-architecture.mjs`
5. GitHub Actions Windows : export bin, compilation Qt, package, PNG `energetik_mural_arcade.png`, `energetik_foyer_stair.png`, `energetik_roof.png`.
6. Inspection manuelle des vues, notamment la totalité de la mosaïque et l'absence de dalles flottantes.

Les tests et captures doivent être rattachés au **même SHA**, et non à un ancien binaire.
