# État des branches — moteur CSE WARP

Audit effectué le 24 novembre 2026. Ce fichier indique le point de départ à utiliser pour éviter de reprendre une branche ancienne par erreur. Les branches non retenues restent conservées.

## Repère immédiat

**Branche de travail active :** [`codex/scene-updater-background`](https://github.com/glaby91/moteur3d/tree/codex/scene-updater-background)  
**PR en cours :** [#87 — LumaTrace et mise à jour de scène non bloquante](https://github.com/glaby91/moteur3d/pull/87) — brouillon, non fusionnée.  
**Branche de base de cette PR :** [`feature/v17-604-lumatrace-one-bounce-cache`](https://github.com/glaby91/moteur3d/tree/feature/v17-604-lumatrace-one-bounce-cache).

Pour continuer le travail contenu dans la PR #87, partir de `codex/scene-updater-background`. Elle porte les commits les plus récents de cette série (dernier commit observé : 24 novembre 2026, 20:24 UTC).

## Rôle des branches à ne pas confondre

| Branche | Rôle observé | Utilisation |
|---|---|---|
| `master` | Branche par défaut GitHub ; README et documentation générale | Référence de dépôt, pas le travail moteur V17 le plus récent |
| `official/v17-lumatrace` | Branche nommée comme référence officielle V17 + LumaTrace | Référence à consulter ; dernier commit observé le 22 novembre 2026. Elle ne contient pas les commits récents de la PR #87 |
| `codex/scene-updater-background` | Tête de la PR brouillon #87, basée sur `feature/v17-604-lumatrace-one-bounce-cache` | **Branche de travail active pour la série LumaTrace / mise à jour de scène** |
| `feature/v17-604-lumatrace-one-bounce-cache` | Branche de base de la PR #87, avec cache de rebond LumaTrace | Base de la série #87, pas sa dernière tête |
| `feature/v17-diagnostic-dashboard-tab-20261123` | Tête de la PR brouillon #85, indépendante et non fusionnée | Travail parallèle en attente ; ne pas la prendre pour la tête active de la série #87 |
| `main` dans `moteur3d-scene` | Branche par défaut du dépôt de scène ; pointe sur V17.623 | Source actuelle des données de scène |

## Dépôts examinés

- [`glaby91/moteur3d`](https://github.com/glaby91/moteur3d) : branche par défaut `master`, **149 branches** observées sur deux pages, nombreuses branches historiques, expérimentales et de versions empilées.
- [`glaby91/moteur3d-scene`](https://github.com/glaby91/moteur3d-scene) : branche par défaut `main`, **4 branches**. Aucun PR ouvert lors de l’audit. `main` et `codex/v17-623-glass-mosaic` pointent actuellement sur le même commit ; `main` reste la source à suivre.
- La recherche des dépôts `moteur3d` du compte `glaby91` a retourné ces deux dépôts : moteur et scène.

## Règle de travail

1. Pour poursuivre la PR #87, reprendre `codex/scene-updater-background`.
2. Pour une autre modification du moteur, vérifier d’abord sa branche de base avec le propriétaire du travail ou une PR récente ; ne pas partir automatiquement de `master` ni d’une vieille branche nommée `feature/*`.
3. Pour modifier les données de scène, utiliser `glaby91/moteur3d-scene` sur `main), puis publier la scène depuis ce dépôt.
4. Les branches non actives sont **conservées**. Leur présence ne signifie pas qu’elles sont le point de départ actuel. Aucune branche n’a été supprimée, fermée, renommée ou déplacée pendant cet audit.
5. Mettre à jour ce fichier quand la branche de travail active ou la PR de référence change.

## Limites de l’audit

Les noms, branches par défaut, têtes des PR ouvertes et commits récents ont été consultés. Les 149 branches moteur n’ont pas toutes été relues fichier par fichier ; les branches actives ont été distinguées à partir des PR ouvertes et des commits récents.
