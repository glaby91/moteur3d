# Format tokenisé QEDIT v1 — CPC6128

Interface proposée entre la ROM de l'éditeur et la future ROM d'exécution QBasic.

## Octets

- `00h–7Fh` : texte ASCII brut.
- `80h–C1h` : 66 tokens QBasic, `80h + index` dans la table `TOKEN_POINTERS`.
- `C2h–FFh` : réservés, ne pas les interpréter actuellement.

Quelques identifiants de référence : `PRINT=80h`, `FOR=81h`, `NEXT=82h`, `IF=83h`, `THEN=84h`, `END=86h`, `DIM=87h`, `REM=9Ah`. Les noms et l'ordre complets sont définis dans [QEDIT_V04_CORE.asm](editor/QEDIT_V04_CORE.asm).

## Record de ligne, RAM paginée

Chaque ligne non vide est stockée dans l'une des banques de 16 Ko (`C4h` à `C7h`) :

`[1 octet longueur] [N octets de texte ASCII et tokens]`

La table principale, à `#9800`, contient 1 500 entrées de trois octets chacune :

`[banque: 0..3] [adresse faible] [adresse forte]`

Une entrée `FFh` représente une ligne vide. Les adresses de records sont comprises entre `#4000` et `#7FFF`, dans la banque correspondante. Le firmware utilise la configuration mémoire standard `C0h` après chaque accès.

## Lexique et limitations

- Un mot-clé est tokenisé **uniquement à une frontière de mot** ; par exemple `PRINT` est converti, pas `PRINTER`.
- Les chaînes entre `"` et le texte des commentaires `REM` ou `'` sont conservés.
- Les noms de variables, les valeurs numériques et le texte inconnu restent en ASCII.
- Pas de numéros de ligne obligatoires : l'index en RAM donne seulement la position physique du programme.
- La version des tokens doit être conservée lorsque la ROM d'exécution sera créée ; les IDs ne sont **pas** ceux du Locomotive BASIC.
- Les fichiers `.BAS` sont actuellement exportés en **ASCII**, pas dans ce format interne.

**Statut :** format de travail v1 implémenté dans la bêta QEDIT v0.4.0, compilation vérifiée, essais WinAPE encore nécessaires.
