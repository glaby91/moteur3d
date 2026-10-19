# V8.1 — Accélération du rasteriseur CPU texturé

**19 octobre 2026** — Retour utilisateur : le stage « Raster Z caméra » prend environ **200 ms à 800 × 600** avec les textures V8 dans son navigateur. Le but est de réduire ce coût sans sacrifier les occultations correctes ni les ombres Z.

## Correctifs programmés

- Trois modes dans « Raster » : **Accéléré — Z puis texture visible** (défaut), **Intermédiaire — texturage direct tuilé**, et **Référence V8 — LOD par pixel**.
- **Rendu natif** en mode accéléré et intermédiaire : on rasterise les triangles de la surface originelle de l'arbre détaillé et des autres objets (8 322 décor + 100 personnage) au lieu des subdivisions coplanaires redondantes (27 416 décor + 1 000 personnage). Le maillage V7 complet reste en mémoire et la référence V8 conserve toutes ses faces. Aucune forme, position ni volume de collision n'est retiré.
- **Première passe Z uniquement** : le depth buffer est calculé par pixel et un identifiant de face gagnante est écrit dans un tampon entier. Aucun texel n'est lu lors de cette passe.
- **Deuxième passe texture** : on parcourt les pixels finaux et on texture uniquement la face gagnante. Les surfaces recouvertes par d'autres objets ne déclenchent plus d'échantillonnage de texture.
- **LOD par bloc d'écran 16×16 et par face visible** : les dérivées UV sont calculées sur la surface perspective-correcte, puis le choix de mip est réutilisé pour les pixels de cette face à l'intérieur de la tuile. Contrairement au calcul LOD par pixel de V8, le choix varie au mieux d'un bloc à l'autre. Cela peut induire de très petites différences de couleur.
- **RGB565 optimisé** : pour les textures aux dimensions puissances de deux, les coordonnées répétées utilisent un masquage binaire plutôt que des modulos généraux ; échantillonnage nearest/bilinéaire/trilinéaire conservé.
- **Statistiques agrégées** par tuile et par face plutôt que mises à jour pour chaque pixel, et petit cache d'état pour les faces voisines.
- **Profilage séparé** du Z caméra et du passage « Textures (passage visible) ». Le statut indique leur **somme**, seule base correcte pour comparer aux anciens timings du « Raster Z caméra » V8, qui comprenait déjà le texturage.
- Source lumineuse, cartes Z d'ombre V4, filtre PCF des ombres et lissage optionnel des silhouettes conservés sans modification.

## Vérifications automatisées en environnement V8 avec Canvas simulé

| Résolution | V8 référence, raster Z **et textures** | V8.1 accéléré, Z | V8.1 accéléré, textures | Temps combiné V8.1 |
| --- | ---: | ---: | ---: | ---: |
| 320 × 240 | 408 ms | 65 ms | 106 ms | **171 ms** |
| 800 × 600 | 1 911 ms | 297 ms | 628 ms | **925 ms** |

Ces temps sont des mesures d'un **banc d'exécution JavaScript avec DOM et Canvas simulés**, et **pas** des FPS Firefox/Chrome du poste utilisateur. Le rapport approximatif pour le cœur du raster est 1911/925 ≈ **2,1×** à 800 × 600 dans ce banc. **Il ne faut pas en déduire que les 200 ms réels deviendront nécessairement 95 ms : le résultat dépend du navigateur et du processeur.**

### Invariants de rendu à 800 × 600

- **0 pixel de différence dans le Z-buffer**, **0 pixel d'écart d'identifiant de surface visible** entre la référence et l'optimisation, avec la caméra et l'état de scène testés.
- **28 378 triangles rasterisés** en référence et **8 420 triangles rasterisés** en mode accéléré (les triangles entièrement dégénérés ou éliminés sont exclus).
- **597 053 échantillonnages de texture** pour V8 contre **334 180** dans le passage visible V8.1, soit environ 44 % de moins.
- **4 894 évaluations de mip** en V8.1, contre une évaluation par pixel texturé en référence.
- Couleur légèrement différente sur certaines zones (LOD échantillonné par tuiles et arrondi bilinéaire/trilinéaire) ; écart maximum observé : **2 unités sur 255 par composante** à 800 × 600.
- Le test d'occultation Z de deux triangles superposés, dans les deux ordres, reste réussi. Le test des niveaux de mip RGB565 reste réussi.

## Comment comparer réellement

1. Ouvrir `index.html` localement (sur GitHub cliquer Download raw file).
2. Passer en résolution **800 × 600**, avec mêmes paramètres de lumière, distance et filtrage.
3. Dans « Raster », sélectionner **Référence V8**, relever la somme du temps Z + textures, puis sélectionner **Accéléré — Z puis texture visible** et relever le même indicateur.
4. Laisser stabiliser la compilation JIT et la génération des textures 4096² avant de comparer. Mesurer le temps médian et le P95 plutôt qu'une seule frame.
5. Vérifier visuellement les petits détails de l'arbre et les transitions mip aux limites des blocs 16×16.

## Limites

- Le rendu demeure intégralement CPU avec `putImageData` en sortie. Une fluidité de 60 images/s à 800×600 n'est pas démontrée.
- La suppression de triangles **coplanaires** ne constitue pas une décimation de la vraie forme du tronc et des feuilles ; les 8 422 triangles natifs préservent l'apparence géométrique. Le mode référence demeure disponible pour vérification.
- Les mipmaps 4096² restent générés progressivement ; la pyramide 256² est utilisée au lancement.
- Le niveau de détail de texture est choisi par face et tuile, pas par pixel ; le Z-buffer et les UV interpolées restent bien par pixel.
- La prochaine étape, si le coût reste trop élevé, sera un binning des triangles par tuile, un cache de transformations, ou une variante matérielle GPU/FPGA dédiée, avec benchmark séparé.