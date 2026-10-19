# V9.2 — Accélération du ciel + choix de précision de calcul

**19 octobre 2026.** Variante distincte de la V9.1 ; la V9 et la V8.1 restent disponibles. Démonstrateur autonome : `index.html`.

## Origine

Sur le navigateur utilisateur, image 16:10 **1920 × 1200** : environ **1 371 ms pour une image** avec le moteur V9 et filtrage nearest, dont plus de **1 000 ms** pour le calcul de couleur. Le rendu a été jugé plus joli, mais trop long à calculer.

Cette révision offre deux optimisations **indépendantes** :

### 1. Ciel / nuages accélérés — sans changer les rayons

Le ciel était évalué pixel par pixel à l'aide de sinus, cosinus, puissances et de dégradés. V9.2 calcule la couleur atmosphérique sur une grille plus espacée, puis réalise une interpolation bilinéaire des couleurs :
- Pour un rendu **jusqu'à 800 pixels de largeur** : pas de **2 pixels**, 9×9 échantillons par tuile 16×16.
- Pour un rendu **plus large** : pas de **4 pixels**, 5×5 échantillons par tuile 16×16.
- Les évaluations du ciel ne sont déclenchées que si la tuile contient effectivement des pixels sans intersection.
- **Un seul rayon primaire par pixel de la résolution de calcul** reste lancé. La géométrie, les intersections, les UV des objets, les ombres PCF et les textures sont inchangés.
- Commutateur **Ciel / nuages : Rapide / Exact**. En mode Exact, l'algorithme V9.1 est conservé.

À 1920×1200 native, test en JavaScript simulé : la version rapide du ciel a utilisé **84 600 évaluations de nuages**, au lieu de calculer chaque pixel de ciel séparément. La différence d'image comparée au ciel original ne concerne que le ciel ; écart maximal observé **6 unités / 255 par canal** dans le test 1920×1200. À 640×400 avec pas 2 px, jusqu'à **8 unités / 255** ; l'atmosphère est donc une approximation volontaire.

### 2. Mode de précision / vitesse — compromis explicite

Le sélecteur **Précision du rendu** conserve par défaut le mode :

- **Native — 1 rayon par pixel affiché.** À 1920×1200, **2 304 000 rayons** caméra par image, entièrement conforme au principe du projet.

Et propose en option :

- **Rapide — moitié largeur/hauteur.** À 1920×1200 affiché, calcul du rendu en **960×600**, donc **576 000 rayons primaires**, puis agrandissement 2× par Canvas avec interpolation. Le format d'affichage reste 16:10. **Ce mode n'envoie pas un rayon pour chaque pixel affiché**, mais bien **un rayon pour chacun des pixels calculés**. La perte de netteté est assumée et clairement signalée dans l'interface. Le mode natif reste toujours accessible.

La géométrie mathématique, le BVH, les textures, les ombres, la lumière et les quatre autres résolutions d'affichage ne changent pas. Tous les compteurs de rayons reflètent la résolution **interne réellement calculée**, pas la résolution d'affichage.

## Test comparatif de référence à 1920×1200 affiché

**Exécution JavaScript avec DOM/Canvas simulés — ce ne sont PAS des performances mesurées dans Firefox.** Paramètres de scène identiques, position caméra correspondant à la capture utilisateur (distance 10, angle 115°, élévation 8°, FOV 93°, nearest, ombres 160×160) ; mode optimisé V9.1 et ciel rapide dans les deux cas.

| Mesure | V9.2 native | V9.2 rapide |
| --- | ---: | ---: |
| Résolution affichée | 1920×1200 | 1920×1200 |
| Résolution calculée | 1920×1200 | 960×600 |
| Rayons primaires | 2 304 000 | **576 000** |
| Tests d'intersection | 2 475 520 | 773 632 |
| Traitement intersections | 4 362 ms | 1 262 ms |
| Coloration / textures | 6 473 ms | 1 624 ms |
| Image complète | 10 932 ms | **2 915 ms** |
| Évaluations nuages | 84 600 | 21 350 |
| Lectures texels | 1 457 714 | 364 424 |

**Gain simulé : environ ×3,75 sur le temps complet d'image**, par diminution volontaire de la résolution **interne**. Il ne faut pas appliquer directement ce facteur aux **1 371 ms relevées dans le vrai navigateur** : performances, moteurs JIT, contrôles et rafraîchissements diffèrent.

## Tests de cohérence réalisés

- À 640×400 natif : **256 000 rayons exactement**.
- À 640×400 affiché en mode rapide : **320×200 calculé et 64 000 rayons**.
- À 1920×1200 affiché en mode rapide : **960×600 calculé et 576 000 rayons**.
- Aucune modification des formules d'intersection ou du mécanisme BVH.
- Test intégré des primitives (ciel, plan, ellipsoïde, boîte, cône) réussi.
- Grille atmosphérique échantillonnée toutes les 2 unités d'écran à faible résolution pour réduire la différence visuelle, et toutes les 4 unités en haute résolution.

## Utilisation / test réel recommandé

1. Ouvrir `index.html` en Firefox et choisir **1920×1200**.
2. Choisir **Ciel / nuages : Rapide**.
3. En **Précision du rendu**, comparer **Native** puis **Rapide**.
4. Relever « ms / image complète », « ms intersections BVH », « ms éclairage / texels », le nombre de rayons et la netteté de l'écorce / du banc / des feuilles.
5. Revenir au mode Native si les détails deviennent trop flous ; le mode Rapide n'est **pas équivalent** à un vrai 1920×1200.

## Limites

Cette révision n'ajoute ni WebGPU, ni WebGL, ni workers multithread, ni accélérateur FPGA. À 1920×1200 **natif**, le ray casting à un rayon par pixel reste coûteux sur CPU : le mode rapide réduit la résolution interne, pas la complexité mathématique par pixel. Les grandes accélérations sans réduire la précision demanderaient un moteur parallèle GPU/FPGA ou une architecture multicœur à évaluer séparément.
