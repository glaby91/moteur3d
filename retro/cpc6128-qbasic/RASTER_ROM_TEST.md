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
