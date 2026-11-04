# Cahier de reconstruction — Palais Energetik

État de travail : passe de scène préparée pour le build `261104-0009`. La reconstruction reste un blockout détaillé, à valider visuellement sur Windows ; les volumes et certains intérieurs doivent encore gagner en fidélité.

## Références prioritaires

- [Maquette extérieure Skeiron — Google Arts & Culture](https://artsandculture.google.com/story/palace-of-culture-energetik-skeiron/ggUh4kq6wkwB7w?hl=en) : volume général, implantation, toiture, chemins et végétation alentour.
- [Galerie du Palais Energetik — Chernobyl Gallery](https://www.chernobylgallery.com/galleries/palace-of-culture/) et captures fournies par l’utilisateur : façade, foyer, fresque, gymnase, auditorium, toiture et état d’abandon.
- Répartition des salles ci-dessous : indications fournies par l’utilisateur. Les photos guident l’aspect et les ouvertures ; elles ne donnent pas un plan complet coté.
- [PhotoBuildings — bâtiment de Pripiat](https://photobuildings.com/object/80424/?lang=ru) et [fiche du projet type](https://photobuildings.com/projects/2736?lang=en) : identifient la série 2С-06-9/70 et documentent le bâtiment et ses variantes.
- [TipDoc — projet 2С-06-9/70](https://tipdoc.ru/catalog/14090/1640132/) et [Proekt.sx — dossier de réfection](https://proekt.sx/tip/dk781-detail.html) : pistes pour les dessins techniques; les fiches distinguent le projet type, le passeport résumé et le dossier de réfection.
- [Chernobyl.one — galerie Energetik](https://www.chernobyl.one/pripyat-palace-of-culture-energetik/) : vues de l’état intérieur, du théâtre, du gymnase, des fresques et des autres espaces.

## Référence technique et niveau de confiance

- Le bâtiment de Pripiat est référencé sous le projet type **2С-06-9/70**. La fiche générique du projet décrit un club avec salle de 800 places, trois niveaux et murs extérieurs en brique; la fiche du bâtiment de Pripiat donne deux niveaux. Ces fiches ne suffisent donc pas à reconstruire seules les adaptations locales.
- Le dossier Proekt.sx décrit une réfection d’un bâtiment utilisant ce projet type : 30,4 × 81,3 m, auditorium de 720 places et salle de 24 × 18 m. Ces dimensions sont un repère de comparaison, pas encore une cote validée pour Energetik à Pripiat.
- Le matériau propre à reproduire dans la salle cinéma est le **tuf volcanique rouge/rose** : des fragments de ce parement sont signalés comme encore visibles à l’intérieur. Pour l’extérieur, la finition en **marbre blanc** est documentée. Les ossatures, dalles alvéolaires, briques et vitrages décrits par les fiches génériques restent à confirmer sur les plans ou photos du bâtiment précis.
- TipDoc signale que les dessins PDF sont disponibles mais vend séparément le passeport résumé et le projet complet; Proekt.sx propose une documentation de réfection payante. Aucun fichier de plan coté propre à la variante de Pripiat n’a encore été obtenu. Les proportions de la scène restent donc des estimations photographiques jusqu’à réception d’un plan ou d’un relevé.

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

1. Compiler le paquet Windows `261104-0009`.
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

## Septième passe — salle cinéma de référence visuelle

- La salle cinéma/auditorium devient le premier espace traité comme une composition complète : volume double hauteur, scène profonde, proscenium, écran en retrait, rideaux, poutres apparentes et plafond suspendu partiellement effondré.
- Le premier plancher est ouvert au-dessus du public, tout en gardant la dalle sur les ailes et le couloir arrière pour raccorder les autres niveaux.
- Le parement caractéristique en tuf volcanique rouge rosé reçoit une carte procédurale dédiée : joints, pores, éclats et sous-couche de béton visible. Les murs présentent des manques irréguliers et coulures aux ancrages.
- Les rangées de sièges ont une élévation progressive, plusieurs blocs séparés par des allées, des piètements tubulaires, des accoudoirs, des coussins déchirés et quelques sièges effondrés. Des débris restent concentrés dans les passages.
- Les sources chaudes de plafond et de paroi donnent une lecture plus claire des rangées et du fond de scène. Le cadrage 04 vise désormais le centre du volume à hauteur de regard.
- Correction des panneaux acoustiques côté ouest : leurs bornes sont normalisées, ce qui évite des boîtes à largeur négative dans le générateur de scène.

La scène est prête pour un contrôle sur le build Windows à 1366 × 768. Le niveau visuel UT2004 est le repère à atteindre, pas un résultat déclaré sans capture native. Vérifier en priorité la silhouette des rangées, la profondeur de scène, les faces du tuf, le contraste des lampes et l’absence de trous de géométrie.

## Cinquième passe — fresque longue, cage et vues directes

- La fresque du foyer s’étend maintenant sur près de 18 m de mur, en deux pans qui laissent libre la porte du niveau. Les formes peintes, figures et fragments couvrent une longueur de mur au lieu d’un tableau isolé.
- La cage d’escalier haute est maintenant une coque creuse avec une porte de toit. Le trajet guidé sort par cette porte et continue sur la bande praticable du toit avant sa vue finale.
- Le mode vidéo calcule les images à 800 × 600 sur CPU comme sur DirectX 11. Le parcours peut prendre plus de temps sur CPU, mais ne grossit plus un rendu 320 × 240.
- Une liste de dix cadrages au plus est chargée depuis `scene_pripyat_views.json`, annexe liée à la scène. La fiche des noms et usages est dans [VUES-ENERGETIK.md](VUES-ENERGETIK.md).

## Sixième passe — netteté native et cadrage de la fresque

- La référence de rendu visuel devient *Unreal Tournament 2004* : textures nettes à résolution native, lecture claire des matières, détails d’architecture et éclairage plus affirmé. Ce repère décrit la direction visuelle, pas une équivalence de moteur ou une parité déjà atteinte.
- Le filtrage DirectX passe en anisotrope 8× pour réduire le flou des textures vues en biais, notamment sur les marches et les sols.
- Le béton et la pierre utilisent une échelle UV plus serrée, cohérente entre CPU et DirectX, afin que les textures de 1024 px gardent leurs détails à 1366 × 768.
- La fresque longue dispose maintenant d’un mur continu de part et d’autre de la porte et d’un cadrage plus large centré sur les deux pans. La vue 03 reste le contrôle dédié au grand escalier.
- La comparaison sur le portable reste à faire à résolution native : netteté des surfaces, lecture des marches, fresque visible, couleurs et temps d’image.
