# Étude : backend moteur LumaTrace pour V17

## Décision et périmètre

Porter le **moteur de rendu** LumaTrace dans Moteur3D V17 comme backend sélectionnable, avec une priorité au rendu interactif temps réel.

Ne pas intégrer l’application autonome LumaTrace : pas de `main`, de fenêtre Qt LumaTrace, de menus, de chargeur de manifeste dédié ni de scène Porsche embarquée. La fenêtre et le cycle d’interface restent ceux de V17.

Cette branche ne contient que cette étude. Aucun code moteur, scène, version de composant ou configuration de build n’est modifié dans cette étape.

## État des deux moteurs observé

Références inspectées :

- V17 : branche `codex/v17-001-versioning-scene-604-hud-radio-daynight`.
- LumaTrace : branche `codex/lumatrace-live-orbit-02`.

Le cœur LumaTrace se trouve principalement dans :

- `src/render/d3d11_scene_renderer.cpp/.h` : device et contexte D3D11, swapchain, ressources GPU, passes de rendu et présentation.
- `src/render/scene_bvh.cpp/.h` : construction et structures de données BVH4.
- `src/render/shaders/` : prépass Z, raster PBR et calcul HLSL d’éclairage/parcours BVH4.

Le rendu suit le chemin : prépass Z et ID des triangles visibles, marquage HLSL des triangles vus, calcul des rayons et parcours BVH4 pour l’éclairage, raster PBR puis présentation DXGI. Le chemin interactif ne demande pas de copie de l’image sur le CPU à chaque frame ; la lecture staging sert à la capture/export.

L’interface LumaTrace appelle le renderer depuis son propre viewport. Le renderer compile les shaders depuis un dossier `shaders` adjacent à l’application, possède son device et sa swapchain, et travaille actuellement à une taille fixe de 960 × 540 avec présentation VSync.

## Interface de portage proposée

Créer ultérieurement un backend moteur V17 isolé, compilé dans le build V17, avec une interface de ce type (à définir pendant l’implémentation) :

- initialisation sur le HWND dédié à ce backend ;
- réception de la scène V17 active, de sa caméra et de la taille de viewport ;
- préparation et téléversement des données de scène ;
- rendu d’une frame avec angle/paramètres lumineux ;
- mise à jour caméra, redimensionnement et libération explicite sur le thread propriétaire.

L’application V17 conserverait son dispatch de backend, son choix de fenêtre, son statut, son compteur de temps/FPS et son cycle de rendu asynchrone. Une option « LumaTrace — D3D11 BVH4 » serait ajoutée dans les sélecteurs principaux et rapides. Les contrôles de rotation des lumières et de souris/Échap seraient pilotés par V17.

Le renderer et ses shaders seraient regroupés dans une cible moteur dédiée, sans construire ni lier l’exécutable LumaTrace. Le chemin de copie des shaders serait adapté au packaging V17. La scène autonome et son chargeur GLTF ne sont pas nécessaires si l’adaptateur utilise le runtime V17.

## Adaptation de la scène : travail principal

LumaTrace attend `lumatrace::scene::Scene` et `SceneBvh`. V17 expose `m3d::Scene`, avec ses propres matériaux et sa propre hiérarchie d’accélération. Le BVH V17 n’est pas directement réutilisable : les layouts CPU/GPU et les conventions de nœuds diffèrent.

Il faudra donc un adaptateur qui :

1. transforme les sommets et triangles V17 en triangles LumaTrace dans l’espace monde, en préservant normales, UV et identifiants de matériau ;
2. triangule les objets V17 non triangulaires quand ils sont présents (boîtes, round-boxes, capsules, cônes et ellipsoïdes) ;
3. convertit les matériaux et textures V17 vers les entrées PBR utilisées par les shaders LumaTrace ;
4. construit le BVH4 à partir de cette représentation et téléverse triangles, ordre des triangles, nœuds et matériaux ;
5. garde cette représentation tant que la géométrie de scène ne change pas.

Le renderer charge ses ressources de scène à l’initialisation ; son paramètre BVH de `render()` n’est pas re-téléversé à chaque frame. C’est adapté aux déplacements de caméra. Un changement de scène ou une mutation géométrique devra toutefois invalider et reconstruire BVH et ressources GPU. La compatibilité exacte des formes, textures alpha, normales, matériaux et éclairage reste à vérifier pendant l’implémentation.

## Raccordement temps réel et contraintes

- Le renderer LumaTrace est aujourd’hui fixé à 960 × 540 : il faut rendre les cibles et la swapchain redimensionnables et respecter le DPI et la taille de fenêtre V17.
- Sa présentation appelle `Present(1, 0)` : conserver le VSync au premier jalon, puis mesurer la cadence intégrée.
- Les appels D3D11 et la destruction des ressources doivent rester cohérents avec le thread propriétaire. Utiliser le worker persistant V17 pour initialisation, rendu et libération, et laisser l’interface réactive.
- Le rendu continu doit être déclenché explicitement quand l’orbite lumineuse est active, sans lancer plusieurs frames concurrentes. Réutiliser la boucle asynchrone V17 ; ne pas bloquer le thread UI comme le viewport autonome LumaTrace.
- La fenêtre native dédiée suit le modèle des backends V17 existants. Un device D3D11 propre au backend est le premier choix d’intégration, sous réserve de mesurer son coût mémoire et les interactions DXGI.
- L’orbite actuelle LumaTrace se base sur les bounds globaux de la scène. Pour les grandes scènes V17, elle doit devenir paramétrable ou être rapportée au sujet/cadrage ; sa distance actuelle ne garantit pas une lumière à moins de deux mètres.
- Le rendu LumaTrace a son propre PBR et son fond. La correspondance avec ciel, météo, jour/nuit et conventions de matériaux V17 doit être décidée ; la composition Qt ne peut pas être supposée active lors d’une présentation swapchain directe.

## Étapes d’implémentation recommandées

1. Extraire le renderer, le BVH4 et les shaders en backend moteur autonome du code d’application LumaTrace.
2. Définir le contrat d’entrée V17 et construire l’adaptateur géométrie/matériaux sur une scène contrôlée.
3. Présenter une image par swapchain dans une fenêtre dédiée V17, avec caméra fixe, puis ajouter redimensionnement et mise à jour caméra.
4. Ajouter la boucle de rendu interactif et l’orbite lumineuse, avec une seule frame en vol et affichage du temps/FPS.
5. Comparer l’image à LumaTrace autonome sur la Porsche, puis vérifier une scène V17 représentative. Commencer à un rayon par triangle ; mesurer ensuite les réglages supérieurs et l’impact de la taille de scène.
6. Une fois les limites de fidélité et de performance connues, raccorder l’option durablement aux sélecteurs et diagnostics V17.

## Critères de validation ultérieurs

- Le build V17 ne dépend pas de l’exécutable ni des assets de LumaTrace.
- La scène V17 active, et non une scène Porsche codée en dur, est rendue.
- Le mouvement de caméra ne reconstruit pas le BVH.
- L’orbite lumière et le rendu continu restent actifs sans bloquer l’interface.
- Le changement de backend, le changement de scène, le redimensionnement et la fermeture libèrent correctement les ressources D3D11/DXGI.
- La comparaison visuelle documente explicitement les différences de matériaux, d’éclairage, de fond et de qualité.
