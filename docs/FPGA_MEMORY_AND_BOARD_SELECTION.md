# Choix mémoire et carte FPGA — moteur 3D

**Date : 1er novembre 2026**  
**Statut : décision d’architecture provisoire — aucune carte n’est encore validée pour achat**

## Contraintes à conserver

- Le rendu CPU logiciel reste la référence fonctionnelle et visuelle.
- La cible matérielle peut fonctionner sur 1, 2 ou 4 FPGA ; 8 est le maximum et reste un cas rare.
- Le premier FPGA assure les fonctions complètes ; les travailleurs peuvent exécuter un sous-ensemble commun du pipeline.
- Éviter un pilote spécifique à chaque carte : conserver une famille de cartes et son runtime standard, plutôt que mélanger des familles aux outils et runtimes distincts.
- Objectif de coût exprimé : puce FPGA ≤ 200 €, carte complète avec mémoire environ 1 000–1 500 €.
- La GDDR est souhaitée. Elle ne doit pas être annoncée comme compatible avec une puce qui ne possède pas le contrôleur et le PHY adaptés.

## Options examinées

| Option | Mémoire et compatibilité | Coût / état | Conclusion |
|---|---|---|---|
| AMD Artix-7 XC7A100T | Contrôleur mémoire externe DDR3 documenté ; pas de contrôleur GDDR6 intégré. | Composant affiché autour de 112–147 € selon le boîtier et le distributeur ; prix du PCB PCIe assemblé non confirmé. | Option de prototype économique seulement si l’on accepte DDR3. Ne satisfait pas la demande GDDR. |
| Achronix Speedster7t | Contrôleurs GDDR6 intégrés ; la carte VectorPath S7t-VG6 est une cible commerciale possible. | Aucun prix public vérifié pour une carte complète à 1 000–1 500 € ni pour la puce à moins de 200 €. Le fabricant de la carte indique que ses outils ne sont plus maintenus pour les versions récentes des outils FPGA et systèmes d’exploitation. | Seule piste GDDR6 identifiée, mais pas une référence d’achat acceptable sans devis et vérification de la chaîne logicielle. |
| AMD Alveo U55C | HBM2 intégrée au boîtier, 16 Go et 460 Go/s ; ce n’est pas de la GDDR. | Prix affiché par AMD : 4 747 $US pour la carte. | Écartée : hors budget et mémoire différente de la GDDR demandée. |
| AMD Alveo U50 | HBM2, pas GDDR. | AMD a fixé au 30 octobre 2026 la date limite de commandes finales, sous réserve de disponibilité. | Écartée pour une nouvelle cible durable. |

## Décision provisoire

**Ne pas figer de référence de carte aujourd’hui.** Les contraintes « puce ≤ 200 € », « carte ≤ 1 500 € » et « mémoire GDDR » ne sont pas toutes satisfaites par une solution dont le prix, le support logiciel et l’approvisionnement ont été vérifiés.

La cible de conception conserve donc une interface mémoire indépendante du matériel. Le moteur pourra être testé sur le backend CPU ; une carte DDR peut servir à une mise au point FPGA seulement si l’écart avec la cible GDDR est explicitement accepté. La cible GDDR6 ne sera verrouillée qu’après obtention d’un devis pour la carte complète et confirmation du runtime, des pilotes standards, du support des outils et du fonctionnement multi-cartes.

## Interface mémoire à préserver dans l’architecture

Le pipeline ne doit pas coder en dur « DDR », « GDDR » ou « HBM ». Le backend FPGA expose au planificateur des opérations explicites :

- initialiser et sonder le périphérique, sa mémoire et ses banques ;
- charger une scène une fois, puis transférer des lots de tâches ;
- accéder à des plages alignées avec taille et stride définis ;
- synchroniser les écritures et publier l’état de complétion ;
- retourner les pixels et diagnostics d’une tuile ;
- signaler la capacité, l’alignement, le nombre de banques et les limites de débit mesurées.

La scène, les textures et les structures BVH résident en mémoire externe. Les petits tampons actifs, les files d’instructions, les données de tuiles et les fenêtres de travail N/N+1/N+2 sont candidats au cache local FPGA. La taille de chaque cache doit être calculée à partir des formats réels et du nombre de tuiles en vol, puis confirmée sur carte.

Le planificateur hôte répartit des tuiles pondérées entre 1, 2 ou 4 cartes et peut en gérer jusqu’à 8. Chaque carte annonce ses capacités ; la charge s’adapte au débit réellement mesuré. La prise en charge de 8 cartes n’est pas une promesse de débit linéaire.

## Points de passage avant sélection d’une carte

1. Obtenir un devis daté du composant, de la carte, de la mémoire, du refroidissement et des accessoires requis.
2. Vérifier les prix unitaires aux quantités 1, 2, 4 et 8.
3. Confirmer l’existence d’un runtime maintenu et d’un pilote standard pour le système hôte.
4. Confirmer que le même runtime sait énumérer et piloter les cartes choisies sans pilote maison par modèle.
5. Vérifier les limites électriques, thermiques, PCIe, la mémoire visible par le FPGA et les conditions d’installation.
6. Mesurer séparément les transferts, les caches, les étapes de calcul et le rendu complet sur matériel réel.
7. Comparer l’image et les compteurs au backend CPU de référence.

Aucun temps de rendu FPGA n’est estimé ici : aucune carte de cette architecture n’a encore été testée avec le moteur.

## Sources consultées

- [AMD Artix-7 — interfaces mémoire de la série 7](https://docs.amd.com/r/en-US/conversion-methodology/External-Memory-Interfaces)
- [DigiKey France — XC7A100T-1FTG256C, prix affiché](https://www.digikey.fr/en/products/detail/amd/XC7A100T-1FTG256C/3925807)
- [Achronix — Speedster7t et mémoire GDDR6](https://careers.achronix.com/high-speed-interfaces)
- [BittWare — VectorPath S7t-VG6, statut des outils](https://www.bittware.com/products/s7t-vg6/)
- [AMD — fiche U55C](https://docs.amd.com/r/en-US/ds978-u55c/Product-Details)
- [AMD — prix U55C affiché dans la boutique](https://shop-us-en.amd.com/adaptive-embedded-computing/data-center-accelerator-cards/)
- [AMD — avis de fin de commandes U50](https://docs.amd.com/v/u/en-US/XCN26004)
