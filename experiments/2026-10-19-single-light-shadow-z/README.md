# Moteur 3D — expérience 19/10/2026 : une lumière Z-bufferisée

## Portée réelle

Prototype HTML autonome basé sur le rendu **tuiles anti-stuttering** du 18 octobre 2026.
Ouvrir `index.html` après téléchargement local.

- **Une seule lumière directionnelle mobile est calculée.** La position visible de la lumière sert à déterminer sa direction; il ne s'agit pas d'une lampe omnidirectionnelle.
- Shadow-map logicielle 128², 192² (défaut) ou 256², avec vrai tampon de profondeur `Float32Array`.
- Rasterisation dans la vue de la lumière des **2 310 triangles fixes et 100 triangles du personnage**.
- Tests de profondeur à quatre voisins (PCF 2×2) : ombres sur le sol, sur les faces du décor et sur le personnage.
- Suppression des anciennes silhouettes d'ombres projetées sur sol et mur.
- Aperçu graphique de la shadow-map et chronométrage séparé « Z de la lumière » / « Ombres vues caméra ».
- Cache de profondeur statique réutilisé par direction quantifiée; le déplacement du personnage ne recalcule que ses 100 triangles et repart d'une copie du buffer de profondeur du décor.
- Masque d'ombre du sol calculé à 1/6 de la résolution écran et rééchantillonné pour limiter le coût.
- Cache des tuiles d'image invalidé lorsque le personnage mobile peut modifier une ombre visible. Le bon rendu prime sur une accélération trompeuse.

## Vérifications effectuées

- Syntaxe de l'ensemble du JavaScript compilée dans V8.
- Test d'exécution automatisé avec DOM/Canvas simulés : scène construite, remplissage effectif de la shadow-map, profondeur finie sur plus de 2 000 texels à 192², affichage d'une image sans exception dans ce test.
- Validation de la réutilisation du Z statique quand seul le personnage avance.
- **Aucun benchmark visuel en navigateur réel ni mesure FPGA validés dans cette révision**. Le passage de 192² à 256² peut provoquer une baisse de cadence. Les temps mesurés dans le DOM simulé ne sont pas des FPS navigateur.

## Limitations explicites

- Le **Z-buffer caméra n'est pas implémenté** : le rendu final reste un tri Canvas par profondeur moyenne.
- L'unique carte Z est **orthographique, donc directionnelle**, et non une carte omnidirectionnelle de lumière ponctuelle.
- Résolution de shadow-map limitée; ombres filtrées par PCF 2×2, puis rééchantillonnées au sol : possibles bords adoucis et effets de biais de profondeur.
- Les surfaces sont ombrées à l'échelle du triangle, pas avec un test shadow-map à chaque pixel du triangle. Le sol, lui, est interrogé sur une grille image à résolution réduite.
- Le filtrage temporel et le cache de direction quantifiée peuvent rendre les ombres imparfaites lors des mouvements de lumière.
- Les statistiques « opérations estimées » ne comptent pas les instructions machine, ni les cycles FPGA.

## Architecture préparée (non activée)

`lightingScene.lights` est un registre de sources contenant identifiant, type et attributs d'ombre; `lightingScene.shadowMaps` / `staticDepthMaps` sont indexés par source.

**Cibles futures**, NON implémentées : 32 lumières actives, 8 contribuant au maximum à une tuile 16×16, 4 sources autorisées à créer des ombres dynamiques. Voir `docs/ECLAIRAGE_MULTI_SOURCES.md`.

## Prochains tests recommandés

1. Sur un vrai navigateur, mesurer médiane et P95 des temps d'image avec / sans ombre Z à 640×480 puis 800×600, avec lumière fixe/mobile et acteur fixe/mobile.
2. Vérifier visuellement occlusion et fuites d'ombre au sol, contre les murs, aux pieds du personnage et sous les feuilles.
3. Comparer la lisibilité des ombres à 128² / 192² / 256², sans confondre débit FPS et calcul Z.
4. Ajouter le Z-buffer caméra par tuiles 16×16 dans une expérimentation séparée.
5. Préparer la deuxième source directionnelle, avec cumul des contributions et un budget explicite par tuile.
