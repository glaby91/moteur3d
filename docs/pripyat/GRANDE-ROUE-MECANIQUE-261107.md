# Grande roue de Pripiat — implantation et système mécanique (2026-11-07)

## Localisation sur le terrain

Objet : roue « Круговой обзор / Circular Overview » du parc d'attractions, à l'arrière **nord-ouest** du Palais Energetik.

- **Point de l'attraction** : latitude **51.408348**, longitude **30.055847** (WGS84), [Coasterpedia — Circular Overview](https://coasterpedia.net/wiki/Circular_Overview) et [attractions du parc](https://coasterpedia.net/wiki/Pripyat_Amusement_Park).
- [Wikimedia Commons, photographie avec coordonnées de l'objet](https://commons.wikimedia.org/wiki/File:Ferris_wheel_in_Pripyat,_Ukraine.jpg) : **51.408386, 30.055791**, soit environ 6 m du point choisi. Les métadonnées d'autres photos portent sur la **caméra**, pas sur la roue : elles ne doivent pas être utilisées comme centre de l'attraction.
- **Point de contrôle Energetik** : 51.406719444, 30.056550 (catégorie Wikimédia ; non garanti centre du bâtiment). Décalage WGS84 converti : environ **48,8 m vers l'ouest**, **181,3 m vers le nord**, soit **187,75 m**.
- **Placement rendu actuel** : X≈**−14,57 m**, Z≈**−122,66 m** dans la scène, via la transformation de la carte OSM déjà implémentée (échelle **0,4882691**, rotation **0,21319471 rad**, ancre **X=−10 / Z=−31,1**).
- **Limite fondamentale** : la transformation carte → scène a été ajustée pour les premières emprises provisoires et **n'est pas encore un géoréférencement métrique certifié**. Le GPS de la roue est référencé ; les coordonnées d'affichage de la roue sont cohérentes avec la carte *actuelle*, pas encore un levé au mètre exact de l'ensemble du centre-ville. En particulier la différence géodésique de ~188 m est compressée par l'échelle de scène de 0,488. **Ne pas qualifier X/Z d'exacts** tant que les bâtiments n'ont pas été recalés sur trois points de contrôle.

## Sources pour la mécanique et la photographie

- [Vue technique et reconstruction artisanale du type soviétique, MART_i3D](https://cults3d.com/en/3d-model/architecture/koleso-obozreniya-pripyat-sovetskoe-tipovoe-koleso-obozreniya-1-100-podvizhnoe) : diamètre **24,6 m**, hauteur **27,5 m**, **20 nacelles**, tour **6 minutes**. **Données de reproduction secondaire**, pas document d'usine ni cote d'origine garantie. Aucun modèle CAD tiers n'est importé.
- [Guide du parc](https://find-way.com.ua/en/regions/chernobyl-exclusion-zone/pripyat/pripyat-city-amusement-park) : hauteur ~26 m et **16 nacelles** annoncées, en contradiction avec la référence technique précédente. La version moteur choisit **20** comme hypothèse, modifiable au niveau de `M3D_FERRIS_WHEEL.numberOfGondolas`.
- [Photos Wikimedia Commons de la roue](https://commons.wikimedia.org/wiki/Category:Ferris_wheel_in_Prypiat), [roue et nacelles jaunes](https://commons.wikimedia.org/wiki/File:Pripyat_Ferris_Wheel.jpg) : A-frames métalliques bruns corrodés, double cerclage, rayons et haubans, nacelles jaunes circulaires à pare-soleil. **Références visuelles sans incorporation de leurs pixels comme textures.**
- [Photo du 14 avril 2026 depuis le gymnase de l'Energetik](https://www.gettyimages.se/detail/nyhetsfoto/the-ferris-wheel-is-seen-through-the-windows-of-a-gym-in-nyhetsfoto/2271207569) : confirme la relation entre les grandes baies du gymnase et la roue visible derrière. Photo protégée par licence, non utilisée comme texture.

## Arbre mécanique conçu

```text
GrandeRoue_Pripyat
├── SITE_GPS : position géographique / orientation provisoire
├── FIXE : semelles, deux portiques en A, traverses, arbre, paliers, moteur-réducteur, quai
└── ROTOR [pivot centre, axe parallèle à Z, angle θ(t)]
    ├── double couronne, rayons, contreventements, moyeux
    ├── pivots des 20 suspentes
    └── GONDOLE_01...20 [assemblages indépendants]
        ├── pivot sur couronne (tourne avec θ)
        ├── rotation locale -θ (compense le parent) ; corps vertical en monde
        ├── 2 suspentes, armature, panier circulaire
        └── canopy jaune + assise + protections
```

Les **nacelles ne doivent jamais être fusionnées au mesh de la roue** : leur pivot individuel permet gravité, balancement, amortissement et embarquement futur.

### Cinématique de base

- Période nominale : 360 s, vitesse `1/6 rpm` (entièrement paramétrable).
- Angle roue : `θ(t) = 2π * t * RPM / 60`.
- Position pivot nacelle `i` : `[Cx + R sin(θ + i·2π/N), Cy + R cos(θ + i·2π/N), Cz]`.
- Rotation locale nacelle : `−(θ+i·2π/N)` ; orientation verticale monde = zéro en l'absence de balancement.
- Modèle de deuxième étape : `I α'' + c α' + m g d sin(α) = −I θ''` autour de chaque axe de suspente, gestion inertie/arrêt/démarrage, amortissement réglable. **Non simulé dans l'export actuel**.
- Articulation des supports : entièrement fixe ; entraînement séparé au moyeu / galets, sans rotation du moteur avec la roue.

## Matériaux PBR (création procédurale)

- `wheelSteel` : acier âgé, peinture brune, masques de corrosion, micro-rayures.
- `wheelRust` : acier exposé, rouille diffuse, salissures de pluie.
- `wheelYellow` : peinture jaune passée, écailles de peinture, points rouillés aux fixations.
- `wheelTrim` : armatures et barres de suspension.
  
Pour chaque matériau, **albédo, normale et rugosité** sont générées dans `export_scene.js` et embarquées dans le BIN, sans photo filigranée. Les coefficients de réflectivité sont définis dans le registre de matériaux existant ; ce moteur ne stocke pas encore de carte metallic indépendante pour ces objets.

## État d'implémentation

- **Code** : `M3D_FERRIS_WHEEL`, `m3dWheelSite`, `m3dWheelPose`, `m3dBuildWheelStatic`, `m3dBuildWheelRotor`, `m3dBuildWheelGondolas`, `m3dBuildPripyatWheel` dans `scene_pripyat_central.js`.
- **Export V14** : génération du rendu à un angle défini par `M3D_WHEEL_TIME_SECONDS` et `M3D_WHEEL_RPM`. Par défaut roue immobile à la pose `t=0`, malgré son rig paramétrique. Le moteur V14 utilise un **BVH de géométrie statique** : il ne tourne pas automatiquement en temps réel.
- **Manifest pour moteur futur** : `scene_pripyat_wheel_mechanism.json` généré par `tools/export_pripyat_wheel_mechanism.js`, avec nœuds fixe/rotor/nacelles, axes, pivots, phases et recommandations de transformation native CPU/GPU.
- **Validation automatique** : `test-pripyat-ferris-wheel.mjs` : 20 pivots, révolution 360s, quarter-turn, nacelles verticales, générateurs séparés, boîtes non inversées.
- **Vues** : caméra 06 vers la roue à travers la salle de sport, caméra 11 vue extérieure du mécanisme.
- **À faire pour une rotation effectivement temps réel** : sérialiser les objets par groupe hiérarchique, modifier le renderer natif pour appliquer les transforms rotor/nacelles et refitter une structure BVH dynamique par frame. La seule existence d'une pose exportable n'est **pas** une animation en direct.

## Critères d'acceptation

1. Transformer GPS→scène avec au moins trois homologues de plan mesurés et documenter l'erreur résiduelle.
2. Vérifier la caméra 06 : roue visible entre les piliers, cadrage comparé à la photographie prise depuis le gymnase.
3. Vérifier la caméra 11 : silhouette complète, nacelles indépendantes, 20 points d'articulation ; couleur jaune/brun.
4. Tester deux exports `M3D_WHEEL_TIME_SECONDS=0` et `90` : supports identiques, rotor tourné d'un quart de tour, nacelles toujours verticales.
5. Toute vitesse en fonctionnement doit s'appliquer au rotor, pas aux supports et ni aux textures PBR.
