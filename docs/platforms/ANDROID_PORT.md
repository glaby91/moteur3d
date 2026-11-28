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

## Build 5 : audit préventif en quatre passes (avant Build 6)

- P1 — SDK et CMake : SDK/NDK, Qt hôte/cible, SPIR-V et CMake réussis. Qt 6.10.3, Qt6AndroidMacros.cmake, fonction _qt_internal_android_get_sdk_build_tools_revision : définir la variable globale empêche le calcul automatique sans renseigner la valeur de sortie. La propriété de cible QT_ANDROID_SDK_BUILD_TOOLS_REVISION est donc obligatoire dans ce cas. Le template Qt build.gradle ligne 43 confirme le champ en cause. Fixe 36.0.0 et compile SDK 36 (pas extension implicite). Préflight JSON avec fixtures positives/négatives, avant compilation.
- P2 — natif : Build 5 a compilé ET lié moteur3d_v16_core et cse_android_probe pour ARM64 et x86_64 ; aucun changement du noyau C++ ni shader. Contrôle du vrai ABI et des bibliothèques dans l'APK.
- P3 — packaging : utilise le chemin APK exact issu de Ninja, diagnostic verbose androiddeployqt, contrôle du manifeste, de l'ABI, du debug/signature et des SHA-256 de tous les assets indexés (scène originale et SPIR-V). Sauvegarde les paramètres de déploiement et rapport Gradle.
- P4 — QA : bootstrap explicite du SDK sur le runner séparé, KVM, AVD générique + résolution tablette forcée, SwiftShader logiciel (pas Adreno), journal persistant, vérification PNG et absence de readback dans la présentation native. 4 Gio de RAM émulateur par choix de CI ; ne représente pas le matériel cible 12 Gio. Workflow QA appelé seulement avec les deux APK réussis.
Contrôles statiques avant push, exécution du préflight et de la QA à venir : pas de prétention de simulation réelle ni de compatibilité matérielle.

## Build 6 : APK validés, émulateur incompatible avec la politique stricte

Run 46143932536 : ARM64 et x86_64 compilés, signés, assets vérifiés. APK ARM64 15263215651, x86_64 15262469916. Installation et extraction réussies sous Android 16. Échec avant triangle : GPU SwiftShader Device (Subzero), type CPU 4, API 1.3.0 (4206592), loader invité 1.4.0 (4210688). Source : vkjson.txt, artifact 15261909958 et logs de simulation. La limite du GPU, et non du SDK, déclenche le refus volontaire du moteur. Contrôles du diagnostic : 2048 couches, 16384 taille image, 65536 UBO, 64 samplers, anisotropie disponible, queue graphique et timestamps, heap 2 Gio. Pas de validation de surface ni de rendu à ce stade.

Build suivant : exception STRICTEMENT réservée au binaire Android x86_64 diagnostic, au CPU SwiftShader nommé exactement comme dans le rapport, avec opt-in logiciel explicite et API exactement 1.3.0. Aucun changement du minimum 1.3.204 pour ARM64, desktop, ou loader. Aucun contournement de capacités Vulkan ni repli CPU du moteur. Test C++ isolé de 40 assertions sur les combinaisons de version et opt-in (exercé localement avec encodage Vulkan équivalent, puis vérifié avec les vrais headers NDK en CI). Log du périphérique sélectionné, suivi des tests triangle, scène, swapchain sans readback et tactile. L'exception de diagnostic n'est ni une validation de tablette ni une preuve de performance.

## Build 8 — validation de l'exploration
Build 7 entièrement vert (run 46144977522). Extension limitée au frontend Android : demande de paysage via template Qt, navigation tactile multipoint (gauche joystick virtuel, droite rotation, bas droite accroupissement et saut), réutilisation directe du contrôleur marche/collisions V17 et de sa suite de tests desktop dans l'APK x86_64. Les zones tactiles ne sont pas encore dessinées ; pas de HUD complet. Le contrôle du mouvement réel et la rotation sont requis en émulation, saut/accroupissement/collisions sont testés par la suite physique déterministe, pas encore intégralement via interactions UI. Le binaire ARM64 saute les exports diagnostiques au démarrage. MSAA1 et textures 1024 conservés pour les comparaisons, sans certification du profil final.
Cinq vues communes Android SwiftShader / Linux Lavapipe (preset initial et ANO-011 piscine, ANO-005 fresque, ANO-007 vitrage, ANO-013 éclairage) avec pose et FOV du catalogue. Images côte à côte et écarts RGB, pas de seuil arbitraire ni de déclaration de conformité avant revue. Le workflow Linux existant n'est pas modifié : référence isolée dans la campagne Android. Le nouveau run doit valider effectivement le code.

## Build 9 — interface tactile
HUD natif Android Canvas transparent, composé au-dessus de la surface Vulkan ; aucun readback de l'image 3D. Zones tactiles dessinées (joystick, regard, saut, accroupissement), HUD masquable, télémétrie réelle (dimensions du rendu, ms/image et FPS de la cadence ARM64), menu d'échelle 100/75/50 %, MSAA 1/2/4 (valeur effective affichée), caméra suivante, sensibilité, réglages persistants en ARM64. La swapchain reste à la résolution native ; seul le framebuffer Android est redimensionné, blit GPU existant. Le noyau desktop ne reçoit aucun changement actif (gardes __ANDROID__). Le cycle émulateur reste à la demande et ne représente pas les FPS d'une tablette. Tests réels ADB sur les boutons, rendu effectif des dimensions GPU, mise en arrière-plan et reprise, cinq vues de parité conservées. Le HUD Android est une interface personnalisée, pas encore une interface accessible TalkBack. Le profil physique 12 Go, les performances thermiques et la compatibilité des différentes dalles restent à mesurer sur tablettes.

## Build 9 — diagnostic de l'échec de navigation
Run 46148501618 : les deux APK et la référence Linux compilent. Les cinq vues, le triangle, la régression physique et la présentation native passent. Capture réelle : HUD et commandes visibles en 1920x1080. SwiftShader met environ 14 à 23 secondes par frame native ; deux marqueurs rotation arrivent avant expiration du test WALK_MOVE. Aucun marqueur mouvement ni échec Vulkan explicite. Hypothèse vérifiable : le rendu synchrone bloque la livraison des événements tactiles, aggravée par l'enchaînement des gestes.
Build 10 : dissociation de l'entrée tactile et de la présentation pendant les gestes dans le diagnostic x86_64 (rendu final à la relâche), remise à zéro des IDs en début et fin de séquence, logs début/fin de geste et déplacement horizontal, test d'un recul depuis le monument avec une exigence réelle >5 cm. ARM64 garde les frames en continu. Validation UI et graphisme encore à exécuter ; pas de réduction de la qualité native.
