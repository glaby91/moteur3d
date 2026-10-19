# Moteur 3D — V4, ombres Z stabilisées (19 octobre 2026)

## Pourquoi cette révision ?

Un retour utilisateur du 19/10 : **la géométrie 3D est stable**, mais les **ombres projetées scintillent** (« sapin de Noël »).
La cause probable dans V3 est la combinaison d'une direction lumineuse quantifiée par paliers,
d'un filtrage 2×2 non pondéré (valeurs par sauts de 25 %) et d'un éclairage au centroïde des facettes.

## Correctifs V4

- **Une seule source directionnelle réelle**, sans augmenter le nombre de lumières actives.
- **Deux cartes Z spatiales calculées aux directions adjacentes de la même lumière** : les deux tests de visibilité sont interpolés selon la position lumineuse réelle, de sorte que franchir un palier ne change pas brutalement de tampon Z.
- **Filtrage PCF sous-pixel bilinéaire** de quatre comparaisons de profondeur par carte : la proportion éclairée évolue avec les coordonnées fractionnaires de la carte, contrairement aux pas de 25 % précédents.
- Décor fixe réutilisé par direction; les cartes de direction future sont **préparées progressivement en tranches de 220 triangles** et conservées à courte portée en cache pour atténuer les pics.
- Les **100 triangles du personnage** sont reprojetés dans chaque carte active lorsque sa position change; le décor de 2 310 triangles reste caché par angle discret.
- Contraste des auto-ombres réduit sur le feuillage afin d'éviter les facettes qui s'éteignent brutalement.
- Filtrage explicite de l'agrandissement du masque d'ombre au sol.
- Nouvelle case **« Ombres stabilisées (interpolation Z) »** activée par défaut, décochable pour comparer.
- La géométrie caméra et les collisions sont inchangées.

## Vérifications effectuées

- Analyse syntaxique JavaScript par V8 : **réussie**.
- Test d'exécution avec DOM et Canvas simulés : **réussi** ; les 2 410 triangles sont présents, profondeur finie de la carte Z, deux cartes disponibles.
- Comparaison numérique de 1 024 récepteurs au sol juste avant et après une limite de direction (`0,042 ± 0,00003 rad`) : **aucun écart supérieur à 0,2 de visibilité** ; écart maximal d'environ **0,00072**.
- Test de 36 positions de lumière : caches et nouvelle carte de profondeur mis à jour sans exception.
- **Pas encore de validation visuelle réelle par navigateur ni de validation de la fluidité sur l'ordinateur utilisateur**. Les tests de DOM simulé ne sont pas des FPS du navigateur.

## Limites et points à surveiller

1. Les deux cartes représentent **une seule lumière**; elles ne sont pas deux sources distinctes.
2. Le Z-buffer **caméra** n'est pas encore implémenté; le décor reste dessiné via l'ordre des triangles Canvas.
3. La shadow-map est directionnelle, pas omnidirectionnelle, et le masque au sol reste sous-échantillonné.
4. Les ombres sur les faces de la scène sont évaluées au centroïde de chaque triangle, pas par pixel. Le filtrage améliore la continuité mais ne remplace pas un rendu Z par pixel côté caméra.
5. Une direction précalculée et interpolée peut créer une ombre un peu plus douce ou double lorsqu'une arête est en mouvement. Ce compromis est intentionnel pour tester la stabilité.
6. Deux cartes et deux tests d'ombre augmentent les accès mémoire; la préparation progressive réduit les pointes mais le coût sur machine réelle doit être mesuré.
7. Au démarrage ou si la lumière saute immédiatement sur un angle éloigné, deux cartes peuvent devoir être construites d'un coup : une pause initiale reste possible.

## Prochains essais visuels

- Laisser tourner caméra fixe + lumière mobile et regarder les contours d'ombre au pied de l'arbre et du personnage.
- Refaire lumière fixe + caméra mobile pour dissocier scintillement des ombres et instabilités d'échantillonnage écran.
- Comparer case de stabilisation ON / OFF aux trois résolutions Z (128² / 192² / 256²).
- Relever moyenne, médiane et P95 des temps de rendu, **y compris les pics lors d'un changement de direction**.
- Puis décider si l'ombre doit passer à un récepteur par pixel, sur les zones concernées uniquement.

La version v3 est conservée dans `../2026-10-19-single-light-shadow-z/`, la
version géométriquement stable d'origine dans `../2026-10-18-anti-stuttering-tiles/`.
