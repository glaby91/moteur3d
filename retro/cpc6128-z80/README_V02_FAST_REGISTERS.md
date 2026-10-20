# CPC6128 Raster Z80 — V0.2 Fast Registers

**19–20 octobre 2026** · [Source assembleur V0.2](CPC_RASTER_V02_FAST_REGISTERS.asm) · [Version de référence V0.1 préservée](CPC_RASTER_MODE0.asm) · [Architecture théorique](OPTIMISATION_V02_FAST_REGISTERS.md)

Cette version est **une véritable nouvelle implémentation Z80**, pas uniquement une proposition de conception. Elle reste **expérimentale et non assemblée/testée en émulateur réel**, faute d'assembleur CPC disponible ici.

## Ce qui a effectivement changé dans le code

| Optimisation | V0.1 | V0.2 |
|---|---|---|
| Triangles exécutés/image | 24 | **12 visibles**, autres faces retirées avant compilation |
| Transformation 3D | 8 poses préprojetées | 8 poses préprojetées, inchangé |
| Données d'une pose | 528 octets | **264 octets** |
| Banque supplémentaire | 2 poses par banque, 1056 octets | 2 poses par banque, **528 octets** |
| État d'arête | Position et pente fréquemment relues en RAM | **`HL'` = position X en 8.8 et `DE'` = pente**, échangés avec `EXX` |
| Petits segments de peinture | Boucle écriture octet | Boucle octet conservée |
| Grands segments de peinture | Boucle écriture octet | **`PUSH BC`**, écrit deux octets couleur à la fois, avec **`SP` temporaire** |
| Nettoyage vidéo par image | 16 000 octets de fond | **3 660–3 720 octets**, rectangle commun ancienne/nouvelle pose |
| Écran | Mode 0, 160×200, 16 encres | Inchangé |
| Texture / Z-buffer | Aucun / aucun | Inchangé |
| OS | ROMs et interruptions coupées | Inchangé |

Attention : les deux pixels vidéo contenus dans un octet ont toujours la même couleur dans cette version ; la géométrie horizontale est rasterisée sur 80 colonnes *d'octets* (160 pixels affichés).

## Cache « L1 » de registres alternatifs

Les six octets `BC'/DE'/HL'` du Z80 ne constituent pas un cache mémoire au sens architectural. Le mécanisme `EXX` échange en une instruction les trois paires 16 bits. Dans notre boucle réelle :

- Préparation **une fois par arête** de `HL'=X` et `DE'=dX/dY`, dans le format 8.8.
- Pour chaque ligne : `EXX; LD A,H; ADD HL,DE; EXX`.
- La position évolue dans les registres, sans lecture ni écriture de `EdgeX` ou `StepX` en RAM à chaque ligne.
- Le jeu principal utilise `C` comme numéro de ligne et le pointeur `3000/3100` pour les limites de spans.

La couleur est copiée deux fois dans `BC` pour les spans longs. Le programme sauve la vraie pile dans `SavedSP`, pointe temporairement SP vers la fin de la ligne vidéo, effectue les `PUSH BC` décroissants, puis restaure SP avant tout retour ou autre appel.

## Vérifications logicielles déjà effectuées

J'ai interprété les enregistrements de triangles des versions V0.1 et V0.2 avec un simulateur de leurs spans, en conservant exactement les pas X en 8.8.

- **8 poses × 12 triangles = 96 enregistrements**, chacun de 22 octets, présents.
- Le rectangle de nettoyage est `x=8..67` (60 octets) et `y=77..138` ou `78..138` (61–62 lignes).
- Sur les 8 poses, les images dessinées avec le **nettoyage local** sont **exactement égales octet pour octet** aux images de la V0.2 où le fond serait effacé intégralement.
- Les 8 images avec culling diffèrent légèrement de la V0.1 sur 3 poses, de **73, 14 et 43 octets** respectivement, principalement parce que la V0.1 dessinait aussi des faces arrière. Les cinq autres poses sont identiques dans cette simulation.
- Les 8 remises à zéro de l'écran passent de **128 000 à 29 520 écritures d'octets de fond**, soit **76,9 % de moins**. Cette comparaison exclut les écritures des triangles et les coûts d'adressage/sauvegarde de la pile.
- Estimation statique des déplacements des sauts `JR`/`DJNZ` : aucun saut relatif hors de sa plage ; instruction inconnue dans la grille de validation : aucune. **Ceci ne remplace pas un assemblage**.

## Ce qui reste à mesurer et à améliorer

- **FPS réels inconnus** : les cibles initiales de 12,5–20 FPS pour des scènes précalculées n'ont **pas** été démontrées. Il manque une compilation RASM et un émulateur CPC à timing fiable.
- Corriger, si nécessaire, les problèmes de compatibilité de syntaxe assembleur et vérifier le Gate Array sur véritable CPC6128.
- Projeter dynamiquement les sommets 3D (les **huit poses sont toujours préprojetées**), en s'appuyant ensuite sur la LUT de 256 cosinus 8.8 proposée.
- Mesurer combien de cycles sont économisés par la suppression des 12 triangles cachés, les `EXX` et la restauration locale du fond.
- Envisager le clipping, l'occlusion entre objets, la synchronisation avec le VBL et un vrai double tampon vidéo.

**Ne pas confondre** la V0.2 *programmée* avec une V0.2 *validée sur matériel*. Le programme assembleur est archivé, mais **aucun binaire .BIN ou .DSK testé n'est encore produit**.

### Vérification complémentaire du noyau de remplissage

L'émulation logique des spans sur les huit poses donne **846 segments longs** traités par la méthode `PUSH BC` et **1 807 segments courts**, pour **17 181 écritures d'octets de triangles**. Les contenus octet par octet de ces deux routines concordent avec le remplissage de référence. Aucune écriture de triangle hors VRAM CPC `&C000–&FFFF` n'a été relevée dans la simulation des données. Cela valide les plages d'adresses et l'ordre des écritures, **mais pas la durée, l'assemblage ni l'exécution sur le matériel**.


## Mise à jour — compilation RASM effectuée le 20 octobre 2026

Les deux versions **ont maintenant été assemblées avec succès**, à l'aide de **RASM 3.2.4bis** compilé depuis le dépôt officiel sur un environnement GitHub Actions Ubuntu. Les sources `.asm` ont été corrigées pour remplacer les littéraux hexadécimaux `&xx` par `#xx`, pris en charge par RASM. Résultats du [run GitHub Actions](https://github.com/glaby91/moteur3d/actions/runs/39394874757) : **V0.1 = 5 069 octets**, **V0.2 = 3 160 octets**, codes de sortie **0** ; les binaires et logs sont dans l'artefact `cpc6128-rasm-build`. **La compilation ne prouve pas le fonctionnement en émulateur CPC, qui reste à tester ; les FPS ne sont toujours pas mesurés.**
