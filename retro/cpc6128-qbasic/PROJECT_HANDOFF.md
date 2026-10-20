# QEDIT CPC6128 — Dossier de reprise du projet

**Mise à jour : 20 octobre 2026.** Document de référence permettant de reprendre le travail dans une nouvelle conversation, sans présumer que les essais non confirmés ont réussi.

## 1. Objectif et règles à préserver

Développer un **éditeur de type Microsoft QBasic** pour **Amstrad CPC 6128**, écrit en assembleur **Z80**, démarré depuis une **ROM d'extension 16 Kio**, avec programmes **sans numéros de ligne**. Cible : confort d'édition, rapidité de navigation et stockage compact de programmes pouvant atteindre 1 500 lignes selon leur taille. Interface voulue en **MODE 2 (640 × 200, 80 colonnes)**. Le programme QBasic doit à terme disposer de son propre moteur d'exécution : **la ROM actuelle est un éditeur, pas un interpréteur**.

Préserver les fonctions qui ont été testées en réel ; éviter de remplacer la branche stable sans validation WinAPE. Travailler sur **branches séparées**, publier directement **les sources sur GitHub**, ainsi que les binaires compilés lorsqu'ils sont disponibles.

## 2. Où sont les sources ?

Dépôt : https://github.com/glaby91/moteur3d

- Projet : `retro/cpc6128-qbasic/`
- ROM principale : `editor/QEDIT_ROM.asm`
- Moteur tokenisé + RAM paginée : `editor/QEDIT_V04_CORE.asm`
- Description du format des tokens : `TOKEN_SPEC.md`
- Tests statiques : `tests/test_token_model.py`
- Compilation originale : `.github/workflows/build-qedit-v040.yml`
- Prototype raster intégré : branche `qedit-rom-raster-v0.4.2-experimental`, compilation `.github/workflows/build-qedit-raster-rom.yml`.

**Branches clés (attention : elles divergent) :**

| Branche | Usage | État connu |
|---|---|---|
| `qbasic-cpc6128-v0.3.2-stable` | Retour arrière fiable | Menus, sauvegarde et navigation testés en WinAPE |
| `qbasic-cpc6128-v0.4.1-tested` | Base 1 500 lignes avec tokenisation | Utilisateur a confirmé le test 1 500 lignes et la modification de lignes, mais pas le test sauvegarde/rechargement long |
| `qbasic-cpc6128` | Développement normal, **v0.4.2 avec Ctrl+G** | Source et build disponibles, test WinAPE de Ctrl+G à confirmer |
| `qedit-rom-raster-v0.4.2-experimental` | Prototype raster intégré à la ROM dérivé de v0.4.1 | Build GitHub Actions **réussi** sur commit `7182492d9a5b470cb2c8bdd9698cb72113a34605`; pas encore testé dans WinAPE ; **n'inclut pas nécessairement Ctrl+G du développement normal** |
| `qedit-raster-mode2-demo` | Démo raster BASIC + binaire autonome séparé | Build autonome réussi ; ce n'est pas la ROM de l'éditeur |

Ne jamais supposer que la branche raster est la version fonctionnelle la plus récente : **v0.4.2 Ctrl+G et v0.4.2 raster sont deux lignes de développement distinctes** et nécessiteront une fusion revue/testée.

## 3. Architecture existante

- ROM d'extension 16 384 octets dans `#C000–#FFFF` ; lancement depuis BASIC après `MEMORY &5FFF` et `|QEDIT`.
- MODE 2 : texte standard 80 colonnes, deux couleurs *simultanées*. Les rasters peuvent changer les **couleurs de la bande horizontale**, pas colorer arbitrairement chaque mot d'une même ligne.
- 21 lignes visibles éditables mises en cache dans la RAM de base, 80 octets chacune.
- Jusqu'à 1 500 entrées indexées par numéro **logique** 16 bits, sans numéro dans le texte ; table d'index : 1 500 × 3 octets.
- Quatre banques 16 Kio CPC6128 sélectionnées par le Gate Array `C4` à `C7` dans `#4000–#7FFF` ; stockage compact à enregistrements `[longueur][octets tokenisés/ASCII]`.
- 66 tokens QBasic internes, différents des codes Locomotive BASIC. Les chaînes et commentaires ne sont pas tokenisés comme mots-clés. Fichier `.BAS` actuellement exporté en ASCII.
- Modifications stockées par ajout de records : la RAM paginée peut se remplir après modifications répétées ; **compaction mémoire automatique à développer**.

