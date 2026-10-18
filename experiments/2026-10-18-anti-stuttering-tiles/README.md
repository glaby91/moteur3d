# Simulateur 3D — Tuiles adaptatives anti-stuttering

Sauvegarde du 18 octobre 2026, basée sur la démonstration **cour Low Poly / collisions / 32 buffers**.

## Utilisation

Ouvrir `index.html` dans un navigateur moderne (application HTML autonome, sans installation ni dépendance externe).

## État du prototype

- Cour Low Poly d'environ 2 410 triangles, personnage avec collisions et patrouille, arbre, murs et obstacles.
- Deux fenêtres synchronisées : caméra et vue de diagnostic extérieure.
- Caméra orbitale et lumière animée avec prévisions de mouvement et d'éclairage jusqu'à N+5.
- 32 buffers RGB565 expérimentaux et statistiques de performances.
- **Mode par défaut : Tuiles adaptatives (anti-stuttering).**
- Avec caméra ou lumière mobile : rendu géométrique exact sans pré-calcul lourd concurrent.
- Avec caméra ET lumière fixes : réutilisation des zones 16 × 16 inchangées et redessin de la région affectée par le personnage.
- Mode de reprojection Z désactivé dans l'interface afin d'éviter les artefacts précédents.
- Pré-rendu complet des images futures désactivé dans la boucle temps réel afin d'éviter les pauses.

## Observations utilisateur

- Le **stuttering a disparu** dans cette version.
- Le compteur intitulé **« Reprojection » reste vers 40 ms** selon l'utilisateur. **Ce chiffre n'est pas validé comme temps d'une reprojection géométrique** : en mode tuiles, le champ `copy` inclut le coût mesuré du transfert Canvas `drawImage`. Il faudra séparer copie du framebuffer, conversions éventuelles, reprojection Z et attente GPU/navigateur.
- Les observations antérieures avoisinent 50 FPS, mais les FPS navigateur fluctuent et ne permettent pas seuls de conclure sur le gain d'une optimisation.

## Limitations et suites

- La scène est rendue par Canvas 2D avec tri de profondeur des polygones : ce n'est **pas** un Z-buffer logiciel par pixel généralisé.
- Les ombres et les collisions reposent sur des géométries simplifiées.
- Les prévisions N+1 à N+5 ne sont pas cinq rendus complets prêts à afficher.
- Les 32 buffers servent au banc d'essai temporel, pas à une gestion matérielle FPGA.
- Compteurs d'opérations **estimés**, distincts d'un compte exact d'instructions Z80/FPGA.
- À faire : séparer précisément les chronomètres `canvas-copy`, `geometry`, `raster`, `shadow`, `forecast` et `display` ; benchmark hors présentation, à cadence contrôlée ; puis expérimenter les tuiles sur framebuffer logiciel avec Z-buffer.

## Historique

La version précédente est conservée dans [`../2026-10-18-cour-interactive-32buffers/`](../2026-10-18-cour-interactive-32buffers/).
