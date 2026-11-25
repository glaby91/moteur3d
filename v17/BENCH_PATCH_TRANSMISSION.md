# Test du correctif de transmission de l'eau

## Lancement

1. Extraire le paquet dans un nouveau dossier, par exemple `V17_PATCH_TRANSMISSION`.
2. Double-cliquer sur `LANCER_BENCH_PATCH_TRANSMISSION.bat`.
3. Laisser le benchmark terminer. Il rend la scene de reference avec DX11, Vulkan, LumaTrace et CPU, puis teste la transmission du materiau d'eau a 0, 1, 50 et 98,5 %.

Le lanceur utilise `moteur3d_v17_campaign.exe` et `audit_luminosite.ps1` inclus dans le paquet. Le script recupere la scene officielle la plus recente et garde chaque execution dans un nouveau sous-dossier de `benchmark-results`.

## Resultats

Le script produit une archive ZIP locale dans `benchmark-results`. Si Git et GitHub CLI (`gh`) sont installes et connectes, il pousse egalement le dossier de resultats dans `benchmarks/luminosity-audit/runs/` sur la branche `benchmark/luminosity-audit-v1`.

Ce test ne modifie pas l'ancien moteur ni les resultats precedents. Pour garder les anciennes scenes de controle intactes, lancer le paquet depuis un dossier separe.
