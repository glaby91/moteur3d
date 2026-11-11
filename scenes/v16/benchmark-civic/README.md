# Complexe civique — scène de charge V17

**Scène totalement fictive** créée pour tester le moteur : ce n'est ni Pripiat
ni une reconstruction de l'Energetik. L'architecture est volontairement inspirée
des grands ensembles civiques des années 1970 : pavillons connectés,
passerelle, escalier, verrière, colonnade, ailes, bassin, mobilier, arbres et
espace intérieur avec rangées de sièges.

## Production

Un générateur déterministe Node.js (standard library uniquement) construit :
- `scene.json` — **léger** (~20k triangles de boîtes minimum ; PBR max 512).
- `scene-lourd.json` — **lourd** (~150k triangles minimum ; PBR max 1024).
- `scene-extreme.json` — **extrême** (~500k triangles minimum ; PBR max 1024).

Les fichiers de scène et d'instances sont générés avec :

```powershell
node scenes/v16/benchmark-civic/generate.cjs --all
node scenes/v16/benchmark-civic/validate.cjs
```

Le ZIP **CSE-WARP-V17-Exploration-Windows-x64** généré par GitHub Actions
contient les trois profils déjà générés : aucune commande Node n'est
nécessaire pour l'utilisateur Windows.

## Dans V17

Le menu **Fichier → Scènes benchmark** ouvre chacun des profils. Ils sont
également accessibles avec **Fichier → Ouvrir une scène V16** puis
`benchmark-civic/scene*.json` après décompression du ZIP.

Le profil **extrême est volontairement une charge de stress** : choisir
explicitement le profil, puis lancer le rendu quand l'on veut le tester.
On recommande de faire d'abord le test léger et le test lourd.

L'outil **Outils → Benchmark V17** peut être lancé sur le profil léger ou
lourd pour mesurer construction PBR, premier rendu, FPS calculés et latences.

## Composants

- Terrain étendu, routes, chaussées, marquages, dalles et joints.
- Palais civique central à plusieurs niveaux, ailes latérales, passerelle.
- Entrée monumentale avec poteaux, vitrage, marches et garde-corps.
- Bassin d'eau, sculpture métallique, abri d'autobus, bancs, lampadaires.
- Jardins et plusieurs variantes de couronnes de feuillage.
- Hall avec mobilier répétitif et zone d'auditorium.
- Détails de façade « clinker » à densité variable **visibles**, et non un
  objet factice invisible de surcharge.

Six points de caméra sont définis. Les objets utilisent des dimensions en
mètres, le même référentiel V16/V17 et un BVH commun.

## Matériaux

Le catalogue est lu depuis `assets/materials/pripyat-v17/catalogue.json`.
Les **22 IDs `prp_*`** sont recopiés vers les ressources JSON du niveau ;
leurs anciennes cartes PBR binaires ne sont **pas chargées** directement.
Les recettes de texture fournissent un substitut PBR V17 compatible.

## Mesures et limites

Les fichiers `stats/*.json` consignent nombre d'instances, triangles de boîtes
(minimum conservateur), et limite de résolution PBR. Les primitives sphériques
et végétales ajoutent des triangles non inclus dans le compteur minimum.

**Ne pas assimiler les FPS du benchmark à la présentation écran** :
la mesure inclut le rendu et le rapatriement GPU→CPU mais pas le temps
de rafraîchissement Qt. Le mode « extrême » peut épuiser la RAM sur
les petits PC : c'est un test volontaire, pas une configuration recommandée.
