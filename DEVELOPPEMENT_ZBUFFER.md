# OpenGCSE — reprise du moteur 3D

## Sources historiques auditées (branche master)
- `moteur.bas` : prototype v0.1, tableaux de triangles XYZ, hypothèses textures et tampon Z ; pas de projection ni de rasterisation opérationnelle. Erreurs de variables `nbtiranglemax`, `reosx`. Tables de textures dimensionnées de façon très coûteuse.
- `triangle.bas` : expérimentation de triangles filaires en SCREEN 12 ; pas de moteur de rendu 3D.
- `rgb.bas` : lecture de BMP et tests de réduction/mirroir des textures ; code de démonstration non intégré.
- `lecteur bmp.bas` : lecteur BMP expérimental.
- `lightdeffect.bas` : ébauche de création de fenêtre.
- `text1.bmp` : ressource bitmap historique.

## Ajout sur feature/zbuffer-primitives
`zbuffer_demo.bas` est un **nouveau prototype autonome QB64/QB64-PE**, sans remplacement des fichiers historiques.
- Scène composée de **deux boîtes paramétriques** (centre et dimensions), transformées en triangles uniquement pour le rasteriseur.
- Projection perspective (caméra à l'origine, regard vers Z positif).
- Remplissage des triangles par coordonnées barycentriques et profondeur interpolée via 1/Z.
- Test de profondeur pixel par pixel dans un tampon 800×600 ; rendu des triangles dans l'ordre inverse pour illustrer l'indépendance de l'ordre de dessin.
- Touche Échap pour quitter.

## Exécution
Ouvrir `zbuffer_demo.bas` avec **QB64/QB64-PE**, compiler et lancer. Aucun exécutable compilé ni test d'exécution réel n'est fourni à cette étape ; validation visuelle et compilation restent nécessaires.

## Limites connues / étapes suivantes
1. Vérifier la compilation sous QB64-PE et corriger tout écart de dialecte.
2. Ajouter le clipping des triangles qui traversent le plan proche : ils sont actuellement rejetés en totalité.
3. Ajouter rotation, caméra mobile, sélection d'objets et back-face culling optionnel.
4. Définir un modèle de scène paramétrique persistant, puis ajouter cylindres, sphères et assemblages.
5. Intégrer les textures historiques avec échantillonnage UV perspective-correct et mipmaps.
6. Ajouter l'éclairage adaptatif et mesurer les performances. Le Z-buffer ne calcule pas les ombres.
7. Prévoir une voie accélérée GPU si une résolution Full HD est visée : la rasterisation CPU actuelle est pédagogique.

**Séparation stricte :** ce dépôt est distinct de NEXT INDUSTRY CONTROL. Aucun fichier du projet de supervision n'est modifié.
