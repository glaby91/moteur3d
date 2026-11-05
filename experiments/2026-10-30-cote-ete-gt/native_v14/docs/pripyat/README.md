# Pripiat — plan spatial et registre des références visuelles

Cette page relie les photographies et vidéos de référence au plan de construction de la scène native. Le but est de reconstruire progressivement le centre de Pripiat tel qu’il était autour du 26 avril 1986, tout en utilisant les vues postérieures uniquement pour les formes des bâtiments, la végétation et les dégradations qu’elles permettent réellement d’observer.

## Fichiers de construction

- [Scène centrale et coordonnées des objets](../scene_pripyat_central.js)
- [Vues/cadrages de la scène](../scene_pripyat_views.json)
- [Catalogue d’objets Pripiat](../PRIPYAT-MATERIALS-CATALOGUE.md)
- [Références documentaires et premières captures](../../../../../../docs/PRIPYAT-REFERENCES-VISUELLES-20261105.md)

Dans le code, **Y est l’axe vertical**. Les objets du plan sont placés dans le repère local **X/Z**; le parcours caméra arrive depuis **+Z**. Les plages de coordonnées ci-dessous sont des limites de volumes présentes dans le code, pas des coordonnées géographiques GPS. Il faut les recaler sur un vrai plan d’archives avant de les présenter comme une implantation urbaine exacte.

## Plan de la zone déjà construite

| Élément | Position locale / dimensions approximatives | Statut dans la scène | Références à rapprocher |
|---|---|---|---|
| Place centrale pavée | X −53 à +53 ; Z −15 à +61 | Dallage, joints, reprises, trottoirs et voies périphériques déjà modélisés | Vues aériennes Souterrain-Lyon; photographies d’archives avant l’accident |
| Palais de la culture Energetik | X −50,5 à +30,5 ; Z −43,4 à −18,8; enveloppe env. 81 × 24 m | Bâtiment principal détaillé, façade, accès, halls, auditorium, escalier et fresque | Série Reddit r/chernobyl; images du hall; vidéos et photos qui montrent l’extérieur |
| Hôtel Polissya | X +40 à +63 ; Z −42 à −20; hauteur max. env. 20 m | Masse contextuelle et fenêtres, avec peu de détails | Photos aériennes et vues de centre-ville avant l’accident |
| Liaison Energetik–hôtel | X +30 à +40 ; Z env. −24 à −18 | Volume bas vitré déjà présent | Photos de l’ensemble Energetik / Polissya; vérifier le raccord dans le plan |
| Immeubles périphériques | 4 volumes : X −64 à −57 / +46 à +60 ; Z −37 à −10 et +18 à +52 | Barres d’habitation simples, 5 à 7 étages; position indicative | Vues aériennes et photos pré-1986 à géolocaliser |
| Bancs, fauteuils extérieurs, bacs, arbres et lampadaires | Dispersés dans la place entre X −56 et +53, Z −12 et +68 | Objets déjà positionnés dans les tableaux de coordonnées du code | Photos de la place, vues aériennes et vues au sol; vérifier espacement et essence des arbres |
| Entrée monumentale de Pripiat | Hors emprise actuelle de la zone centrale | Non modélisée dans cette scène | Photo de l’enseigne « ПРИПЯТЬ »; à placer sur une future zone d’accès, pas sur la place Energetik |

### Repères d’implantation utiles dans le code

- Le parvis d’Energetik est sur le côté **Z positif** du bâtiment; les marches et l’accès principal sont centrés autour de **X ≈ 0**.
- L’hôtel Polissya se trouve à côté du bord **X positif** d’Energetik, relié par le volume bas.
- Les immeubles, bancs, bacs, arbres et lampadaires ont des listes de positions explicites dans le code : les modifier seulement après comparaison avec une carte ou une photo localisée.
- Le code actuel décrit un secteur central; il ne représente pas toute la ville. Ne pas étendre la grille de la place aux districts non documentés.

## Registre des sources vidéo et des objets à relever

