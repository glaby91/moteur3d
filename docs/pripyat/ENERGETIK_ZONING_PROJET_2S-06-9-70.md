# Energetik — identification architecturale des zones (audit 2026-11-06)

## Références et prudence
- Projet type **2С-06-9/70**, attesté pour le palais Energetik : https://photobuildings.com/object/80424/?lang=ru ; https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat
- Fiche descriptive détaillée d'une réalisation du projet type, **pas un relevé de Pripyat** : https://proekt.sx/tip/dk781-detail.html
- Gabarit en axes de la réalisation documentée : 81,3 x 30,4 m, surface de construction 2953 m² (incluant accès), 3 niveaux hors sol et 1 sous-sol. **À ne pas confondre avec un relevé exact de l'Energetik.**
- Le modèle actuel a une enveloppe approximative de 81 x 24,6 m ; écart de profondeur à examiner avant toute modification.
- L'Energetik de Pripyat possède des modifications par rapport au projet type : grande façade vitrée, piscine, espace boxe selon témoignages ; ne pas copier aveuglément le projet générique.
- Projet d'une reconstitution indépendante fondée sur des plans : https://www.reddit.com/r/chernobyl/comments/19fhv08/currently_recreating_the_palace_of_culture/ ; plans non disponibles dans cette recherche.

## Zoning fonctionnel à reconstruire
| ID | Fonction | Indications du projet type | Statut pour Pripyat |
|----|----------|----------------------------|---------------------|
| A | Entrée, terrasse, vestibule-foyer, vestiaires | Foyer de distribution 540,6 m² dans la réalisation documentée | Confirmé fonctionnellement ; emplacement précis à recaler |
| B | Auditorium / cinéma / concert | Salle 24 x 18 m, 408,7 m², hauteur 11,6 m dans la réalisation documentée | Confirmé ; enveloppe/gradins à mesurer |
| C | Scène, coulisses, loges, réserves | Scène 18,3 x 18 m, 273,3 m², hauteur de cage 21 m dans la réalisation documentée | Fonction confirmée ; hauteur spécifique Pripyat inconnue |
| D | Galerie, escaliers, fresque | Circulation verticale / foyer ; fresque attestée par photos | **Priorité 1** : coordonnées de mur, niveaux et vide à relever |
| E | Salles de danse et réunions | Grande salle de danse double hauteur, salles de cérémonies | À positionner sur photos/plans |
| F | Clubs, ateliers, bibliothèque, sport/piscine/boxe | Salles d'activité aux niveaux supérieurs, adaptations Pripyat | À positionner, ne pas inventer |
| G | Administration, logistique, technique | Bureaux, ateliers, accès de service | À positionner |
| H | Sous-sol | Ventilation, sous-scène, locaux techniques | À positionner et documenter |

## Erreurs confirmées dans le modèle
- `scene_pripyat_central.js` dispose des dalles répétées à 3,25 et 6,35 m sans correspondance démontrée avec le plan.
- Le hall et la fresque étaient traversés par un niveau intermédiaire ; correction provisoire de la hauteur de fresque commit `b5a2e58d` **non validée par photo**.
- Le volume auditorium est représenté comme un rectangle simplifié, insuffisant pour les gradins et la cage de scène.
- Le gabarit actuel 81 x 24,6 m est moins profond que les 30,4 m en axes de la réalisation documentée ; ne pas extrapoler une correction sans relevé.
- Le projet type et l'Energetik ne sont pas interchangeables.

## Plan d'intégration sans perte
1. Conserver la scène actuelle comme référence testée ; travailler uniquement sur `integration/moteur3d-unified-v15`.
2. Introduire un calque de zones A-H en coordonnées locales **indicatives** et le documenter ; ne pas attribuer de coordonnées exactes aux salles sans plan.
3. Construire l'atrium à double hauteur et ses dalles découpées en se basant sur les photographies, puis replacer le mur de fresque.
4. Modéliser l'auditorium avec gradins, scène et hauteurs indépendantes des niveaux courants.
5. Vérifier séparément piscine, boxe, bibliothèque et espaces annexes.
6. Mettre à jour le parcours de caméra et les vues d'inspection de chaque zone.
7. Vérifier export de scène, build Windows, rendu CPU et DX11, comparaison photographique.

## Sources
- https://photobuildings.com/projects/2736/
- https://photobuildings.com/object/80424/
- https://proekt.sx/tip/dk781-detail.html
- https://commons.wikimedia.org/wiki/Category:Palace_of_culture_in_Prypyat
- https://pripyat.fandom.com/ru/wiki/Энергетик
