# Palais et piscine — design sobre / scène V17

Date : 12 novembre 2026. **Fiction artistique**, inspirée de l'architecture moderniste soviétique, **non** présentée comme une restitution historique de Pripiat. Les deux œuvres du dossier d'origine ne sont ni remplacées ni altérées.

## Modifications réellement codées

- **Place et arbres** : 26 arbres de premier plan (peupliers élancés, bouleaux clairs) avec tronc, quatre branches et au moins cinq masses de feuillage asymétriques. D'autres sujets sont ajoutés aux profils lourd/extrême, en arrière-plan. Ancien défaut corrigé : les troncs étaient hors cadre, à environ x = ±120 m.
- **Butte ovale** : banc béton conservé avec ses 96 sections ; plusieurs centaines de plantes colorées, avec des tiges et pétales géométriques dans les massifs proches ; ajout d'arbustes au bord des cheminements.
- **Statue** : ancienne silhouette en gros blocs, flambeau et rayons supprimés. Nouveau monument original de deux figures élancées (« Jeunesse » et « Science »), socle commun et petite étoile métallique. Géométrie procédurale en ellipsoïdes et éléments simples ; ce n'est pas une sculpture haute définition. Pas d'attribution à un monument réel.
- **Enseigne et banderoles** : enseigne géométrique ЭНЕРГЕТИК maintenue, désormais sans les grandes orbites de néon et les ailettes ; 16 ampoules discrètes et éclairage chaud. Deux banderoles de toiture en cinq bandes de tissu rouge/écru avec insigne abstrait.
- **Façade** : corniches et pilastres discrets ajoutés, tout en conservant la mosaïque extérieure fournie par l'utilisateur et l'entrée de 10 m.
- **Piscine 50 × 25 m** : eau, dix couloirs, 55 diffuseurs lumineux et galeries existants conservés ; deux lignes de fanions et quatre échelles réelles ajoutées. La fresque Energetik authentique reste sur le mur du hall, au-dessus de son escalier.

## Débit et coût de rendu

Le design s'appuie uniquement sur les instances déjà prises en charge (boîtes, ellipsoïdes et canopées instanciées). Le rendu final n'est **pas** censé ressembler trait pour trait aux images conceptuelles générées par IA. L'architecture principale, les limites d'accès et le budget des 20k/150k/500k triangles par profil restent pilotés par `generate.cjs`. Les nouveaux arbres et plantes augmentent le nombre total d'instances ; FPS à mesurer sur la compilation Windows.

## Contrôles

1. `node scenes/v16/benchmark-civic/generate.cjs --all`
2. `node scenes/v16/benchmark-civic/validate.cjs`
3. GitHub Actions **V17 Windows x64**, chargement de scène, test 25 % CPU/Compute, 11 caméras sur DirectX WARP, piscine 14 h/19 h et simulation totale.
4. Examiner **les images réellement calculées**, disponibles dans l'artefact Total-Simulation-WARP, sous `lighting-ab/civic-camera-views/` ; les photos de conception ne sont pas des rendus du moteur.

## Points de contrôle visuel

- Caméra `passerelle` : présence du duo sculpté, fleurs et banc ovale, sans anciens pics.
- Caméra `facade` : feuillages lisibles sur les côtés, entrée ouverte, mosaïque distincte, enseigne simplifiée.
- Caméra `vue_haut` : deux banderoles, densité et répartition des arbres, absence d'autres immeubles.
- Caméras `bassin` et `piscine_soir` : visibilité des fanions et garde-corps, lumière à 19 h.
- Caméra `hall` : fresque Energetik toujours intacte et au bon étage.

Ce document accompagne la branche `feature/v17-365-paysage-monument-sobre-261112` et doit être révisé après inspection des PNG et mesures de performances.
