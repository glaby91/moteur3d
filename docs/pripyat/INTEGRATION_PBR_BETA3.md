# Moteur3D V15.0 Beta 3 — Intégration des matériaux Energetik

## Implémenté
- Générateur reproductible de 31 textures 2048² avec 12 mipmaps (albedo, normales, rugosité), format M3DPBR1 gzip.
- Export Windows GitHub Actions : génération des textures avant l'export de scene_pripyat_central.bin ; inclusion des fichiers dans le paquet Qt.
- Exporteur : sélection des 31 matériaux et sérialisation des trois canaux dans le binaire de scène. La fresque conserve son asset propre.
- Scène : nouveaux identifiants pour les sols du hall, auditorium et aile est, sol de sport, toile et cordages du ring.
- Version : 15.0-beta.3.

## Limites et validations requises
- Les textures sont **procédurales**, inspirées de photographies, mais non extraites ni étalonnées sur les photos.
- Le zoning de certaines pièces demeure hypothétique ; les UV et l'échelle physique des motifs doivent être contrôlés.
- Les matériaux de portes, plafonds, murs, bibliothèque et mobilier sont générés mais **pas tous affectés aux surfaces** : leur couverture est partielle.
- Le rendu CPU et DX11, la consommation mémoire et le succès de compilation doivent être validés avant toute annonce de livrable final.
- Le programme Windows et la scène sont distribués ensemble uniquement après réussite du workflow.

## Construction
Le workflow `.github/workflows/build-moteur3d-v14-qt.yml` compile Qt pour Windows et publie un artefact contenant l'exécutable et `scene_pripyat_central.bin`.

Pour générer localement les matériaux :

```powershell
cd experiments/2026-10-30-cote-ete-gt/native_v14
python tools/generate_energetik_room_pbr.py
$env:M3D_ENERGETIK_PBR_DIR = 'assets/pripyat/textures/rooms'
$env:M3D_TEXTURE_SIZE = '2048'
$env:M3D_SKIP_IMPORTED_CAR = '1'
node export_scene.js source_v1314.html scene_pripyat_central.bin scene_pripyat_central.js
```
