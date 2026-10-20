# QBasic CPC6128 — QEDIT v0.3.2

Éditeur QBasic sans numéros de ligne, pour Amstrad CPC6128 Z80, au format ROM d'extension **16 Ko**. Affichage **Mode 2 standard 80 × 25**.

## État de validation

- **v0.2 :** démarrage de `|QEDIT`, saisie et sauvegarde `Ctrl+S` testés par l'utilisateur dans WinAPE.
- **v0.3.1 :** menus rapides et retour au BASIC avec `ESC` testés et validés par l'utilisateur dans WinAPE.
- **v0.3.2 :** optimisations de navigation/scrolling compilées avec RASM v3.2.4bis et contrôlées par SHA-256 ; **le comportement réel de cette nouvelle version dans WinAPE doit encore être validé**.

## Installer la ROM dans WinAPE

Placer [QEDIT.ROM](editor/QEDIT.ROM) dans **Upper ROM 1**, sans remplacer BASIC 1.1 en ROM 0 ni AMSDOS en ROM 7. Redémarrer le CPC6128. Depuis BASIC :

```basic
MEMORY &5FFF
|QEDIT
```

## Nouveautés v0.3.2 : rapidité et défilement

- Les flèches mettent à jour **l'ancien caractère du curseur, les chiffres de la barre d'état et le nouveau curseur**. Il n'y a plus de réaffichage de 76 caractères par déplacement normal.
- En quittant l'écran de 21 lignes vers le bas ou le haut, le firmware **SCR SW ROLL (#BC50)** fait défiler uniquement la région de texte : colonnes 2 à 77, lignes 3 à 23. Une seule nouvelle ligne est réimprimée.
- Le compteur de colonne ne recouvre plus le mot **INS** dans la barre d'état.
- Ajout d'un **programme de 50 lignes** dans la ROM, sans numéro de ligne en début de code. Ce programme est également fourni comme fichier [LONG_DEMO_test.bas](editor/LONG_DEMO_test.bas).

### Test dans WinAPE, sans préparer de disquette

1. Lancer l'éditeur avec `|QEDIT`.
2. Appuyer sur **Ctrl+L** (ou `Ctrl+T` → **Options** → **Test 50 lignes**). **Attention : ce test remplace le contenu en cours ; sauvegardez-le avant.**
3. Appuyer ou maintenir **flèche bas** au-delà de la **ligne 21**, puis jusqu'à **ligne 50**. L'en-tête et la barre inférieure doivent rester fixes et le défilement ne doit présenter ni duplications ni trous.
4. Remonter jusqu'à **ligne 1** avec **flèche haut**.
5. Vérifier **gauche/droite**, puis `Ctrl+T` (menus), `Ctrl+S` (sauvegarde de `DEMO.BAS` si souhaitée) et `ESC` (retour au BASIC).

Le trajet de la ligne 1 à la ligne 50 exige théoriquement **29 défilements d'une ligne** ; le retour nécessite 29 défilements inverses. Ce parcours a été vérifié par un test logique statique, **pas** par une exécution dans WinAPE.

## Contrôles conservés

`Ctrl+T` menus, `Ctrl+S` sauvegarde, `Ctrl+O` ouvre `DEMO.BAS`, `Ctrl+N` nouveau programme, `Ctrl+L` démo 50 lignes, `ESC` quitte avec restauration de l'affichage BASIC.

## Compilation

```sh
rasm retro/cpc6128-qbasic/editor/QEDIT_ROM.asm -ob retro/cpc6128-qbasic/editor/QEDIT.ROM
```

Empreinte SHA-256 du binaire compilé (16384 octets) :

```
11422563767479f58c980405a19db17249a0330425ab4532e7a1509e2edefa4b
```

[Workflow de compilation RASM](../../.github/workflows/build-qedit.yml).

**Limites :** exécution QBasic et débogage non encore implémentés, noms de fichier non éditables, pas d'overscan ; la V0.3.2 nécessite encore un test réel sous WinAPE pour confirmer la fluidité et le défilement.
