# CSE WARP — référence LumaTrace 0/1, circuit intégral HD 620
**Campagne du 26 novembre 2026 — V17.016, build 772, scène V17.629, module 001.**

## Source et reproductibilité
Archive fournie par l'opérateur : `parcours30-20261126-205922068.zip`. Voir [provenance.json](provenance.json) (empreintes SHA-256, révisions, matériel). Le commit testé est le merge CI `c3c2e23c706c09159bf5855ede030f196671abd1`, dont le parent benchmark est `361ce0f229ca0f09db5b801158c527f2186f66f4`. La scène réellement téléchargée est V17.629, et non V17.604 indiqué par les métadonnées du package. L'exécutable, le pilote et les conditions sont identifiés. L'archive brute complète n'est pas copiée ici : le CSV des 522 comptages d'images et les agrégats ci-dessous sont une extraction du ZIP original, pas une reproduction octet pour octet.

Dell Vostro 15-3568, i5-7200U (2 cœurs/4 threads), Intel HD Graphics 620, 8 Gio DDR4 (2 x 4 Gio, 2133 MT/s configurés), Windows 10 Pro 19045, pilote 31.0.101.2145. 1280 × 720, swapchain native, VSync demandée OFF et déclarée OFF, 0 buffer de readback, geometry prep et sondes ON. 29 caméras dans l'ordre source, balayage ±12°, 6 s chacune. Deux paires OFF/ON par horaire fixe, ordre inversé à la seconde répétition; une paire pour la transition (6 paliers de 17 h à 21 h). 18 circuits, 522 lignes OK, 261 paires, 3132 s de fenêtres nominales (52 min 12 s); somme des durées mesurées 3145,57874 s (52 min 25,6 s). Temps d'initialisation hors mesures.

## Résultat global
| Indicateur | OFF | ON |
|---|---:|---:|
| Séquences | 261 | 261 |
| Images présentées dans les fenêtres | 32832 | 33233 |
| FPS (images / 1566 s nominales par mode) | 20,9655 | 21,2216 |
| Moyenne des P50 de présentation | 51,0459 ms | 50,7400 ms |
| Moyenne des P95 de présentation | 56,6343 ms | 56,5306 ms |

**+1,2214 % de FPS**, 401 images supplémentaires; 127 paires améliorées, 40 identiques, 94 dégradées. Total 66 065 images. P50/P95 sont des moyennes de percentiles par fenêtre, pas des percentiles globaux ni des timestamps GPU. Le gain est modeste, comparable à la variabilité des circuits.

| Horaire | FPS OFF | FPS ON | Gain | Budget 0 ON moyen | P95 OFF / ON (ms) |
|---|---:|---:|---:|---:|---:|
| 08 h | 21,713 | 22,486 | +3,56 % | 93,01 % | 55,89 / 54,84 |
| 14 h 45 | 22,152 | 22,172 | +0,09 % | 93,01 % | 54,07 / 56,84 |
| 19 h | 19,865 | 20,391 | +2,65 % | 10,58 % | 59,33 / 56,91 |
| 23 h | 20,428 | 20,391 | -0,18 % | 10,58 % | 57,42 / 56,93 |
| Transition | 20,374 | 20,115 | -1,27 % | 24,72 % | 56,29 / 57,73 |

Le budget 0 concerne les groupes certifiés sans radiance locale, non la totalité des opérations de rendu. Sur 1 311 273 triangles, environ 1,22 million sont classés à zéro en journée, mais le cache de radiance limite le travail effectivement évitable sur les images suivantes. Le masque a aussi un coût. Ce test ne mesure pas séparément ces coûts.

## Zones critiques
Moyennes ON sur neuf fenêtres par caméra : premier palier 13,61 FPS, aérienne 13,87, monument face 14,35, façade 14,69, jardin 14,69, place 15,43. À l'opposé : galerie Est 35,07, deuxième palier 31,81, hall 31,22, échelle inox 29,28. Piscine/fresque 26,00 FPS. Les vues extérieures sont souvent plus coûteuses, mais le premier palier montre qu'il ne faut pas généraliser. Le benchmark piscine seul n'était pas représentatif.

