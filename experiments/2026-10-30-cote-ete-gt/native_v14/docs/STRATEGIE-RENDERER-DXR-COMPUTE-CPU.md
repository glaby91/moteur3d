# Politique de rendu — DXR d'abord, Compute de secours, CPU en référence

**Décision architecturale :** 7 novembre 2026  
**Statut :** orientation de développement, **pas une fonctionnalité DXR livrée**.  
**Dépositaire :** moteur 3D natif Qt/C++ de Pripyat.

## Hiérarchie cible

1. **DXR matériel Direct3D 12**, si toutes les conditions sont réunies : adaptateur reconnu, `RaytracingTier` disponible, pilote adapté, et **backend DXR réellement implémenté, testé et conforme**.
2. **Ray tracing GPU Compute D3D11 / HLSL**, voie de repli sur les GPU compatibles D3D11 Feature Level 11_0 ne disposant pas de DXR matériel, ou si le DXR du moteur est absent/échoue. C'est un **vrai ray tracing par shaders**, et non un substitut raster.
3. **CPU ray tracing logiciel**, dernier recours lorsque les chemins GPU sont indisponibles ; il reste également la référence d'image et le moteur complet hors ligne.
4. **D3D11 raster existant**, voie distincte pour l'aperçu interactif, à conserver sans la confondre avec les trois chemins du ray tracing.

## État réel du code au moment de la décision

- `src/m3d_dxr.cpp` : la détection D3D12, `ID3D12Device5`, `D3D12_FEATURE_D3D12_OPTIONS5` et `RaytracingTier` est présente.
- **`capabilities.dxrBackend = false`** : aucun rendu DXR exploitable n'est implémenté aujourd'hui, même sur une carte matériellement compatible.
- `src/m3d_compute11.cpp` : calcul HLSL du ray tracing GPU déjà opérationnel et validé par Windows CI/WARP, testé sur l'Intel HD 620 avec un benchmark utilisateur.
- `src/moteur3d_qt.cpp` : mode Auto **actuellement orienté CPU**, tandis que Compute D3D11 reste sélectionnable explicitement.
- L'Intel HD Graphics 620 ne dispose pas de l'accélération matérielle DXR ; notre Compute D3D11 doit donc continuer de fonctionner sur ce GPU.
- Le raster D3D11 est déjà présent et **n'est pas à supprimer**.

**Ne jamais présenter la présence d'un adaptateur DXR comme une preuve que le moteur DXR tourne.**

## Roadmap conditionnelle pour un véritable DXR

### A. Contrat de détection robuste

- Énumérer les adaptateurs avec DXGI : ne pas confondre adaptateur DXR et adaptateur par défaut, gérer les ordinateurs multi-GPU.
- Lire le `RaytracingTier` (1.0 ou 1.1) et la prise en charge du pilote.
- Ne valider `dxrBackend=true` que lorsque le moteur effectue réellement un rendu correct et que le backend est compilé/activé.

### B. Chaîne DXR à développer

- `ID3D12Device5`, commandes/queues, gestion des ressources, barrières et synchronisation.
- Structures d'accélération **BLAS/TLAS**, adaptées aux primitives du moteur ; triangles matériels et intersections procédurales pour les primitives analytiques, lorsque nécessaires.
- Bibliothèque de shaders DXIL (ray generation, miss, closest-hit/any-hit et intersection), pipeline state, shader tables.
- Matériaux, PBR, transformations, caméras, éclairage, ciel, transparence : parité contrôlée avec les capacités ciblées.
- Affichage Qt et choix entre présentation de texture GPU et lecture CPU selon l'architecture de fenêtre.

### C. Repli fiable

- À l'initialisation : **DXR si pilote + GPU + backend complet validés ; sinon Compute ; sinon CPU**.
- En cas d'échec d'un backend, libérer correctement ses ressources et réessayer le suivant ; rapporter les raisons techniques à l'utilisateur.
- Ne pas basculer silencieusement vers une voie visuellement moins fidèle : afficher les fonctionnalités non encore équivalentes et permettre une sélection manuelle.
- Ne jamais considérer le raster D3D11 comme du ray tracing DXR.

### D. Validation, sans métriques inventées

- Compiler Windows et valider les tests automatisés du backend Compute (WARP).
- Détecter la compatibilité matérielle DXR par interrogation du pilote ; un test DXR réel exige une carte compatible.
- Comparer les images à scène, résolution, heure, cadrage et matériaux identiques ; tolérances justifiées.
- Benchmarker FPS, médiane, P95, coût GPU et transfert GPU→CPU, en séparant le coût du shader et celui de l'affichage.
- Conserver le HD 620 comme machine témoin **sans DXR** : test systématique du chemin Compute puis CPU.

## Décision de périmètre

**Le Compute D3D11 doit être robuste et performant, mais n'est pas notre voie matérielle ultime.** À long terme, le chemin DXR matériel est prioritaire si une vraie accélération du ray tracing et une qualité conforme sont possibles. On **ne développe pas OpenGL** maintenant et on ne multiplie pas les backends sans justification.

Cette note ne modifie **aucune sélection de backend dans la build actuelle**. Le mode Auto restera tel qu'implémenté jusqu'à une demande explicite de changement et des essais de qualité sur matériel réel.
