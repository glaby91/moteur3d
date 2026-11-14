# CSE WARP V17 — Chronométrage GPU, lecture différée et qualité configurable

**Implémentation expérimentale sur la branche V17.build** (novembre 2026).

## Objectif

Le moteur ne doit pas masquer une baisse de qualité derrière des FPS élevés.
Le Dell Intel HD 620 sert de machine de référence *modeste*, tandis qu'une
configuration plus rapide doit pouvoir afficher les mêmes scènes et PBR à
résolution, AA, détails et éclairage supérieurs. Les résultats WARP dans
GitHub Actions **ne sont pas représentatifs de la performance de la HD 620**.

## Mesures DirectX 11

Le moteur crée, quand le pilote le permet, des requêtes GPU
`D3D11_QUERY_TIMESTAMP_DISJOINT` et `D3D11_QUERY_TIMESTAMP` par
tampon de lecture. Les horodatages GPU sont pris :

1. Au début de la préparation/dessin (avant Clear/Draw).
2. Après le dernier Draw (y compris les surfaces transparentes).
3. Après Resolve/Copy vers un tampon de staging.

Ces horodatages donnent `gpuDrawMs`, `gpuTransferMs`, `gpuTotalMs`.
La fréquence GPU vient du `D3D11_QUERY_DATA_TIMESTAMP_DISJOINT` ; si
l'intervalle est disjoint, non disponible ou invalide, on renvoie
`gpuTimingValid=false` et les nombres GPU **ne doivent pas être
interprétés comme un vrai temps de GPU**.

Les chronomètres CPU calculent séparément :

