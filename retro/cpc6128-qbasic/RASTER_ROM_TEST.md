# QEDIT ROM v0.4.2 — prototype raster MODE 2

Branche d'essai dérivée de **v0.4.1-tested**, sans modification de la branche stable.

## Fonctionnement

- ROM d'extension 16 Ko, MODE 2, 80 colonnes, 1500 lignes et tokens v1 conservés.
- À l'entrée de l'éditeur, installation d'un relais d'interruption IM1 en RAM centrale `#8C00`, qui continue la routine système originale.
- La palette de l'encre 1 change selon une phase à six interruptions (environ 300 Hz, 52 lignes de balayage par phase).
- Blanc, rouge et jaune : **bandes horizontales approximatives**, et non coloration syntaxique par mot ni validation des paramètres.
- La routine est en RAM pour ne pas être désactivée lorsque AMSDOS change la ROM supérieure.
- Quitter avec ESC rétablit le vecteur d'interruption original.

## Essai WinAPE

Installer `editor/QEDIT.ROM` de l'artifact de la compilation GitHub Actions en ROM supérieure 1. Puis :
```basic
MEMORY &5FFF
|QEDIT
```
Le fichier `QEDIT.ROM` déjà présent dans la branche de départ est **l'ancienne v0.4.1** tant qu'un nouveau binaire n'a pas été compilé et publié dans un artifact. Ne pas confondre.

## Réserves importantes

Ceci est un test à risques : une interception de `#0038` peut affecter le firmware ou le timing disque, et le déphasage des bandes dépend du cycle des interruptions. Le prototype teste que l'interception IM1 et le chaînage du firmware sont possibles. Aucun test WinAPE ni vérification de compilation n'est encore affirmé. Privilégier un snapshot WinAPE et garder la ROM v0.4.1 de secours. **Pas de validation syntaxique active.**

## Retour WinAPE utilisateur, 20 octobre 2026

L'utilisateur observe **des couleurs très aléatoires** : tantôt jaune, tantôt rouge, mais souvent écran blanc. **Raster non validé** : le code utilise une phase d'interruption 0..5 sans ancrage VSYNC certifié ni association ligne écran/diagnostic. La modification suivante (commit `5a404283f95f19cebc37f452323b017578061a75`) inverse le chaînage firmware et les écritures de palette, compilation GitHub Actions réussie, mais **elle ne garantit PAS la synchronisation verticale** et n'est pas confirmée en WinAPE.

Prochaine correction : calage fiable sur début de trame (VSYNC / compteur Gate Array), bande fixe précisément positionnée et restauration du firmware ; tester ensuite avant coloration de lignes syntaxiques. Garder v0.4.1 de secours.

## Défaut menu confirmé sur capture WinAPE

À partir de « Execution », la sélection n'était pas alignée avec les intitulés, indépendamment du défaut raster. Le menu statique commence aux colonnes (1-based) `2, 11, 20, 31, 42, 52, 61, 70`, tandis que `MENU_HEAD_X` indiquait auparavant `2, 11, 20, 32, 44, 54, 63, 73`. Correction faite dans `QEDIT_ROM.asm` (commit `36f3b85e825ed6a16b4440e2f74d2bb50472cdd8`). À recontrôler visuellement dans WinAPE. Les bandes raster aléatoires restent un défaut séparé.

## 20 octobre — redémarrages répétés, WinAPE CRTC Type 0

Le test réel de la ROM raster provoque des redémarrages en boucle avec CRTC 0. Défaut critique : **version raster non utilisable**. Pour isoler et éliminer le hook dangereux, le commit `cebe4b13c0e2ed046608f0ca48574cc1c558ff4d` désactive l'appel à `RASTER_INSTALL` au lancement, tout en gardant le code expérimental inactif. C'est un **build de diagnostic sans raster**, pas une ROM raster corrigée. Ne pas déclarer le problème de raster résolu. Repenser l'interception d'interruption et vérifier le respect du firmware CPC avant réactivation.
