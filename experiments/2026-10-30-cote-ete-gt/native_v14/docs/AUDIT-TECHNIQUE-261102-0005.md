# Audit technique — build 261102-0005

**Date :** 2 novembre 2026  
**Moteur :** V13.14, rendu natif C++ x64  
**Branche :** `experiment/manhattan-silent-hill-2026-10-30`  
**Révision auditée :** `250acab0c74bb534b88a225c2c66634d8330063b`

## Résumé

Le build est versionné sur GitHub. Les workflows GitHub Actions ont compilé avec succès l’application Win32 native x64 ainsi que l’application Qt sous Windows et Linux. Le workflow natif a aussi exporté la scène Rapture, chargé le binaire et exécuté son smoke test de rendu.

Le moteur est un **moteur de rendu logiciel fondé sur le lancer de rayons CPU** : ce n’est pas un moteur GPU. Sa structure modulaire, le format de scène V4, les matériaux PBR partiels, les sources locales et l’interface de chargement de scènes constituent les évolutions principales. La scène Rapture est utilisable, mais son rendu actuel reste une étude géométrique stylisée; son amélioration visuelle est reportée à la prochaine session.

## Évolutions récentes

- **Code séparé par fonction :** chargement des scènes, éclairage, intersections/BVH, matériaux, rendu, post-traitement et interfaces sont maintenus dans des modules C++. `assemble_native.js` recrée de manière déterministe le fichier source compilé par MSVC.
- **Deux interfaces :** l’interface Win32 native est conservée; l’interface Qt Widgets partage le moteur et se compile sous Windows et Linux.
- **Scènes ouvrables depuis l’application :** le menu Scène charge les fichiers `.bin`. Le chargeur accepte les formats V3 et V4 et conserve la scène active si le nouveau fichier est invalide.
- **Format V4 enrichi :** réflexion/transmission/indice de réfraction par matériau, chaînes mip RGB565 et cartes couleur, normale et rugosité; jusqu’à 256 lumières ponctuelles par scène.
- **Ombres et matériaux :** ombres solaires par carte de profondeur 1024 × 1024 avec filtre PCF 5 × 5; bump procédural; réflexion secondaire à un rebond; transparence avec Fresnel et réfraction.
- **Réglages d’image :** sorties jusqu’à 4K, échelles de calcul réduites, FSAA 16×, lissage adaptatif des contours, DOF, brouillard et bloom optionnels.
- **Calcul parallèle et affichage :** rendu CPU par tuiles avec 1 à 32 threads réglables; l’image complète est composée dans un tampon avant affichage. L’aperçu rotatif ouvre une fenêtre de 800 × 600 calculée en 400 × 300.
- **Provenance :** la révision Git, la branche et le numéro du build sont visibles dans l’application et les métadonnées Windows.

## Fonctions disponibles et limites

| Domaine | Fonction présente | Limite actuelle |
|---|---|---|
| Géométrie | Triangles, boîtes, boîtes arrondies, capsules, cônes et ellipsoïdes; normales lissées et UV sur les triangles | Pas d’éditeur de scène interactif; les changements se font dans les sources JS puis sont réexportés |
| Accélération | BVH exporté, validé au chargement et parcouru pour les rayons | Le plafond de validation de 4,2 millions d’objets est une limite de format, pas une garantie de temps réel ou de débit |
| Profondeur | Le rayon le plus proche détermine la visibilité dans la vue principale | Pas de Z-buffer raster traditionnel; la carte de profondeur séparée sert aux ombres |
| Éclairage | Soleil, ciel, éclairage ambiant et jusqu’à 256 lumières ponctuelles locales | Les lumières ponctuelles locales ne projettent pas d’ombres; la carte d’ombres concerne le soleil |
| Matériaux | Couleur, normal map, roughness map, bump procédural, réflexion et transmission | Ce n’est pas un pipeline PBR complet : pas d’illumination globale, d’occlusion ambiante ni de pipeline HDR |
| Reflets et verre | Réflexion secondaire à un rebond; Fresnel-Schlick, réfraction et parcours jusqu’à huit couches transmissives | Le verre est modélisé comme une surface mince; pas de volumes épais, dispersion ni absorption spectrale |
| Antialiasing | FSAA 16× et filtre de contours à quatre voisins indépendant des modes d’échelle | FSAA multiplie par 16 les rayons primaires par pixel et peut devenir très lent, surtout en 4K |
| Parallélisme | Tuiles CPU et choix de 1 à 32 threads | Le réglage ne détecte pas encore la capacité optimale du processeur; 32 threads peuvent le surabonner |
| Physique | Contrat d’interface prévu pour un backend physique | PhysX n’est ni lié ni exécuté dans ce build |
| Animation | Aperçu rotatif de caméra à basse résolution | Pas d’animation d’objets ni de flou de mouvement temporel |

Le lissage des contours, le bloom et les réflexions sont calculés sur CPU. La profondeur de champ consomme 16 échantillons par pixel; elle partage ce budget avec le FSAA lorsqu’il est activé.

## Scène Rapture

L’export vérifié contient **207 661 objets**, **148 203 triangles**, **107 855 nœuds BVH** et **42 lumières locales**. Il comprend les volumes Art déco, des tuyauteries, garde-corps, kiosques vitrés, jardinières marines et éclairages. Les matériaux pierre, bronze et verre utilisent des cartes couleur, normale et rugosité; le verre est transmissif.

L’inspection de la capture du build 0005 confirme le chargement et l’éclairage de la scène, et met en évidence trois priorités visuelles pour la suite :

1. remplacer la bathysphère trop sphérique et peu détaillée;
2. réduire la dominance des garde-corps et tuyaux dans l’axe de la caméra;
3. casser la répétition des panneaux et carreaux, et renforcer la lecture des vitrages vers l’extérieur sous-marin.

Ces changements de scène ne sont pas inclus dans ce build.

## Vérifications réalisées

- Export natif Rapture réussi : 207 661 objets, 148 203 triangles et 42 lumières.
- Smoke test du chargeur et du rendu natif réussi dans GitHub Actions.
- Compilation Win32 native x64 réussie : [run 78](https://github.com/glaby91/moteur3d/actions/runs/41378399153).
- Compilation et packaging Qt Windows et Linux réussis : [run 13](https://github.com/glaby91/moteur3d/actions/runs/41378399104).
- `git diff --check` ne signale pas d’erreur d’espacement.

Le premier passage CI avait détecté un cas manquant dans l’exporteur quand une scène ne déclarait pas de table de transmission. Le repli vers une table vide a été ajouté; le commit final audité passe les deux workflows.

## Suite proposée

La prochaine session peut améliorer la composition et les formes de la scène sans modifier les fonctions du moteur. Les évolutions moteur à plus long terme sont de mesurer le rendu sur des processeurs différents, puis d’évaluer séparément les ombres des lumières locales, un matériau PBR plus complet et une vraie animation d’objets.
