# Portage macOS ARM64 — validation expérimentale du 28 novembre 2026

**CSE WARP V17.031, Build macOS 4, scène V17.634.** Validation fonctionnelle CI hors écran du backend Vulkan Raster via MoltenVK. Le portage reste sur `test/macos-metal-vulkan`, sans fusion dans `master`.

- [Source exacte compilée](https://github.com/glaby91/moteur3d/commit/a49db9a2e21e937c16c959c87e3fad11cd722e9b)
- [Build 4 — tests et compilation réussis](https://github.com/glaby91/moteur3d/actions/runs/46094113757)
- [Package expérimental ARM64 (71 093 036 octets, archive Actions)](https://github.com/glaby91/moteur3d/actions/runs/46094113757/artifacts/15243415241)
- [Diagnostics, huit PNG, logs et status.json](https://github.com/glaby91/moteur3d/actions/runs/46094113757/artifacts/15243255647)
- [Workflow](../../.github/workflows/build-macos-vulkan.yml)

Les artefacts Actions sont soumis à rétention. La scène est versionnée séparément ; le Build 4 a embarqué V17.634 avec contrôle SHA-256 de la publication.

## Environnement et versions

| Composant | Valeur |
|---|---|
| Runner | macos-26 standard, ARM64, GPU Apple Paravirtual device |
| Qt | 6.10.3 |
| SDK LunarG | 1.4.363.1 |
| Loader Vulkan | 1.4.363 |
| API du périphérique | 1.3.357 |
| Minimum moteur | 1.3.204 |
| MoltenVK | 1.4.2 |
| Metal 4 | Indisponible dans ce runner (probe préalable) |
| Vulkan Raster / Resources | 011 / 003 |
| Présentation | Qt QImage, relecture GPU/CPU, offscreen CI |
| CPU ARM64 | Chemin scalaire ; AVX2 indisponible, NEON non implémenté |

Le GPU paravirtualisé expose une API graphique accélérée ; ce n'est pas un benchmark sur un Mac physique identifié.

## Historique de résolution

| Étape | Résultat |
|---|---|
| [Probe Metal/MoltenVK](https://github.com/glaby91/moteur3d/actions/runs/46090946880) | Calcul 256 × 256, 65 536 pixels exacts, SHA-256 identique ; KosmicKrisp échoue à l'énumération des périphériques. Metal natif validé uniquement en compute. |
| [Build 1](https://github.com/glaby91/moteur3d/actions/runs/46091606630) | Compilation C++ ARM64 franchie, linkage bloqué par AGL absent du SDK Xcode 26.6. |
| [Build 2](https://github.com/glaby91/moteur3d/actions/runs/46092580858) | Qt 6.10.3 résout AGL ; binaire Mach-O et dylib compilés ; crash lors de l'exécution Raster. |
| [Diagnostic A/B LLDB](https://github.com/glaby91/moteur3d/actions/runs/46093274934) | Crash dans les deux profils, y compris essai avec couche de validation ; la trace situe le problème à la liaison mémoire des images MoltenVK/Metal virtualisé. |
| [Build 3](https://github.com/glaby91/moteur3d/actions/runs/46093745682) | Images 3D RGBA32F privées dédiées créées ; crash déplacé vers createImage 2D. |
| [Build 4](https://github.com/glaby91/moteur3d/actions/runs/46094113757) | Toutes les images optimales utilisent le même allocateur privé dédié sous Apple ; tous les tests obligatoires passent. |

Le port conserve les shaders d'origine, les formats, le filtrage, le Z-prepass et les buffers de staging/relecture accessibles au CPU. La politique de mémoire des images Windows/Linux reste inchangée. Le format GI RGBA32F est vérifié avec ses capacités de sampling, filtrage linéaire et transfert. La correction est expérimentale et spécifique à macOS ; aucun repli silencieux vers une précision inférieure.

## Résultats Build 4

| Test | Résultat |
|---|---|
| Compilation moteur, bibliothèque partagée et outils | OK (Mach-O ARM64) |
| Tests unitaires Vulkan | 5/5 |
| Triangle GPU, profils optimisé et standard, 320 × 180 | OK, deux PNG |
| Scène complète, profils optimisé et standard, 640 × 360 | OK, trois images par profil |
| Navigation réelle de l'application Qt, offscreen | OK : accroupissement, retour debout, saut/atterrissage, ouverture/fermeture Pip-Boy |
| Initialisation rendu GUI 1000 × 680 | OK, processus exit 0 |
| Gate CI finale | Aucun contrôle manquant, tous codes exigés à zéro |

Scène de référence V17.634 ; 1 508 appels de dessin par image ; MSAA 1× ; Z-prepass activé. Mesures de temps total du rendu, non du GPU seul (caméra statique, seulement trois images par profil) :

| Profil | Initialisation + première image | Image 2 | Image 3 |
|---|---:|---:|---:|
| Optimisé | 5 240,01 ms | 21,4008 ms | 17,9037 ms |
| Standard | 3 894,6 ms | 21,7942 ms | 24,392 ms |

La dernière image optimisée correspond à ~55,9 images/s instantanées et la standard à ~41,0. Pas de FPS soutenu ni de mesure en déplacement ; pas de preuve de parité visuelle CPU/DX11. La GI a créé les textures de secours 1 × 1 × 1 ; le chargement et l'utilisation d'une vraie table GI ne sont pas établis.

## Démarrage du package développeur

Après extraction de l'archive Actions puis de l'archive tar.gz, installer Qt 6.10.3 macOS avec Multimedia et le SDK LunarG 1.4.363.1, puis adapter les chemins locaux :

```bash
export QT_ROOT_DIR="/chemin/vers/Qt/6.10.3/macos"
export VULKAN_SDK="/chemin/vers/VulkanSDK/macOS"
cd CSE-WARP-V17.031-macOS-ARM64-MoltenVK-build-4
./lancer.sh benchmark-civic/scene.json
```

Package non autonome, non signé/notarisé. Le script est fourni mais le package extrait sur un autre Mac n'a pas encore été validé. Il ne s'agit pas d'un bundle .app final.

## Validations restantes

- Rendu et navigation dans une vraie fenêtre Cocoa sur macOS ; présentation directe Metal/WSI non portée.
- Fidélité visuelle et comparaison avec CPU/DX11, parcours en mouvement, Full HD, MSAA 2×/4×.
- Vraie table GI précalculée, rendu CPU et GI ARM64, audio local et radio réseau.
- Tests sur Mac physique ; package autonome et vérification de ses dépendances.
- Optimisations NEON éventuelles ; LumaTrace GPU et Compute DX11 non portés.
- Régression Windows/Linux et comparaison avec la tête actuelle de master avant toute fusion.

Aucune fusion automatique. Le Build 4 constitue une référence de portage expérimental, pas une validation exhaustive.
