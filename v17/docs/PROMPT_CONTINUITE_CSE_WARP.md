# Consigne de continuité — CSE WARP

Tu travailles sur le dépôt `glaby91/moteur3d`. Le but est de livrer le changement demandé sur la bonne révision, avec un résultat vérifié et sans perdre les correctifs déjà présents.

## Avant toute modification ou publication

1. Lire la demande active et conserver les contraintes déjà données : révision visée, fichiers concernés, compiler ou non, et comportement attendu.
2. Vérifier le dépôt, la branche, `git status`, `HEAD` et la tête distante de la branche. Comparer l’historique avant d’éditer. Ne jamais supposer que la copie locale est à jour.
3. Quand une écriture GitHub utilise les objets Git, créer le nouvel arbre depuis **l’arbre exact du commit attendu** (`<commit>^{tree}`), puis créer le commit avec ce commit comme parent. Contrôler le SHA attendu au déplacement de la référence. Ne pas utiliser le parent du commit par erreur.
4. Après toute publication, récupérer la branche et vérifier des fichiers de contrôle complets (au minimum le début, la taille et l’absence de texte de troncature) ainsi que le diff du commit. Un envoi accepté par l’API n’est pas une validation du contenu.

## Vérifier les données réelles

5. Pour un test de scène, examiner le **paquet final réellement livré** après tous les overlays et installateurs. Ne pas déduire les identifiants de matériaux, objets ou géométries depuis le générateur seul. Confirmer les chemins, matériaux, transforms, triangles et la révision directement dans le paquet.
6. Prendre en charge les variantes historiques réellement présentes dans les paquets (par exemple une fresque unique `cse_energetik_fresco` ou huit panneaux `v7_fresco_*`). Exclure les murs de support et autres faux positifs.
7. Définir clairement ce que mesure le test. Un audit de visibilité géométrique ne prouve pas le transport d’énergie, le BRDF, le Fresnel ou une image rendue. Ne jamais présenter un contrôle source ou synthétique comme un rendu du moteur.
8. Tester d’abord les maths et le script sur l’archive réelle approuvée. Inspecter les résultats produits et leur cohérence avant de modifier le workflow. Garder un test synthétique complémentaire, sans le substituer au paquet réel.

## Build et compte rendu

9. Respecter la consigne de compilation : ne pas compiler localement si l’utilisateur l’a interdit. Une demande explicite de test GitHub autorise le workflow nécessaire, même s’il compile ; signaler clairement ce fait.
10. Sur échec CI, relever la première erreur causale, distinguer un défaut du changement d’un défaut préexistant, corriger le minimum nécessaire et relancer. Ne pas compter l’échec d’un autre composant comme test réussi.
11. Avant de conclure, vérifier l’état final du workflow, que le test ciblé a effectivement été exécuté, les artefacts attendus et leur contenu. Indiquer les limites et les révisions exactes.

## Garde-fous de version

- Pas de force-push sans besoin précis et sans `expected_sha` vérifié.
- Pas de remplacement ou réécriture d’un arbre distant complet pour ajouter quelques fichiers ; préserver explicitement tous les fichiers de la base courante.
- Pas de lien de téléchargement annoncé avant vérification de l’artefact et de son nom.
- Si une hypothèse vient d’être réfutée par le paquet réel, corriger le script avant toute nouvelle publication.
