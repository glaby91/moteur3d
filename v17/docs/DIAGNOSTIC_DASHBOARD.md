# Tableau de bord de diagnostic CSE WARP

## État

- Fonction intégrée dans la branche `feature/v17-diagnostic-dashboard-tab-20261123`, créée depuis `master` au commit `0189e6bff9f33916e6e17c9e104e6ea57458b84b`.
- Révision moteur proposée : **V17.004**. La scène n’est pas modifiée ; sa révision demeure celle du manifeste chargé.
- Aucun build ou test n’a été lancé pour ce changement.

## Utilisation prévue

Pendant l’exploration, `Tab` ouvre le tableau de bord plein écran. `Tab` revient au viewport et réactive le regard souris. `Échap` conserve sa fonction habituelle : il arrête le mode caméra et le tableau de bord se ferme avec lui. `F11` garde sa fonction actuelle. Le menu **Affichage → Tableau de bord moteur plein écran** permet aussi de l’ouvrir.

Les pages sont **Aperçu**, **Scène**, **Lumières**, **Ressources** et **Diagnostic**. Le rafraîchissement se fait à 750 ms uniquement lorsque le panneau est visible.

## Origine des mesures

| Mesure | Source |
|---|---|
| Version moteur | Version produit compilée depuis `v17/version.json` |
| Révision moteur | `REVISION.txt` embarqué, ou « développement local » si absent |
| Révision de scène | Champ `revision` retourné par le manifeste chargé |
| Objets source | Objets du runtime V16 construit |
| Triangles | Nombre d’éléments de la scène de rendu triangulée |
| Matériaux / albedos image / lumières | Structures de la scène de rendu |
| FPS | Intervalles de présentation mesurés pendant l’exploration |
| Dernier temps moteur | Durée fournie par le dernier rendu accepté |
| Résolution et caméra | Viewport actif et pose courante |
| RAM système | Charge mémoire physique retournée par Windows |
| Mémoire du processus / VRAM | Non instrumentées, affichées comme indisponibles |
| Erreur de présentation | État d’échec natif DX11/Vulkan/LumaTrace déjà suivi par l’interface |

En présentation directe, le temps du rendu peut mesurer le travail CPU de soumission et ne prouve pas la fin d’exécution GPU. Le panneau le rappelle explicitement.

## Reprise

1. Reprendre la branche ci-dessus.
2. Compiler le projet V17 selon le workflow Windows existant.
3. Vérifier manuellement l’ouverture/fermeture par `Tab` et `Échap`, le retour du contrôle caméra, le maintien du comportement `F11`, ainsi que les révisions, compteurs et états d’indisponibilité.
4. Vérifier sur une scène avec plusieurs matériaux/lumières que les valeurs affichées correspondent aux structures chargées.
5. Ne fusionner dans `master` qu’après revue et validation du rendu plein écran.

La scène n’a pas été retouchée par cette fonctionnalité.
