# QBasic CPC6128 — éditeur QEDIT

Éditeur de programmes QBasic **sans numéros de ligne**, en assembleur Z80 et en ROM d'extension **16 Ko**, conçu pour le **CPC6128 (128 Ko RAM)**.

## Versions et validation

| Version | Statut | Notes |
|---|---|---|
| **v0.3.2** | **Stable, testée par l'utilisateur dans WinAPE** | Menus rapides, clavier fluide, 50 lignes défilantes, sauvegarde et sortie ESC |
| **v0.4.1** | **Vérifiée dans WinAPE pour la navigation jusqu'à 1500 et la modification du programme** | Correction de l'affichage des lignes et du champ Col
| **v0.4.2** | **Bêta, compilation et tests statiques réussis — WinAPE encore à tester** | Ctrl+G / Recherche → Aller à la ligne |

La version stable est conservée sur la branche `qbasic-cpc6128-v0.3.2-stable`. La V0.4.0 bêta est conservée sur la branche `qbasic-cpc6128-v0.4.0-beta`. Le fichier `editor/QEDIT.ROM` sur `qbasic-cpc6128` est la **V0.4.2 bêta (Ctrl+G)**. La V0.4.1 testée avec 1500 lignes est sauvegardée sur la branche `qbasic-cpc6128-v0.4.1-tested`.

## Installer et démarrer dans WinAPE

Installer `editor/QEDIT.ROM` comme **Upper ROM 1** sous **Settings → Memory**. Conserver BASIC 1.1 (ROM 0) et AMSDOS (ROM 7). Redémarrer l'émulateur :

```basic
MEMORY &5FFF
|QEDIT
```

Commandes : `Ctrl+T` pour les menus, flèches pour naviguer, `Ctrl+S` pour sauver `DEMO.BAS` au format ASCII, `Ctrl+O` pour relire `DEMO.BAS`, `Ctrl+N` pour un nouveau programme, `ESC` pour quitter, `Ctrl+L` pour le test de 50 lignes et **`Ctrl+K` pour générer 1 500 lignes de démonstration**. Les deux raccourcis de test remplacent le document ouvert.

## Nouveautés V0.4.2 — saut direct à une ligne

- **Ctrl+G** ouvre une saisie du numéro de ligne ; on peut entrer **1 à 1500** et confirmer par **Entrée**.
- La même commande se trouve sous **Recherche → Aller à la ligne**.
- Le nombre doit correspondre à une ligne qui existe dans le document ; sinon QEDIT signale l'erreur.
- **Retour arrière** efface un chiffre et **ESC** annule sans quitter l'éditeur.
- La page est centrée autour de la ligne cible lorsque possible. Un seul rechargement des 21 lignes du cache est effectué, sans simuler des centaines d'appuis sur les flèches.
- **Test WinAPE demandé** : Ctrl+K pour remplir le document, puis Ctrl+G vers 1000 et 1500 ; modifier une ligne, refaire Ctrl+G vers la première, puis revenir vérifier la conservation du texte.

## Nouveautés V0.4.1 — correction de la barre d'état

La capture WinAPE de la V0.4.0 montrait le libellé **Col:** partiellement effacé et un numéro de ligne incorrect. Deux causes ont été corrigées : le code rafraîchissait la colonne en position 17 au lieu de 20, et la conversion Z80 des nombres à quatre chiffres utilisait une mauvaise détection de retenue.

La ligne est désormais affichée sur **quatre colonnes (10–13)**, la colonne sur **trois colonnes (20–22)**, sans effacer `Col:` ou `INS`. Contrôles statiques effectués notamment sur les lignes 1, 999, 1000 et 1500. **Test réel dans WinAPE encore nécessaire.**

## Architecture introduite en V0.4.0 bêta

- **66 tokens** pour les mots-clés QBasic courants : `PRINT`, `FOR`, `NEXT`, `DIM`, `IF`, `THEN`, `AS`, `INTEGER`, `CLS`, etc. Les chaînes et les commentaires restent du texte.
- **Lignes de longueur variable** : un record en RAM contient un octet de longueur, suivi du texte compacté.
- **1 500 entrées de ligne**, indexées par un numéro logique 16 bits, sans numéros de ligne dans le code.
- **RAM paginée** : quatre banques CPC6128 de 16 Ko (configurations Gate Array `C4` à `C7`) pour une capacité brute d'environ 64 Ko de données tokenisées.
- **Tampon rapide de 21 lignes** dans la RAM principale pour conserver la navigation et le défilement efficaces.
- **Sauvegarde ASCII** : les tokens sont décompressés avant enregistrement dans un fichier `.BAS`, puis réencodés à l'ouverture.

### Plan mémoire

| Zone | Utilisation |
|---|---|
| `#6000–#668F` | Cache des 21 lignes × 80 octets |
| `#8800–#8867` | États, positions 16 bits, sélection de banque |
| `#8A00` / `#8B00` | Tampons de 76–80 octets de codage/décodage |
| `#9000–#97FF` | Buffer disque AMSDOS (2 Ko) |
| `#9800–#A993` | Index 1 500 × 3 octets |
| Banques 4, 5, 6, 7 à `#4000–#7FFF` | Contenu compact, jusqu'à 64 Ko bruts |

La limite de **1 500 lignes dépend du volume total de texte compressé**. Elle n'est pas une garantie de 1 500 lignes longues de 76 caractères.

## Tester V0.4.0 dans WinAPE

1. Installer la nouvelle ROM ; vérifier la saisie, les menus (`Ctrl+T`) et `ESC`.
2. Tester `Ctrl+L` et faire défiler 50 lignes.
3. Tester **`Ctrl+K`**, puis descendre jusqu'aux lignes **22, 128, 256, 512, 999, 1000 et 1500** ; vérifier que `Ligne:`, `Col:` et `INS` restent lisibles, puis remonter en haut.
4. Modifier une ligne, changer de page, revenir dessus et vérifier la conservation du texte.
5. Tester `Ctrl+S` / `Ctrl+O` avec un document court, puis avec un document long.

**Limites de cette bêta :** records en ajout seul, donc des modifications répétées consomment progressivement la RAM paginée ; pas encore de compaction mémoire automatique ; pas de moteur d'exécution QBasic. **La V0.4.2 n'est pas déclarée stable sans validation de Ctrl+G dans WinAPE.**

## Sources et reproductibilité

- [Source principale](editor/QEDIT_ROM.asm)
- [Moteur RAM paginée et tokens](editor/QEDIT_V04_CORE.asm)
- [Format des tokens](TOKEN_SPEC.md)
- [Compilation GitHub Actions V0.4](../../.github/workflows/build-qedit-v040.yml)

Avec RASM v3.2.4bis, depuis le dossier `editor/` :

```sh
rasm QEDIT_ROM.asm -ob QEDIT.ROM
```

SHA-256 du binaire V0.4.2 bêta (16 384 octets) : `503f815beb059ec88249af986a9a42773fb11eb3e276f1df29f1ea7962466e46`.
