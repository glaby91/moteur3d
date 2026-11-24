# État des branches — moteur CSE WARP

Audit de travail du 24 novembre 2026. Ce fichier distingue la branche moteur candidate de la branche par défaut `master` ; les branches historiques et expérimentales sont conservées.

## Repère immédiat

**Branche de travail candidate :** [`codex/v17-engine-benchmarks-readme`](https://github.com/glaby91/moteur3d/tree/codex/v17-engine-benchmarks-readme)

**PR vers `master` :** [#88 — V17.006, benchmarks, documentation et scène V17.623](https://github.com/glaby91/moteur3d/pull/88).

**Base moteur :** [`codex/scene-updater-background`](https://github.com/glaby91/moteur3d/tree/codex/scene-updater-background), qui contient le moteur modulaire, les backends, les contrôles d’exploration et la mise à jour de scène en arrière-plan.

La PR #88 ajoute les corrections de benchmark, la documentation complète et l’intégration de la release de scène V17.623 dans le paquet Windows. Tant qu’elle n’est pas fusionnée, `master` reste la branche par défaut et ne contient pas cette proposition.

## Branches à distinguer

| Branche | Rôle observé | Utilisation |
|---|---|---|
| `master` | Branche par défaut GitHub | Référence publiée du dépôt ; à jour après fusion de la PR #88. |
| `codex/v17-engine-benchmarks-readme` | Branche candidate de la PR #88 | Version complète à relire/compiler avant promotion vers `master`. |
| `codex/scene-updater-background` | Base moteur de cette série | Architecture V17 modulaire et gestionnaire de scène. |
| `official/v17-lumatrace` | Branche nommée comme référence officielle V17 + LumaTrace | Référence historique à consulter ; ne pas supposer qu’elle contient les commits les plus récents. |
| `main` dans `moteur3d-scene` | Branche par défaut du dépôt de scène | Source des scènes publiées, dont la release V17.623. |

Les branches `feature/*`, `codex/*` et versions historiques restent disponibles. Leur nom seul ne suffit pas à indiquer la branche à compiler : vérifier la PR et son statut avant de reprendre le travail.
