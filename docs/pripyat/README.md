# Pripiat — références pour le moteur 3D

Branche de travail dédiée à une scène inspirée de Pripiat : collecte de plans, repères urbains et références photographiques.

## Références cartographiques

- **Atlas cartographique de Pripiat.png** — atlas déjà fourni dans la Library du projet (plan d’urbanisme 1988 et plan général). Le fichier source reste dans la Library ; il n’est pas dupliqué dans ce dépôt.
- [Plan OpenStreetMap de Pripiat (Wikimedia Commons)](https://commons.wikimedia.org/wiki/File:Map_of_Pripyat.svg) — utile pour le réseau viaire et l’emprise urbaine. Licence indiquée sur la page du fichier.
- [Plan des microdistricts (Wikimedia Commons)](https://commons.wikimedia.org/wiki/File:Map_of_Pripyat_microdistricts.svg) — repères des quartiers, centre, hôpital et équipements. Licence indiquée sur la page du fichier.
- [Catégorie Wikimedia Commons : bâtiments de Pripiat](https://commons.wikimedia.org/wiki/Category:Buildings_in_Pripyat)
- [Catégorie Wikimedia Commons : intérieurs de bâtiments](https://commons.wikimedia.org/wiki/Category:Building_interiors_in_Pripyat)
- [Catégorie Wikimedia Commons : panoramas](https://commons.wikimedia.org/wiki/Category:Panoramas_of_Pripyat)

Les plans servent à reconstruire l’organisation générale. Les photos servent de références visuelles ; elles ne sont pas intégrées au dépôt, car les droits et conditions diffèrent d’une image à l’autre.

## Constituer un index de 500 références photo

Depuis la racine du dépôt, avec Python 3 et `requests` :

```bash
python tools/pripyat/build_reference_index.py --limit 500 --output docs/pripyat/photo-references.csv
```

Le script recherche des fichiers image sur Wikimedia Commons, lit les métadonnées de licence de chaque fichier, déduplique les résultats et écrit un CSV de liens vers les pages source et les originaux. Il ne télécharge pas et ne redistribue pas les photos. Le nombre final peut être inférieur à 500 si Commons ne fournit pas assez de résultats admissibles ; le script l’indique explicitement.

Avant tout usage dans une texture distribuée, vérifier la licence sur la page source, respecter l’attribution et conserver les informations de licence. La page Commons est la référence en cas de changement de licence ou de métadonnées.

## Axes de classement suggérés

Le CSV comprend un champ de thème déduit du titre : urbanisme et plans, logements, équipements publics, hôpital, école, loisirs, intérieur, végétation et vues générales. Ce classement est indicatif ; valider les images avant de les associer à un objet précis de la scène.

## Périmètre du dépôt

Cette branche part de `master`, qui contient les archives QBasic et les démos HTML/JavaScript historiques. Le code du moteur natif x64 visible dans les captures n’est pas présent sur cette branche de `moteur3d`. Les références préparées ici sont indépendantes de son format de scène.