**Mémoire** : cache `#6000–#668F`, états `#8800` et suivants, tampons `#8A00/#8B00`, tampon AMSDOS `#9000–#97FF`, index `#9800–#A993`. La branche raster expérimente un relais ISR copié en `#8C00` : vérifier les conflits de RAM avant toute fusion.

## 4. Fonctionnalités et état des validations

| Fonction | État |
|---|---|
| Menu rapide, clavier, défilement d'écran, sauvegarde standard | Fonctionnels dans v0.3.2, testés utilisateur |
| 1 500 lignes et déplacement sur ce programme | **Test WinAPE confirmé par l'utilisateur le 20/10/2026** |
| Modification d'une ligne dans les 1 500 | **Confirmée par l'utilisateur** |
| Sauvegarde puis rechargement de 1 500 lignes modifiées | **Non testé par l'utilisateur** |
| Ctrl+G pour aller à un numéro précis | Implémenté dans la branche `qbasic-cpc6128` v0.4.2 ; **test WinAPE non confirmé** |
| Ctrl+B début / Ctrl+E fin ; PageUp/PageDown | Souhait de navigation, **ne pas présenter comme implémenté** |
| Analyse des commandes et validation des paramètres | Objectif **non implémenté** |
| Coloration selon validité de ligne | Objectif **non implémenté** |
| Raster dans QEDIT | Source intégrée à branche expérimentale, compilée, **affichage et comportement WinAPE non validés** |
| Compaction RAM | À concevoir |

## 5. Vision du contrôle de syntaxe

La demande est de distinguer les lignes **valides, incorrectes, incomplètes**, **en vérifiant aussi les paramètres**, pas seulement la présence d'un token reconnu. Exemples :
- `FOR I = 1 TO 100` : valide syntaxiquement ;
- `FOR I = TO 100` : erreur de paramètre/expression ;
- `LOCATE 10,` : instruction incomplète.

Analyser lors de la validation/quittée de ligne plutôt qu'à chaque frappe ; distinguer **syntaxe**, **structure de blocs** et **erreurs d'exécution**. Il n'y a **pas encore** d'analyseur intégré. Prévoir messages précis dans la ligne d'état.

Le MODE 2 possède deux encres simultanées. Un raster peut permettre fond/texte de couleurs différentes selon la zone **verticale** : il ne suffit pas à colorer plusieurs mots de différentes couleurs sur une même ligne. Préférer au besoin inversion vidéo d'une erreur, symboles de marge (`+`, `X`, `?`) et message d'erreur.

## 6. Prototype raster ROM : état exact et risques

Branche `qedit-rom-raster-v0.4.2-experimental` basée sur `qbasic-cpc6128-v0.4.1-tested`, pas sur `qbasic-cpc6128` v0.4.2 Ctrl+G.

- Ajout de `RASTER_INSTALL` au démarrage et `RASTER_REMOVE` à la sortie.
- Copie d'un relais d'interruption IM1 en RAM `#8C00`, puis chaînage vers le vecteur firmware d'origine.
- Alternance approximative de bandes blanc/rouge/jaune à partir des interruptions du firmware (~300 Hz), **pas synchronisées sur une instruction précise**, ni sur les erreurs de l'éditeur.
- Premier build GitHub Actions : échec assembleur (bloc introduit au mauvais endroit, label `ROM_END` dupliqué et `COPY_ROM_END` manquant). Source reconstituée depuis la v0.4.1 saine ; **build du commit `7182492d9a5b470cb2c8bdd9698cb72113a34605` réussi** (run `39490383945`).
- **Ne pas considérer la ROM comme fonctionnelle dans WinAPE sur la seule foi du build** : vérifier restauration de vecteur et interruptions, routage firmware, timing, stabilité avec clavier/sauvegardes/disquette et couleur réelle à l'écran.
- Éviter d'employer les rasters comme mécanisme de contrôle syntaxique : ils ne réalisent que l'affichage. Protéger des corruptions éventuelles et conserver une ROM v0.4.1 de secours.

