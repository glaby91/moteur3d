# Moteur3D — Plan qualité visuelle, géométrie et PBR

Statut : proposition technique à valider par des tests, sans prétendre que le moteur dispose déjà d'un pipeline PBR complet.

## Constat Rapture
La scène Rapture a été poussée jusqu'à environ un million de triangles au cours des expérimentations, sans résultat visuel satisfaisant selon le retour utilisateur. Le nombre de triangles n'est donc pas un indicateur suffisant de qualité. Les causes envisagées (géométrie, proportions, matériaux et éclairage) sont des hypothèses à vérifier, non des diagnostics démontrés.

## Priorités
1. **Géométrie** : valider silhouettes, dimensions, raccordements, normales, UV et tangentes avant d'ajouter des détails.
2. **Matériaux** : auditer les formats et le chemin d'import ; vérifier base color, metallic, roughness, normal map, emissive, AO, transmission/IOR. Ne pas assimiler la table historique de pourcentages de réflectivité à un PBR metallic/roughness.
3. **Éclairage** : vérifier intensités, espace colorimétrique linéaire, exposition, ombres et réflexions avant d'attribuer les défauts aux textures.
4. **Parité CPU/GPU** : comparer une caméra et des données de scène identiques, avec sorties de diagnostic (albedo, normales, profondeur, matériaux) avant le rendu final.
5. **Performance** : mesurer les temps CPU et D3D11 Compute seulement après validation visuelle minimale ; distinguer temps de calcul, synchronisation et transfert vers Qt.

## Scène étalon : bathysphère
Construire ou corriger une bathysphère isolée : coque avec épaisseur réelle, ouvertures traversantes, vitrage, intérieur visible, métal vieilli et éclairage intérieur. Utiliser une caméra fixe, une lumière de référence et des captures reproductibles. Conserver les anciennes scènes Rapture comme référence historique, sans les déclarer compatibles avec le moteur courant tant que l'export et le chargement n'ont pas été testés.

## Critères d'acceptation
- Silhouette et proportions validées visuellement sous plusieurs angles.
- Normales et UV corrects ; absence de surfaces obstruant les hublots.
- Tests de matériaux isolés : diélectrique rugueux, métal lisse/rugueux, verre, émissif.
- Captures CPU/GPU comparables, différences documentées et absence de géométrie manquante.
- Mesures de performance reproductibles, avec résolution, caméra, backend et matériel consignés.

## Ordre de travail
Audit du pipeline existant → tests géométriques et PBR → bathysphère étalon → comparaison CPU/HLSL → intégration progressive à Pripyat → éventuelle refonte de Rapture.

**Décision :** privilégier la justesse de la géométrie, des matériaux et de la lumière plutôt que l'augmentation du nombre de triangles. Ne pas réécrire Qt ni le backend HLSL pour cette seule raison.
