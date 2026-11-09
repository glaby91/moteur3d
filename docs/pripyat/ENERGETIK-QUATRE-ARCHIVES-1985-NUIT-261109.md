# Palais Energetik — quatre nouvelles archives, 9 novembre 2026

**Projet :** Moteur3D, scène historique de Pripiat. **Statut :** étude photographique et correctifs du moteur. Les nouvelles vues ne constituent pas des relevés métriques, et la scène n'est pas certifiée photogrammétriquement.

## Fiches photographiques nouvelles

| ID | Sujet et transcription | Provenance visible, limites |
|---|---|---|
| ENE-HIST-01 | Vue oblique : bâtiment bas latéral, volume haut aveugle, aile de plusieurs étages et façade principale vitrée du Palais. Enseigne : **ДВОРЕЦ КУЛЬТУРЫ ЭНЕРГЕТИК** | Watermark `pripyat-city.ru © Ирина Якушева`. Date et auteur original à vérifier. Les différentes hauteurs doivent rester des volumes séparés. |
| ENE-HIST-02 | Vue nocturne très granuleuse : enseigne, fenêtres éclairées, points lumineux et grand bâtiment avec contours illuminés | Crédit, année, focale et caméra non identifiés. Donne une ambiance, **pas** une mesure photométrique. |
| ENE-HIST-03 | Façade frontale en couleur avec panneau latéral blanc et silhouette sombre, décor central **1945 — 40 — 1985**, grande roue derrière le palais | Filigrane `PRIPYAT.COM` ; présence d'un décor temporaire du quarantième anniversaire de 1945. Texte détaillé du panneau latéral non lisible, ne pas l'inventer. |
| ENE-HIST-04 | Grande place avec foule et autobus, bâtiment bas sur la gauche et aile du palais sur la droite | Date, auteur et événement exacts inconnus. **Ne pas assimiler automatiquement l'image à l'évacuation d'avril 1986.** |

Les fiches précédentes de l'atlas sont conservées ; ces références sont ajoutées sans les remplacer. Les photos ont été transmises dans le chat et **ne sont pas republiées** dans ce dépôt public, compte tenu de leurs droits non établis.

## Modifications apportées à la scène (lot du 09/11/2026)

1. **Aucune modification des cotes globales** de l'Energetik, du Polissya et de la colonnade courbe autonome : les nouvelles images ne permettent pas de déduire des coordonnées fiables. Les volumes du palais restent à comparer de face et en perspective.
2. Variante de décor `M3D_ENERGETIK_FACADE_DISPLAY=victory1985` : ajout d'un **panneau vertical clair, avec silhouette 3D anonyme et stylisée**, à gauche du décor central 1945–1985. Le dessin précis de l'original n'est pas reconstitué ni attribué à une personne.
3. Variante par défaut `congress1986` inchangée (draperies rouges et douze portraits procéduraux non nominatifs) ; variante `abandoned` sans panneau commémoratif.
4. **Enseignes lumineuses séparées** : `ДВОРЕЦ КУЛЬТУРЫ` bleu et `ЭНЕРГЕТИК` rouge. Les lettres rouges possèdent le matériau `neonRed` (PBR couleur, émission CPU, DirectX11 et compute) et les sources ponctuelles correspondantes ; le titre bleu utilise le matériau `neon` existant avec huit points de lumière froide. La variante abandonnée débranche ces nouveaux éclairages.
5. **Quatre vues supplémentaires** (15–18) dans `scene_pripyat_views.json` : oblique, nuit, façade 1985 et place/autobus. Caméras d'étude approximatives, sans prétention de recalage des photos d'origine. Les 14 vues précédentes sont conservées.

## Contrôles

```sh
node experiments/pripyat-survey/test-photo-references-261108.mjs
node experiments/pripyat-survey/test-energetik-facade-1986.mjs
node experiments/pripyat-survey/test-energetik-archives-261109.mjs
```

Les tests ne prouvent pas la qualité visuelle d'un rendu ; la build Windows et des captures comparatives doivent être examinées séparément.

## Sources externes et liens de contexte

- https://www.chernobyl.one/pripyat-palace-of-culture-energetik/
- https://mikhed.com/world/2008-08-zone-pripyat.html — description de la structure arquée devant le secteur de l'hôtel.
- https://www.forgottenchernobyl.net/pripyat-then-and-now-part-1-main-square
- `docs/pripyat/ENERGETIK-FACADE-1986-CONGRES-VITRAGE-PBR-261108.md` : variantes et iconographie précédentes.
- `docs/pripyat/RECALAGE-PHOTO-POLISSYA-ENERGETIK-261108.md` : géométrie de la place déjà mise en place.
