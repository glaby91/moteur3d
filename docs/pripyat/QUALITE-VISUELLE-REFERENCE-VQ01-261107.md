# Pripyat — Référence qualité visuelle (VQ-01)

Date : 2026-11-07. Branche de départ : `feature/gpu-profiling-and-bvh-261107`.

## Objectif

Créer un environnement Pripyat beau, architecturalement cohérent et immersif. Les images d'ambiance choisies dans la discussion sont des **références artistiques**, non des rendus du moteur. Le photoréalisme n'est pas le critère de réussite.

- Extérieurs : référence 3 (architecture soviétique, végétation organique, profondeur et lumière naturelle).
- Intérieurs : référence 2 (matières patinées, fenêtres, éclairage naturel, contraste maîtrisé).
- Référence 4 : **ne pas reproduire ses arbres ni son feuillage**, jugés artificiels.

## Scène étalon VQ-01

Utiliser la scène native Pripyat centrale et une caméra extérieure immuable donnant à voir le palais Energetik, l'hôtel Polissya et la végétation. Choisir une vue existante et conserver ses paramètres (position, orientation, FOV, résolution, soleil, exposition, heure, version de scène) dans un manifeste de capture avant toute retouche.

Pour chaque changement, produire deux images à caméra et conditions identiques :
1. référence CPU ray tracing ;
2. rendu GPU Compute D3D11.

Conserver également une image « avant » archivée. Les rendus doivent être nommés avec le commit et le backend ; ne pas présenter des images générées ou des photographies comme des captures du moteur.

## Critères de qualité (revue humaine + vérifications)

| Axe | Critère |
| --- | --- |
| Géométrie | Bâtiments, arcade indépendante, routes et échelles sans incohérences manifestes |
| Végétation | Troncs, branches, couronnes asymétriques, variations d'essences, transparence et lumière filtrante ; pas de masses vertes uniformes |
| Lumière | Soleil directionnel cohérent, ombres lisibles, transition intérieur/extérieur, hautes lumières non écrêtées |
| Matériaux | Béton, verre, bois, métal, peinture et feuilles distinguables par couleur, rugosité et relief |
| Image | Pas de flou global, d'aliasing excessif, de bandes ou de teinte CPU/GPU divergente |
| Performance | Temps de rendu et coût de transfert GPU mesurés séparément, sans sacrifice visuel silencieux |

## Ordre de réalisation

1. **Geler la caméra et capturer le rendu initial** avec la vraie build Windows et la scène Pripyat complète.
2. **Éclairage et colorimétrie** : caractériser ciel, soleil, ombres, exposition et tonemapping avant toute modification globale.
3. **Matériaux** : corriger la répétition des textures et améliorer la variation de rugosité et de relief ; contrôler le budget de l'atlas GPU.
4. **Arbres** : silhouettes irrégulières, branches visibles, feuillage stratifié et variation de densité ; mesurer le nombre de triangles et la charge GPU.
5. **Comparer CPU et GPU** sur une même scène, sans exiger des algorithmes identiques ; documenter les différences acceptées.
6. **Valider les intérieurs** sur une seconde caméra fixe du hall Energetik, de l'escalier et de la fresque.

## Barrières avant validation

- Aucun benchmark n'est considéré représentatif si la résolution native et le backend ne sont pas attestés.
- Les tests synthétiques WARP ne remplacent pas un rendu complet sur Intel HD 620.
- Toute correction de cache GPU doit invalider les données lorsque la géométrie, le BVH, les matériaux ou les textures changent.
- Ne pas annoncer « meilleur que S.T.A.L.K.E.R. » sans comparaison visuelle documentée.
- Ne pas fusionner les branches historiques V15 sans examiner les changements divergents.

## État d'avancement

- [x] Direction artistique et critères consignés.
- [x] Branche isolée issue de la version GPU + végétation.
- [ ] Caméra de référence gelée et captures réelles archivées.
- [ ] Comparaison CPU / Compute à conditions identiques.
- [ ] Première amélioration visuelle démontrée par captures avant/après.
- [ ] Compilation Windows complète et essai sur GPU réel.

Ce document est un **point de départ technique**, pas une affirmation que la qualité visuelle est déjà atteinte.

## Automatisation de comparaison CPU / Compute (08/11/2026)

La branche contient une commande de capture GPU `moteur3d_qt.exe --snapshot-compute 12 <sortie.png>`, alignée sur la caméra et l'heure de la capture CPU `--snapshot 12`. Le workflow `.github/workflows/pripyat-visual-qa-windows.yml` génère les 13 vues CPU, un contact-sheet et un rapport de diagnostics, puis exécute **la scène complète** sur Compute D3D11 avec WARP pour la vue 12. Il produit une différence PNG et un JSON des écarts RGB.

**Limites :** WARP est un moteur de test logiciel D3D11, pas une mesure de vitesse du HD 620. Les rendus CPU et GPU n'ont pas encore une parité de lumière et de matériaux démontrée. Le workflow a été commité ; sa réussite et la présence des artefacts doivent être vérifiées sur GitHub Actions avant de cocher les validations correspondantes. Les images d'origine CNEWS/AFP et Roaming Required restent des références externes (voir `REFERENCES-PHOTOS-CNEWS-ROAMING-261107.md`).
