# V17 — préparation de géométrie statique et mesure A/B

Branche de travail : `perf/v17-static-geometry-prepare-261125`  
Base : `official/v17-lumatrace`  
État : implémentation sur la branche de travail ; résultats de performance à mesurer sur le matériel cible.

## Objectif

Préparer la géométrie opaque destinée aux backends GPU après triangulation. Fusionner les sommets strictement identiques sur l'ensemble de leurs attributs, puis les référencer dans un flux d'indices. L'ordre des triangles reste inchangé : cette première étape ne fait pas de réordonnancement pour le cache GPU.

La géométrie transparente conserve son chemin et son ordre de composition. Le tableau CPU `Scene::obj`, l'ordre du BVH4 et les identifiants des triangles restent la référence. Pour LumaTrace, `primitiveId` est inclus dans la clé du sommet afin de ne pas fusionner des identifiants source différents.

## Entrée A/B du benchmark

Le benchmark accepte :

- `--geometry-prep on` : préparation activée ;
- `--geometry-prep off` : chemin non indexé de référence.

Lancer deux campagnes séparées, avec les mêmes scène, caméra, résolution, qualité, MSAA, échauffement et nombre d'images. Chaque lancement crée son propre processus et reconstruit ses ressources, ce qui évite qu'un cache du premier mode fausse le second. Le mode par défaut est `off`. Le rapport `summary.json` mémorise le mode effectif ; `campaign_results.csv` expose aussi les compteurs de géométrie.

Exemple :

```bat
moteur3d_v17_campaign.exe scene.json bench-prep-off --geometry-prep off --rounds 6 --frames 12 --warmup 2
moteur3d_v17_campaign.exe scene.json bench-prep-on  --geometry-prep on  --rounds 6 --frames 12 --warmup 2
```

Le commutateur s'applique aux chemins GPU DX11, Vulkan et LumaTrace utilisant la préparation de scène commune. Les cas CPU servent de contrôle et restent inchangés.

## Mesures et interprétation

Pour chaque backend et variante, comparer les mêmes scènes, caméras et réglages. Le benchmark rapporte :

- temps de rendu par image : médiane, P95, moyenne et débit ;
- temps de préparation géométrique et d'upload ;
- sommets source et uniques, nombre d'indices, tailles des buffers ;
- triangles soumis, draw calls, mémoire du processus ;
- image de référence et indicateur de stabilité pixel à l'intérieur de chaque configuration.

Les temps de préparation et d'upload ne sont pas inclus dans les temps d'image mesurés après échauffement. Une comparaison d'images entre les dossiers `on` et `off` est nécessaire pour valider la parité visuelle. Le benchmark ne doit pas revendiquer un gain avant la mesure réelle sur la machine cible.

## Fonctions séparées

Le code partagé sépare le hachage des sommets, leur comparaison exacte et la construction du flux préparé. Les uploads Vulkan séparent l'allocation/copie générique des buffers de sommets et d'indices. Les backends gardent chacun le choix du chemin indexé ou de référence.

## Suite de validation

1. Compiler le moteur et le test de fumée `static_geometry_prepare_smoke`.
2. Lancer les deux campagnes A/B sur la même machine.
3. Comparer P50/P95 et débit par backend, ainsi que temps de préparation/upload et mémoire.
4. Vérifier l'écart d'image entre `on` et `off`, notamment les matériaux, les coutures UV et LumaTrace.
5. Garder la préparation désactivée par défaut jusqu'à validation des résultats.
