# Cœurs AMD Athlon et Zen : continuité et rupture

## À retenir

**Athlon est une marque commerciale, pas un cœur unique.** Elle a couvert plusieurs générations de processeurs AMD. Comparer « un Athlon » à « Zen » demande donc de préciser le modèle et sa microarchitecture.

## Ce qui se retrouve d’une génération à l’autre

- La compatibilité avec l’ISA x86; avec l’Athlon 64, AMD a introduit AMD64, l’extension 64 bits ensuite conservée et étendue dans les processeurs Zen.
- Les briques générales d’un processeur moderne : prédiction de branchement, caches, unités entières et flottantes, et exécution hors ordre. Leur organisation, leur largeur et leur rendement ont beaucoup évolué.
- L’intégration progressive de fonctions auparavant externes au cœur. L’Athlon 64 disposait déjà d’un contrôleur mémoire intégré et d’HyperTransport.

Ces continuités ne signifient pas que Zen reprend le cœur Athlon 64 : son microarchitecture a été redessinée.

## La rupture des générations Bulldozer et Zen

La génération Bulldozer (famille AMD 15h), rencontrée dans certains FX et Athlon, organisait deux blocs entiers au sein d’un module qui partageait certaines ressources. Cette organisation pouvait économiser de la surface, mais les deux fils d’exécution se disputaient des ressources communes selon la charge.

Zen a réorganisé le cœur autour d’une nouvelle microarchitecture et du SMT, où un cœur physique peut exécuter deux fils matériels. Les produits Zen utilisent aussi une interconnexion et, selon la gamme, des chiplets pour assembler plusieurs groupes de cœurs. Les détails changent selon la génération et le modèle.

## Conséquence pour notre moteur

Le nom Athlon ou Zen ne suffit pas à prédire le gain d’une optimisation. Le moteur doit vérifier les extensions réellement exposées par le processeur et le système (par exemple AVX2 et FMA3), puis mesurer chaque chemin sur la machine cible. C’est la raison pour laquelle le benchmark compare x64 standard et AVX2 + FMA3 avec les mêmes réglages.

## Sources AMD

- [Fiche technique officielle de l’Athlon 64](https://www.amd.com/content/dam/amd/en/documents/archived-tech-docs/datasheets/24659.pdf)
- [Guide AMD Family 15h — modèles 00h–0Fh](https://www.amd.com/content/dam/amd/en/documents/archived-tech-docs/programmer-references/42301_15h_Mod_00h-0Fh_BKDG.pdf)
- [Architecture de cœur AMD « Zen »](https://www.amd.com/en/technologies/zen-core.html)
