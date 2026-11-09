# Palais Energetik — relevé photogéométrique du seul bâtiment (09/11/2026)

## Périmètre strict
Édifice **Energetik exclusivement**, à partir des deux captures aériennes transmises par l'utilisateur. **Hôtel Polissya, arcade et place sont hors traitement**. Les originaux photographiques demeurent dans la conversation et ne sont pas reproduits dans le dépôt public.

Les deux captures correspondent à des points de vue obliques différents, dont la deuxième est incorporée dans une capture du lecteur YouTube. Son espace image utile est un recadrage `[16,153,950,680]` sur une capture de 1366×768 px. Les annotations de la deuxième vue sont stockées en coordonnées **du recadrage**, et non de tout l'écran.

## Segmentation en régions (non métriques)

- A — grande façade principale vitrée, trame verticale, bandeau inférieur.
- B — grand volume surélevé rectangulaire, visible sur le côté gauche **dans les images**. La fonction exacte (cage de scène, salle technique, tour) n'est pas établie par ces deux vues.
- C — toiture terrasse principale et son acrotère ; l'enseigne est sur le bord de ce volume.
- D — volume arrière rectangulaire séparé en silhouette, dont la fonction exacte reste non vérifiée.
- E — aile plus basse / retour de façade visible sous des angles différents.

Les polygones manuels approximatifs sont enregistrés dans `tools/photo2scene/data/pripyat/energetik-palace-aerial-only-261109.json`. Ils servent à **définir des régions de recherche** et non des points de contrôle en 3D.

## Comparaison technique au modèle actuel

Dans `scene_pripyat_central.js` (branche `feature/pripyat-polissya-arcade-261107`) :
- le bloc du bâtiment est déclaré `x=-50.5..30.5`, `z=-43.4..-18.8`, hauteur provisoire `11.4` ;
- la tour de toiture est construite sur le côté `x=20.5..30.5`, pour une altitude supérieure `14` ;
- l'aile arrière `sports` occupe `x=-47..-3`, `z=-68.9..-42.7` avec enveloppe de travail `11.4`.

Le volume B apparaît à gauche de la façade **sur les deux clichés**, alors que la tour du modèle se trouve en X positif. Cela signale une **incohérence visuelle potentielle** mais pas encore une erreur démontrée : la calibration des caméras, leurs conventions de main droite/gauche, et les correspondances de toiture doivent être fixées avant tout déplacement des volumes.

Première tentative calculée SIFT sur les masques de l'édifice des deux images : seulement **5 appariements** après filtrage strict ; insuffisant pour un recalage 3D fiable. Il ne faut pas fabriquer de poses 3D ni de longueur en mètres à partir de cette tentative.

## Décisions prises

1. Préserver le modèle existant, sa fresque, ses niveaux, ses vitrages, et ses textures.
2. Créer cette fiche photo à cinq régions avec une distinction nette entre silhouette visible et fonction supposée.
3. Comparer plus tard sur au moins deux photographies supplémentaires l'orientation du grand volume B et la séparation volumétrique C/D.
4. Reconstituer d'abord les lignes verticales de la façade et les points de fuite, puis estimer les poses de caméra avec contrôle de reprojection indépendant.
5. **Ne pas toucher** à la courbe d'arcade / à l'hôtel Polissya dans cette branche.

**Statut :** relevé en pixels et comparaison documentaire ; ni reconstruction certifiée ni modification géométrique finale.
