# Cahier de reconstruction — Palais Energetik

État de travail : passe de scène préparée pour le build `261104-0008`. La reconstruction reste un blockout détaillé, à valider visuellement sur Windows ; les volumes et certains intérieurs doivent encore gagner en fidélité.

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

## Deuxième passe — lumière et exploration

- L’éclairage diffus de base est relevé pour éclaircir les zones à l’ombre sans supprimer l’éclairage directionnel.
- Le ciel de jour devient bleu pâle plutôt que beige, en conservant la teinte chaude au lever et au coucher du soleil.
- Des lumières de rebond larges et discrètes éclairent la profondeur de la façade ouverte.
- La case « Visite fluide » garde un aperçu léger après les déplacements dans Pripiat ; « Calculer l’image » force toujours un rendu complet.
- La visite automatique garde une définition interne de 800 × 600 sur CPU comme DirectX; elle avance à la cadence réellement calculée par le moteur.

## Troisième passe — escaliers, fresque et nuit

- Le déplacement manuel balaie maintenant le point de visée, la position de la caméra et deux points latéraux de son volume ; le déplacement panoramique utilise la même collision que le clavier.
- Les marches du grand foyer ont été relevées pour rejoindre le palier sans jour visuel.
- Les panneaux de fresque étaient placés derrière la coque latérale. Ils sont maintenant sur les faces intérieures, dont un panneau plus large ; le matériau de fresque est moins répétitif sur ces surfaces.
- Les points du trajet guidé désignent maintenant la position réelle de la caméra. Le décalage de 3 m entre caméra et point visé retenait l’appareil sur la cloison de l’entrée vers 21–24 s ; la visite passe par les ouvertures centrées.
- La caméra tourne progressivement avant les angles du parcours pour éviter les changements de direction brusques.
- Les plafonniers de plusieurs salles et niveaux ont été ajoutés avec des sources chaudes. Le bouton « Vue de nuit — 22 h » place la caméra devant la façade et règle la scène pour montrer les éclairages intérieurs.

## Validation restante

1. Compiler le paquet Windows `261104-0008`.
2. Comparer la façade DirectX et CPU à la maquette Skeiron et aux photos : silhouette, ouvertures, matériaux, couleur et lumière.
3. Examiner le foyer, la fresque, l’auditorium et le gymnase à plusieurs points de vue.
4. Vérifier les déplacements aux portes, coursives, escaliers et transitions de niveau, ainsi que la visite guidée autour de 21 secondes.
5. Mesurer le temps d’image DirectX à 1366 × 768 après la mise à jour des textures.

## Quatrième passe — détails de scène

- Le linteau avant reçoit des éclats d’enduit et de pierre, des zones de béton mis à nu et des coulures de rouille. La grande façade reste ouverte, sans vitrages.
- Le retour intérieur du foyer reçoit une fresque murale en relief : pigments rouges, bleus et ocre écaillés, figure centrale et motif végétal. Elle est placée près du grand escalier pour être visible pendant la visite.
- Une trame de plafond métallique, des dalles tombées et quelques câbles pendants rendent les dégâts intérieurs lisibles depuis le hall.
- Les éléments ajoutés utilisent les matériaux du moteur et une géométrie légère, sans intégrer les photos de référence au binaire.

Cette passe ne prétend pas être une reconstruction photogrammétrique ni une validation visuelle finale.

## Cinquième passe — fresque longue, cage et vues directes

- La fresque du foyer s’étend maintenant sur près de 18 m de mur, en deux pans qui laissent libre la porte du niveau. Les formes peintes, figures et fragments couvrent une longueur de mur au lieu d’un tableau isolé.
- La cage d’escalier haute est maintenant une coque creuse avec une porte de toit. Le trajet guidé sort par cette porte et continue sur la bande praticable du toit avant sa vue finale.
- Le mode vidéo calcule les images à 800 × 600 sur CPU comme sur DirectX 11. Le parcours peut prendre plus de temps sur CPU, mais ne grossit plus un rendu 320 × 240.
- Une liste de dix cadrages au plus est chargée depuis `scene_pripyat_views.json`, annexe liée à la scène. La fiche des noms et usages est dans [VUES-ENERGETIK.md](VUES-ENERGETIK.md).
