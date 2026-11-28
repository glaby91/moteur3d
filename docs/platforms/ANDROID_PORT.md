# CSE WARP V17 — Android ARM64 tablet port

Date: 28 novembre 2026. Branche expérimentale `test/android-vulkan-arm64`, issue de master `43642bdc493b12325c32da081ef6dedc4d9bbf88`. Cible décidée : tablettes Android haut de gamme uniquement. Aucun changement du moteur à ce stade.

## Matériel cible (liste de support visée, non certifiée)

| Tablette | SoC / GPU | Conditions |
|---|---|---|
| REDMAGIC Astra 2 | Snapdragon 8 Elite Gen 5 / Adreno 840 | Référence minimale, variante 12 Go ou supérieure |
| Lenovo Legion Tab Gen 5 | Snapdragon 8 Elite Gen 5 / Adreno 840 | 12 Go ou supérieure |
| OnePlus Pad 4 | Snapdragon 8 Elite Gen 5 / Adreno 840 | Variante 12 Go uniquement |
| Samsung Galaxy Tab S11 Ultra | Dimensity 9400+ / Immortalis-G925 | 12/16 Go, cible alternative à valider séparément |

Liste de cibles issue de l'étude préalable ; références commerciales, configurations et versions de pilotes à confirmer sur appareils réels avant certification. Samsung est une cible alternative, non une preuve d'équivalence de performances avec Adreno 840.

## Configuration minimale de développement

- ARM64 (arm64-v8a), Android 16 (API 36), 12 Go de RAM physique (hors extension virtuelle), stockage nominal 256 Go.
- Snapdragon 8 Elite Gen 5 ou GPU haut de gamme équivalent validé individuellement ; 16 Go recommandés. Écran tablette paysage 8,8 pouces ou plus, 60 Hz minimum, 120 Hz recommandé ; refroidissement adapté à la charge soutenue.
- Vulkan loader et périphérique >= 1.3.204 selon la politique actuelle du moteur. Vérifier version, fonctionnalités, formats, limites et capacités de présentation au démarrage. Vulkan 1.4 optionnel selon capacités, jamais obligatoire. Voir [plan commun](VULKAN_14.md).
- Objectif Full HD interne 1920×1080 à 60 FPS (16,67 ms/frame), textures PBR 2048, scène intégrale, budget mémoire moteur cible <4 Go, aucune réduction silencieuse de qualité. Ce sont des objectifs, pas des mesures acquises.
- Présentation Vulkan native swapchain en priorité ; QImage réservé au diagnostic explicite. Pas de repli CPU silencieux. UI tactile paysage, collisions et navigation communes. Scènes versionnées séparément.

## Plan de portage

1. AND-01 — chaîne indépendante `.github/workflows/build-android-vulkan.yml` à créer sur cette branche : Qt Android ARM64, SDK/NDK/JDK compatibles, CMake/Ninja et glslangValidator hôte ; sélectionner uniquement cibles APK utiles ; aucune modification des workflows Windows/Linux/macOS.
2. AND-02 — adapter CMake v16/v17 avec conditions Android, compilation Vulkan réelle (pas stub), écarter AVX2 ARM64 ; scalaire d'abord, NEON après mesure. Conserver les shaders SPIR-V et le moteur commun.
3. AND-03 — test Vulkan triangle puis scène V17.634 épinglée avec SHA-256 ; déclarer clairement émulateur/logiciel ou GPU physique ; test de capacités et absence de fallback silencieux.
4. AND-04 — surface Android (VK_KHR_android_surface / QWindow Vulkan selon intégration Qt), swapchain, cycle Activity, pause/reprise, orientation, resize, surface lost/device lost ; aucune copie image GPU→CPU en présentation normale.
5. AND-05 — shaders, matériaux, caches et scène dans chemins Android accessibles ; assets APK et stockage privé, extraction contrôlée, téléchargement atomique vérifié, updater scène indépendant, permissions minimales ; aucun lanceur .exe ni dépendance externe au SDK sur tablette.
6. AND-06 — commandes tactiles multipoint, joystick déplacement, caméra, saut, accroupissement, HUD, menus et Pip-Boy ; conserver collisions et logique de scène. Audio Qt AudioSink adapté au cycle de vie Android.
7. AND-07 — APK développeur installable, validation sur les tablettes cibles, contrôle RAM/VRAM, thermique et autonomie, benchmarks froid/fixe/mobile (p50/p95/p99), test prolongé 30 min, qualité visuelle et comparaison de références. Ne pas annoncer compatibilité ni FPS avant mesures.

## Règles de sécurité du projet

Master et chaînes de compilation existantes préservés. Aucun changement de code ni compilation dans le commit d'ouverture. Pas de fusion avant validation explicite. Pas de portage DX11/LumaTrace GPU dans la première phase. Vérifier les différences graphiques et conserver les chemins desktop.

Statut : premier prototype de validation engagé sur cette branche. Le workflow construit le vrai noyau ARM64 et x86_64 (émulateur uniquement), compile les shaders Vulkan d'origine et assemble un APK diagnostic Qt 6.10.3 / NDK r27c avec V17.634 vérifiée. La QA est un workflow réutilisable appelé après la compilation ; elle vérifie triangle, scène PNG, surface native et déplacement tactile de la caméra. Pas de prétention de compatibilité sur tablette ni de parité graphique ou FPS. Le shell Android de validation n'est pas encore l'interface complète V17 (collisions, marche/saut, audio, menus, updater et tests de cycle de vie approfondis restent à porter). Le rendu PNG hors écran est réservé au diagnostic initial ; la présentation interactive utilise la surface Vulkan empruntée à Qt et n'effectue pas de repli QImage silencieux. Les 1024 px de textures et 640×360 du smoke sont des réglages de test, pas le profil final Full HD/2048. Vulkan 1.3.204 reste obligatoire ; un émulateur qui ne l'expose pas doit échouer honnêtement.