Artifact CI : https://github.com/glaby91/moteur3d/actions/runs/39490383945

## 7. Installer et tester

Dans WinAPE : `Settings → Memory`, installer `QEDIT.ROM` en **Upper ROM 1**, conserver BASIC 1.1 (ROM 0) et AMSDOS (ROM 7), redémarrer :

```basic
MEMORY &5FFF
|QEDIT
```

Raccourcis de référence de la base : `Ctrl+T` menus, `Ctrl+S` sauvegarder `DEMO.BAS`, `Ctrl+O` ouvrir, `Ctrl+N` nouveau document, `Ctrl+L` générer 50 lignes test, `Ctrl+K` générer 1 500 lignes test, `ESC` quitter. `Ctrl+G` est disponible sur la branche principale de développement v0.4.2, **pas garanti sur la branche raster**.

**Recette minimale** : comparer une capture d'écran v0.4.1 et de la ROM raster ; vérifier curseur, défilement, menus, accès ligne 1 500, frappe et retour à BASIC ; puis sauvegarde, nouveau programme et rechargement, d'abord court puis long. Photographier les défauts de position des couleurs et conserver le journal GitHub Actions.

## 8. Prochaines étapes recommandées

1. Essai visuel et stabilité du raster dans WinAPE, sans remplacement de la branche principale.
2. Tester le Ctrl+G déjà développé dans `qbasic-cpc6128`.
3. Valider `Ctrl+S` puis `Ctrl+O` sur 1 500 lignes avec modifications.
4. Concevoir **l'analyse syntaxique des commandes et paramètres** et son format de diagnostic (valide / incomplète / erreur).
5. Si le raster est utile, le caler par rapport aux lignes logiques et garantir le firmware ; sinon privilégier affichage normal/inversé en MODE 2.
6. Ajouter navigation début/fin, page par page, recherche, puis compaction mémoire.
7. Fusionner les fonctionnalités prudemment dans une nouvelle branche, tests statiques + vérification réelle WinAPE avant qualification stable.

## 9. Consigne pour toute reprise du projet

**Lire d'abord ce dossier et le README de la branche visée**, comparer le commit de la ROM testée aux sources à modifier, consulter les workflows et leurs logs ; ne pas confondre les deux branches v0.4.2. Demander seulement les résultats d'essai encore inconnus ; ne pas recommencer la tokenisation existante. Documenter à chaque version le commit, la taille binaire ROM, les tests réels et les défauts connus. Le but n'est pas uniquement esthétique : **éditeur QBasic rapide et fiable, pouvant gérer de grands programmes sur un vrai CPC 6128**.


## Journal des versions et essais WinAPE — 20 octobre 2026

**Référence de travail validée : QEDIT v0.4.2 B001**, branche `qedit-v0.4.2-versioned-rom`, fichier `QEDIT_V042_B001.ROM`, compilation GitHub Actions #39499725866, artefact #12298919037. Cette identification B001 apparaît aussi dans la ligne de titre de l'éditeur. Ne jamais remplacer cette version de référence par une expérience non validée.

