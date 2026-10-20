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
