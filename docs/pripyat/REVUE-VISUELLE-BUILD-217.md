# Revue des dix captures de la build Windows 217
Référence : https://github.com/glaby91/moteur3d/actions/runs/42360474089
Artifact: `Pripyat-Architecture-Inspection-217` (10 PNG, 640 × 360).
L'examen ci-dessous porte sur de **vraies captures calculées** et non des images imaginées.

| Vue | Observation visuelle | Décision |
|---|---|---|
| 01, façade de jour | Palais visible en largeur, arbres et place au premier plan ; bâtiment relativement lointain | Conserver comme vue d'ensemble et ajouter plus tard un cadrage photo apparié |
| 02, fresque | Fresque complète sur un bandeau peint, piliers visibles en partie basse ; pas de dalle traversante masquant son centre | **Correction confirmée visuellement sur build 217** |
| 03, grand escalier | Marches, palier, garde-corps et murs texturés présents | Contrôler ensuite la hauteur des marches sur plan coté |
| 04, auditorium | Rangées de sièges et écran/scène visibles ; salle encore simplifiée | Géométrie fonctionnelle, fidélité historique non certifiée |
| 05, mezzanine | Fresque visible latéralement ; piliers, galerie et éclairages présents | Garder comme vue secondaire de raccordement |
| 06, salle de club | Vue surtout vide (plafond, mur, fenêtre), non exploitable pour qualification de la distribution intérieure | Cadrage déplacé vers la pièce réellement modélisée dans l'aile est (commit ultérieur) |
| 07, cage d'escalier | Volée complète visible depuis le bas, mais cadrage très axial | Conserver pour contrôle de montée |
| 08, sortie vers le toit | Ouverture vers l'extérieur visible | Vérifier navigation et collision de la porte |
| 09, toiture | Ouverture de l'atrium et enseigne visibles | Préserver l'ajourage du toit, éviter le retour d'un bloc plein |
| 10, façade de nuit | Bâtiment et enseigne visibles dans l'éclairage nocturne | Conserver une vue nocturne de contrôle |

## Modifications depuis la build 217
- Test automatique de trois lignes de visée vers la fresque (`test-energetik-sightlines.mjs`) ;
- hauteur des volumes de toiture rattachée au datum ;
- correction du cadrage de la vue 06 ;
- déplacement provisoire d'un immeuble de contexte initialement superposé au Polissya ;
- test automatique des emprises des bâtiments.

**Attention :** ces changements ont été commités après la build 217. Cette revue prouve les observations de la 217 uniquement ; les vues des builds suivantes nécessitent leur propre inspection.
