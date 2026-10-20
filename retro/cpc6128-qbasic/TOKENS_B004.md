# QEDIT v0.4.2 B004 — Extension du dictionnaire QBX 7.1

- Base: B003, copie sur une branche indépendante `qedit-v0.4.2-b004-qbx71-tokens`.
- 66 tokens d'origine **inchangés**, IDs `0x80..0xC1`.
- 62 tokens supplémentaires issus du langage Microsoft BASIC PDS/QBX 7.1, IDs `0xC2..0xFF`, soit **128 tokens** en tout.
- Groupes ajoutés : maths et conversions, graphismes, chaînes, commandes systèmes et déclarations.
- Même table utilisée pour encoder/décoder les lignes. Reconnaitre un token ne signifie ni valider sa syntaxe ni compiler son instruction.
- Dictionnaire QBX 7.1 **non encore exhaustif**. Pour aller au-delà de 128 tokens, élaborer un encodage échappé multi-octets sans réaffecter les anciens IDs (et tenir compte des valeurs de contrôle internes).
- `Ctrl+K` reste retiré, capacité théorique 1500 lignes préservée. WAIT et diagnostics minimaux B003 conservés.
- Sauvegarde `Ctrl+S` encore défectueuse : redémarrage du CPC confirmé sur les builds précédentes. Éviter de l'utiliser.
- ROM `QEDIT_V042_B004.ROM` : compilée 16384 octets, **9095 octets utiles**, 7289 octets de remplissage `0xFF`.
- **Fonctionnement sous WinAPE non testé** : seule la compilation automatique a réussi.

## Tests WinAPE conseillés
1. Contrôler l'affichage du titre B004 et de WAIT.
2. Entrer `PRINT SIN(0)`, `SCREEN 1`, `CIRCLE (100,100),20` et vérifier que le texte réapparaît correctement après saut de page.
3. Vérifier les anciens tokens `PRINT`, `FOR`, `NEXT`, `DIM` avec l'éditeur.
4. Ne pas lancer `Ctrl+S`.