- `cpuSubmitMs` : soumission des commandes GPU (inclut mise à jour de
  l'état de scène lors du premier rendu).
- `cpuMapWaitMs` : durée de l'appel **bloquant** `Map(READ)`.
- `cpuPixelConvertMs` : copie et conversion des pixels ARGB pour Qt.
- `postMs` : métrique historique couvrant la phase post-soumission,
  **à ne pas appeler « durée du transfert PCIe »**.

Une forte valeur de `cpuMapWaitMs` peut signifier que le GPU **calcule
encore** ; elle ne prouve pas que copier les pixels coûte tout ce temps.

## Lecture GPU à 1, 2 ou 3 tampons

Le backend expose deux APIs supplémentaires, utilisées **sur le même
thread DX11** et dans l'ordre :

```cpp
std::wstring error;
m3d::enqueueDirectX11Preview(scene, shadow, camera, w, h, 3, error);
// soumettre jusqu'à trois frames, même résolution et même scène
std::vector<uint32_t> pixels;
m3d::Stats statistics;
m3d::receiveDirectX11Preview(pixels, statistics, error); // image la plus ancienne
```

La soumission planifie la résolution de l'image et sa copie GPU vers un
`D3D11_USAGE_STAGING` indépendant. La lecture de la plus ancienne image
appelle `Map` et **peut encore attendre**. Il s'agit d'une vraie
séparation soumission/réception avec tamponnage et ordre FIFO, pas
d'une promesse de suppression totale de la synchronisation.

Restrictions : profondeur 1, 2 ou 3 ; ne pas changer résolution,
profondeur ou scène sans vider la file ; utiliser un seul thread GPU
dédié ; annuler/vider les images anciennes avant de présenter une
nouvelle scène. Le rendu GUI traditionnel reste au chemin
**1 tampon synchronisé**, tandis que le benchmark peut éprouver le
pipeline indépendamment avant une adoption dans l'exploration.

Le tamponnage occupe davantage de RAM graphique partagée : environ
4,2 Mo par tampon à 1366×768 et 33,2 Mo à 3840×2160, hors cible couleur,
profondeur, MSAA et textures PBR.

## Benchmark V17

La fenêtre **Outils → Benchmark** propose :

- **1 tampon** : référence synchrone inchangée ;
- **2 tampons** : soumission/lecture en pipeline ;
- **3 tampons** : soumission/lecture en pipeline ;
- **Comparer automatiquement 1/2/3** : exécute les trois cas avec la
  même caméra, la même résolution et les mêmes matériaux, puis compare
  les pixels et exporte CSV/JSON/PNG.

Options du programme de mesure :

```text
moteur3d_v17_benchmark.exe scene.json dossier --quick --compare-readback
moteur3d_v17_benchmark.exe scene.json dossier --quick --readback-depth 2
```

`meanMs` mesure la **latence individuelle** entre soumission et
réception. `streamMeanMs` mesure la durée totale du lot, remplissage
et drainage compris, divisée par le nombre d'images ; `streamFps`
est l'inverse correspondant. Cette distinction est indispensable :
le pipeline peut améliorer le débit tout en augmentant la latence
d'une image. Aucune de ces statistiques n'inclut l'affichage final Qt
ou la synchronisation VSync. Comparer plusieurs lots, pas un seul
rendu isolé.

**A/B sécurité :** le test WARP compare les images retournées par les
tampons 2 et 3 à l'image synchrone. Les pixels doivent être identiques.
Les horodatages GPU sont facultatifs si le pilote les refuse.

## API de qualité moteur — appelée par l'hôte ou par Qt

Dans `m3d_engine.h` :

```cpp
auto high=m3d::rasterQualityProfile(m3d::RasterQualityProfile::High);
std::wstring error;
bool ok=m3d::setRasterQualitySettings(high,error);
auto current=m3d::rasterQualitySettings();

m3d::RasterQualitySettings custom=current;
custom.msaaSamples=4;          // 1,2,4 selon matériel
custom.anisotropy=16;          // 1,2,4,8,16
custom.lodQualityPercent=180; // 50..200 : plus = détails conservés plus loin
custom.frustumCulling=true;
custom.dynamicLod=false;      // géométrie de détail maximal
ok=m3d::setRasterQualitySettings(custom,error);
```

Ces paramètres sont enregistrés **atomiquement** dans la DLL
partagée du moteur ; chaque image prend un seul instantané. Le
changement de MSAA reconstruit uniquement les cibles d'image à la
prochaine frame ; celui de l'anisotropie recrée le sampler GPU.
Ne pas basculer la taille des cibles tant que des lectures différées
restent en attente.

Préréglages :

| Profil | MSAA | Anisotropie | LOD qualité | LOD dynamique |
|---|---:|---:|---:|---|
| Performance | 1× | 2× | 60 % | oui |
| Équilibrée | 2× | 8× | 85 % | oui |
| Élevée (rendu V17 historique) | 4× | 16× | 100 % | oui |
| Ultra (détails conservés) | 4× | 16× | 200 % | non |
| Personnalisée | au choix | au choix | au choix | au choix |

Le **frustum culling** reste activé par défaut dans tous les profils ;
il élimine seulement ce qui est hors caméra et ne dégrade pas l'image.

L'interface V17 partage ces réglages, avec sauvegarde des préférences
dans `QSettings` et une barre latérale défilante. Sur un lancement
sans préférences précédentes, les paramètres fournis par l'application
hôte restent prioritaires.

**Portée des réglages actuels :** MSAA, filtrage, culling et LOD du
Raster DX11. La résolution est déjà contrôlable dans la fenêtre V17.
Le choix du nombre de textures PBR 2048 et les futurs réglages d'ombres,
GI et post-traitements demanderont des extensions de l'API :
ils ne sont pas encore pilotés par ce petit objet de réglages.

## À valider sur du matériel réel

1. Compiler et passer les tests WARP et les scènes benchmark.
2. Sur le Dell, capturer 1/2/3 tampons avec le même cadrage et
   collecter les nouvelles statistiques `gpuTimingValid`,
   `gpuDrawMs`, `cpuMapWaitMs`, `streamMeanMs`.
3. Vérifier sur une carte GPU plus performante que les gains éventuels
   changent avec le matériel et la résolution.
4. Ne déplacer la boucle interactive vers le pipeline 2/3 images
   qu'après validation de la latence, de la stabilité et du respect
   des images/caméras, sans afficher de frames obsolètes.
5. Les objectifs 25 ms/40 FPS ne sont **pas** considérés atteints.

## Compatibilité et retour arrière

Le chemin historique `renderDirectX11Preview(...)` reste disponible.
Le benchmark teste le pipeline sans l'imposer à l'interface.
Un hôte peut choisir le profil **Élevée** pour récupérer le même
réglage graphique que la génération précédente. Il faut toujours
vérifier le rendu sur l'Intel HD 620 et sur un GPU moderne.