| Version / branche | Modifications | État constaté |
| --- | --- | --- |
| v0.3.2 stable / `qbasic-cpc6128-v0.3.2-stable` | Éditeur initial | Point de référence historique. |
| v0.4.1 / `qbasic-cpc6128-v0.4.1-tested` | 1500 lignes, magasin tokenisé et cache de 21 lignes | Navigation/édition déjà testées. Sauvegarde de 1500 lignes non validée. |
| v0.4.2 initiale / `qbasic-cpc6128` | Ctrl+G, sauts de ligne | Le code existait, plusieurs problèmes d'affichage/usage ont nécessité des corrections. |
| Raster expérimental / `qedit-rom-raster-v0.4.2-experimental` | Palette par interruptions | Redémarrages WinAPE CRTC 0, fonctionnalité désactivée ; ne pas fusionner. |
| Navigation / `qedit-v0.4.2-navigation-safe` | Ctrl+B/E/U/D/G, WAIT, correction d'indexation des lignes du cache | L'utilisateur confirme que WAIT fonctionne. Des sauts affichaient des caractères incohérents à cause de l'index absolu utilisé à la place du numéro de ligne de cache ; correctif compilé. |
| **B001** / `qedit-v0.4.2-versioned-rom` | Nom de ROM et titre avec version/build, navigation et WAIT | **L'utilisateur confirme que Ctrl+E fonctionne** ; WAIT confirmé fonctionnel. La sauvegarde `Ctrl+S` fait redémarrer le CPC, même sur le petit programme de démonstration. |
| Sauvegarde diagnostic / `qedit-save-diagnostic-v0.4.2` | Ctrl+S ouvre un fichier test et tente d'écrire `OK` | **Redémarrage CPC confirmé**, même sans sauvegarder de lignes du document. L'appel firmware, non la tokenisation d'un long programme, est suspect. |
| **B002** / `qedit-v0.4.2-b002-save-bridge` | Relais RAM des appels disque, tentative de resélection de ROM | **Redémarrage CPC toujours confirmé** avec `Ctrl+S`. Échec : ne PAS déclarer la sauvegarde corrigée, ne pas substituer B002 à B001 comme référence. |

### Diagnostic sauvegarde confirmé
- Dans WinAPE, le BASIC standard parvient à enregistrer un fichier avec `SAVE "ESSAI.BAS"` : le lecteur, la disquette et AMSDOS fonctionnent dans le BASIC.
- Dans QEDIT, `Ctrl+S` provoque un **reset**, même pour le document de démonstration et pour le diagnostic consistant à écrire seulement `OK`.
- Le changement de ROM avec un relais RAM essayé dans B002 n'a **pas** éliminé le plantage. Il est insuffisant ou erroné ; ne pas reconduire automatiquement cette solution.
- Vérifier soigneusement la convention d'appel des vecteurs `CAS_OUT_OPEN` / `CAS_OUT_CHAR` / `CAS_OUT_CLOSE`, les paramètres et registres préservés, le contexte ROM supérieure et les conflits de plages RAM lors de l'activation d'AMSDOS ; distinguer hypothèses et causes démontrées.
- Stratégie conseillée : B003 distincte, instrumentation avant/après `CAS_OUT_OPEN`, test minimal non destructif sur disquette d'essai, inspection rigoureuse des adresses/du code. La présence d'une ROM de 16 Ko compilée **ne vaut pas** validation sous WinAPE.
- Ne pas faire utiliser la sauvegarde QEDIT pour des programmes importants tant que `Ctrl+S` et un rechargement `Ctrl+O` n'ont pas été vérifiés sur de petits puis de grands programmes.

### Discipline versioning
- Chaque distribution aura un nom unique : `QEDIT_V042_B001.ROM`, `QEDIT_V042_B002.ROM`, puis `QEDIT_V042_B003.ROM`.
- Afficher `QEDIT v0.4.2 Bxxx` dans l'éditeur et fournir l'artefact ZIP correspondant, avec statut « compilé », « testé WinAPE » et problèmes connus explicites.
- Conserver sur GitHub branches historiques et expérimentales, et partir de B001 pour toute modification future du clavier ou de l'affichage.
- Priorité : fiabiliser la sauvegarde sans faire régresser Ctrl+E, Ctrl+G, déplacements page haut/bas, 1500 lignes et WAIT.
