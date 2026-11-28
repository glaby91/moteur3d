# CSE WARP — moteur 3D V17

**Moteur : V17.031 · scène de démarrage de référence : V17.634 · scène CI Z Reflect : V17.631 (version de scène indépendante)**
CSE WARP V17 réunit l’interface d’exploration, plusieurs backends de rendu, le noyau moteur partagé avec V16, les outils de benchmark et un mécanisme de mise à jour de scène. La V17 reste une version expérimentale : les performances et la présentation native doivent être mesurées sur le GPU visé.

> La branche principale du moteur est `master`. [État des branches](BRANCH_STATUS.md) · [Validation L01/L02 V17.029](v17/docs/L01_SOLAR_PCF_V17029.md) · [Build V17.031 validée](https://github.com/glaby91/moteur3d/actions/runs/46068889675).

**Accès rapide :** [Démarrage et profil](#v17031--profil-de-démarrage-et-philosophie-warp) · [Créer une scène](#construire-une-scène-v16v17--guide-du-créateur) · [Backends](#backends-disponibles) · [Compiler](#compiler) · [Linux/Vulkan](#linux-natif--vulkan-x86_64) · [Benchmarks](#benchmarks-en-ligne-de-commande) · [Audit historique](#audit-historique-et-registre-des-optimisations--26-novembre-2026)

## V17.031 — profil de démarrage et philosophie WARP

**Recréer ce qui paraît réel, sans en avoir le coût.** Priorité à la cohérence visuelle, spatiale et temporelle, non à un pourcentage arbitraire de fidélité physique. Corriger les causes communes, précalculer et réutiliser les données ; préférer une approximation crédible et peu coûteuse si le calcul exact est trop lourd. Équilibrer CPU/GPU sans multiplier les transferts ni les synchronisations.

### Profil de démarrage Windows

- LumaTrace BVH4, Ultra, MSAA 8× demandé (repli matériel possible), anisotropie 16×, LOD 200 %, LOD adaptatif désactivé, culling conservatif activé.
- Rebonds indirects, couleurs équilibrées et compression filmique actifs ; exposition 0,85. Nuages désactivés par défaut pour LumaTrace, Z Reflect et Z Material actifs ; Z Atmosphere reste opt-in.
- Caméra piscine de la scène V17.634, ajoutée en mémoire côté moteur sans modification du dépôt scène : eye (29.90612030029297, 2.245000123977661, -18.339569091796875), target (29.043092727661133, 2.286057472229004, -18.843053817749023), FOV 61°. Les anciennes caméras et raccourcis sont conservés ; autres scènes gardent leurs vues d'origine.
- Le profil Ultra est réappliqué au lancement normal (les contrôles restent modifiables pendant la session). `M3D_V17_LEGACY_STARTUP=1` restaure l'ancien démarrage ; `M3D_V17_HD620_INTERACTIVE_REDUCE=1` autorise la réduction temporaire du MSAA à 2× en exploration HD620. Le profil Vulkan explicite et les smokes restent inchangés. La qualité maximale peut fortement réduire les FPS sur HD620.
- Pour exporter un cadrage : Fichier → Copier la position caméra (JSON). Le raccourci Ctrl+Maj+C peut être intercepté par la fenêtre GPU dédiée.

### Corrections et modules cumulés

| Élément | Révision | État |
|---|---|---|
| L01/L02, receiver-plane PCF | V17.029 | Validées visuellement DX11/Vulkan/LumaTrace |
| DX11 raster / shader partagé | 018 / 009 | Z Material intégré |
| Vulkan raster | 010 | Z Material intégré |
| LumaTrace compute / CPU | 010 / 003 | Noyaux inchangés, bénéficie du shader partagé |
| Z Material | 001 | V17.030 compilée ; amélioration visuelle L03 confirmée, problème métallique non entièrement résolu |
| Scene runtime | 003 | Filtrage des matériaux actifs avant génération des textures |
| Z Reflect / Z Atmosphere | 001 / 002 | Conservés, atmosphère opt-in |

[Z Material 001 : architecture et benchmark](v17/docs/ZMATERIAL_001.md). Catalogue moteur unique de 8 192 références PBR synthétiques, maximum 256 matériaux actifs par scène ; textures de la scène et propriétés personnalisées prioritaires. Pas de nouveau fichier par scène. La V17.030 a passé les tests CI [build 1](https://github.com/glaby91/moteur3d/actions/runs/46064723180). L03 reste partiellement résolu : métal, lissage des normales et réflexions environnementales à étudier globalement. L04 (aliasing des arêtes) et L05 (scintillements) à diagnostiquer ; rendu périphérique à revoir après stabilisation de la qualité globale. Les mesures GPU réelles restent à effectuer.

## Audit rendu V17.031 — 27 novembre 2026

[Audit complet, simulations analytiques, registre AR01–AR17 et feuille de route](docs/audits/AUDIT_RENDU_V17_031_2026-11-27.md)

| Backend | Note technique indicative /10 |
|---|---:|
| LumaTrace BVH4 | 7,7 |
| DX11 Raster | 7,3 |
| CPU GI 4 rebonds | 7,1 |
| CPU GI 2 rebonds | 6,9 |
| Vulkan Raster | 6,5 |
| GPU DX11 Compute | 6,2 |
| CPU PBR direct | 6,0 |

Notes estimatives à poids égaux (visuel, matériaux, éclairage, efficacité, maturité), pas de nouveaux FPS mesurés. Constats majeurs : normales de faces du bridge, métallicité source des sondes LT ignorée, parité des réglages Vulkan (MSAA/anisotropie/exposition) et énergie solaire nocturne, sélection des lumières, cohérence des ombres et aliasing spéculaire. Priorités proposées : Z Normal 001, Z Material 002, harmonisation des backends, Z Light et stabilité temporelle. Aucun chantier implémenté. Le profil Ultra décrit plus haut correspond à la demande UI, pas à une garantie de réglages effectifs identiques entre backends.

## Scène de référence et paquet Windows

**[Compilations Windows x64 de master](https://github.com/glaby91/moteur3d/actions/workflows/build-main-v17.yml)**

Extraire le ZIP dans son dossier de build nommé, puis lancer `LANCER_CSE_WARP.bat`. Le paquet officiel contient le moteur, les outils et le catalogue Z Material, **pas la scène**, qui conserve son propre dépôt et son versioning. Au démarrage, le moteur vérifie la publication officielle avant de charger la scène locale ; en cas d'indisponibilité réseau, la scène locale reste utilisable. En l'absence de scène locale, accepter son téléchargement. Les vérifications suivantes sont asynchrones et proposent une mise à jour sans redémarrage forcé. Ctrl maintenu : accroupissement ; Espace : saut ; Tab ou < brièvement : Pip-Boy ; F2 : HUD.

- [Scène V17.634 — référence du cadrage piscine](https://github.com/glaby91/moteur3d-scene/releases/tag/scene-reflect-V17.634).
- [Scène CI Z Reflect V17.631](https://github.com/glaby91/moteur3d-scene/releases/tag/scene-reflect-V17.631) : géométrie V17.629, politique optique et caches BVH4/index schema 2 précalculés.
- [Dernière scène officielle](https://github.com/glaby91/moteur3d-scene/releases/latest)
- [Validation matérielle HD620 — V17.022/023](v17/docs/CSP001_ZREFLECT_V17023_VALIDATION.md).

Pour retrouver une ancienne compilation, consulter [V17.006, build 683 avec scène V17.623](https://github.com/glaby91/moteur3d/actions/runs/45493998985) ou [V17.003, build 587](https://github.com/glaby91/moteur3d/actions/runs/45080722276).

## Linux natif — Vulkan x86_64

**Portage Linux validé fonctionnellement — CSE WARP V17.031, build Linux 8, scène V17.634 (28 novembre 2026).**

- [Télécharger le package Linux complet (120 Mo)](https://github.com/glaby91/moteur3d/actions/runs/46085728151/artifacts/15238388787) : moteur, scène, shaders, catalogue Z Material, Qt 6.8.3, ICU, XCB et TLS. Artefact GitHub Actions à rétention limitée.
- [Compilation native et audio validés — build 8](https://github.com/glaby91/moteur3d/actions/runs/46085728151).
- [Validation Vulkan sur la piscine Full HD — Ubuntu 24.04 / Mesa Lavapipe](https://github.com/glaby91/moteur3d/actions/runs/46083487533) · [PNG CPU/Vulkan et statistiques JSON](https://github.com/glaby91/moteur3d/actions/runs/46083487533/artifacts/15237877283).
- [Dernières compilations Linux](https://github.com/glaby91/moteur3d/actions/workflows/build-linux-vulkan.yml) · [Documentation Linux/Vulkan](v17/docs/LINUX_VULKAN.md).

| Fonction | Validation |
|---|---|
| Application Qt native, démarrage X11, chargement de scène hors ligne | OK |
| CPU Ray Tracing PBR direct | OK |
| CPU illumination globale, 4 rebonds, 16 SPP | OK |
| Vulkan Raster, scène complète, 1920 × 1080 | OK sur Mesa Lavapipe |
| Radio locale automatique, spatialisation et sortie PCM virtuelle | OK (build 8) |
| GUI Ubuntu, 10 caméras et navigation | OK (build 8) |
| GPU physique Intel / AMD / NVIDIA | À valider |
| Présentation Vulkan native X11 / Wayland sans relecture | Non portée |

Test comparatif CI (build 6 de référence) : scène V17.634, caméra `v7_pool_fresco`, 1920 × 1080, textures 2048, éclairage 14 h. CPU PBR direct : 9 714,26 ms ; Vulkan logiciel : première image 4 901,56 ms, suivantes 457,683 et 467,494 ms (moyenne 462,59 ms, soit ~2,16 FPS). Le ratio ~21× en régime stabilisé compare des techniques de rendu différentes et ne constitue ni une mesure sur GPU physique ni une preuve de parité visuelle. Écarts connus : Vulkan plus lumineux, ombres atténuées, réflexions dans l'eau différentes.

[Audit GUI Ubuntu — 5 campagnes](https://github.com/glaby91/moteur3d/actions/runs/46088634614) · [Rapport détaillé et anomalies ouvertes](v17/docs/LINUX_GUI_QA.md). Les 26 processus ont terminé normalement, mais les contrôles Raster liés à DX11 et le lanceur GI échouent ; la restauration de caméra et le changement de scène restent à diagnostiquer. La résolution Full HD du GUI reste à vérifier indépendamment du rendu hors écran validé.

Ubuntu 24.04 x86_64, Vulkan loader et périphérique API >= 1.3.204. Même moteur que Windows, sans Wine. Le package contient la dernière scène Reflect vérifiée par SHA-256 au moment du build ; la scène conserve son dépôt et son versioning indépendant via [moteur3d-scene](https://github.com/glaby91/moteur3d-scene/releases/latest).

Après extraction du ZIP GitHub puis du fichier `.tar.gz` :

```bash
sudo apt install libvulkan1 mesa-vulkan-drivers libgl1 libegl1 libxkbcommon-x11-0 libxcb-cursor0 libpulse0 unzip
cd CSE-WARP-V17.031-Linux-Vulkan-GitHub-8-x86_64
./lancer.sh
```

Pour démarrer directement hors ligne avec la scène embarquée : `./lancer.sh benchmark-civic/scene.json`. Le lancement normal conserve la vérification des mises à jour et sélectionne la scène locale la plus récente.

Utiliser le pilote Vulkan adapté au GPU. Vulkan est sélectionné par défaut ; les modes CPU sont disponibles sans GPU. La présentation Linux passe par QImage avec relecture GPU/CPU. DirectX 11 et LumaTrace GPU/Z Reflect ne sont pas portés sous Linux. Audio local de secours validé via Qt AudioSink/PulseAudio virtuel ; écoute physique et flux Internet non validés. GPU physique et parité graphique encore à valider.

## Construire une scène V16/V17 — guide du créateur

Une scène est une base de données de ressources indépendantes, pas une succession d'images précalculées. Le même fichier est interprété par les backends CPU, Compute, DX11, Vulkan et LumaTrace. **Ne pas modifier le moteur pour ajouter un bâtiment, une texture, un objet ou un cadrage.** La scène peut être versionnée et publiée indépendamment de l'application.

### 1. Organisation et coordonnées

```text
ma-scene/
  scene.json                 # manifeste unique
  geometry/meshes.json       # modèles réutilisables
  instances/objects.json     # objets placés dans le monde
  materials/surfaces.json    # propriétés PBR et références
  pbr/textures.json          # recettes et résolution des textures
  lighting/day.json          # heure initiale et sources locales
  cameras/views.json         # cadrages de départ
  environment/site.json      # métadonnées facultatives
  textures/                  # images externes facultatives
  precomputed/               # caches générés, pas des sources d'édition
```

Toutes les références de fichiers sont relatives à la racine de la scène ; chemins absolus, fichiers manquants et sorties de racine (y compris liens symboliques) sont refusés. Le manifeste doit contenir `format: "m3d-scene-v16"` et un tableau `sectors`, avec des identifiants de secteur uniques. Chaque secteur référence des tableaux de chemins pour les catégories `geometry`, `instances`, `materials`, `pbr`, `lighting`, `cameras`, `environment`. Les secteurs servent à organiser les ressources : **ne pas supposer un streaming automatique par pièce ou une hiérarchie de transformations**. Le chargeur runtime agrège les ressources de tous les secteurs.

- Unités : mètres ; Y vertical, X/Z horizontaux. Une caméra à hauteur d'homme se place environ 1,70 m au-dessus du sol fini.
- Les instances de boîtes et sphères sont centrées sur leur position. Une dalle épaisse de 0,20 m dont le dessus est à Y=0 est centrée à Y=-0,10.
- Les maillages importés utilisent leur origine locale comme pivot. `size` multiplie leurs coordonnées locales, alors que pour une boîte/sphère il représente les dimensions. Avec des sommets déjà exprimés en mètres, utiliser `size: [1,1,1]` pour conserver l'échelle.
- `rotationY` est exprimé en degrés et appliqué autour de Y aux boîtes et aux maillages importés ; ne pas supposer des rotations X/Z ni une rotation effective des sphères et feuillages. `parent` est un regroupement logique, pas une transformation héritée.

### 2. Définir la géométrie et placer les objets

Le chargeur runtime lit `geometry.meshes[]` et `instances.instances[]`. Définir un modèle une fois, puis l'instancier autant que nécessaire. Les identifiants de modèles et de textures doivent être uniques à l'échelle de la scène (le chargeur peut écraser un doublon de modèle/texture) ; les doublons d'identifiants d'objets ou de matériaux sont rejetés. Un objet possède un identifiant, une référence de géométrie, un matériau, une position et une taille strictement positive.

| Type de géométrie | Usage et limite actuelle |
|---|---|
| `box` | Murs, sols, poutres, vitrages plans ; 12 triangles par boîte, possibilités d'instancing côté GPU. |
| `uv_sphere` | Objets ellipsoïdaux, végétation simple ; tessellation interne fixe (ne pas considérer les champs segments/rings comme pilotables dans le runtime actuel). |
| `foliage_canopy` | Couronne végétale procédurale, géométrie déterministe. |
| `triangle_mesh` | Maillage triangulé : sommets XYZ, faces à trois indices (base zéro), UV facultatives par sommet. Maximum 200 000 sommets et 80 000 faces par maillage importé dans le chargeur actuel. |

Exemple de maillage local, à ajouter au tableau `meshes` :

```json
{"id":"panneau","type":"triangle_mesh",
 "vertices":[[-1,0,0],[1,0,0],[1,2,0],[-1,2,0]],
 "faces":[[0,1,2],[0,2,3]],
 "uv":[[0,0],[1,0],[1,1],[0,1]]}
```

**Pour un asset GLB/GLTF/OBJ externe**, convertir hors ligne en géométrie triangulée et UV compatibles, séparer les primitives par matériau, conserver l'échelle et le pivot, vérifier orientation des faces et coutures UV. Le runtime de scène ne charge pas directement un chemin GLB dans une instance. Le bridge actuel reconstruit une normale de face pour les triangles importés : les normales de sommet personnalisées du GLB ne sont pas encore conservées par ce format. C'est une limite réelle pour les surfaces courbes (L03-B), pas une raison de densifier tous les objets. Un champ `normals` ajouté au JSON ne résout pas cette limite sans évolution du chargeur.

Construire d'abord la structure, puis les ouvertures et les volumes secondaires, ensuite les détails, les meubles et la végétation. Ne pas poser deux surfaces coplanaires l'une sur l'autre (risque de Z-fighting) ; éviter les triangles dégénérés, les faces internes inutiles et les petits détails géométriques pouvant être représentés par une texture normale. Garder les vrais reliefs de silhouette, les ouvertures et les surfaces de collision. Vérifier les pièces traversables en exploration.

### 3. Matériaux, Z Material et textures

Chaque instance référence un matériau par son identifiant de scène. Les propriétés PBR sont indépendantes du maillage :

| Champ matériau | Interprétation |
|---|---|
| `id` | Identifiant unique de scène ; peut être partagé par plusieurs objets. |
| `baseColor` | RGB normalisé 0–1, utilisé comme couleur albédo encodée sRGB pour la génération de texture. |
| `roughness` | Rugosité nominale 0–1 ; basse = reflet plus concentré. |
| `roughnessScale` | Multiplicateur de rugosité (défaut 1 ; borné à 0,01–2 par le bridge). |
| `metallic` | 0–1 ; 0 diélectrique, 1 métallique. |
| `transmission` | 0–1 ; 0 opaque. Ce n'est pas un moteur de réfraction exact. |
| `ior` | Indice de réfraction (défaut 1,5 ; eau typiquement 1,333). |
| `textureSet` | Identifiant obligatoire d'un jeu de textures présent dans `pbr.sets[]`. |
| `libraryMaterial` | Référence facultative Z Material, ex. `inox_01_01` ; complète seulement les champs absents. |
| `surfaceModel` | `"standard"` ou `"water"` ; classification explicite, préférable à une reconnaissance par nom. |
| `emission` | Objet `{"color":[1,0.9,0.7],"intensity":2}` : RGB 0–1 et intensité 0–100 ; surface lumineuse. |
| `planarReflection` | Autorisation optique `"auto"`, `"enabled"`, `"disabled"`. |
| `textureDetail` | Politique de conservation de détail, par exemple `{"mode":"preserve","minResolution":1024}`. |

**Z Material 001** : 64 familles × 8 couleurs × 16 rugosités = 8 192 références synthétiques, dans un seul [catalogue moteur](v17/materials/zmaterial8192.json). Format d'identifiant `famille_CC_RR` (CC 01–08 ; RR 01–16). Le catalogue est ouvert et parsé seulement si un matériau actif le référence ; les couleurs de bibliothèque linéaires sont converties en sRGB et les propriétés explicites du créateur sont prioritaires. Le jeu de textures reste propre à la scène. Aucun fichier Z Material n'est ajouté à chaque scène.

Le CPU filtre les matériaux déclarés d'après les références des instances, préserve l'ordre des déclarations actives et génère les textures uniquement pour ces matériaux. **Maximum 256 identifiants de matériau actifs** (1–256 dans une scène rendue), même si plusieurs identifiants renvoient au même preset ; il n'y a pas de dédoublonnage automatique par égalité des propriétés. Une scène à 257 matériaux actifs est refusée. La table de constantes GPU existante contient les coefficients de shading ; leur préparation a encore lieu lors du remplissage des constantes par image, pas dans un cache permanent par matériau. Ne pas confondre le nombre de presets et les ressources GPU.

Les jeux PBR choisissent `pattern` (par exemple `concrete`, `metal`, `wood`, `brick`, `glass`, `water`, `porcelain`, `smooth_plaster`, `fabric`, `foliage`, `lawn`) et `resolution` : puissance de deux de 64 à 2048, adaptée au détail nécessaire. Le bridge génère albédo, normales et rugosité, puis leur pyramide de mipmaps et peut réutiliser un cache PBR sur disque. Le champ historique `recipe` ne remplace pas `pattern` dans ce chemin. `imageFile` peut remplacer l'albédo par une image de la scène ; il ne constitue pas un import arbitraire de toutes les cartes PBR. Pour une fresque/enseigne, préserver le domaine UV et utiliser `textureDetail.mode: "preserve"` seulement si nécessaire ; `minResolution` doit être une puissance de deux et ne pas dépasser la résolution source. La politique du matériau prime sur celle du jeu PBR. Toutes les textures ne doivent pas être en 2048.

### 4. Lumières, eau et réflexions

Définir les sources visibles via une géométrie et un matériau émissif ; définir les lumières locales dans `lighting.lights[]` avec `position` en mètres, `radius`, `intensity` et `color` RGB **0–255** (contrairement aux couleurs de matériaux). Le champ `group: "exterior"` identifie une source extérieure pour le contrôle jour/nuit ; éviter de compter deux fois la même énergie entre émissif et lumière d'appoint. `hour` est l'heure initiale (le bridge accepte 6–19 à la lecture) ; le cycle solaire du moteur calcule son propre éclairage. Les champs historiques `sun`, `ambient` et certaines métadonnées `environment` ne doivent pas être présentés comme des commandes toutes exécutées par le runtime actuel. Le sol est une géométrie, pas une extension automatique de `groundSize`.

Pour une piscine, séparer bassin, parois, fond et surface d'eau ; conserver la normale et les UV, utiliser `surfaceModel: "water"` et une transmission/IOR appropriés. Pour un vitrage, utiliser une géométrie plane avec `transmission` et rugosité adaptées. Pour un miroir, le matériau peut porter `opticalMirror: true`. Éviter de déclarer chaque tube métallique comme miroir planaire.

Z Reflect respecte la décision du créateur : `planarReflection` sur l'instance > règle du matériau dans `scene.json.zReflectAuthorPolicy.materialRules` > propriété du matériau > défaut du manifeste. Exemple dans le manifeste (facultatif) :

```json
"zReflectAuthorPolicy": {
  "schema": 1,
  "sceneRevision": "V17.001",
  "materialRules": {
    "verre_fenetre": {"planarReflection": "auto"},
    "verre_ascenseur": {"planarReflection": "disabled"}
  },
  "sharedProbes": []
}
```

L'autorisation `enabled` ne garantit pas une caméra pour chaque surface : le moteur doit encore extraire un plan valide et le sélectionner suivant la vue et le budget de cibles. Les réflexions planaires ne sont pas de la réfraction multi-interface ; les sondes partagées déclarées restent non rendues. Ne pas ajouter de nouveau fichier de politique par scène si les propriétés existantes suffisent. [Politique optique et historique](v17/docs/CSP001_ZREFLECT_AUTHOR_POLICY.md).

### 5. Exemple complet : un laboratoire simple à deux matériaux

Créer les sept fichiers suivants. Il s'agit d'un exemple documentaire cohérent avec le chargeur V17.031, pas d'une scène dont le rendu a été exécuté dans cet audit. Le manifeste utilise une révision de scène d'exemple indépendante de la version du moteur. Les formats et clés sont issus du [Pavillon 01](scenes/v16/pavillon01/scene.json) et du runtime courant.

<details>
<summary>Afficher les sept fichiers JSON d'une scène minimale (sol et sphère inox)</summary>

**`scene.json`**

```json
{
  "format": "m3d-scene-v16",
  "name": "Laboratoire matériaux",
  "version": 1,
  "revision": "V17.001",
  "units": "m",
  "sectors": [
    {
      "id": "laboratoire",
      "geometry": [
        "geometry/meshes.json"
      ],
      "instances": [
        "instances/objects.json"
      ],
      "materials": [
        "materials/surfaces.json"
      ],
      "pbr": [
        "pbr/textures.json"
      ],
      "lighting": [
        "lighting/day.json"
      ],
      "cameras": [
        "cameras/views.json"
      ]
    }
  ]
}
```

**`geometry/meshes.json`**

```json
{
  "format": "m3d-geometry-v1",
  "units": "m",
  "meshes": [
    {
      "id": "unit_box",
      "type": "box",
      "size": [
        1,
        1,
        1
      ],
      "uv": "planar_per_face"
    },
    {
      "id": "unit_sphere",
      "type": "uv_sphere"
    }
  ]
}
```

**`materials/surfaces.json`**

```json
{
  "format": "m3d-materials-v1",
  "materials": [
    {
      "id": "sol_beton",
      "baseColor": [
        0.65,
        0.65,
        0.63
      ],
      "roughness": 0.83,
      "metallic": 0,
      "transmission": 0,
      "ior": 1.5,
      "textureSet": "tex_beton"
    },
    {
      "id": "inox_brosse",
      "libraryMaterial": "inox_01_01",
      "roughness": 0.35,
      "textureSet": "tex_inox",
      "planarReflection": "disabled"
    }
  ]
}
```

**`pbr/textures.json`**

```json
{
  "format": "m3d-pbr-v1",
  "sets": [
    {
      "id": "tex_beton",
      "material": "sol_beton",
      "pattern": "concrete",
      "resolution": 512
    },
    {
      "id": "tex_inox",
      "material": "inox_brosse",
      "pattern": "metal",
      "resolution": 512
    }
  ]
}
```

**`instances/objects.json`**

```json
{
  "format": "m3d-instances-v1",
  "instances": [
    {
      "id": "dalle",
      "parent": "Structure/Sol",
      "geometry": "unit_box",
      "material": "sol_beton",
      "position": [
        0,
        -0.1,
        0
      ],
      "size": [
        12,
        0.2,
        12
      ],
      "rotationY": 0
    },
    {
      "id": "sphere_inox",
      "parent": "Essais/Metaux",
      "geometry": "unit_sphere",
      "material": "inox_brosse",
      "position": [
        0,
        1,
        0
      ],
      "size": [
        2,
        2,
        2
      ],
      "rotationY": 0
    }
  ]
}
```

**`lighting/day.json`**

```json
{
  "format": "m3d-lighting-v1",
  "hour": 14.75,
  "lights": [
    {
      "position": [
        -2,
        3,
        1
      ],
      "radius": 6,
      "intensity": 1.3,
      "color": [
        255,
        220,
        185
      ]
    }
  ]
}
```

**`cameras/views.json`**

```json
{
  "format": "m3d-cameras-v1",
  "cameras": [
    {
      "id": "vue_01",
      "name": "01 · Sphère inox",
      "position": [
        4,
        1.7,
        5
      ],
      "target": [
        0,
        1,
        0
      ],
      "fovDegrees": 61
    }
  ]
}
```

</details>

Le sol supérieur est à Y=0 et la sphère de diamètre 2 m repose dessus. Les deux références de matériaux sont actives, pas les 8 192 presets. Changer de matériau ne demande ni duplication du maillage ni compilation moteur.

### 6. Caméras, validation et précompilation

Les caméras sont enregistrées dans `cameras.cameras[]` : `id`, `name`, `position`, `target`, `fovDegrees`. Elles définissent un point de vue métrique. Ajouter une caméra intérieure, extérieure et une vue des détails sensibles ; ne pas faire coïncider position et cible. Dans le JSON exporté par l'interface, `eye` correspond à `position`, `fovDeg` à `fovDegrees`. Le cadrage piscine V17.634 injecté au démarrage de V17.031 est une exception moteur de référence, pas une obligation de format pour d'autres scènes.

Ouvrir le manifeste avec **Fichier → Ouvrir une scène V16 (scene.json)…**. Contrôler : géométrie/pivots, normales/UV, matériaux, collisions, éclairage jour et nuit, transparence, reflets, caméra en mouvement et stabilité des détails. Tester DX11, Vulkan et LumaTrace, comparer ponctuellement au CPU sans exiger un résultat physiquement identique. Exporter les poses de diagnostic via le menu Fichier.

Depuis le dossier moteur extrait, préparer les accélérateurs hors ligne :

```bat
cse_scene_precompiler.exe "C:\scenes\ma-scene\scene.json"
cse_scene_precompiler.exe "C:\scenes\ma-scene\scene.json" --catalog-only
```

La première commande charge la scène et produit `precomputed/csp001-bvh.bin`, `precomputed/zreflect-index.json` et `precomputed/zreflect-candidates.json`, puis contrôle leur relecture. La seconde est un diagnostic du catalogue optique ; elle ne remplace pas la précompilation complète. Le chargeur vérifie les caches et revient à la construction native si nécessaire. Recalculer après une modification pertinente de géométrie, matériaux ou politique optique, sans réutiliser manuellement des caches d'une autre révision. `M3D_CSP001=0` permet un A/B du BVH précalculé ; `M3D_V17_DISABLE_PBR_CACHE=1` un A/B du cache de textures.

### 7. Publier et maintenir une scène

- Conserver les sources lisibles, les images et la provenance/licence des assets ; versionner les ressources et garder les identifiants stables.
- Pour une scène privée, le dossier suffit : aucun dépôt GitHub ni manifeste de publication supplémentaire n'est nécessaire.
- Pour la scène civique officielle, publier dans le dépôt distinct [moteur3d-scene](https://github.com/glaby91/moteur3d-scene), avec une nouvelle révision de scène et le contrat de release `V17-scene.zip` + `SCENE_VERSION.json` identique dans l'archive et à côté. Précompiler les caches et vérifier leurs empreintes. L'archive officielle contient `benchmark-civic/scene.json` et les ressources relatives ; conserver les conventions du dépôt de scène et les contrôles d'intégrité de l'updater. Le moteur sélectionne les releases Reflect compatibles, pas arbitrairement la dernière release de tout type.
- Valider avant publication : dossier autonome, fichiers JSON valides, références présentes, 256 matériaux actifs maximum, textures limitées, caches reproductibles, scènes chargées sur les backends cibles. Une correction de contenu ne demande pas de reconstruire le moteur, sauf si elle nécessite une évolution du format ou d'une fonctionnalité moteur.

Sources : [chargeur de manifeste](experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_scene_v16.cpp) · [bridge runtime](experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_v16_bridge.cpp) · [précompilateur CSP001](v17/tools/cse_scene_precompiler.cpp) · [Z Material](v17/docs/ZMATERIAL_001.md) · [documentation historique des scènes V16](docs/V16-MODULAR-SCENE.md). Les documents historiques ne priment pas sur le code V17 actuel.

### Scène historique de Pripiat — place centrale

La place centrale de Pripiat avec le Palais de la Culture Energetik et l’hôtel Polissya est un package séparé, compatible avec V16/V17. Il n’est pas fusionné dans la scène civique par défaut. Après extraction, l’ouvrir avec **Fichier → Ouvrir une scène V16 (scene.json)…**, puis sélectionner `Pripyat-V16/pripyat/scene.json`.

[Télécharger le package de la place centrale de Pripiat](https://github.com/glaby91/moteur3d-scene/releases/download/pripyat-scene-v16-compat-20261124.1/Pripyat-place-centrale-V16-V17-compatible.zip) · [Détails et version](https://github.com/glaby91/moteur3d-scene/releases/tag/pripyat-scene-v16-compat-20261124.1)

## Fonctionnalités

- Exploration interactive de la scène depuis ses caméras, avec compteur FPS et informations caméra.
- Choix du moteur de rendu depuis l’interface ; modes CPU et GPU conservés ensemble pour comparer qualité, coût et compatibilité.
- Fenêtres de rendu natives dédiées aux backends GPU. DX11, Vulkan et LumaTrace peuvent présenter par swapchain GPU directe sur Windows ; le chemin Qt/QImage reste disponible en solution de repli. Le chemin direct évite le readback d’image systématique, mais le temps de soumission CPU affiché ne garantit pas la durée GPU réelle.
- Navigation au sol avec collisions approximatives, escaliers, accroupissement, saut et échelles de piscine ; option de vol libre.
- Pip-Boy de diagnostic avec données moteur/scène, éclairage, ressources et état du backend.
- Radio 3D spatialisée à quatre haut-parleurs, flux Internet et fichier WAV local de secours ; bouton et volume dans l’interface.
- Mise à jour asynchrone de la scène civique, découplée du chargement initial.
- Benchmarks autonomes V16/V17 et cuisson hors ligne de la table d’éclairage global CPU.

## Commandes et touches

| Touche / action | Fonction |
|---|---|
| `Z Q S D` ou flèches | Marcher dans la direction regardée (AZERTY) |
| Souris | Orienter la caméra pendant l’exploration |
| `Maj` | Accélérer |
| `Ctrl` maintenu | S’accroupir en mode marche ; la caméra ne se redresse pas sous un plafond bas |
| `Espace` | Sauter depuis le sol en mode marche ; monter en vol libre |
| `Ctrl` en vol libre | Descendre |
| `E` près d’une échelle de piscine | Monter ou descendre l’échelle si le personnage est à proximité |
| Clic droit | Activer le zoom/lunette ×3 |
| `Tab` ou `<` brièvement | Afficher/masquer le Pip-Boy compact au poignet |
| `Tab` ou `<` maintenu environ 550 ms | Ouvrir le tableau de bord plein écran |
| `Échap` | Fermer le tableau de bord et arrêter l’exploration |
| `F11` | Basculer le viewport en plein écran |
| `F2` | Afficher/masquer le HUD de la fenêtre de rendu |
| `Ctrl + Maj + A` | Capturer l’anomalie courante en PNG et JSON |

La case **Marcher au sol** active le déplacement à hauteur d’homme (yeux à 1,70 m), les collisions et le saut. La décocher rend les commandes verticales de vol libre à `Espace` et `Ctrl`. Les collisions sont une approximation de navigation par rayons, pas une simulation physique complète.

Le Pip-Boy compact donne accès au tableau de bord plein écran. Celui-ci regroupe les pages **Aperçu**, **Scène**, **Lumières**, **Ressources** et **Diagnostic** : révision moteur/scène, objets et triangles, matériaux, lumières, caméra, résolution, cadence et signalements du backend. Une valeur non instrumentée est marquée comme indisponible plutôt que devinée.

La radio se pilote depuis le panneau latéral **SON AMBIANT** : démarrer/arrêter et régler le volume. Le lecteur tente le flux en ligne puis bascule sur `audio/pripyat_radio.wav` si le flux ne peut pas être lu. Le son spatial suit la caméra ; il ne bloque pas la boucle de rendu.

## Backends disponibles

| Choix dans l’interface | Calcul | Usage / limites |
|---|---|---|
| **CPU Ray Tracing PBR — direct** | Rayon primaire et éclairage direct CPU | Référence lente, utile pour comparer les rendus et les matériaux. |
| **GPU D3D11 Compute — rapide, ombres + verre** | Compute shader HLSL partagé avec V16 | Voie historique GPU, conservée pour compatibilité et diagnostics. |
| **CPU Éclairage global — 2 rebonds** | Ray tracing CPU et rebonds GI | Qualité réglable, coût élevé. |
| **CPU Éclairage global — 4 rebonds** | Ray tracing CPU et davantage de rebonds GI | Calcul plus lourd, destiné aux rendus de qualité. |
| **V17 GPU D3D11 Raster** | Rasterisation matérielle, profondeur et shaders | Backend recommandé pour explorer en temps réel. Éclairage raster approximatif. |
| **V17 Vulkan Raster** | Rasterisation Vulkan | Expérimental ; dépend du pilote et de la disponibilité du SDK à la compilation. |
| **LumaTrace BVH4 — éclairage GPU + Z Reflect** | Raster DX11 + Compute GPU BVH4, éclairage direct/indirect par sondes, réflexions planaires | Backend hybride temps réel, pas un path tracer multi-rebonds ni DXR. Z Reflect activé par défaut. |

Sur Windows, l’option **Affichage GPU direct natif** permet aux backends DX11, Vulkan et LumaTrace de présenter dans leur propre fenêtre. Désactivez-la pour utiliser la sortie Qt/QImage. Vulkan peut rester indisponible si le SDK n’était pas présent à la compilation ou si le pilote ne permet pas son initialisation. Le menu indique l’état du backend.

## LumaTrace — architecture hybride GPU

LumaTrace s'appuie sur le pipeline raster Direct3D 11 pour la visibilité principale, les matériaux PBR, les textures, la profondeur et la présentation native. Les calculs massifs restent sur le GPU : compute HLSL, parcours BVH4, radiance directe par triangle et éclairage indirect approximé par sondes spatiales. La voie de sondes utilise deux passes et n'est pas un path tracer à rebonds arbitraires. Les données de scène et buffers indexés restent autant que possible résidents sur GPU ; en présentation native, aucune lecture des pixels vers le CPU n'est nécessaire. Le CPU prépare la géométrie, les index, les sélections et commandes ; il ne recalcule pas une image GPU. Les caches précalculés de la scène réduisent le travail au chargement. Le module adaptatif de budget 0/1 reste optionnel (`M3D_V17_LUMA_ADAPTIVE=1`), pas activé arbitrairement.

### Z Reflect — réflexions planaires activées par défaut

1. Le catalogue optique et son index spatial identifient les surfaces autorisées par le créateur de scène. La politique distingue AUTO et exclusions ; l'ascenseur est exclu de la réflexion planaire. La sonde partagée de l'ascenseur reste une déclaration, pas un rendu actif.
2. Le CPU interroge les plans visibles, calcule leur surface projetée réelle (triangles découpés), réutilise le résultat à caméra strictement identique et retient les plans admissibles. Le HUD affiche `ZR ON actifs/total` ; les plans différés sont instrumentés. V17.631 comporte 5 plans, 121 objets candidats, 29 vues de référence ; 2 cibles partagées à demi-résolution sont disponibles à l'exécution. Ce plafond de ressources ne limite pas le catalogue.
3. Une caméra miroir est calculée pour chaque plan actif. Eau et vitrage utilisent les mêmes cibles réutilisables. Frustum et filtrage CPU conservatif des blocs entièrement derrière le plan réduisent la géométrie soumise. Les passes secondaires sont opaques, sans récursion. Le Z Prepass principal reste actif ; le prépass miroir est désactivé par défaut (configuration C validée).
4. La texture est composée dans le shader selon Fresnel, matériau et, pour l'eau, distorsion. Ce sont des reflets planaires, pas de vraie réfraction multi-interface ; les miroirs non planaires et sondes partagées ne sont pas implémentés.

Le coût n'est pas nul : une caméra miroir rasterise et éclaire une partie de la scène une seconde fois. En 720p HD620, la campagne V17.022 a retenu C : 21,71 FPS moyens sur quatre vues contre 19,63 pour B ; 1,77 ms GPU de reflet piscine et 18,07 ms façade. Les valeurs ne garantissent pas les FPS de V17.024. Objectif à terme : conserver la crédibilité visuelle (notamment silhouettes d'arbres dans les vitrages) pour un coût marginal, idéalement 5–10 % du budget image. Z Warp (rayons ciblés en escargot, matrices adaptatives) est une piste différée [#94](https://github.com/glaby91/moteur3d/issues/94), non implémentée.

| Option | Défaut / effet |
|---|---|
| `M3D_V17_ZREFLECT` | ON en LumaTrace ; 0 désactive les vitrages planaires, l'eau reste active |
| `M3D_V17_ZREFLECT_CPU_CULL` | 1, filtrage conservatif miroir |
| `M3D_V17_ZREFLECT_PREPASS` | 0, prépass miroir seul |
| `M3D_V17_DISABLE_Z_PREPASS` | 0, contrôle global (ne pas confondre avec le prépass miroir) |
| `M3D_V17_ZREFLECT_GPU_TIMING` | 1 pour activer les timestamps GPU non bloquants de réflexion native |

Benchmarks : `LANCER_PARCOURS_30S.bat` (DX11/Vulkan/LumaTrace), `LANCER_ZREFLECT_AB.bat` (16 mesures), `LANCER_ZREFLECT_MATRIX.bat` (32 mesures A/B/C/D), `LANCER_LUMATRACE_01_AB.bat` (circuit complet), `LANCER_LUMATRACE_01_PISCINE.bat`. Les scripts téléchargent la scène officielle et vérifient ses empreintes ; le CSV mesure les FPS, la sélection/soumission CPU, le temps GPU cumulé des reflets et les triangles par plan. SYSTEM_INFO.json identifie le matériel, la compilation et les options.

Sources : [validation C](v17/docs/CSP001_ZREFLECT_V17023_VALIDATION.md), [matrice A/B/C/D](v17/docs/CSP001_ZREFLECT_V17022_MATRIX.md), [module adaptatif](v17/docs/LUMATRACE_ADAPTIVE_001.md). La CI principale vérifie les caches de la scène V17.631, les sélections, les quatre configurations et la parité pixel par pixel sur quatre vues WARP, ainsi que les plans de benchmark et les commandes d'exploration. L'ancien workflow exhaustif demeure disponible manuellement ; les tests matériels restent indispensables.

## Z Material et Z Atmosphere — état actuel

Z Material 001 améliore la restitution des métaux opaques non émissifs par pondération du diffus et approximation directionnelle de l'environnement ; les chemins optiques spéciaux sont conservés. DX11 et LumaTrace partagent le shader, Vulkan possède son implémentation. Le correctif ne crée ni rayon, ni caméra, ni lecture de texture ou tampon GPU supplémentaire. Le gain de cohérence visuelle constaté en V17.030 ne clôt pas L03 ; les 31 850 496 cas du [benchmark synthétique](v17/materials/benchmark_8192.py) ne mesurent ni FPS réels ni fidélité de l'image complète. Comparer avec `LANCER_SANS_ZMATERIAL.bat` (variable `M3D_V17_ZMATERIAL=0`).

Z Atmosphere 002 est une passe additive de grains lumineux, pas une simulation volumétrique physique ni un brouillard. Désactivée par défaut ; `LANCER_ZATMOSPHERE.bat` ou `M3D_V17_ZATMOSPHERE=1` l'active ; gain par défaut 0,002. Aucun nouveau rayon BVH4, coût GPU à mesurer. [Description et limites](v17/docs/ZATMOSPHERE_001.md).

## DirectX 11 — R001 validé sur HD620 (V17.025)

Branche de développement validée, intégrée au moteur principal master V17.029. R001 reste ON par défaut pour DX11 Raster (A/B : `M3D_V17_DX11_R001=0`). AABB conservatif, masques partagés profondeur/couleur, filtrage des lumières hors champ ou sans énergie, suppression des lectures d'ombre à daylight nul, contrôles des limites. Cette optimisation R001 ciblait DX11 Raster ; les changements ultérieurs du shader partagé (L01/L02, Z Material) bénéficient également à LumaTrace. Aucun changement de scène dû à R001.

Benchmark HD620, 1366×768, MSAA 2x, 4 × 90 images OFF/ON/ON/OFF, trajectoire déterministe, preview matériel avec readback : **14,3576 → 16,7388 FPS (+16,59 %)** ; GPU draw 55,94 → 44,20 ms ; 12 captures PNG identiques par indice. CI Windows et huit tests WARP jour/nuit réussis. [Rapport R001](v17/docs/DX11_R001.md) · [Données comparatives](v17/docs/benchmarks/DX11_R001_COMPARAISON.csv) · [Compilation validée](https://github.com/glaby91/moteur3d/actions/runs/46008251282).

**Décision historique du 27/11/2026 (avant L01–L03) :** R002 différé. Conserver Geometry Prep, ordre des triangles, rendu indexé, instancing et Z Prepass ; aucune réorganisation de géométrie sans preuve préalable. Ombres GPU, backface sélectif, Z Light par tuiles, compression et transparences restent des pistes différées. Simulations et 3–5 vérifications avant toute compilation d'une future modification. Fusionner sur master uniquement après décision explicite et contrôles de non-régression.

## Architecture modulaire

V16 et V17 assemblent les mêmes modules partagés ; la V17 n’est pas une deuxième copie indépendante du noyau. [`engine/render_modules.json`](engine/render_modules.json) sert de registre commun : identifiant, révision, sources, dépendances et caractère optionnel de chaque module. Une correction ciblée doit incrémenter le module concerné plutôt que modifier un numéro global sans traçabilité.

Le découpage récent place l’orchestration de l’interface dans `v17/moteur3d_v17_qt.cpp` et les chemins de rendu dans des fichiers distincts :

- `v17/moteur3d_v17_render.cpp` : orchestration partagée et préparation du rendu ;
- `v17/moteur3d_v17_render_cpu.cpp` : ray tracing CPU et GI ;
- `v17/moteur3d_v17_render_compute.cpp` : dispatch du backend Compute ;
- `v17/moteur3d_v17_render_d3d11.cpp` : raster DX11 ;
- `v17/moteur3d_v17_render_vulkan.cpp` : raster Vulkan ;
- `v17/moteur3d_v17_render_lumatrace.cpp` : interface de rendu LumaTrace ;
- `v17/scene_update_manager.h` : vérification, téléchargement et installation de scène ;
- `v17/diagnostic_dashboard.h` : interface Pip-Boy et pages de diagnostic ;
- `v17/vulkan/m3d_vulkan_resources.*` : durée de vie des buffers, images et mémoires Vulkan.

Le moteur partagé se trouve principalement dans `experiments/2026-10-30-cote-ete-gt/native_v14/src/`. Les modules CPU couvrent intersection, éclairage, matériaux, SIMD, post-traitement et GI. Les backends GPU restent spécialisés. Le registre et les révisions se trouvent dans `engine/render_modules.json` ; les configurations CMake V16/V17 assemblent les mêmes composants.

Détails du découpage : [`v17/README.md`](v17/README.md) · [registre des modules](engine/render_modules.json) · [architecture et validation Vulkan](v17/vulkan/README.md).

## Compiler

La compilation de référence est le workflow [CSE WARP V17 — Main Windows x64](https://github.com/glaby91/moteur3d/actions/workflows/build-main-v17.yml). Il construit l’application, les backends disponibles et les outils, vérifie la scène de référence épinglée et produit un artefact moteur sans scène.

Pour compiler localement, installer Visual Studio 2022 avec C++ x64, CMake 3.24 minimum, Qt 6.8.3 (module Multimedia) et le Vulkan SDK (CI : 1.4.309.0). Extraire d'abord une scène officielle, par exemple V17.634, dans un dossier extérieur au dépôt. La configuration exige un `scene.json` dans `V17_SCENE_SOURCE_DIR` : le répertoire historique `scenes/v16/benchmark-civic` ne contient pas la scène complète dans le dépôt principal. Depuis la racine du dépôt, en adaptant les chemins :

```powershell
cmake -S v17 -B build-v17 -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" `
  -DV17_SCENE_SOURCE_DIR="C:/scenes/V17.634/benchmark-civic" `
  -DM3D_REQUIRE_VULKAN=ON
cmake --build build-v17 --config Release --parallel 4
```

Le workflow principal télécharge les scènes de référence épinglées (V17.631 optique, V17.633 émission, V17.634 atmosphère), vérifie leurs empreintes et distribue un paquet moteur sans scène. Le build CMake local copie la scène source dans son dossier de sortie ; il ne faut pas confondre cette sortie avec l'artefact officiel. Pour un essai local de matériaux de bibliothèque, copier aussi `v17/materials/zmaterial8192.json` dans `materials/` à côté de l'exécutable si absent (le packaging CI le fait). Une compilation locale seule n’exécute pas automatiquement toutes les étapes de contrôle et de packaging Windows de la CI.

## Benchmarks en ligne de commande

Le paquet Windows fournit `LANCER_BENCHMARK.bat`, qui appelle `moteur3d_v17_campaign.exe` sans ouvrir l’interface. Les commandes s’exécutent depuis le dossier extrait. Les chemins d’entrée/sortie peuvent être relatifs à ce dossier.

### Campagne Full Benchmark multi-backend

```bat
LANCER_BENCHMARK.bat "benchmark-civic\scene.json" "results\full" --full-benchmark --all-scenes --include-cpu --rounds 6 --frames 12 --warmup 2
```

La matrice compare DX11 Raster (pipelines de lecture à 1/2/3 tampons), Vulkan Raster et LumaTrace aux mêmes tailles raster, puis Compute DX11, CPU ray tracing PBR direct et GI CPU à 2 et 4 rebonds dans un groupe de diagnostic basse résolution. Les cas CPU/Compute ne sont pas présentés comme comparables en FPS aux images raster 1366×768 et 1920×1080.

### Options de campagne (`moteur3d_v17_campaign.exe`)

La syntaxe complète est :

```text
moteur3d_v17_campaign scene.json outdir [--full-benchmark] [--rounds N] [--frames N] [--warmup N] [--all-scenes] [--include-cpu] [--smoke] [--simulation] [options raster]
```

| Option | Effet / valeurs |
|---|---|
| `--full-benchmark` | Lance la matrice étendue des backends ci-dessus. |
| `--rounds N` | Nombre de séries indépendantes, de 1 à 30 (défaut 6). |
| `--frames N` | Images mesurées par série, de 2 à 240 (défaut 12). |
| `--warmup N` | Images d’échauffement par série, de 0 à 20 (défaut 2). |
| `--all-scenes` | Parcourt les scènes de benchmark distribuées. Sans cela, utilise seulement le fichier scène fourni. |
| `--include-cpu` | Ajoute le profil de threads CPU et le diagnostic Compute DX11 hors Full Benchmark. |
| `--smoke` | Contrôle court : une scène, 320×180, 2 séries de 2 images et 1 image d’échauffement. Ignore `--all-scenes` et `--include-cpu`. Incompatible avec `--simulation`. |
| `--simulation` | Réduit les textures à 256 px, utilise 256×144 / 480×270 et fixe 2 séries, 2 images, 1 échauffement. Contrôle fonctionnel, pas une mesure de FPS matérielle. |
| `--raster-msaa N` | MSAA 1, 2, 4 ou 8. |
| `--raster-anisotropy N` | Filtrage anisotrope 1, 2, 4, 8 ou 16. |
| `--raster-lod-percent N` | Qualité/distance LOD, 50 à 200 %. |
| `--raster-culling 0\|1` | Désactive/active le culling de frustum. |
| `--raster-lod-enabled 0\|1` | Désactive/active le LOD dynamique. |

Chaque exécution crée un sous-dossier horodaté avec `summary.json`, `campaign_results.csv`, `campaign_frames.csv`, `campaign_report.html` et les PNG de référence disponibles. Le cas LumaTrace non pris en charge est marqué `skipped` plutôt que compté comme échec de toute la scène.

### Benchmark DX11 par caméras et captures fixes

```bat
moteur3d_v17_benchmark.exe "benchmark-civic\scene.json" "results\raster" --all-cameras --compare-readback --raster-msaa 2 --raster-anisotropy 8 --raster-lod-percent 85 --raster-culling 1 --raster-lod-enabled 1
```

Options utiles : `--quick` mesure une caméra à 1366×768 (6 images) ; `--smoke` fait un contrôle CI court avec textures réduites ; `--all-cameras` parcourt toutes les caméras ; `--camera-id ID` sélectionne une seule caméra ; `--readback-depth 1|2|3` règle le nombre de tampons GPU/CPU ; `--compare-readback` compare les pixels en 1/2/3 tampons ; `--compare-v16` ajoute les voies V16 Compute HLSL et CPU direct ; `--fhd-stills` produit une capture 1920×1080 ; `--hour H` choisit l’heure entre 0 inclus et 24 exclu. Les cinq options raster sont celles décrites dans le tableau précédent.

Pour un rendu CPU fixe, préciser la caméra et la résolution :

```bat
moteur3d_v17_benchmark.exe "benchmark-civic\scene.json" "results\cpu" --camera-id v7_pool_fresco --hour 14 --cpu-direct-still --cpu-direct-width 960 --cpu-direct-height 540
```

`--cpu-direct-width` et `--cpu-direct-height` acceptent 320 à 1920 pixels. `--cpu-direct-still` nécessite `--camera-id`.

### Trajectoire GPU matérielle DX11

```bat
moteur3d_v17_gpu_trajectory.exe "benchmark-civic\scene.json" "results\trajectory"
```

Ce benchmark Windows cible le GPU matériel, la scène et la caméra de vitrage `v8_glazing_raccords`. Par défaut il mesure une trajectoire de six secondes à 1366×768 sur 90 images ; `--fixed-camera` garde une vue fixe. `--ci-smoke` réduit le test à 640×360 et 4 images pour la CI. Les interrupteurs expérimentaux `--z-prepass=on|off` et `--light-aabb=on|off` permettent une comparaison A/B. Un adaptateur GPU matériel est requis hors `--ci-smoke`.

### Cuisson GI et benchmark V16

```bat
moteur3d_v17_gibake.exe "benchmark-civic\scene.json" "results\scene.m3dgi" --bench "results\gi.json"
moteur3d_v16_benchmark.exe "pavillon01\scene.json" "results\v16" --cpu-only
```

`moteur3d_v17_gibake.exe scene.json cache.m3dgi` calcule puis écrit une table GI CPU. `--smoke` utilise un volume réduit ; `--full-gi` garde la table complète même en smoke ; `--bench report.json` ajoute les médianes de cuisson, relecture disque et interpolation. Le benchmark V16 accepte `--quick` (équivaut à CPU-only rapide) ou `--cpu-only` (désactive le profil GPU Compute HLSL).

### Interpréter les mesures

Les rapports séparent chargement, compilation de scène, textures/PBR, construction BVH et rendu. Ils produisent médiane/P50, P95/P99, dispersion et FPS estimés lorsque le protocole s’y prête. Une construction BVH plus rapide ne signifie pas nécessairement un rendu plus rapide.

Les benchmarks d’aperçu lisent les pixels hors écran : ils ne mesurent ni la présentation native swapchain ni le rafraîchissement de l’écran. Le temps de soumission CPU en affichage direct n’est pas le temps réel d’exécution GPU. La CI Windows peut utiliser WARP, un GPU logiciel ; ses résultats ne prédisent pas les FPS d’une Intel HD 620 ou d’une carte matérielle. Pour comparer deux versions, garder ordinateur, scène, caméra, résolution, backend, qualité et protocole identiques, puis répéter les mesures.

## Validation et limites

Le workflow Windows x64 compile les cibles V16/V17 et les outils, vérifie le paquet, exécute les précontrôles Z Material, les diagnostics natifs, les tests R001, Z Reflect, émission et atmosphère sur WARP, ainsi que le chargement et les contrôles de navigation. Il ne réalise pas automatiquement une campagne FPS matérielle complète. Une CI verte valide les chemins exécutés sur le runner, pas la fluidité sur le GPU de l’utilisateur. Vulkan et LumaTrace restent expérimentaux ; les performances matérielles, la parité visuelle complète et les coûts de présentation native doivent être vérifiés séparément.

La version **LumaTrace BVH4** est un backend hybride d’éclairage GPU et de réflexions planaires. Elle ne remplace pas le moteur CPU complet et ne constitue pas encore un path tracer multi-rebonds. Les résultats de benchmark doivent donc être lus comme des chemins distincts, pas comme une comparaison de qualité physique universelle.

## Organisation du dépôt

- `v17/` : interface, intégration V17, backends, shaders, gestionnaire de scène et outils.
- `v16/` : exécutable historique, benchmark et configuration du noyau partagé.
- `engine/render_modules.json` : registre des modules partagés, versions et dépendances.
- `v17/materials/` : catalogue Z Material 8192, précontrôles et benchmark numérique synthétique.
- `scenes/v16/` : scènes/fixtures internes de test et benchmark ; la scène civique de référence est publiée séparément ; elle peut être copiée dans une sortie CMake locale, mais pas dans le paquet officiel moteur.
- `experiments/` : code moteur partagé et prototypes conservés pour comparaison.
- `.github/workflows/` : compilation, tests de chargement/rendu, packaging et publication des artefacts.
- `docs/` et `v17/docs/` : architecture, validation, outils et protocoles de benchmark.

Références : [Guide moteur V17](v17/README.md) · [Full Benchmark](v17/docs/FULL_BENCHMARK.md) · [Version moteur](v17/version.json) · [Pistes d’optimisation raster](v17/docs/RASTER_PERFORMANCE_ROADMAP.md).

## Benchmark de déplacement de 30 secondes

1. Placer `LANCER_PARCOURS_30S.bat` à côté de `moteur3d_v17.exe`, dans le package Windows V17 décompressé avec ses DLL et ses plugins Qt. Le même BAT se réutilise à chaque campagne. [Télécharger le lanceur](https://raw.githubusercontent.com/glaby91/moteur3d/master/v17/tools/LANCER_PARCOURS_30S.bat).
2. Double-cliquer sur le BAT. Le paquet actuel inclut le script PowerShell, `route30.exe` et le noyau partagé utilisé par l’exploration. Un BAT téléchargé seul récupère le script sur GitHub. Le script consulte la dernière publication de `glaby91/moteur3d-scene`, télécharge `V17-scene.zip` au premier lancement et affiche la révision chargée. La scène du package peut être absente ou incomplète. Le benchmark extrait sa propre copie avec les textures. Il conserve les archives dans `benchmark-tools/archives`, en contrôle la taille et le SHA-256 à chaque lancement et télécharge à nouveau toute archive incomplète. [Publications de la scène](https://github.com/glaby91/moteur3d-scene/releases/latest).
3. Pendant la campagne, garder la fenêtre de rendu visible. À la fin, récupérer `benchmark-results/parcours30-<date>.zip`. Il contient `SCENE_VERSION.json` et `resultats.csv`. Si un backend échoue, sa ligne indique l’erreur, les autres parcours continuent et le ZIP conserve les mesures réussies.

DX11 Raster, Vulkan Raster et LumaTrace présentent directement les images dans une fenêtre de **1280 × 720 pixels**, via DXGI ou la swapchain Vulkan. Dans le parcours actuel, la synchronisation verticale est désactivée (intervalle DXGI 0 et mode Vulkan IMMEDIATE demandé). Le benchmark ne doit pas comparer silencieusement un backend plafonné par VSync à un autre non plafonné. Le benchmark contrôle à chaque image que le moteur rapporte zéro tampon de relecture. Les trois passages suivent le même mouvement latéral sinusoïdal devant la piscine et la fresque, à 14 h 45. Chacun commence par deux images d’échauffement avant les 30 secondes chronométrées.

Dans le CSV, `frames_completed_30s` compte les appels de présentation réussis avant la fin des 30 secondes et `fps_30s` vaut ce nombre divisé par 30. `mean_render_ms` mesure le temps écoulé côté application, y compris l’attente éventuelle de présentation et la dernière image commencée. Les mesures valides portent `presentation=native_swapchain`, `vsync=off` et `readback_buffers=0`. L’ancien parcours utilisait 1000 × 680 avec relecture GPU vers CPU. Le changement simultané de résolution et de présentation empêche d’en déduire le coût de la copie seule.

La préparation des triangles statiques est **activée par défaut** dans l’exploration et dans ce parcours pour DX11, Vulkan et LumaTrace. Elle a lieu avant les 30 secondes chronométrées. Dans `resultats.csv`, `geometry_prep_requested=on` indique la demande et `geometry_prep_enabled=true` confirme son application par le backend. `geometry_source_vertices` et `geometry_unique_vertices` comptent les sommets avant et après préparation. `geometry_index_count` donne le nombre d’indices du maillage préparé. Les compteurs restent vides si le backend échoue.

**Validation du 26 novembre 2026.** Sur la scène V17.629, le parcours piscine de 30 s en 720p a confirmé `geometry_prep_enabled=true` et `readback_buffers=0` pour les trois backends. DX11 a présenté 691 images (23,03 FPS), Vulkan 884 (29,47 FPS) et LumaTrace 815 (27,17 FPS). Ces valeurs correspondent à un passage historique par backend sur la machine de test, avec le protocole de cette révision ; elles ne sont pas des mesures du parcours actuel VSync OFF ni du profil Ultra V17.031. [Révision du parcours validé](https://github.com/glaby91/moteur3d/commit/900bf11df4833e881141682741ae814202cf2403).

Pour comparer avec la préparation désactivée, enregistrer le script à côté du BAT et lancer `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\route30.ps1 -Root . -GeometryPrep off` depuis ce dossier. Les deux modes utilisent le même exécutable et le même parcours. [Script du parcours](https://raw.githubusercontent.com/glaby91/moteur3d/master/v17/tools/route30.ps1).

## Audit historique et registre des optimisations — 26 novembre 2026

**Archive de l'état au 26/11, non feuille de route active :** les validations et décisions plus récentes du début de ce README priment (notamment L01/L02 résolues et L03 partiel). Les anciennes mesures sont conservées sans les réinterpréter.

**Périmètre :** origines BASIC (2019), premiers moteurs HTML/JavaScript d’octobre 2026, V9–V13.16, portage natif V14–V15, V16 et V17 jusqu’à V17.010. Audit documentaire et statique, sans nouvelle compilation ni mesure. Les résultats anciens restent liés à leur scène, leur matériel et leurs paramètres. Ce registre distingue les acquis, les prototypes, les essais décevants et les propositions ; il ne certifie pas la présence de chaque optimisation dans la tête actuelle.

### Historique algorithmique

| Génération | Travaux et enseignements |
|---|---|
| BASIC 2019 | Triangles, couleurs, éclairage, lecture BMP ; patrimoine conservé. |
| HTML initial (18 octobre) | Canvas, tri par profondeur moyenne, 32 buffers RGB565, prévisions N+1 à N+5, collisions simplifiées, patrouille. Les buffers futurs ne sont pas des rendus complets validés. |
| Anti-stuttering | Tuiles 16×16, réutilisation des régions stables lorsque caméra et lumière sont fixes ; pré-rendu concurrent et reprojection Z désactivés après artefacts. Le compteur de copie Canvas (~40 ms) ne mesure pas la reprojection seule. |
| V3–V4 | Carte Z lumineuse unique, cache statique, PCF, interpolation de deux cartes d'ombre pour stabiliser le mouvement. |
| V5 | Z-buffer caméra par pixel, clipping proche, framebuffer RGBA ; test d'inversion de 2 310 triangles : 0 pixel différent sur 76 800 pour la pose testée. |
| V6–V7 | Stress de tessellation ×10 (24 100 triangles), arbre détaillé, PCF 4×4, lissage de contours optionnel ; réutilisation du maillage natif pour les ombres. |
| V8–V8.1 | RGB565, mipmaps 8–4096, UV perspective-correctes, filtrage, LOD par tuile et face ; suppression de subdivisions coplanaires ; Z puis texturage des seuls pixels visibles. |
| V9–V9.2 | Ray casting analytique, BVH, candidats par tuile, un rayon par pixel interne ; arrêt anticipé PCF, cache matériaux, suppression d'allocations, LUT de brouillard ; ciel interpolé et résolution interne optionnelle. |
| V9.3 | Banc N+1 Web Workers synthétique, étude du rendu par frontières de visibilité, cache de surfaces et reprojection contrôlée. |
| V11–V13.16 | Scènes Côte été GT/restomod, nombreuses révisions HTML, modes FHD/upscale, contrat de backend CPU V13.15, workers V13.16 ; détail de chaque diff intermédiaire restant à auditer. |
| V14–V15 | C++/Qt/Win32, BVH, tuiles multicœurs, SIMD AVX2/FMA3, AABB, PBR/HDR, débruitage, Compute HLSL, DX11 raster, détection DXR. |
| V16 | Scènes modulaires, matériaux PBR, GGX/Schlick, espace linéaire, ACES/sRGB, GI CPU/GPU, cache indirect à demi-résolution, comparaison des backends. |
| V17 | Exploration, instancing, LOD, Z-prepass, light culling, sondes et tables GI, Vulkan, présentation GPU native, LumaTrace BVH4, cache de radiance, préparation géométrique indexée, scène séparée et versioning modulaire. |

Archives : [HTML et expérimentations](experiments/), [V9.2 archivée](https://github.com/glaby91/moteur3d/tree/archive/master-v9-before-warp-v17-261114), [V11–V13.16](experiments/2026-10-30-cote-ete-gt/), [audit V15](docs/audits/AUDIT_GLOBAL_MOTEUR3D_V15_2026-11-06.md), [audit V17 du 14 novembre](docs/audits/AUDIT_GLOBAL_CSE_WARP_FONCTIONNALITES_OPTIMISATIONS_2026-11-14.md), [audit du 18 novembre](docs/AUDIT_GLOBAL_MOTEUR3D_2026-11-18.md).

### Résultats historiques à préserver

| Expérience | Résultat | Limite / décision |
|---|---|---|
| V8.1 Z puis textures | 1 911 → 925 ms à 800×600 ; Z identique, erreur couleur max 2/255 | DOM/Canvas simulé ; conserver séparation visibilité/shading. |
| V9.1 shading | 5 824 → 5 039 ms | Banc simulé ; conserver les courts-circuits exacts et caches. |
| V9.2 demi-résolution | 10 932 → 2 915 ms à 1920×1200 affiché | Banc simulé, perte de résolution interne assumée. |
| N+1, i5-7200U | 3 758 ms sans worker ; 4 428 / 4 904 / 5 733 ms avec 1/2/3 workers | Charge synthétique concurrente, pas de vraies images candidates ; ne pas multiplier les workers aveuglément. |
| Rapture CPU FHD | 21,14 / 11,33 / 6,10 / 5,98 s avec 1/2/4/8 threads | Seulement ~2 % gagnés de 4 à 8 ; pool adapté au matériel. |
| AABB rejet anticipé | 12–16 % sur un relevé | Reprofiler le chemin actuel. |
| Feuilles AVX2 de 8 triangles | ~33 % de tests d'objets supplémentaires, gain non convaincant | Ne pas généraliser ; comparer feuilles sœurs et scalaire. |
| CPU GI Pavillon FHD | 4,03 s à 1 spp ; 15,87 s à 4 spp ; 94,91 s à 16 spp/2 rebonds | Le grain à 64 spp restait signalé ; débruitage guidé à étudier. |
| Triple buffering | +3,84 % débit moyen ; latence P50 35,60 → 91,58 ms | Ne pas imposer en exploration. |
| Z-prepass HD620, 1366×768 MSAA2 | GPU draw 64,41 → 39,94 / 41,18 ms ; débit calcul +50–52 % | Trois PNG identiques par campagne ; trajet ciblé, pas toutes caméras. |
| Multirésolution périphérique | ~12 % sur WARP dans une variante avec artefacts | Sous le seuil historique de 15 %, pas de généralisation. |
| Parcours 30 s, 720p, scène V17.629 | DX11 23,03 FPS ; Vulkan 29,47 FPS ; LumaTrace 27,17 FPS | Mesure documentée le 26/11, un passage par backend, préparation ON et zéro readback ; pas une comparaison ON/OFF. |

Sources : [V8.1](experiments/2026-10-19-v8-1-optimized-raster/README.md), [V9.2](experiments/2026-10-19-v9-2-fast-sky-16x10/README.md), [N+1](experiments/2026-10-29-v92-workers-4k-target/README.md), [Z-prepass HD620](docs/performance/V17.432-Z-PREPASS-RESULTATS-HD620-20261113.md), [campagne historique](docs/audits/AUDIT_GLOBAL_CSE_WARP_FONCTIONNALITES_OPTIMISATIONS_2026-11-14.md).

### Registre des pistes d'amélioration

Priorités proposées : P0 = fidélité/robustesse bloquante ; P1 = optimisation prioritaire sans perte visuelle ; P2 = expérimentation ciblée ; R = recherche séparée. Les statuts historiques ne valent pas certification de la V17.010.

| Domaine | Piste | État / action |
|---|---|---|
| Visibilité | Z-buffer, clipping, Z-prepass | Acquis ; conserver test d'invariance à l'ordre des faces opaques, vérifier tous backends. |
| Visibilité | Frustum/AABB, rejet spatial exact, tri opaque front-to-back | P1 : mesurer CPU/GPU, préserver les objets et ombres hors champ utiles. |
| Visibilité | Occlusion hiérarchique (Hi-Z) | P1 : prototype conservateur, repli pour cas ambigus, mesurer le coût. |
| Géométrie | Indexation, préparation statique, buffers persistants | P1 : préparation documentée active DX11/Vulkan/LumaTrace ; A/B à scène et parcours identiques. |
| Géométrie | Instancing, regroupement matériaux, LOD projeté | P1 : vérifier présence et efficacité, sans modifier silhouettes ni transparences. |
| Géométrie | Suppression subdivisions coplanaires redondantes | P1 : préserver normales, UV, matériaux, frontières, collisions et identifiants. |
| Géométrie | Tessellation adaptative près des sources | P2 : mesurer le compromis radiance/coût ; ne pas densifier partout. |
| Lumières | Boucle globale, limite de sources, rejet spatial | P1 : conserver les contributions réelles ; ne pas supprimer des lampes pour afficher un gain artificiel. |
| Lumières | Frustum des sphères d'influence (PR 44) | Prototype ; benchmark sur master actuel. |
| Lumières | Masques par tuile 32×32 (PR 45) | Prototype ; capacité historique 256 sources, mesurer coût CPU/upload. |
| Lumières | Z-light culling / tiled ou clustered lighting | P1 : exploiter profondeur min/max du Z-prepass, vérifier transparences et absence de lumière perdue. |
| Ombres | Cache carte Z, PCF anticipé, interpolation temporelle | P1 : réévaluer les optimisations V4/V9.1, mesurer lectures et stabilité. |
| Matériaux | RGB565 historique, mipmaps, UV, cache PBR disque | P1 : contrôler formats, bande passante, intégrité et cache sans baisser la qualité. |
| Matériaux | Normal mapping, GGX, Fresnel, transmission | P0 : parité et classification eau/verre, normales GLB, texture source, orientation et énergie. |
| Matériaux | Reflet piscine / caméra miroir | P0 : vérifier plan, matrice miroir, projection, clipping, coordonnées et dépendance caméra ; cause non confirmée. |
| Éclairage | Sondes 3D (16×8×16), 25 états horaires, table GI CPU | P2 : vérifier cache et fuites lumineuses, interpolation temporelle et cohérence d'énergie. |
| Éclairage | GI GPU à demi-résolution | P2 : reconstruction guidée par profondeur/normales ; comparer au rendu complet. |
| Éclairage | Débruitage CPU/GPU | P2 : guides albédo/normales/profondeur en HDR avant tone mapping ; préserver les détails. |
| LumaTrace | BVH4, cache de radiance par triangle, quatre rayons diffus déterministes | P1 : profiler premier dispatch et réutilisation ; comparer au CPU de référence. |
| LumaTrace | Regroupement des émissifs en cellules de 4 m, jusqu'à 16 sources d'appoint | P1 : valider projection des lampes, ombres, énergie, éviter double comptage. |
| LumaTrace | Invalidation locale / progressive du cache, jour-nuit | P1 : distinguer géométrie, matériaux, lumière locale et soleil ; cohérence temporelle. |
| LumaTrace | Interpolation de radiance, échantillonnage près des lampes, ombres de contact | P2 : vérifier fuites et discontinuités, budget de rayons fixe. |
| LumaTrace | Transparences, spéculaire, deuxième rebond sélectif | P0 pour transparence ; P2 pour transport additionnel. Pas un path tracer complet. |
| LumaTrace | BVH compact/quantifié, cache disque, calcul asynchrone | P2 : mesurer précision, mémoire, synchronisation et latence. |
| CPU | Pool persistant, ordonnancement des tuiles, localité mémoire | P1 : adapter le nombre de workers au matériel. |
| CPU | SIMD, SoA, AVX2/FMA3, feuilles sœurs, sélection adaptative | P2 : comparer au scalaire, ne pas présumer un gain. |
| GPU | Ressources/shaders persistants, atlas, formats compacts | P1 : mesurer allocations et transferts, éviter recompilation par frame. |
| GPU | Présentation DXGI / Vulkan WSI, zéro readback | Acquis documenté ; vérifier fallback Qt, resize, shutdown, temps GPU et latence écran. |
| GPU | 1/2/3 buffers, staging, UMA, frames in flight | P2 : privilégier fluidité et latence, pas seulement débit. |
| GPU | Vulkan vs DX11, durée de vie des ressources | P1 : profiler les mouvements et le cycle jour/nuit, vérifier absence de fuite. |
| GPU | MSAA, resolve, copy, anisotropie | P1 : instrumenter séparément ; conserver qualité de référence. |
| GPU | Multirésolution périphérique, résolution interne | P2 : options explicites, pas d'activation silencieuse. |
| Temporel | 32 buffers et prédiction N+1–N+5 | R : préférer anticipation ciblée des ressources plutôt que calcul spéculatif concurrent massif. |
| Temporel | Reprojection, régions stables, cache de surfaces | R : validation profondeur/identifiants, désocclusion et repli exact ; éviter les artefacts historiques. |
| Recherche | Rendu par frontières de visibilité | R : prototype analytique réduit, mesurer coût des frontières et pixels ambigus avant extension. |
| Recherche | PHOTON-RT adaptatif | R : le prototype PNG et le mode natif uniforme 25 % sont distincts ; véritable ordonnanceur adaptatif à valider. |
| Recherche | OpenGL, DXR, FPGA/Z80 | R : conserver les prototypes ; DXR détecté mais pas de backend matériel validé ; FPGA nécessite étude des transferts. |
| Scène | Photo2Scene, multivues, Gaussian Splatting | R : reconstruction d'assets distincte du rendu ; vérifier métrique/provenance. |
| Robustesse | Jour/nuit, ciel interpolé, radio asynchrone | P1 : profiler les saccades, découpler les tâches longues et les invalidations. |
| Robustesse | Scènes/versioning/manifestes, tests Release, packaging | P1 : tester réellement le démarrage, les contrôles et les transitions ; ne pas dépendre d'assertions désactivables. |
| Robustesse | Linux, licences et provenance des assets | P2 : qualifier avant distribution. |

Références de recherche : [rendu par frontières](experiments/2026-10-29-v92-workers-4k-target/CONCEPTION_RENDERING_PAR_FRONTIERES.md), [PHOTON-RT](docs/PHOTON_RT_ADAPTIVE.md), [CPU/FPGA](docs/CPU_FPGA_RENDER_BACKEND.md), [roadmap raster](v17/docs/RASTER_PERFORMANCE_ROADMAP.md), [LumaTrace one-bounce](v17/docs/V17_604_LUMATRACE_ONE_BOUNCE.md), [émissifs V17.008](v17/docs/V17_008_LUMATRACE_EMISSIVE_LIGHTS.md).

### Inventaire des PR historiques (77 recensées)

| Famille | Numéros |
|---|---|
| Temporel | 1 |
| CPU/PBR/Compute | 2, 3, 5, 6, 7 |
| Architecture V16 | 8 |
| Photo2Scene | 9–15 |
| Parité CPU/GPU | 16–20 |
| Exploration DX11/OpenGL | 21, 22 |
| Cache PBR/ombres | 23 |
| Présentation/versioning | 26 |
| GI et énergie | 27, 28, 30, 31 |
| Navigation/distribution | 29, 32 |
| Scène | 33, 35–38, 40, 43, 46, 49 |
| Raster/lumières | 39, 41, 42, 44, 45 |
| Z-prepass | 47, 48 |
| HUD/audio/navigation | 51, 52, 53, 55 |
| Anomalies | 56–60 |
| Profilage/multirésolution | 61–63 |
| Promotion master/enseigne | 64, 65 |
| Vulkan/présentation native | 69, 71–73 |
| Scène/GI/matériaux | 74–77 |
| Versioning/HUD/radio | 78 |
| LumaTrace et consolidation | 82–88 |
| Préparation géométrique | 89, 90 |

[Liste complète des pull requests](https://github.com/glaby91/moteur3d/pulls?q=is%3Apr). Une PR ouverte peut avoir été reprise dans une branche cumulative : vérifier son diff et son ascendance avant réintégration. L'inventaire n'est pas une revue ligne par ligne des 77 diffs.

### Feuille de route proposée

1. Geler la référence moteur/scène/build et reproduire les anomalies de lumière, réflexion piscine et transparence.
2. Comparer préparation géométrique ON/OFF et Z-prepass ON/OFF sur le même parcours de 30 s, puis élargir à plusieurs caméras.
3. Mesurer le Z-light culling et les masques de lumières par tuile avec contrôle strict de fidélité.
4. Profiler l'invalidation LumaTrace au cycle jour/nuit ; expérimenter la mise à jour locale sans blocage.
5. Réduire le coût de shading et de bande passante à qualité constante ; contrôler les temps GPU réels, P50/P95/P99 et latence de présentation.
6. Reprendre PHOTON-RT, rendu par frontières et reprojection dans des branches de recherche isolées ; ne les promouvoir qu'après validation visuelle et matérielle.

Objectif historique HD620 : 40–45 FPS (budget ~25–22,2 ms par image), non acquis ; aucune cible chiffrée de fidélité physique ni de FPS n'est garantie en Ultra. Objectif historique 4K/100 FPS : 10 ms par image, recherche à long terme sur matériel adapté. Aucun gain futur ne doit être annoncé avant mesure. Ne pas additionner les gains de campagnes hétérogènes ni assimiler WARP logiciel à un GPU matériel.

État relevé le 26/11 : [code V17.010](https://github.com/glaby91/moteur3d/commit/ef5461e06a9b4326620f451575b1599f70426066), [PR 90 / V17.009](https://github.com/glaby91/moteur3d/pull/90), scène [V17.629 build 42](https://github.com/glaby91/moteur3d-scene/releases/tag/scene-V17.629-42). Le code le plus récent ne constitue pas à lui seul une preuve de publication d'un package validé.

### V17.026 — correctif matériaux émissifs DX11/LumaTrace
Les déclarations d'émission de la scène sont désormais transmises au shader raster partagé. [Rapport et tests](v17/docs/DX11_EMISSION_V17026.md). R001 reste actif ; cette note décrit l'état à la V17.026, supersédé par les évolutions V17.028 à V17.031.
