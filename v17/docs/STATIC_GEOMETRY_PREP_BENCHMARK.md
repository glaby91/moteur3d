# V17 — préparation de géométrie statique et mesure A/B

Branche de travail : `perf/v17-static-geometry-prepare-261125`  
Base : `official/v17-lumatrace`  
État : spécification de chantier ; aucune performance n'est encore revendiquée.

## Objectif

Préparer une fois, au chargement de la scène et après triangulation, la géométrie opaque statique destinée aux backends GPU. Réutiliser les sommets lorsque position, normale, UV et matériau sont identiques, puis réordonner les indices pour améliorer la localité de lecture.

La géométrie transparente conserve son chemin et son ordre de composition. Le tableau CPU `Scene::obj`, l'ordre du BVH4 et les identifiants des triangles restent la référence de correspondance. La préparation GPU doit transporter une table explicite de correspondance vers les triangles source ; elle ne doit pas réordonner silencieusement `Scene::obj` ni casser `primitiveId` de LumaTrace.

## Entrée A/B du benchmark

Ajouter le sélecteur `Préparation géométrique statique` au benchmark, avec trois modes :

- `both` : exécuter automatiquement les deux variantes, optimisation activée puis désactivée ;
- `on` : exécuter uniquement la variante préparée ;
- `off` : exécuter uniquement la géométrie actuelle, référence.

L'option doit être disponible pour **DX11 Raster, Vulkan Raster et LumaTrace BVH4**. Chaque backend doit rapporter le mode réellement utilisé. Pour les campagnes automatisées, exposer aussi `--geometry-prep=both|on|off`. Une variable d'environnement peut servir de forçage technique, mais le rapport doit enregistrer la source de configuration et la valeur effective.

## Protocole identique

Pour chaque backend et chaque scène/caméra retenue, garder constants : matériel, scène, caméra, résolution, éclairage, qualité, MSAA et paramètres de rendu.

- Faire un échauffement avant le chronométrage.
- Mesurer séparément préparation de géométrie, transfert GPU et rendu par image.
- Rapporter les temps en ms : médiane, P95 et nombre d'images mesurées.
- Rapporter mémoire GPU estimée ou octets des buffers, sommets uniques, indices, triangles soumis et draw calls.
- Sauvegarder les images de référence et calculer l'écart pixel à pixel / luminance linéaire entre variantes.
- Pour LumaTrace, vérifier explicitement que le `primitiveId` reçu par chaque triangle correspond toujours à l'identifiant source utilisé par le BVH4 et la table de radiance.
- Exécuter chaque variante dans un état de renderer propre ; ne pas laisser un cache construit par la première variante fausser le temps de la seconde. Inverser l'ordre lors d'une répétition pour contrôler l'effet de température/cache.

## Champs à exporter

Ajouter dans `summary.json` et `results.csv` :

`geometryPrepMode`, `geometryPrepEnabled`, `uniqueVertexCount`, `indexCount`, `vertexBufferBytes`, `indexBufferBytes`, `geometryPrepareMs`, `gpuUploadMs`, `renderMedianMs`, `renderP95Ms`, `measuredFrames`, `submittedTriangles`, `drawCalls`, `imageParity`, `backend`, `scene`, `camera`, `resolution`.

Les mesures de temps d'ouverture et les temps de rendu par image doivent rester séparés. Ne pas déduire un gain de FPS du seul nombre de sommets supprimés.

## Critères d'acceptation

1. Résultats A/B disponibles séparément pour DX11 Raster, Vulkan Raster et LumaTrace BVH4.
2. La variante désactivée reproduit le chemin de référence.
3. La géométrie opaque donne des images identiques ou des écarts expliqués par une tolérance définie ; transparence, coutures UV, normales, matériaux et ordre des surfaces transparentes sont préservés.
4. La correspondance source ↔ triangles BVH4 / `primitiveId` est vérifiée.
5. Le rapport permet de conclure sur le gain ou la régression en temps de rendu, préparation et mémoire, par backend.
6. CI compilation et tests de régression verts ; benchmark matériel avant d'activer l'optimisation par défaut.

## Ordre d'implémentation

1. Introduire une représentation GPU préparée et immuable, construite une fois après triangulation.
2. Ajouter un validateur CPU qui vérifie les attributs fusionnés et la correspondance avec les triangles source.
3. Brancher DX11 Raster, Vulkan Raster et LumaTrace en conservant le chemin de référence commutable.
4. Ajouter l'entrée A/B et les colonnes d'export ci-dessus.
5. Compiler et exécuter les tests CI, puis comparer sur le matériel cible avant toute activation par défaut.
