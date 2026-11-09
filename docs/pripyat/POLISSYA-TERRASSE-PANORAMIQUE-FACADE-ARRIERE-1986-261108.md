# Hôtel Polissya — loggia panoramique et façade arrière (état 1986)

**Date du relevé :** 8 novembre 2026. **Destination :** grand atlas illustré et scène Moteur3D.  
**Statut :** photographie + interprétation géométrique, sans relevé métrique certifié.

## Pourquoi le dernier niveau doit être reconstruit

La silhouette du Polissya comprend un dernier niveau **en loggia ouverte** : de grandes travées sans vitrages pleins s'intercalent entre les poteaux de béton sous un toit plat légèrement débordant. Le nom ukrainien de l'hôtel, `ГОТЕЛЬ ПОЛІССЯ`, s'élève au-dessus de la **corniche supérieure** et non au-dessus d'un volume technique construit en plein.

Les récits de visiteurs et de guides historiques décrivent l'étage supérieur comme une **terrasse / café d'été panoramique**. Il existe des incertitudes sur son fonctionnement exact avant le 26 avril 1986. Dans notre livre, nous décrivons donc cette destination comme **prévue / rapportée**, plutôt que d'en affirmer l'exploitation commerciale précise à une date non documentée.

### Texte pour le grand atlas

L'hôtel Polissya, sur la place centrale de Pripyat, n'était pas une simple barre de chambres surmontée d'une enseigne. Son couronnement accueillait une terrasse panoramique largement ouverte sur la ville. Une série de grands portiques en béton, un garde-corps plein et une couverture débordante donnaient à ce dernier niveau un rythme singulier. Depuis cet étage, la vue s'étendait sur la place, le Palais de la culture Energetik et les quartiers résidentiels. Un café est mentionné dans plusieurs récits, mais il convient de distinguer le projet architectural de son usage réellement attesté en 1986. L'architecture du toit est également liée à un épisode postérieur : pendant les opérations consécutives à l'accident, le sommet de l'hôtel servit de point d'observation aux équipes engagées sur le site.

### Photo de référence — terrasse ouverte, vue extérieure

- [Wolniej — Hotel Polissya, façade et terrasse photographiées](https://www.wolniej.com/wymarle-miasto-prypec-dzien-drugi)
- [Digi24 — vue serrée sur l'enseigne et les ouvertures du toit](https://www.digi24.ro/stiri/externe/cum-arata-cernobil-si-pripiat-la-38-de-ani-de-la-dezastrul-nuclear-care-a-marcat-omenirea-2773883)
- [Nomadic Niko — enseigne métallique et béton du couronnement](https://nomadicniko.com/2013/05/09/pripyat-main-square/)

**Lecture visuelle :** les ouvertures supérieures doivent rester totalement dégagées entre les cadres en béton. Un bloc plein à la place de la loggia est une erreur géométrique, même si le rendu ressemble vaguement à un immeuble soviétique. Les photographies modernes illustrent la *forme* de la loggia, pas l'usure des matériaux que l'on souhaite restituer en 1986.

### Photo de référence — façade arrière

Une photographie arrière a été signalée par l'utilisateur durant la discussion du 8 novembre. La reconstruction doit conserver le rythme des fenêtres sur la façade opposée à la place et vérifier les bandes verticales et horizontales lors du recalage. Les photographies modernes servent à relever les trumeaux et les ouvertures ; **elles ne justifient pas** les fenêtres cassées, la rouille ni les arbres spontanés en 1986. L'emplacement exact des baies et leur nombre doivent être revus avec une photographie arrière en pleine résolution.

### Sources historiques complémentaires

- [ChernobylX — Hotel Polissya, café et terrasse panoramique](https://testin.chernobylx.com/chernobyl-hotels-this-is-what-you-need-to-know-before-spending-a-night-in-chernobyl/)
- [Find-Way — Polissya, dernier étage et usage décrit](https://find-way.com.ua/en/regions/chernobyl-exclusion-zone/pripyat/hotel-polissya-pripyat)
- [Quintin Lake, 2007 — photographie de la terrasse après abandon](https://blog.quintinlake.com/2010/03/02/photographs-of-the-abandoned-hotel-polissia-in-pripyat-the-radioactive-ghost-town-next-to-the-chernobyl-nuclear-power-plant/)

**Droits des photographies :** auteurs et détenteurs identifiés à vérifier avant reproduction dans l'atlas distribué ; liens de référence, pas appropriation des images.

## Modifications effectives de la scène 3D

Fichier : `experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`

- Retrait du gros bloc opaque (`14 m` de longueur, presque `3,2 m` de hauteur) qui fermait artificiellement le toit.
- Construction d'une **couverture légère** plus large que les ouvertures ; bords de la terrasse en béton ; parapets bas avant et arrière.
- **Quatre ouvertures par façade supérieure**, séparées par **cinq cadres/poteaux**, dont les jours restent vides.
- Positionnement de l'inscription `ГОТЕЛЬ ПОЛІССЯ` au-dessus du vrai toit de la loggia (hauteur du modèle ≈`20 m`).
- Restauration des rangées de vitrages sur la **façade arrière** avec montants, impostes et appuis de fenêtres réels.
- Variante de 1986 : verre intact et enseigne métallique ; variante abandonnée : fenêtres pouvant être endommagées et matériaux patinés.
- Annexe basse de l'hôtel et galerie courbe autonome conservées. Les immeubles provisoires `east-south` et `east-north` restent exclus.
- **IMG-022 demeure exclue** de toutes les décisions géométriques concernant la galerie.

Les dimensions restent celles du blockout photographique ; elles ne prétendent pas remplacer un relevé architectural.

## Contrôles

- `node experiments/pripyat-survey/test-polissya-rooftop-1986.mjs` : teste les huit ouvertures, les dix cadres/poteaux, l'absence de cube, la hauteur de l'enseigne et les baies arrière.
- `scene_pripyat_views.json` : la **vue 14** est prise à l'arrière de l'hôtel pour juger façade et couronnement.
- La chaîne de compilation Windows exporte et photographie maintenant **quatorze** vues CPU (au lieu de treize).

**Limite importante :** tests géométriques et compilation ne remplacent pas un recalage photo. Il reste à comparer la vue 14 au cliché arrière et à ajuster les proportions et le nombre exact de travées.
