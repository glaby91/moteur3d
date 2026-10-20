# Profil de compatibilité QBX 7.1 / CPC6128 — préparation B005

La source commune initiale est [qbx71-cpc-profile.json](qbx71-cpc-profile.json). Elle inventorie les **128 tokens** actuellement attribués (IDs 0x80 à 0xFF) dans QEDIT B004, avec leur catégorie de compatibilité.

## Catégories
- **native** : instruction ou fonction BASIC 7.1 pertinente sur CPC, à implémenter dans QCOMP.
- **adapted** : comportement Microsoft dépendant du PC et devant utiliser l'écran, l'AY, le clavier ou AMSDOS du CPC.
- **extension** : instruction ajoutée pour le matériel CPC, distincte du langage Microsoft officiel.
- **unsupported** : commande PC/DOS non transposable raisonnablement ; QEDIT devra pouvoir la reconnaître et signaler cette limitation.

**Important :** la colonne `stage: token_only` de toutes les 128 entrées signifie que la présence du token dans QEDIT **ne garantit aucune exécution ou compilation Z80**. Les commandes ajoutées à `additional_candidates` ne possèdent **pas encore d'ID token**. Les profils sont des décisions préliminaires à affiner commande par commande.

## Contrat QEDIT / QCOMP
- Préserver les identifiants existants 0x80..0xFF et leur ordre.
- Utiliser ce fichier comme source de vérité du statut de compatibilité ; les règles syntaxiques et la signature des paramètres feront l'objet de champs et de générateurs ultérieurs.
- Prévoir un format de token étendu avant tout 129e mot-clé ; ne pas réaffecter un identifiant actuel.
- Le BASIC Microsoft PDS 7.1 reste la référence ; QB64 n'est **pas** la cible.
- Pas de changement de ROM ni de sauvegarde dans cette étape : B001 reste référence validée, la sauvegarde reste en panne.

## État
B005 est à ce stade **une préparation documentaire et structurée**, pas une nouvelle ROM compilée. QEDIT B004 n'est pas altérée.


## Inventaire étendu au 21 octobre 2026
Le fichier JSON contient désormais **128 tokens attribués** et **110 entrées candidates ou constructions de grammaire** non encore tokenisées : fonctions absentes (DEFINT, MKI$, CVI, SEEK...), opérateurs (XOR, EQV, IMP), commandes dépendantes de DOS, extensions CPC et expressions composées (END IF, LINE INPUT, ON ERROR).

Chaque entrée reçoit une catégorie de compatibilité et un état de réalisation. Des exemples de signatures sont inclus pour les instructions déjà étudiées. **Ne pas interpréter 110 candidats comme 110 tokens nouveaux** : certains noms sont des modes d'instruction (OUTPUT, APPEND), des familles de bibliothèques ou des suites lexicales. L'annexe alphabétique exhaustive du manuel Microsoft reste à valider avant toute attribution définitive d'IDs.

**Pas de ROM B005 à télécharger à ce stade** : les travaux publiés sont le catalogue et le profil. Les IDs historiques 0x80–0xFF sont préservés. Les tests WinAPE de B004, ainsi que la sauvegarde, ne sont pas validés.

## B005 ROM réellement compilée

- Code éditeur : `QEDIT_V042_B005.ROM` (ROM de 16384 octets).
- Code et données utilisés : **9650 octets** ; remplissage libre : **6734 octets**.
- 196 mots-clés tokenisés, soit 128 identifiants historiques + 68 nouveaux ; la table de compatibilité JSON est synchronisée.
- Encodage ancien : un octet `0x80..0xFF`. Encodage nouveau : deux octets `0x01, index` avec `index=0..67`. Le préfixe `0x01` ne figure pas dans le texte éditable ASCII standard.
- Les déclarations de syntaxes composées telles que `END IF` sont analysables à partir de plusieurs tokens élémentaires, sans imposer un token composé ; l'analyse grammaticale détaillée n'est pas réalisée.
- Les fonctions propres au PC/DOS, les extensions CPC et les constructions signalées « non supportées » sont classées dans le profil ; leur prise en charge effective par l'éditeur et le futur compilateur reste à développer.
- QCOMP n'est **pas** implémenté ; le répertoire JSON fournit une base de correspondance commune, pas encore un générateur de ROM compilateur.
- Test automatique : compilation RASM réussie, taille vérifiée ; **aucune validation WinAPE de la B005**. La sauvegarde `Ctrl+S` reste défectueuse.
- Tests prioritaires WinAPE : édition des tokens historiques, `CHDIR`, `CURRENCY`, `XOR`, `MODE`, `PLOT`, déplacement à une ligne précise, retour début/fin et indicateur WAIT. Contrôler l'intégrité du texte après chaque déplacement.

## Limites de complétude

Il reste 43 entrées candidates/constructions dans le profil et l'audit systématique des annexes Microsoft PDS 7.1 n'est pas achevé. Toute affirmation de compatibilité intégrale 7.1 serait prématurée. Les IDs historiques doivent rester stables lors de futures additions.
