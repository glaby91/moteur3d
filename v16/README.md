# Moteur3D V16 — Pavillon 01 (prototype exécutable)

**Périmètre : Pavillon 01 exclusivement.** Le moteur V16 ouvre `scene.json` et les ressources déclarées. Aucun chargeur de scène binaire V3/V4/V5 n'est lié à l'exécutable V16. Pripiat sera traité plus tard par un convertisseur externe.

## Construction et validation — Windows x64 exclusivement

Plateforme cible : **Windows 10/11 x64**, Visual Studio 2022 (MSVC x64), Qt 6.8.3, CMake 3.24+. Nous n'exécutons plus de jobs Linux pendant cette phase V16.

Build local depuis PowerShell, avec Qt6 installé :

```powershell
cmake -S v16 -B build-v16 -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build-v16 --config Release --parallel 4
ctest --test-dir build-v16 -C Release --output-on-failure
```

Le [workflow Windows V16](https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v16.yml) construit `moteur3d_v16.exe`, exécute les tests natifs et un rendu CPU de Pavillon 01, contrôle le PNG, puis publie l'artefact `Moteur3D-V16-Pavillon01-Windows-x64`.

Le ZIP contient l'exécutable, les bibliothèques Qt6, le dossier `pavillon01/`, `shaders/`, `REVISION.txt` et `Pavillon01-CPU-Preview.png`. Décompresser en conservant l'arborescence et lancer `moteur3d_v16.exe`. La scène V16 est ouverte automatiquement; aucune scène `.bin` n'est acceptée.

L'aperçu GPU D3D11 Compute doit encore être éprouvé sur un PC Windows réel; son rendu ne possède pas la parité PBR du CPU.

## Répartition des ressources

```text
scenes/v16/pavillon01/
  scene.json             manifeste des secteurs et dépendances
  geometry/house.json    2 maillages procéduraux réutilisables (boîte, sphère)
  instances/house.json   203 instances, positions, dimensions, hiérarchie
  materials/house.json   11 matériaux PBR, paramètres modifiables
  pbr/house.json         11 jeux de textures et paramètres de génération
  lighting/day.json      soleil et lumières intérieures
  cameras/views.json     5 points de vue
  environment/site.json  environnement / conventions métriques
```

L'arbre `SceneGraph` contient les instances et les groupes `parent`. Le catalogue de ressources conserve les maillages et les matériaux uniques. Chaque matériau référence un jeu PBR. Les mipmaps sont **générées en mémoire au chargement**, et non dupliquées dans les fichiers de chaque instance.

### PBR / mipmaps

- Albedo couleur : cartes RGB888, réduction gamma-correcte sRGB.
- Normales : cartes RGB565, renormalisation à chaque niveau de mipmaps.
- Rugosité : cartes RGB565 en espace linéaire, réduction quadratique.
- Métallicité : facteur scalaire PBR modifiable dans l'inspecteur et sauvegardable.
- Transmission et IOR : propriétés du matériau; vitrages et eau.
- Résolution de référence des ressources : 2048² (12 niveaux, 2048² à 1²), plafonnée à 64/128² pour les tests CI.
- Les textures actuelles sont **procédurales de démonstration**. Il ne s'agit pas encore de fichiers PBR photographiques ni d'un import d'images externes.

L'inspecteur Qt permet de modifier la rugosité multiplicative et la métallicité, puis d'enregistrer uniquement `materials/house.json`. La géométrie reste inchangée.

## Backends et limites

- **CPU :** ray tracing multithread, BVH de triangles, matériaux avec albedo/normal/roughness, vitrages, éclairages et AA des contours.
- **D3D11 Compute :** prévisualisation des triangles/BVH sur Windows ; le shader HLSL n'offre **pas encore la parité visuelle PBR** avec le CPU. Si le matériel, le shader ou son chargement échoue, repli CPU.
- **Qualité :** volumes architecturaux et mobilier de démonstration; détails de menuiserie, véritables assets GLB, cartes PBR photographiques et test sur machine Windows restent à approfondir.
- **Validation CI :** chargeur, références, maillages, BVH, chaînes de mipmaps et image PNG réellement calculée par le CPU.

Les modules génériques de ray tracing restent provisoirement partagés avec le répertoire source historique `native_v14`, mais le point d'entrée de V16 est indépendant et n'utilise pas le chargeur `.bin`.
