# Pripiat 1986 — hôtel Polissya, aile basse et galerie courbe

**Compte rendu de correction :** 8 novembre 2026.  
**Objectif :** retrouver la composition que montrent les photos — un hôtel principal perpendiculaire au Palais Energetik, un bâtiment bas attenant et une galerie courbe passant devant, distincte de la structure de l'hôtel. **Ne pas introduire de bâtiment à droite non identifié.**

## Ce qui était faux dans la scène

La liste `M3D_PRIPYAT_BLOCKOUT.housing` contenait une résidence entièrement provisoire (`east-south`, sept étages, emprise X=86–100 m, Z=−37 à −12 m) juste à côté du Polissya. Aucun document retenu ne validait cette haute construction. Elle parasitait les rendus de la place et se trouvait là où le modèle aurait dû présenter un **volume bas lié à l'hôtel**. Une autre résidence d'ambiance, `east-north`, n'était pas vérifiée. **Les deux sont retirées du rendu** : on ne les remettra pas sans document photographique et localisation.

## Références retenues et textes

### POL-A — Hôtel Polissya historique, vue depuis la place

[Référence publiée par Forgotten Chernobyl (comparaison avant / après)](https://www.forgottenchernobyl.net/pripyat-then-and-now-part-1-main-square) et [vue d'époque Chernobyl Visit](https://chernobyl-visit.com/chernobyl-diaries/pripyat-city-before-the-chernobyl-disaster-and-after/).

La façade principale de l'hôtel est une grande grille de fenêtres, surmontée d'un belvédère ouvert portant l'inscription ukrainienne **ГОТЕЛЬ ПОЛІССЯ**. Au pied du corps haut, un auvent horizontal, soutenu par des poteaux, dessine une esplanade couverte. À l'une de ses extrémités, la composition comprend des volumes bas et un passage vers le reste de la place. Un élément en hauteur à l'arrière-plan ne doit pas être interprété comme un bâtiment immédiatement accolé à l'hôtel sans preuve.

**Usage géométrique :** hôtel sur la grande longueur, séparation façade/auvent, bande de vitrage, volumes bas de services. Photo d'époque comme référence 1986, auteur et date exacte encore non vérifiés.

### POL-B — Galerie courbe devant l'hôtel

[Vue de la colonnade devant Polissya](https://wpblog.vidviday.org.ua/app/uploads/2019/05/07-Hotel-Polissya.jpg) ; [galerie photographique du centre-ville](https://www.atlasobscura.com/places/abandoned-city-of-pripyat).

La galerie couvre un chemin piéton selon une **courbe en plan**, dont la direction change progressivement entre le côté du Palais et l'axe de l'hôtel. Sa toiture est soutenue par une double rangée de poteaux. Elle se termine près du bâti hôtelier, mais doit rester un **ouvrage indépendant avec son passage libre** dans notre lecture retenue. Son profil est légèrement bombé en travers, pas une simple dalle plane dans l'export.

**Référence exclue : IMG-022**, qui donnait une toiture plate non conforme et ne doit plus orienter la géométrie.

### POL-C — Aile basse de services / restaurant attenant à l'hôtel

La composition est documentée dans les vues historiques et modernes de l'hôtel (voir POL-A/B). Une étude de l'ensemble par [Soner Karagoz](https://black_sektor.artstation.com/projects/WXr4q2) traite elle aussi de l'hôtel et du restaurant ; **le modèle de jeu n'est pas une mesure architecturale historique**.

**Texte pour l'atlas :** « À côté du grand volume d'hébergement, une aile basse prolonge les activités de l'hôtel au niveau de la place. Les longues baies vitrées et les accès sous les auvents mettent en relation ce bâtiment public avec le passage piéton couvert et les espaces verts. La composition relève de plusieurs volumes à hauteur et usage distincts, et non d'une simple barre résidentielle supplémentaire. »

**Précaution :** la destination exacte de toutes les pièces de l'aile et sa géométrie métrique ne sont pas vérifiées. Le qualificatif « restaurant / services » est donc provisoire.

## Correctifs effectués dans Moteur3D

Fichier : `experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`.

| Volume | Coordonnées dans la scène (mètres, estimations) | Contrôle |
|---|---|---|
| Palais Energetik | X=−50,5…30,5, Z=−43,4…−18,8 | Volume inchangé |
| Hôtel Polissya principal | X=82…104, Z=−12…26 | Façade orientée selon la rotation 90° déjà définie |
| **Aile basse attenante** | **X=79,2…99,5 ; Z=−28,5…−11,8 ; H=4,10** | **Nouveau module : toit, garde-corps de toiture, baies vitrées, bandeau, entrée et jonction** |
| Accès aile basse → hôtel | X=83…87,55 au joint Z≈−12 | Aligné sur le trou de porte qui existait déjà dans le mur latéral de l'hôtel |
| Auvent de l'hôtel | façade ouest et avance vers la place | Séparé de la galerie, passerelle piétonne préservée |
| Galerie autonome | courbe de Bézier depuis le côté Energetik jusqu'à devant Polissya | Pas de pont maçonné, profil bombé, extrémités arrondies |
| Immeuble parasite `east-south` | X=86…100, Z=−37…−12, sept étages | **Supprimé** |
| Immeuble non sourcé `east-north` | X=47…60, Z=18…50, six étages | **Supprimé** |

**Point clé :** le passage de l'aile vers l'hôtel est un réel jour dans les panneaux de façade et non une porte peinte devant un mur plein. Les cotes exactes de l'aile restent à recalibrer par photo.

## Caméras de contrôle après la correction

- **Vue 12 — Polissya, aile basse, arcade depuis la place :** rapprochée pour montrer les trois volumes ensemble, leur séparation et l'accès.
- **Vue 13 — Surplomb sur la jonction :** rapprochée pour regarder la forme de la galerie, l'aile basse, l'hôtel et leurs distances.

`scene_pripyat_views.json` est modifié ; les treize vues sont produites par **le véritable rendu CPU du moteur** dans le workflow GitHub Windows. Les test de géométrie bloquent le retour d'un immeuble provisoire à droite, l'absence de l'aile basse ou une collision entre elle et l'arcade.

## Ce qui reste non validé

Le code et les tests prouvent la **cohérence interne** des volumes et leur absence de chevauchement. Ils ne prouvent pas encore la fidélité photographique. Il reste à comparer une nouvelle capture CPU des vues 12/13 aux photos historiques (hauteur et longueur de l'aile, vitrage, porte, point de contact réel), en évitant de faire passer une image imaginée pour un rendu compilé.

## Technique de rendu — DXR n'est pas le shader actuel

Le calcul GPU actuel est un **ray tracer compute HLSL sous Direct3D 11**, avec traversée BVH et repli CPU. **Ce n'est pas DirectX Raytracing (DXR) matériel de D3D12.** La réussite d'un test WARP vérifie le code du compute, non une accélération DXR sur le GPU de l'utilisateur. Une vraie intégration DXR demande une couche D3D12, le contrôle de prise en charge du matériel, des structures d'accélération et un chemin de repli sur les anciens GPU. Elle doit être menée séparément des corrections géométriques.

