# CSE WARP Beta X — V17.428, scène et gravure corrigées

## Archive livrée
- Exécutable natif Windows V17.427 + scène V17.428, avec toutes les ressources de V5/V6 préservées.
- CI moteur source compilé et testé : https://github.com/glaby91/moteur3d/actions/runs/43326184870 — SUCCESS.
- CI scène finale V17.428, **sans nouvelle compilation Qt** : https://github.com/glaby91/moteur3d/actions/runs/43327897751 — SUCCESS.
- Artifact : `13992504591` / `CSE-WARP-V17.428-V7-GRAVURE-CORRIGEE-Windows-x64`.
- ZIP vérifié : `CSE-WARP-V17.428-CORRECTIONS-GLOBALES-Windows-x64.zip`, 102069384 octets, 256 entrées, CRC OK, SHA-256 `0a831d255c1714059889849ec3a6f3d1f874a0cf8c94a7bb32eeafc43138670b`.

## Contrôles exacts
- 24 sphères rouges/ocres retirées sans toucher aux fleurs GLB.
- Butte 3,2m, assise élargie, socle incrusté de 38 cm, inscription FR/UK séparée (8 tuiles natives 512px) ; la seconde ligne est remontée de 48px pour dégager la lèvre de la butte.
- Réduction des lampadaires de 12 à 10, ajout de 10 lumières de place et 12 sources au bandeau supérieur de façade avec diffuseurs montés ; extinction/activation progressive au crépuscule dans le moteur C++.
- 16 baies et 22 panneaux de vitrage haut ajoutés, 468 montants/traverses métalliques, deux passages latéraux ouverts au rez-de-chaussée.
- Suppression de l'ancien escalier de piscine, des flights expérimentaux et de 154 pièces de plafonniers et réglettes détachés, fermeture des trémies. Fresque visible sans galerie transverse ni fanions.
- Correction du biais chromatique brun nocturne dans le shader DX11, ciel bleu-gris au lieu de sépia.
- Eau moins opaque en DX11 ; **vraie réfraction physique non implémentée**.
- Rendus DX11 WARP réels à 640×360 (proche) et 1366×768 (départ) après correction finale.

## Limitations et suites
Les tests exécutés sur GitHub Windows avec GPU logiciel WARP ne prouvent ni les FPS Intel HD620 ni la fiabilité de la navigation et des collisions en temps réel sur le PC de l'utilisateur. Les lumières et la visibilité des inscriptions doivent être jugées à l'écran natif ; la qualité de réfraction de l'eau et les derniers détails historiques du projet restent à confirmer. Ne pas qualifier les 60 points historiques de tous résolus.

## Lancement
Extraire intégralement le ZIP dans un dossier distinct, puis lancer `DEMARRER_CSE_WARP_V7.bat`. Le programme ouvrira `benchmark-civic/scene-v7-corrections.json` portant le nom de scène V17.428.
