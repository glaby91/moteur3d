# Audit luminosité CSE WARP V17

## Lancer

Dans le package Windows complet CSE WARP V17, double-cliquer sur
\`LANCER_BENCHMARK_LUMINOSITE.bat\`.

Le batch démarre \`moteur3d_v17_campaign.exe\` sans ouvrir l'interface. Il utilise
la scène \`benchmark-civic/scene.json\` livrée avec ce package, la caméra
\`v7_pool_fresco\` (repli \`v4_piscine\`, puis première caméra disponible), une
image de 1000 × 680 et les quatre chemins de rendu : DX11, LumaTrace, Vulkan et
CPU direct. Le moteur produit les PNG, les temps de rendu et le résumé système.

## Comparaison d'image

Le script vérifie la présence et la résolution des quatre PNG. Il calcule :
- alpha transparent, alpha partiel et couverture opaque par backend ;
- luminance Rec.709 affichée et linéaire sur les pixels opaques de chaque image ;
- luminance sur le masque commun, constitué des pixels opaques dans les quatre PNG ;
- proportion de pixels avec un canal proche de l'écrêtage.

Le masque commun évite de compter le ciel transparent DX11/LumaTrace comme du noir.
Les résultats ne sont pas une calibration photométrique, mais une comparaison
reproductible des sorties d'image de cette même build, caméra et scène.

## Résultats et envoi GitHub

Le dossier \`benchmark-results\` conserve les images, le rapport JSON/CSV et une
archive ZIP. Si GitHub CLI est installé et connecté au compte ayant les droits
d'écriture (\`gh auth login -h github.com\`), le script clone brièvement la branche
\`benchmark/luminosity-audit-v1\`, puis pousse le dossier d'images/mesures dans :

\`benchmarks/luminosity-audit/runs/<horodatage>/\`

Aucun jeton n'est demandé ni stocké par le script. Si Git ou GitHub CLI manque,
le ZIP reste disponible localement pour être transmis manuellement. Le script ne
modifie ni ne recompile les sources du moteur sur le PC.

## Limites des mesures

Les images proviennent des chemins de rendu hors écran du moteur ; elles mesurent
le contenu produit par les backends, pas la composition swapchain native ou le
profil couleur de l'écran. Les timings CPU et GPU ne sont pas strictement
comparables : CPU direct est un rendu ray tracing à 1 échantillon et le raster
GPU reste un chemin raster.
