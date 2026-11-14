# Audit global du moteur 3D — 5 novembre 2026

## Verdict

Le dépôt réunit un moteur natif de rendu logiciel CPU, un aperçu raster Direct3D 11 et un pipeline d’assets Pripyat/Energetik bien documenté. Il reste expérimental et ne remplace pas Unreal Engine : pas de backend DXR, pas d’éditeur de scène interactif, pas de simulation physique raccordée, et pas de preuve de performance 4K à 100 i/s.

Revue statique du dépôt GitHub, du PR #5 et de la copie locale disponible. Aucun test local n’a été exécuté. Le dernier build Windows réussi avant la correction du cadrage est le [run 42118581553](https://github.com/glaby91/moteur3d/actions/runs/42118581553); le build après correction était encore en cours lors de la rédaction.

## État technique

| Domaine | État | Conclusion |
|---|---|---|
| CPU | Ray tracing par BVH, multithreading par tuiles, mipmaps, ombres solaires, réflexions/réfraction simplifiées, profondeur de champ et post-traitement. | Rendu de référence complet, mais lent : les mesures publiées sont d’environ 4,9 s en 1920×1080 et 4,4 s en 1680×945. La cible 4K/100 i/s n’est pas atteinte. |
| GPU | D3D11 raster pour parcourir la scène, avec éclairage et effets réduits par rapport au CPU. | Ce n’est pas une accélération équivalente du rendu CPU complet. |
| DXR | Le moteur détecte le support matériel D3D12/DXR, puis fixe `dxrBackend=false`. | Aucun backend ray tracing matériel DXR n’est implémenté. |
| Scènes / chargement | Chargeur qui contrôle versions, valeurs finies, limites, BVH, chaînes de textures et budget mémoire estimé de 2 Gio. Sources de scènes JS réexportées vers le format natif. | Architecture lisible; pas d’éditeur interactif, les changements passent par les sources et l’export. |
| Assets | Le PR consigne 26 GLB, 105 PNG 2048², 234 références résolues et 272 primitives UV-texturées. La fresque est un GLB panoramique 2048×930 adapté au ratio 9,5×4,3 m, avec mipmaps natives générées. | Intégration prometteuse; import et revue visuelle Unreal restent à confirmer. |
| Saisons | Matériaux `leaves` et `leavesAutumn`. | Pas encore de quatre saisons ni de bascule de saison pour l’ensemble du vivant. |
| Physique | Interface de backend seulement; `physXAvailable()` retourne indisponible. | Pas de simulation physique active. |
| Tests / CI | Le workflow compile et empaquette Windows. `tests/native_smoke.cpp` existe mais n’est pas enregistré dans CMake/CTest ni exécuté par le workflow. La matrice CI actuelle ne contient que `windows-latest`. | Ajouter les smoke tests et décider si Linux est une plateforme CI supportée; le README promet Windows/Linux alors que le workflow actuel ne vérifie que Windows. |
| Documentation | README principal décrit le D3D11 actuel; des notes historiques décrivent encore un moteur sans backend GPU et des builds anciens. | Réconcilier les documents et marquer les états historiques. |

## Fresque : anomalie corrigée dans la branche

Le panneau est à X=8,12 m, Y=2,00–6,30 m, Z=−41,55 à −32,05 m, sur la face −X du mur de retour situé vers X=8,38 m. La vue 02 pointait depuis +X : le mur masquait donc la fresque. Son angle a été corrigé à −90° et la note de vue mise à jour sur la branche du PR #5. Il faut encore confirmer le rendu en vue 02 dans le nouveau build et vérifier l’import Unreal.

L’archive Windows build107 fournie auparavant précède l’ajout de la fresque texturée; elle ne correspond pas à la dernière branche.

## État Git

Le PR #5 est ouvert et brouillon, vers la branche `pripiat`, et n’est pas fusionné dans `master`. La copie locale examinée n’est pas synchronisée avec la tête GitHub et contient des changements non commités de fauteuil extérieur et de textures. Ces changements locaux n’ont pas été inclus dans les corrections de cadrage.

## Priorités

1. Vérifier la fresque dans le moteur après le build courant, puis dans Unreal.
2. Intégrer les smoke tests; ajouter une CI Linux si cette cible doit être livrée.
3. Clarifier la différence entre rendu CPU, aperçu D3D11 et détection DXR.
4. Finir les variantes saisonnières et la mise à jour des documents de référence.
5. Synchroniser la copie locale avant de publier le fauteuil extérieur ou un nouveau paquet.

## Références

- [PR #5](https://github.com/glaby91/moteur3d/pull/5)
- [Workflow Qt](https://github.com/glaby91/moteur3d/actions/workflows/build-moteur3d-v14-qt.yml)
- [Aperçu Direct3D 11](../experiments/2026-10-30-cote-ete-gt/native_v14/docs/DIRECTX11-GPU-PREVIEW.md)
- [État modules/fonctions (note historique)](../experiments/2026-10-30-cote-ete-gt/native_v14/docs/ETAT-MODULES-ET-FONCTIONS.md)
