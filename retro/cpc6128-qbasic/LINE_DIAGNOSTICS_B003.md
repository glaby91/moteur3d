# QEDIT v0.4.2 B003 — Diagnostics minimaux (prototype)

## Fonctionnement
- `!` en colonne 79 : guillemet non refermé ou parenthèses non équilibrées dans la ligne visible. Les caractères entre guillemets, et les commentaires commençant par une apostrophe, sont ignorés pour le test de parenthèses.
- `T` en colonne 79 : un octet de token interne (>= 0x80) ne correspond pas aux 66 entrées du dictionnaire. Détection au décodage des données compactées, pas une erreur de grammaire QBasic.
- Aucun indicateur : aucune erreur détectée par ces contrôles **limités**, ce qui ne garantit pas la validité du programme.
- Les commandes inconnues en texte ordinaire, la correspondance FOR/NEXT, IF/THEN et le typage ne sont pas encore vérifiés.
- La coloration par ligne n'est pas implantée : le MODE 2 utilise deux encres et le raster expérimental reste désactivé.
- `Ctrl+K` n'appelle plus le générateur de 1500 lignes ; capacité maximale du document inchangée.

## Validation
- Build B003 compilé avec RASM en ROM 16384 octets. La validation réelle des marqueurs reste à faire sous WinAPE CRTC 0.
- Sauvegarde `Ctrl+S` non corrigée : risque de reset. Tester exclusivement sur disquette de test ou éviter Ctrl+S.
- Base B001 conservée séparément.

## Exemples de vérification
- `PRINT "BONJOUR"` : pas d'erreur.
- `PRINT "BONJOUR` : `!` guillemet manquant.
- `PRINT (1+2` : `!` parenthèse manquante.
- `PRINT (1+2)` : pas d'erreur.
- `T` n'est pas facile à reproduire au clavier : il nécessite des données tokenisées internes incorrectes.
