# Audit moteur 3D et scène de Pripiat — 261104-0001

Date : 4 novembre 2026  
Périmètre : branche `pripiat`, moteur natif V14, rendu CPU, backend Direct3D 11, benchmark intégré, export de scène et scène Energetik / centre de Pripiat.  
Révision auditée : arbre Git `2dc1e0b734b80ba235a15bfd33002f075981a0a2` (`Add DirectX 11 benchmark mode`).

## Résumé

Le moteur CPU est la voie de référence : ray tracing complet, BVH, matériaux, ombres, réflexions, transmission/réfraction et post-traitement. La cible Windows x64 compile dans GitHub Actions (run 61, résultat réussi). Le backend Direct3D 11 fonctionne comme aperçu raster, mais ne produit pas la même image que le CPU et renvoie chaque image en mémoire CPU avant affichage.

L’audit statique relève deux défauts fonctionnels qui doivent être corrigés avant de considérer la scène et le mode DirectX stabilisés : l’export de Pripiat cumule deux fois ses lumières locales, et les ombres DirectX ne sont pas recalculées lorsque l’heure change. Le benchmark peut aussi afficher un rapport de vitesse entre deux charges graphiques différentes. La scène reste un secteur de démonstration approximatif, avec Energetik comme seul bâtiment intérieur détaillé ; elle ne représente pas encore toute la ville.

## Constats prioritaires

### A1 — Les lumières locales de Pripiat sont exportées deux fois

**Priorité : élevée — défaut confirmé par le chemin d’export.**

`scene_pripyat_central.js` appelle `makeScene()` à sa fin. L’exporteur ajoute ensuite un nouvel appel explicite à `makeScene()` après avoir évalué ce fichier. Chaque appel vide `objects`, mais aucun ne vide `M3D_SCENE_LIGHTS`. Les 11 lumières de lampadaires non cassés et les 10 lumières de façade sont donc ajoutées deux fois, soit 42 entrées au lieu de 21. La géométrie est également reconstruite inutilement pendant l’export.

**Effet :** éclairage local renforcé et coût CPU inutile pour parcourir la liste de lumières à chaque impact.  
**Correction :** choisir un seul propriétaire de l’appel à `makeScene()` et rendre l’initialisation idempotente, notamment en vidant la liste des lumières avec les objets.  
**Références :** `export_scene.js:54`, `scene_pripyat_central.js:12-15, 293-320, 322`.

### A2 — Les ombres DirectX restent liées à l’heure de leur dernier calcul CPU

**Priorité : élevée — défaut confirmé par le flot d’exécution.**

Quand l’heure change, `renderLoop()` met à jour le soleil et le ciel. Il ne reconstruit `shadows_` que si le moteur CPU est sélectionné. En mode DirectX, le shader continue donc de comparer les surfaces avec la carte d’ombres calculée précédemment, généralement à midi au démarrage. `shadowHour_` n’est pas mis à jour sur ce chemin, ce qui laisse l’état incohérent.

**Effet :** direction de lumière et ombres peuvent ne plus correspondre, en particulier à l’aube, au coucher du soleil et après un aller-retour entre CPU et GPU.  
**Correction :** recalculer ou mettre à jour la carte d’ombres à chaque changement d’heure, ou afficher explicitement que l’aperçu DirectX conserve une ombre fixe.  
**Référence :** `moteur3d_qt.cpp:1091-1100, 1112-1124`.

### A3 — Le benchmark CPU/GPU ne compare pas la même qualité de rendu

**Priorité : élevée pour interpréter les résultats.**

Le CPU exécute le ray tracing et l’ombrage complet. Le GPU rasterise une approximation : il ne calcule pas les réflexions de scène ni la réfraction du CPU, et ne reprend pas les chaînes normales/rugosité complètes. Le benchmark désactive bien brouillard et profondeur de champ pour les deux passes, mais ces autres écarts subsistent. Le temps GPU comprend en plus une lecture synchrone de l’image vers la mémoire CPU.

**Effet :** le débit relatif affiché est une comparaison de backend et de charge, pas un gain à qualité d’image égale.  
**Correction :** nommer ce résultat « comparaison des chemins de rendu », afficher les limites dans le tableau/export CSV, et ne calculer un « gain » strict que lorsque les mêmes effets et critères visuels sont actifs.  
**Références :** `moteur3d_qt.cpp:115-116, 905-925`; `m3d_directx11.cpp:42-115, 245-327`.

## Constats techniques importants

### A4 — Le chemin DirectX force une synchronisation GPU → CPU par image

Le rendu se fait dans une texture hors écran, copiée dans une texture `STAGING`, puis lue avec `Map(..., D3D11_MAP_READ, ...)`. Chaque pixel est ensuite copié dans un tampon Qt et `QImage::copy()` crée une autre copie. Cela permet l’intégration actuelle à l’interface, mais bloque le pipeline sur le retour CPU et limite les gains d’un GPU, surtout sur un GPU intégré.

