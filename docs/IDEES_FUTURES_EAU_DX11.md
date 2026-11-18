# Idée à étudier — eau DX11 plus vivante

**Statut :** idée pour une révision ultérieure. Pas une demande de modification immédiate.  
**Référence visuelle :** scène piscine V17.523, capture utilisateur du 18 novembre 2026.

## Intention

Rapprocher l’eau du rendu DirectX 11 Raster direct du rendu compute, en conservant la rapidité et la netteté du raster. Le compute apporte actuellement un relief apprécié à l’eau et de belles ombres, mais présente du bruit et un coût très élevé en exploration.

## Piste technique à évaluer

1. Shader HLSL de l’eau avec deux normales animées de faible amplitude, dans des directions et vitesses différentes.
2. Reflet d’environnement à faible coût (cubemap/probe), modulé par l’angle de vue avec Fresnel.
3. Si le budget GPU le permet, réutiliser/ajouter une ombre raster via shadow map sur le bassin et son environnement.
4. Ne pas réintroduire la GI progressive ou le ray tracing compute dans le chemin DirectX raster.

## Critères de comparaison

- Garder comme référence la capture DX11 actuelle (raster direct par swapchain, dernier rendu indiqué à 2,7 ms sur la capture).
- Comparer eau immobile et en déplacement, captures à caméra/résolution identiques.
- Vérifier que l’eau gagne en relief sans grain visible, scintillement ni régression d’images par seconde.
- Comparer les ombres et les reflets avec le compute, sans chercher une identité parfaite entre les deux modèles de rendu.
- Tester sur l’Intel HD 620 cible avant de conclure; un seul temps de rendu affiché ne suffit pas à établir les performances soutenues.

**À reprendre après stabilisation des backends et du benchmark.**