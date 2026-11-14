# Energetik — arbre stable pièces → objets → matériaux (V15 Beta 3)

**Source de vérité fonctionnelle.** Cette arborescence est le référentiel d'affectation. Ne pas modifier les identifiants des matériaux ni déplacer les objets entre pièces sans mettre à jour ce document. Les emplacements non vérifiés sont signalés *hypothèse*.

## Arbre

```text
PRIPYAT_CENTRE
├── EXTERIEUR
│   ├── Place et trottoirs
│   │   ├── chaussée → asphalt
│   │   ├── dalles et bordures → stone / concrete
│   │   └── joints, réparations, gravats → concrete / debris
│   └── ENERGETIK / façade
│       ├── murs et poteaux → concrete / plaster / volcanicTuff
│       ├── pierres et soubassements → stone
│       ├── coulures et fixations → rust / metal
│       ├── fenêtres et cadres → glass / windowdark / windowFrame [à affecter]
│       └── enseigne lumineuse → sign / neon (asset spécifique)
└── ENERGETIK / INTERIEUR
    ├── RDC / HALL ET FOYER
    │   ├── sol → floorTerrazzo
    │   ├── murs et parements → plaster / paintPeeling / volcanicTuff
    │   ├── colonnes et marches → stone / concrete
    │   ├── garde-corps → rust / metal
    │   ├── plafond dégradé → concrete / debris / ceilingFlaked [à affecter]
    │   └── fresque historique → energetikFresco (texture dédiée, ne pas remplacer)
    ├── RDC / AUDITORIUM
    │   ├── sol usé → floorConcreteDust
    │   ├── murs et parements → volcanicTuff / plaster
    │   ├── gradins → concrete / stone
    │   ├── fauteuils → seatVinylBrown / seatFabric / seatFabricWorn [partiel]
    │   ├── structure métallique → metal / rust
    │   └── plafond et débris → concrete / debris / ceilingFlaked [à affecter]
    ├── RDC / SCENE
    │   ├── plateau, marches, lames → stageWood
    │   ├── rideaux → curtain / seatFabricWorn (zones déchirées)
    │   ├── écran → cinemaScreen
    │   └── encadrement et coulisses → volcanicTuff / rust / paintPeeling
    ├── RDC / COULOIRS ET SERVICE (distribution approximative)
    │   ├── sol → floorTerrazzo / floorCeramic
    │   ├── enduits dégradés → paintPeeling / wallDamp
    │   ├── soubassements carrelés → wallCeramic
    │   ├── portes → doorPaint [à affecter]
    │   └── canalisations → pipePaint
    ├── ETAGE / SALLE DE SPORT (emplacement hypothétique)
    │   ├── sol → floorGym
    │   ├── espaliers et bancs → wood / metal
    │   ├── ring / plateau → wood
    │   ├── ring / toile → boxingCanvas
    │   └── ring / cordages → boxingRope
    ├── ETAGE / BIBLIOTHEQUE-CLUB (emplacement hypothétique)
    │   ├── sol → floorParquet
    │   ├── cloisons → paintPeeling / wallDamp
    │   ├── rayonnages, tables et bancs → wood / floorParquet [partiel]
    │   └── papiers et livres → paperAged [à affecter]
    ├── ETAGE / COULOIR EST
    │   ├── sol → floorCeramic
    │   ├── cloisons → plaster / concrete
    │   └── vitrages → glass / windowdark
    └── ESCALIERS ET TOITURE
        ├── marches et paliers → stone / concrete
        ├── garde-corps → rust / metal
        ├── murs → concrete / paintPeeling [partiel]
        └── toiture et éléments techniques → concrete / rust / pipePaint [partiel]
```

## Règles d'évolution
1. Une pièce possède un identifiant fonctionnel stable (ex. `RDC_AUDITORIUM`).
2. Un objet a un rôle stable (ex. `RDC_AUDITORIUM.SOL`) et référence un **ID de matériau**, jamais directement un fichier .gz.
3. Un matériau pointe vers un seul asset source `<ID>_2048.m3dpbr.gz`, avec mipmaps albedo / normales / rugosité.
4. Les variantes locales (salissure, cassure, humidité) doivent être des objets/surfaces dédiés, sans remplacer le matériau global utilisé ailleurs.
5. Toute nouvelle affectation doit modifier **à la fois** la scène, cet arbre et la table machine `energetik_room_materials.json`.
6. Le rendu de la fresque, la transparence du verre et les néons conservent leurs shaders/ressources dédiés.
7. *[à affecter]* signifie que le matériau existe mais n'est pas encore lié à toutes les surfaces pertinentes. Ne pas le présenter comme terminé.

## Références
- Source scène : `experiments/2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js`.
- Exporteur : `experiments/2026-10-30-cote-ete-gt/native_v14/export_scene.js`.
- Générateur : `experiments/2026-10-30-cote-ete-gt/native_v14/tools/generate_energetik_room_pbr.py`.
