# Cahier de reconstruction — Palais Energetik

État de travail : première passe enregistrée avec le build `261104-0003`. Les changements restent à compiler et à comparer dans l’exécutable Windows.

## Références prioritaires

- [Maquette extérieure Skeiron — Google Arts & Culture](https://artsandculture.google.com/story/palace-of-culture-energetik-skeiron/ggUh4kq6wkwB7w?hl=en) : volume général, implantation, toiture, chemins et végétation alentour.
- [Galerie du Palais Energetik — Chernobyl Gallery](https://www.chernobylgallery.com/galleries/palace-of-culture/) et captures fournies par l’utilisateur : façade, foyer, fresque, gymnase, auditorium, toiture et état d’abandon.
- Répartition des salles ci-dessous : indications fournies par l’utilisateur. Les photos guident l’aspect et les ouvertures ; elles ne donnent pas un plan complet coté.

## Organisation de référence

- **Sous-sol :** stand de tir, réserves et archives.
- **Rez-de-chaussée :** grand foyer et escalier, théâtre/concert et coulisses, cinéma, gymnase, boxe/lutte.
- **Premier étage :** mezzanine du foyer, salle de danse/discothèque Edison-2, fresque, bibliothèque.
- **Niveau supérieur et toit :** salles de clubs, cours et ateliers, accès à la terrasse.

L’emplacement exact des pièces, portes et couloirs devra être déduit des vues lorsque la documentation ne le montre pas. La visite doit rester dans les espaces accessibles et ne pas traverser les murs ; seules les portes et travées ouvertes sont franchissables.

## Critères visuels

- Reproduire les volumes extérieurs et l’implantation de la maquette Skeiron.
- Garder la grande façade ouverte, sans fenêtres ni vitrage, avec ses piliers, coursives et garde-corps dégradés.
- Montrer le nom du bâtiment correctement sur le toit, sans ajouter d’inscriptions inventées.
- Donner au béton, à la pierre et à l’enduit une épaisseur et une usure crédibles : coulures, fissures, éclats, peinture écaillée, salissures et débris.
- Construire des matériaux différenciés pour la fresque colorée mais abîmée, le sol, le métal rouillé, le bois, les gravats, l’herbe et les arbres.
- Ajouter de la végétation de bordure et des jeunes arbres dans les zones ouvertes, cohérente avec les photos.
- Éclairer la scène du matin avec une lumière directionnelle lisible dans le foyer, des ombres cohérentes et de la poussière discrète dans les rais lumineux.
- Conserver le parcours extérieur–intérieur–étages–toit et le réglage d’affichage cible du Vostro, 1366 × 768.

## Première passe technique enregistrée

- La façade principale ne reçoit plus de panneaux de verre ; des garde-corps rouillés incomplets occupent les coursives ouvertes.
- Les textures procédurales du Palais ajoutent des coulures de béton, des zones d’enduit écaillé, une fresque en pigments délavés et des matériaux distincts pour bois, écorce, herbe et débris.
- Le backend DirectX échantillonne maintenant les cartes de normales et de rugosité avec les mipmaps disponibles, afin que le relief et les reflets varient selon le matériau.
- Les références photo restent des guides visuels ; aucun fichier photo protégé n’est intégré au dépôt comme texture.

## Validation restante

1. Compiler le paquet Windows `261104-0003`.
2. Comparer la façade DirectX et CPU à la maquette Skeiron et aux photos : silhouette, ouvertures, matériaux, couleur et lumière.
3. Examiner le foyer, l’auditorium et le gymnase à plusieurs points de vue.
4. Vérifier les déplacements aux portes, coursives, escaliers et transitions de niveau.
5. Mesurer le temps d’image DirectX à 1366 × 768 après la mise à jour des textures.

Cette passe ne prétend pas être une reconstruction photogrammétrique ni une validation visuelle finale.
