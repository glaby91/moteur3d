# Baseline visuelle et performances — Pripyat, build 161

## Mesure observée sur Windows
Capture utilisateur du 7 novembre 2026, scène Pripyat, midi, ray tracing CPU.
- Commit de référence : `d694ed50032e8dd28df08090d28162c7a36fc2e7`
- 4 threads CPU, SIMD x64 standard, temps de rendu 8782,0 ms.
- Post-traitement 35,3 ms, total 8817,4 ms.
- Rayons 7 356 332; impacts 4 492 813; ciel 2 863 519.
- Tests 60 473 143; nœuds BVH 403 249 852; AABB 774 535 428.
- Objets 63 715; triangles 40 912; texels 1 060 696.

## Évaluation visuelle
- Façade Energetik encore trop uniforme et horizontale.
- La place semble rectangulaire, emprise exacte non validée.
- Polissya et logements environnants encore en blockout.
- Conserver l'intérieur, les escaliers, la fresque, le mobilier et les chemins de caméra.
- Comparer les futurs rendus à **même résolution, caméra, heure, précision interne, CPU et nombre de threads**.

## Changements post-baseline
- `m3dBuildEnergetikShell` : enveloppe et tour indépendantes.
- `m3dBuildEnergetikFacade` : colonnade et travées séparées.
- `m3dBuildEnergetikArchitecturalAccents` : entrée, auvent, couronnements et relief.
- `m3dBuildPlaza`, `m3dBuildPolissya`, `m3dBuildHousing`, `m3dBuildConnector` : modules de scène indépendants.

## Vérifications nécessaires
1. Compilation Windows Actions du dernier commit.
2. Contrôle de l'absence de collisions visuelles entre le nouvel auvent et les escaliers.
3. Mesures de positions réelles sur plans et vues satellite avant modification de l'échelle.
4. Rendu CPU à midi et comparaison temps/qualité.
5. Déplacement par ray picking dans la fenêtre Qt non encore implémenté.

**La validation de compilation et de rendu n'est pas acquise par la seule écriture du code.**