## Conclusions et décisions proposées
- Conserver le module 001 disponible, OFF par défaut. Ne pas engager les budgets 2/4 avant une mesure plus concluante. 522 lignes OK valident la campagne de performance sur cette machine, pas une parité visuelle de toutes les vues.
- **Priorité P1 — caméra miroir :** le champ `planar_reflection` est vrai sur les 522 lignes, pour 29 caméras. Audit du code au commit benchmark : [m3d_directx11.cpp, condition et passe miroir](https://github.com/glaby91/moteur3d/blob/361ce0f229ca0f09db5b801158c527f2186f66f4/experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_directx11.cpp#L271) ; condition `lumaBvh && waterReflectionPlane_.valid`, [appel drawScene miroir](https://github.com/glaby91/moteur3d/blob/361ce0f229ca0f09db5b801158c527f2186f66f4/experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_directx11.cpp#L803-L820). Cette condition ne vérifie pas la visibilité de la surface d'eau depuis la caméra principale. Étudier un rejet conservatif de la passe miroir quand aucune surface d'eau pertinente ne peut contribuer, sans faux négatif ni changement visuel. Vérifier les cas à la limite du frustum, les vitrages et les vues intérieures/extérieures. Mesurer durée GPU miroir et principale, draw calls et FPS par caméra; ne pas annoncer de gain avant test.
- P2 — profiler géométrie et culling (frustum, occlusion, nombre de triangles effectivement soumis), Z-prepass et shaders. 10 852 731 sommets source, 5 089 120 uniques; ces chiffres sont des compteurs globaux, pas un décompte visible.
- P3 — A/B/C : référence, adaptatif compute seul, compute + raster. Vérifier la parité d'image WARP et sur la scène réelle. Mesurer le coût du vertex shader et des lectures de radiance séparément.
- P4 — séparer les invalidations radiance locale et sondes : la clé actuelle contient le soleil alors que CSShadeTriangles ne le lit pas. Attention, les lampes intérieures dépendent du daylight et les sondes dépendent du soleil et de la radiance; ne pas réutiliser un résultat obsolète.
- P5 — étudier le masque des lampes par groupe seulement après ces mesures; rejet strictement conservatif, même ordre et limite de 64 lampes, pas de perte visuelle.

## Limites statistiques
Les 522 fenêtres sont des mesures corrélées dans 18 circuits, non 522 expériences indépendantes. Les gains des neuf paires de circuits vont de -1,35 % à +4,72 %. Une seule paire de transition, pas de relevés températures/fréquences GPU/CPU; pas de conclusion de significativité. Une campagne longue peut dériver thermiquement. Les P50/P95 ne sont pas des timings GPU isolés. Les comparaisons de transition utilisent des pas solaires discrets, pas un cycle continu. Conserver cette référence et refaire les mêmes tests sur d'autres GPU avant activation par défaut.

## Fichiers
- [frame_counts.csv](frame_counts.csv) : 522 comptages originaux, dans l'ordre d'exécution, FPS recalculables par division par 6.
- [by_camera.csv](by_camera.csv) : 29 moyennes par caméra, OFF/ON, P50/P95 moyens par mode.
- [by_profile.csv](by_profile.csv) : cinq profils, nombres d'images, FPS, gain, latences et budget zéro.
- [by_circuit.csv](by_circuit.csv) : 18 circuits dans l'ordre d'exécution.
- [provenance.json](provenance.json) : identification et empreintes du ZIP/CSV source.

**Ligne directrice CSE WARP : qualité visuelle maximale pour le coût minimal.** Le rendu CPU GI 4 rebonds reste la référence qualitative. Cible HD 620 : 40–45 FPS en 720p, soit 22–25 ms par image. Conserver le parcours complet pour les validations majeures et le parcours court pour les itérations rapides. Aucun changement de moteur, de scène, de version ou d'algorithme dans cet archivage.
