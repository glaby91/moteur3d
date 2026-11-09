# Energetik — étude de la structure ajourée du parvis (9 novembre 2026)

## Identification confirmée par l'utilisateur

La photographie aérienne transmise en conversation (924 × 530 px) montre le **Palais de la Culture Energetik**. À l'avant-droite du bâtiment est visible un élément courbe ajouré, composé de **deux bords porteurs** et d'une série de traverses. L'identification de ce secteur comme **Energetik** est confirmée par l'utilisateur le 9 novembre 2026. Ne pas confondre cette structure avec l'arcade autonome desservant le secteur de l'hôtel Polissya.

La photographie n'établit pas un plan coté de l'élément ; la position exacte de son point de départ, sa forme au sol, les colonnes et la présence éventuelle d'une ancienne couverture demeurent à confirmer sur d'autres vues. En particulier, **le fait que le toit soit ajouré aujourd'hui ne prouve pas qu'il l'était en 1986**.

## Exécution 3D (réversible)

La branche `feature/energetik-entrance-lattice-261109` ajoute `m3dBuildEnergetikForecourtLattice` dans la scène principale, **sans modifier** la fonction `m3dPolissyaArcadePlan`, ni `m3dBuildConnector`.

Mode d'étude : `M3D_ENERGETIK_FORECOURT_LATTICE=photo-study`.
Mode normal : désactivé, afin de ne pas remplacer des volumes homologués par une géométrie provisoire.

L'étude ajoute une charpente **vraiment ajourée**, composée de :
- 2 longerons périphériques courbes, 1 longeron médian ;
- 12 traverses distinctes, entre les longerons ;
- 5 couples de piliers, avec fondations individuelles ;
- assemblage en triangles 3D avec normales calculées ; aucune surface de toiture opaque.

Le chemin en plan est un Bézier quadratique à **3 points**, dans l'espace provisoire du moteur :
- début : `[energetik.x1 - 1.6, energetik.z1 + 2.6]` ;
- contrôle : `[energetik.x1 + 9.5, energetik.z1 + 3.0]` ;
- fin : `[energetik.x1 + 13.8, energetik.z1 + 26.4]` ;
- demi-largeur de travail : 1,82 unité ; altitude de toiture : 3,78 unités.

**Ces valeurs ne sont PAS des relevés des dimensions réelles** ; elles servent à générer une variante testable dans les coordonnées du blockout. Ne pas les reporter comme cotes d'archive.

## Essai dans GitHub Actions

Le workflow Windows de cette branche exécute :
1. les tests de régression existants ;
2. `test-energetik-forecourt-lattice-261109.mjs` (contrôle du mode OFF, des 1 008 triangles, 20 boîtes de supports, du vide entre traverses et de l'indépendance Polissya) ;
3. l'export de la scène centrale avec `M3D_ENERGETIK_FORECOURT_LATTICE=photo-study` et `M3D_PRIPYAT_FACADE_PERIOD=abandoned` ;
4. la compilation Windows Qt et les 18 vues CPU préexistantes, publiées dans l'artifact d'inspection si tout réussit.

**Définition de terminé :** compilation réussie, fichiers d'export et PNG présents ; cette vérification ne démontre pas la fidélité photographique tant qu'une caméra appariée n'est pas étalonnée.

## Références géométriques

- Photographie du 9 novembre 2026 conservée dans la conversation, non répliquée publiquement.
- Vue partielle de structure : toit ajouré visible côté parvis, bandeau sur les façades et répétition des montants vitrés.
- Code de référence : `scene_pripyat_central.js`, section `m3dBuildEnergetikForecourtLattice`.
- Séquences précédentes : vidéos du Palais, dont la vidéo historique de 2009, utilisées comme **références contextuelles** et non pour imposer de nouvelles dimensions.

### Conditions pour figer définitivement cette géométrie

Il faudra trouver une vue latérale et une vue en plan montrant le tracé complet, identifier des homologues entre ces photographies et les sommets du bâtiment, puis vérifier leurs erreurs de reprojection. La version proposée reste provisoire tant que cette étape n'est pas réussie.
