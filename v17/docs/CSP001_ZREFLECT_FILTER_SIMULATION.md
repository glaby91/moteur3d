# Z-Reflect R001 — simulation du catalogue restrictif, 27/11/2026
Source : archive V17.605 du dépôt scène, commit b01abf27cf96ee47867918a513c9f71a7222a976, base de V17.629. ZIP final 629 non accessible ici. Source code rev2 : e81f34d62103715fb313f2f5c87518b4022bc782. Aucun C++ compilé, aucun workflow. Simulation indépendante JS à partir des véritables instances, matériaux, géométrie des boîtes générée selon box() et trois maillages de coupole, 29 positions de caméra. Rugosité nominale, pas les mips. Les objets non autorisés explicitement restent des candidats hypothétiques ; aucun slot actif GPU. Les seuils expérimentaux sont dans m3d_zreflect_policy.h ; aucun plafond appliqué.

## Comptages
- 5260 objets ; ancien tri 1083 ; filtre optique 125 ; filtre géométrique 121 (120 vitrages + 1 piscine).
- 4 refus géométriques : v439_glazing_segment_2_1, ratio de faces parallèles 87,86 % <90 %, et trois vantaux courbes de coupole (épaisseur 3,39 à 4,04 m, ratio parallèle 12,30 à 18,06 %). Cette restriction est modifiable et ne change pas le PBR.
- Première simulation : 8 groupes, dont doublons aux frontières d'arrondi de la quantification. Correction : rechercher les cases voisines, vérifier la coplanarité et le côté. 5 plans distincts, 1 mm de tolérance. Les groupes séparés de 5 mm ne fusionnent pas. Les faces de la même vitre restent distinctes, seul le dessus de l'eau est retenu.
- Recalcul final : fusion des groupes de faces réellement coplanaires à partir de leurs aires projetées simulées (arrondies à 0,01 point de pourcentage ; aucun résultat à moins de 0,02 point du seuil 2%). 1280x720, FOV horizontal, face côté observateur, clip frustum, sans occlusion, somme des aires (peut surestimer en cas de chevauchement).

| Plan | Offset m | Objets | Aire m² |
|---|---:|---:|---:|
| Verrière + | -3.9875 | 112 | 1108.44 |
| Verrière − | -4.0725 | 112 | 1108.44 |
| Eau + | 0.1925 | 1 | 1246.00 |
| Raccords + | -4.0025 | 8 | 71.64 |
| Raccords − | -4.0675 | 8 | 71.64 |

## Distribution sans plafond
- 0 plan(s) : 7 vues
- 1 plan(s) : 14 vues
- 2 plan(s) : 8 vues
Moyenne 1.0345 plans/vues, total 30 sélections sur 29. Aucun résultat ne peut prouver un gain GPU. Cinq plans globalement disponibles ne signifient pas cinq passes simultanées.
 
| Vue | Verrière + % | Verrière - % | Eau % | Raccord + % | Raccord - % | Sélection |
|---|---:|---:|---:|---:|---:|---:|
| v4_face | 14.19 | 0.00 | 0.16 | 0.92 | 0.00 | 1 |
| v4_place | 6.25 | 0.00 | 0.23 | 0.16 | 0.00 | 1 |
| v4_monument | 8.66 | 0.00 | 0.39 | 0.27 | 0.00 | 1 |
| v4_fleurs | 9.30 | 0.00 | 0.21 | 0.20 | 0.00 | 1 |
| v4_arbres | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0 |
| v4_aerienne | 3.40 | 0.00 | 1.32 | 0.25 | 0.00 | 1 |
| v4_piscine | 0.00 | 0.00 | 21.00 | 0.00 | 0.00 | 1 |
| v4_hall | 0.00 | 0.00 | 6.40 | 0.00 | 0.00 | 1 |
| v6_monument_facade | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0 |
| v6_monument_front | 19.15 | 0.00 | 0.52 | 0.49 | 0.00 | 1 |
| v6_monument_contrechamp | 26.60 | 0.00 | 0.68 | 0.00 | 0.00 | 1 |
| v7_pool_fresco | 0.00 | 0.00 | 17.53 | 0.00 | 0.00 | 1 |
| v7_curtain_wall_detail | 71.14 | 0.00 | 4.80 | 0.00 | 0.00 | 2 |
| v7_pool_stairs_removed | 0.00 | 26.83 | 13.33 | 0.00 | 0.38 | 2 |
| v7_east_gallery_closed | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0 |
| v7_side_entry_open | 47.07 | 0.00 | 1.83 | 0.00 | 0.00 | 1 |
| v8_three_level_stairs | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0 |
| v8_pool_blocks | 0.00 | 0.00 | 29.56 | 0.00 | 0.00 | 1 |
| v8_ladder_stainless | 0.00 | 0.00 | 21.92 | 0.00 | 0.00 | 1 |
| v8_skylight_lighting | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0 |
| v8_glazing_raccords | 65.58 | 0.00 | 3.21 | 0.00 | 0.00 | 2 |
| v8_stair_first_landing | 0.00 | 27.43 | 1.15 | 0.00 | 2.17 | 2 |
| v8_stair_second_landing | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0 |
| v523_cupola_exterieure | 6.00 | 0.00 | 7.48 | 0.16 | 0.00 | 2 |
| v523_cupola_arriere | 0.00 | 9.32 | 6.60 | 0.00 | 0.33 | 2 |
| v523_cupola_interieure | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0 |
| v600_garden_view | 3.79 | 0.00 | 0.57 | 0.24 | 0.00 | 1 |
| v604_mosaic_left | 0.00 | 17.97 | 0.00 | 0.00 | 3.37 | 2 |
| v604_mosaic_right | 0.00 | 16.67 | 0.00 | 0.00 | 4.64 | 2 |

Tests de non-régression C++ écrits (non compilés) : matériau, autorisation, plaque, projection, surface petite, surface pliée, frontière d'arrondi et plans distincts. Prochaine étape : compiler sur autorisation, produire le catalogue réel V17.629 et confronter les compteurs. R03–R09 multi-plan GPU, cache, table lumineuse et politique auteur runtime restent à intégrer. Le plafond de trois est conservé comme paramètre futur mais non appliqué dans cet essai.