| Source / période | Images déjà repérées | Objets et détails à extraire | Usage cartographique |
|---|---|---|---|
| [26 avril 1986 — Valery Yevtushenko, 00:02](https://www.youtube.com/watch?v=Ez43DHJjgcE&t=2s) | À 00:02, un véhicule blindé n° 611 passe devant des immeubles et des arbres. Le texte associé situe aussi la prise de vue près du commissariat, le matin, et décrit une relève de patrouille, un véhicule municipal de lavage et des pigeons. | Blindé n° 611; véhicule municipal; policiers et vêtements civils de l’officier cité; arbres, chaussée et immeubles. La faible définition ne permet pas de fiabiliser les détails fins. | Le lieu est décrit comme proche du commissariat, mais n’est pas encore géolocalisé dans le plan : ne pas placer ces objets automatiquement devant Energetik. |
| [Montage photo Pripiat 1977–2024](https://youtu.be/6i1zngYkWtk) | Montage multi-périodes contenant de nombreuses images. Les timecodes de chaque photo pré-1986 restent à relever. | Immeubles, voitures, mobilier, enseignes, personnes, arbres et aménagements de rues; garder un timecode par image et une date supposée. | Peut apporter des vues pré-accident pour les bâtiments et objets; chaque vue doit être datée et localisée avant le placement d’un objet. |
| [Pripiat en 1988 — vues aériennes](https://youtu.be/E92TnfQuQps) | Survol de Pripiat deux ans après l’accident, d’après le titre et la description de la vidéo. | Volumes urbains, grands axes, couronnes d’arbres et parcs visibles; pas de circulation ni de population ordinaires. | Sert à comparer l’emprise des bâtiments et la trame urbaine, après vérification du secteur filmé; période post-évacuation. |
| [Visite de la zone d’exclusion — Le Figaro](https://youtu.be/NfCXhIJzrcI) | Reportage de visite dans la zone d’exclusion. L’utilisateur signale une qualité vidéo particulièrement utile pour distinguer les détails. | À relever image par image : façades et volumes, panneaux, mobilier, arbres, végétation au sol, véhicules de visite et repères de rue. Les détails d’abandon sont à classer séparément des éléments de 1986. | Sert de référence visuelle de haute qualité pour l’état actuel et les détails de surface. Ne pas utiliser ses panneaux ou véhicules actuels comme objets de la ville de 1986. |
| [Reportage AFP : Pripiat figée dans le temps](https://youtu.be/coF8WGjlhqc) | Vue contemporaine de Pripiat abandonnée. | Végétation, usure et état des bâtiments, si l’image est suffisamment précise. | Comparaison de l’état actuel seulement. |
| [Photos d’archives et vues aériennes — Souterrain-Lyon](https://www.souterrain-lyon.com/pripyat-avant-apres-accident-urbex/) | Captures partagées : enseigne d’entrée de la ville et vues aériennes des quartiers. | Immeubles, axes, trame des îlots, espaces ouverts et repères de l’entrée. | Sources à rapprocher d’un plan avant de convertir les vues en positions X/Z. |
| [Photos nocturnes et Energetik — Reddit, r/chernobyl](https://www.reddit.com/r/chernobyl/comments/1k9t999/rare_photographs_of_pripyat_before_the_explosion/) | Capture de la colonnade d’Energetik et vues nocturnes urbaines. | Forme de la façade, éclairage, fenêtres, silhouettes et rythme des arbres. | Rattacher les vues à Energetik ou à la place seulement quand le bâtiment ou l’axe de prise de vue est reconnu. |

## Objets à construire ou à vérifier

### À créer si les archives confirment leur présence

- **Personnes** : groupes de passants ou de policiers avec silhouettes et vêtements d’époque, à placer dans les vues datées du 26 avril 1986; éviter les figurants génériques si aucune image ne confirme leur emplacement.
- **Véhicules** : véhicule municipal de nettoyage et blindé n° 611 issus du film du 26 avril. Leur implantation doit suivre les images et la localisation du commissariat, pas le centre Energetik par défaut.
- **Panneaux** : enseigne monumentale d’entrée « ПРИПЯТЬ » dans une scène d’accès distincte; autres plaques et signalétiques uniquement si les photos et leur emplacement sont identifiés.
- **Végétation** : séparer les arbres d’alignement et plantations de 1986 des jeunes arbres et broussailles apparus après l’évacuation. Les vues de 1988 et actuelles renseignent l’évolution, pas l’état initial complet.
- **Mobilier urbain** : bancs, bacs, lampadaires et arrêts éventuels; vérifier leurs formes et leur distribution à partir des photos pré-accident avant d’ajuster les coordonnées déjà codées.

### État de la scène à ne pas confondre avec les références

La scène de travail est aujourd’hui une reconstruction d’un centre abandonné : matériaux usés, ruines localisées, herbes, jeunes arbres, vitres et mobilier détérioré. Pour viser le **26 avril 1986**, il faudra prévoir un état temporel distinct : bâtiments et mobilier en état d’usage, végétation entretenue, fenêtres et éclairages fonctionnels, véhicules et personnes seulement aux emplacements attestés. Les images 1988, AFP et Le Figaro restent utiles à l’architecture et aux changements après abandon, mais ne fixent pas cet état initial.

## Convention de relevé des images

Pour chaque image retenue, ajouter une ligne avec : source, URL, timecode ou numéro de photo, période, lieu identifié ou supposé, objets visibles, confiance (confirmé / probable / à vérifier), et élément du code concerné. Ne créer une position X/Z qu’après avoir identifié au moins un repère commun entre l’image et le plan.

Les images originales restent accessibles depuis les sources et ne sont pas recopiées dans Git tant que les droits de réutilisation et le format des captures ne sont pas établis. Les timecodes donnent un accès direct aux images de référence sans ajouter de gros fichiers vidéo au dépôt.
