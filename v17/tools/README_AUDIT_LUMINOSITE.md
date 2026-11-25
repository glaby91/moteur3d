# Audit luminosité CSE WARP V17

## Lancer

Dans le package Windows complet CSE WARP V17, double-cliquer sur
`LANCER_BENCHMARK_LUMINOSITE.bat`.

Le batch récupère le script PowerShell à la volée depuis la branche GitHub
`benchmark/luminosity-audit-v1`, en épinglant le commit utilisé dans le rapport.
Il exécute ensuite l'exécutable moteur déjà installé localement : il ne retélécharge
ni le moteur ni son package. Si GitHub est inaccessible, il utilise la copie locale
du script si elle existe.

Le script distant récupère la dernière scène publiée par `glaby91/moteur3d-scene`
et la place dans
`benchmark-civic`. À la date de cette version, la scène publiée est
`scene-V17.629-42`. Il vérifie ensuite la caméra `v7_pool_fresco` (repli sur
`v4_piscine`, puis première caméra disponible) et lance les rendus DX11,
LumaTrace, Vulkan et CPU direct à 1000 × 680.

Le moteur enregistre lui-même les quatre PNG, les temps de rendu et son résumé.
Le script calcule ensuite les métriques d'image et crée une archive ZIP locale.
Aucune action dans l'interface du moteur n'est nécessaire.

## Comparaison d'image

Le script vérifie la présence et la résolution des quatre PNG. Il calcule :

- alpha transparent, alpha partiel et couverture opaque par backend ;
- luminance Rec.709 affichée et linéaire sur les pixels opaques de chaque image ;
- luminance sur le masque commun, constitué des pixels opaques dans les quatre PNG ;
- proportion de pixels opaques avec un canal proche de l'écrêtage.

Le masque commun évite de compter le ciel transparent DX11/LumaTrace comme du noir.
Les résultats ne sont pas une calibration photométrique, mais une comparaison
reproductible des sorties d'image de cette même build, caméra et scène.

## Résultats et envoi GitHub

Pour créer un nouveau cas de test, ajoute sa scène et ses ressources à la branche,
puis adapte le script distant ; le prochain lancement les récupérera sans rebuild du
moteur. Chaque rapport conserve le commit du script ayant piloté le run.

Le dossier `benchmark-results` conserve les images, le rapport JSON/CSV et une
archive ZIP. Si Git et GitHub CLI sont installés et que GitHub CLI est connecté
avec un compte ayant les droits d'écriture (`gh auth login -h github.com`), le
script pousse les résultats dans la branche
`benchmark/luminosity-audit-v1`, sous :

`benchmarks/luminosity-audit/runs/<horodatage>/`

Aucun jeton n'est demandé ni stocké par le script. Sans GitHub CLI ou sans
connexion, les résultats et le ZIP restent disponibles localement. Le script ne
modifie ni ne recompile les sources du moteur sur le PC.

## Limites des mesures

Les images proviennent des chemins de rendu hors écran du moteur ; elles mesurent
le contenu produit par les backends, pas la composition swapchain native ou le
profil couleur de l'écran. Les timings CPU et GPU ne sont pas strictement
comparables : CPU direct est un rendu ray tracing à 1 échantillon et le raster
GPU reste un chemin raster.


## Mesures par régions et contrôle du script

La mise à jour ajoute un CSV de régions : une mesure plein cadre et une grille normalisée
3 × 3. Pour chaque case, les quatre moteurs sont calculés sur les mêmes pixels opaques,
puis comparés à DX11 dans cette même case. Cela évite le défaut de l'ancien rapport où
une région pouvait être comparée à la moyenne du cadre complet. Le CSV contient la
couverture du masque, la luminance affichée et linéaire moyenne, les percentiles P50/P95
et les écarts relatifs par case.

Le script exécute aussi un auto-contrôle synthétique avant de finaliser le rapport :
images identiques = 0 % d'écart et niveaux de gris 100 contre 50 = -50 % en luminance
affichée. Le résultat est repris dans luminosity_audit.json.

La grille est géométrique, pas sémantique : les cellules ne sont pas encore étiquetées
« eau », « fresque » ou « sol ». Le runner actuel pilote une seule scène et une caméra
fixe v7_pool_fresco; il ne permet pas encore de varier automatiquement le nombre de
sources lumineuses, les surfaces réfléchissantes ou le scénario jour/nuit. Les données
de ce run permettront d'identifier les régions où l'écart est le plus fort. Pour attribuer
la cause, il faudra ensuite un lot de scènes contrôlées (source seule, réflecteur seul,
puis combinaison) et un scénario jour/nuit que le moteur peut charger sans modification
manuelle des réglages entre les backends.