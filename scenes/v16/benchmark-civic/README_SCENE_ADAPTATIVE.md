# Campagne adaptative de la scène — moteur V17.370 figé

Cette campagne ne modifie **aucun fichier du moteur** (C++/HLSL/Qt). Elle utilise le programme Windows x64 V17.370 déjà validé (archive GitHub Actions issue du run `43195320997`) et ne crée que des variantes des fichiers de scène JSON.

## Méthode recommandée : GitHub Actions

Ouvrir [Scene-only adaptive campaign](https://github.com/glaby91/moteur3d/actions/workflows/scene-palais-adaptive.yml).

- Lorsqu'elle est disponible sur la branche par défaut, cliquer **Run workflow**, choisir la branche contenant la campagne et **10 passes**.
- Sur la branche actuelle `feature/v17-365-paysage-monument-sobre-261112`, une modification de `adaptive_campaign.py` déclenche également une exécution automatiquement.
- Le workflow télécharge le **même moteur V17.370** et son dossier `benchmark-civic` ; il ne lance ni CMake, ni Visual Studio, ni compilation.

Les résultats sont publiés sous l'artefact **CSE-WARP-V17.370-scene-adaptive-<run_number>** : pour chaque passe, cinq PNG (façade, monument, vue aérienne, piscine à 19 h, hall), un fichier `review.json`, ainsi qu'un CSV récapitulatif `passes.csv`. La variante retenue est indiquée dans `result.json` par `accepted_scene`.

## Exécution en local sur Windows

Décompresser l'archive **CSE-WARP-V17.370-Windows-x64** dans un dossier `fixed-v17`, installer Python avec Pillow, puis ouvrir un terminal à la racine du dépôt :

```powershell
py -m pip install Pillow==11.3.0
py scenes/v16/benchmark-civic/adaptive_campaign.py `
  --engine fixed-v17/moteur3d_v17_benchmark.exe `
  --scene fixed-v17/benchmark-civic `
  --output resultats/scene-adaptative `
  --passes 10
```

**Aucune modification de l'exécutable n'est requise.**

## Comment la correction est choisie

La boucle lit les *véritables* PNG de la passe précédente, calcule des indicateurs par caméra (contraste local, détails de contours, zones de végétation, saturation) et examine le nombre de pièces géométriques. Elle choisit alors le défaut prioritaire restant et ajuste **uniquement les objets concernés dans une copie de la scène**.

Les réparations peuvent ajouter des instances 3D : branches et petits houppiers, tiges et pétales, parties anatomiques du monument, plis de banderoles, reliefs de façade, détails de piscine ou de hall. Chaque candidat est rendu et comparé à la scène acceptée précédente. Un candidat inefficace ou causant une régression importante est écarté.

**Limite importante :** les indicateurs d'image sont des mesures automatisées et ne remplacent pas un jugement esthétique humain. Les concepts initiaux restent des références artistiques, pas des images à recopier pixel à pixel. Les objets demeurent construits avec les primitives que sait gérer le moteur ; pour un bond de réalisme, il faudra des modèles plus complexes.

## Protection de la scène

- La scène de base et ses deux véritables textures de mosaïque sont copiées sans altération.
- L'architecture principale, le bassin 50 × 25 m et les caméras restent fixes.
- Les corrections acceptées sont **des fichiers de scène en sortie**, pas des modifications du moteur.
- Pour promouvoir une variante définitivement, vérifier les cinq images puis recopier les fichiers JSON retenus vers la branche de la scène uniquement.
