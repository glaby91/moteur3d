# Référence officielle du langage : Microsoft BASIC Professional Development System 7.1 (QBX)

Décision du 21 octobre 2026 : adopter **Microsoft BASIC PDS 7.1 / QuickBASIC Extended (QBX)** comme référence du vocabulaire, de la grammaire et du comportement attendu de QEDIT + QCOMP sur CPC6128, en remplacement de QuickBASIC 4.5.

Documentation historique de référence :
- Microsoft BASIC 7.1 *Language Reference* : https://www.pcjs.org/documents/books/mspl13/basic/b7lang/
- Microsoft BASIC 7.1 *Programming Guide* : https://www.pcjs.org/documents/books/mspl13/basic/b7prog/
- Microsoft BASIC 7.1 *Getting Started* : https://www.pcjs.org/documents/books/mspl13/basic/b7start/

## Règles de conception

1. Répertorier les instructions, fonctions, opérateurs, types, métacommandes et formes syntaxiques de PDS 7.1. Séparer le langage intrinsèque des bibliothèques complémentaires (ISAM, outils graphiques, interface, etc.).
2. Définir chaque entrée dans **une unique source de vérité** pour produire les tables de QEDIT et QCOMP avec des IDs stables et une version explicite du dictionnaire. Ne pas attribuer arbitrairement des IDs aux nouvelles commandes sans stratégie de compatibilité des fichiers existants.
3. Statuts indépendants pour chaque entrée : **répertoriée**, **tokenisée**, **diagnostiquée**, **compilable Z80**, **supportée à l'exécution**, **incompatible CPC**, **extension CPC**.
4. QEDIT (ROM 1) valide rapidement les erreurs syntaxiques certaines et n'affiche pas d'erreur de commande inconnue tant que le catalogue et la gestion des identificateurs utilisateur ne sont pas prêts. QCOMP (ROM 2) réalise la vérification complète et compile en Z80 natif. Bibliothèque d'exécution facultative.
5. Respecter la limite ROM 16 Kio par module, en utilisant des définitions partagées au moment de la compilation des ROM et en évitant les changements de ROM à chaud non validés.
6. Adapter les spécificités PC/DOS/OS/2 (interruptions, ISAM, fichiers, VGA, sons) aux capacités du CPC6128 ou les signaler explicitement comme non supportées ; la référence de langage n'implique **pas** une compatibilité fonctionnelle intégrale.

## État réel

QEDIT v0.4.2 B003 dispose **seulement de 66 tokens** et de diagnostics minimaux, sans compilateur QCOMP actif. Le catalogue PDS 7.1 complet et son analyse syntaxique **restent à réaliser**. Maintenir la sauvegarde Ctrl+S comme défectueuse, distincte de ce chantier. Ne pas changer les ROM B001 validées sans nouveau build et tests WinAPE.

## Étapes suivantes

1. Établir l'inventaire des commandes et paramètres à partir du manuel Microsoft, avec catégories et dépendances.
2. Définir un format de token extensible pour dépasser les 66 valeurs actuelles sans casser les programmes déjà tokenisés.
3. Générer les dictionnaires partagés et ajouter les diagnostics syntaxiques par paliers, puis le compilateur QCOMP.
