# Pavillon 01 — scène étalon Moteur3D
Version de spécification : 0.1.0 — 2026-11-09
Statut : **conception initiale**, ni modèle 3D terminé, ni rendu ni build validé.

## Mission
Scène de validation indépendante de Pripiat, exploitable par :
A. ray tracing CPU multithread et compute HLSL (repli CPU);
B. DirectX 12 raster (DXR optionnel);
C. export glTF/GLB puis import Unreal Engine 5.

## Conventions immuables
- Unité mètre; repère droitier, Z vertical, X longueur, Y largeur. À l'import UE5 : convertir en cm et adapter la base/orientation; vérifier les normales et winding.
- Origine (0,0,0) au centre du rez-de-chaussée, niveau fini z=0.
- Emprise principale 18 m (X) × 12 m (Y); toiture z=7 m max; 2 niveaux. Toutes cotes sont **inventées pour le prototype**.
- Identifiants d'assets et d'instances persistants; scènes ne contiennent pas de textures embarquées ni de géométrie dupliquée.
- Textures source 2048×2048 par défaut, résolution ajustable par matériau; mipmaps précalculées en cache non canonique.
- Convention PBR metal-rough : baseColor, normal tangent space, metallic, roughness, occlusion, emissive, alphaMode; colorimétrie sRGB baseColor/emissive, linéaire données.
- Lumières définies par entités de scène, sans éclairage peint dans les albedos.

## Architecture
- Bâtiment moderniste béton clair, vitrages toute hauteur, bois chaleureux, métal sombre, toiture plate avec acrotère.
- RDC : hall double hauteur, séjour, cuisine, sanitaire; étage : mezzanine et bureau. Escalier unique.
- Extérieur : terrasse, bassin peu profond, 4 arbres instanciés, 6 luminaires; vitrages ouvrant sur le jardin.
- Géométrie en modules séparés : fondations, dalle RDC, plancher étage, toit, façade, panneaux de vitrage, cadres, escalier, portes, mobilier.
- Prévoir vitrages réellement transparents et ombres plausibles, sans supposer une physique du verre identique entre backends.

## Ressources
Assets: building shell, glass panels, stairs, railings, furniture, poplar tree prototype, water plane.
Matériaux : concrete_light, glass_clear, wood_warm, metal_dark, plaster_white, water, foliage, emissive_warm.
Textures : sources indépendantes; ne pas versionner des caches GPU spécifiques à une carte.
Instancing : arbres, luminaires, cadres, chaises; mesurer mémoire et draw calls.
LOD : au moins 2 niveaux pour arbres et mobilier, conservation des pivots.

## Lumières et ambiances
- Soleil directionnel, ciel/environnement, spots intérieurs, points extérieurs, panneaux lumineux, émissions décoratives.
- Trois presets : day, sunset, night. Les positions sont identiques pour A et B.
- Exposition, balance des blancs et espace de sortie explicités; comparer les rendus avec tolérances documentées.

## Caméras de référence
C01 exterior_front : façade, verre, ombres.
C02 interior_hall : lumière traversante, double hauteur.
C03 stair : marches, normales, occlusion.
C04 terrace : eau, réflexions, végétation.
C05 night : émissions, éclairage artificiel.
C06 overhead : emprise, instanciation et visibilité.

## Contrats techniques
scene.json : références d'assets, instances, transforms, lumières, caméras et presets.
assets.json : catalogue logique de ressources et canaux PBR.
Les chemins sont relatifs à la racine du projet, jamais à une machine.
Un validateur doit détecter IDs dupliqués, assets manquants, transforms invalides et références cycliques.
Les données doivent rester compatibles avec une migration versionnée.

## Tests d'acceptation (non encore exécutés)
T01 : charge la scène sans modifier/recompiler le cœur.
T02 : aucune géométrie/texture dupliquée entre instances.
T03 : mêmes caméras, unités et positions sur A/B.
T04 : render CPU de référence déterministe.
T05 : compute disponible ou repli CPU explicite.
T06 : DX12 raster, sans obligation de DXR.
T07 : PBR, alpha, normales et mipmaps vérifiés.
T08 : lumières day/sunset/night modifiables sans modifier les assets.
T09 : export glTF/GLB puis import UE5 avec validation des transforms, matériaux et instances.
T10 : benchmark mémoire, chargement, géométrie, rendu, GPU/CPU et temps de frame.
T11 : packaging Windows; Linux pour les backends effectivement compatibles.

## Jalons
P0 : schémas + scène manifeste (présent).
P1 : maillages réels + textures PBR sources (à faire).
P2 : chargeur, caméra et premier rendu CPU (à faire).
P3 : rendu compute et DX12 (à faire).
P4 : export UE5, benchmarks et tests (à faire).

La présente documentation ne constitue pas un moteur compilé ni une scène rendue.