**Suite recommandée :** afficher via une surface Direct3D/Qt ou une swap chain, garder l’image sur le GPU et ne lire les pixels côté CPU que pour une capture ou un benchmark explicitement orienté lecture.  
**Références :** `m3d_directx11.cpp:320-334, 411-418`; `moteur3d_qt.cpp:1130-1137`.

### A5 — La conversion raster consomme beaucoup de mémoire et développe toute la scène

Le backend développe les primitives en triangles non indexés, ajoute un sol quadrillé avec un pas fixe de 0,5 m, puis conserve les sommets dans un unique buffer. La limite est de 12 millions de sommets. `GpuVertex` occupe 40 octets : atteindre cette limite représente environ 480 Mo pour le seul buffer de sommets, hors buffers transparents, textures, cibles et tampons CPU. La conversion complète se refait quand la scène change.

**Effet :** risque de pression mémoire et d’échec d’allocation sur des GPU intégrés ou quand la scène de Pripiat s’étend. Le coût croît avec la surface du sol et le nombre de primitives.  
**Suite recommandée :** partager les sommets, instancier les objets répétés, créer le sol par tuiles visibles, ajouter niveaux de détail et mesurer la mémoire en plus du temps.  
**Références :** `m3d_directx11.cpp:18-19, 118-145, 226-241, 340-409`.

### A6 — Le chargeur valide le graphe BVH mais pas son enveloppe spatiale

Le chargeur contrôle les indices, les plages de feuilles, les cycles et la couverture des objets. Il ne vérifie pas que les bornes d’un nœud contiennent celles de ses enfants ou des objets associés. Un fichier de scène incorrect peut donc être accepté tout en faisant manquer des intersections.

**Suite recommandée :** valider l’inclusion des bornes au chargement et ajouter des limites de mémoire calculées avant les allocations pour les scènes externes.  
**Référence :** `m3d_scene.cpp:21-45`.

## État de la scène de Pripiat

- Le contenu livré est un secteur d’environ 128 × 158 m autour de la place centrale, Energetik et des façades proches, pas la ville entière ni quatre secteurs explorables.
- Energetik possède une enveloppe, un foyer, des salles, un auditorium, des escaliers et un toit reconstruits pour le parcours. Les volumes et l’organisation sont approximatifs ; ils ne constituent pas un relevé photogrammétrique ni une reconstruction pièce par pièce.
- L’hôtel Polissya et les immeubles résidentiels sont des masses contextuelles répétitives. Les proportions, ouvertures et dommages restent à confronter aux références documentaires.
- La palette utilise des matériaux procéduraux et des textures génériques du moteur. Les références photo servent à guider le travail, mais il n’y a pas encore de textures photo documentaires calibrées pour Pripiat ni de cartes PBR dédiées à cette scène.
- La scène constitue une base navigable pour une démo d’Energetik ; la fidélité visuelle proche des photos et la couverture de la ville restent à produire.

Ces limites sont déjà partiellement reconnues dans `docs/pripyat/README.md` et `docs/pripyat/PRIPYAT-MATERIALS-CATALOGUE.md`.

## Build, benchmark et portée de la validation

- **Compilation Windows x64 :** GitHub Actions run 61, commit `fb443240986a83ef1e2ff370a6e91681eb42114b`, succès. Le paquet Windows contient l’exécutable et la scène exportée.
- **Rendu sur matériel :** non vérifié dans cet audit. La compilation ne prouve pas le rendu visuel sur le Vostro ni la stabilité du pilote Direct3D.
- **Tests automatisés :** aucun test automatisé ou test de scène n’a été exécuté pendant cet audit ; les constats A1 et A2 sont établis par lecture des chemins de code concernés.
- **Workflow :** son nom et certains commentaires annoncent Windows et Linux, mais sa matrice ne contient que `windows-latest`. Les étapes Linux sont inatteignables dans cette configuration. Les documents qui annoncent un build Linux par ce workflow sont donc inexacts.  
  **Référence :** `.github/workflows/build-moteur3d-v14-qt.yml:1, 27-33, 38-51`.

## Ordre de correction recommandé

1. Corriger l’appel double à `makeScene()` et l’accumulation des lumières.
2. Corriger la cohérence de la carte d’ombres avec l’heure en mode DirectX.
3. Rendre explicite la non-équivalence CPU/GPU dans le benchmark et son CSV.
4. Réduire les copies CPU/GPU et la mémoire du maillage avant d’étendre la scène.
5. Valider les volumes et matériaux d’Energetik sur des photos et plans, puis détailler les secteurs voisins.

