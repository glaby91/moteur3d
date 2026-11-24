# Versioning CSE WARP V17

Le numéro moteur, les révisions des modules de rendu, la révision de scène et le numéro du build CI sont des identifiants distincts.

- Version moteur : `engine.major` et `engine.build` dans `v17/version.json`. La série repart de `V17.000`; la révision courante de cette branche est `V17.006`.
- Modules : leurs versions indépendantes sont suivies dans `engine/render_modules.json`. Une modification ciblée incrémente le module concerné.
- Scène : la scène civique actuelle est `V17.613`, publiée dans le dépôt séparé `glaby91/moteur3d-scene`. La valeur de la release, issue du manifeste livré avec l'archive, fait foi.
- Build CI : le numéro GitHub Actions identifie une exécution, pas une version du moteur.
- Scène de diagnostic historique : le workflow `v17-493-vulkan-complete.yml` assemble encore une base `V17.499` avec les overlays `V17.500` et `V17.523`. Son artefact est explicitement étiqueté comme diagnostic et ne représente pas le package officiel V17.006 / scène V17.613.

Les versions affichées par le produit conservent les zéros initiaux. Incrémenter le moteur pour une révision globale, le module concerné pour une évolution ciblée, et la scène uniquement lorsqu'elle change.
