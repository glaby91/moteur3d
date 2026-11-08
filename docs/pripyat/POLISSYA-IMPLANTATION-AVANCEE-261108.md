# Polissya — orientation de l'avancée et de l'arcade (08/11/2026)

## Erreur identifiée puis corrigée

Une première série d'exports, bien que techniquement valide, plaçait **la longue
avancée du rez-de-chaussée parallèlement à la façade principale** du Polissya.
Elle paraissait donc longer l'hôtel dans les vues aériennes de notre Moteur3D.

Les photographies frontale et oblique annotées par l'utilisateur montrent au
contraire **une extension basse qui PART DE LA FAÇADE VERS L'ESPLANADE**, avec
une galerie indépendante courbe atteignant son extrémité côté place.

La correction a nécessité **une rotation de 90° de l'axe longitudinal de
l'avancée**, et le recalage de l'arrivée de l'arcade. Cette erreur ne pouvait
pas être corrigée par des textures ou un changement de caméra.

## Emprises actuelles (coordonnées de scène, mètres ; ESTIMATIONS)

- Hotel principal (monde) : X=82…104, Z=-12…26.
- Sa longue façade sur la place est sur le plan X≈82.
- Extension basse : **axe longitudinal X≈60…82, orienté vers la place**.
- Largeur transversale (Z) de l'extension : **17,5…22,9** (5,4 m).
- Recul transversal de l'extrémité proche de l'angle de l'hôtel :
  Z=26−22,9≈**3,1 m**, ordre de grandeur d'une travée de fenêtre.
- Centre du dernier segment de l'arcade : près de X≈57,7, Z≈17,2.
- Les deux structures ont un petit espace libre à leur rencontre visuelle :
  **ne pas ajouter de raccordement physique artificiel**.

## Détails à conserver

- Toiture de l'arcade courbe en plan, caissons réels et rebords.
- Poteaux carrelés, soubassements brun-rouille (matériau non identifié).
- Quatre marches larges et peu hautes depuis l'esplanade, suivant la courbe.
- L'avancée de Polissya est un volume bas et long, bandeau en béton,
  piliers et façade en retrait, avec maçonneries/baies.
- Passage latéral avec rambarde métallique dégradée et petit escalier
  distinct de quatre marches principales.
- Haut du Polissya et panorama : tâche séparée et non prioritaire.

## Vérification

Le test `experiments/pripyat-survey/test-polissya-arcade.mjs` vérifie
explicitement que la grande dimension de l'avancée suit **X** (direction de
l'esplanade) et non **Z** (longueur de façade de l'hôtel). La QA automatique
teste la validité de la construction ; les rendus réels doivent encore
être confrontés aux photographies de référence.

**Important :** toutes les dimensions sont des estimations photoguidées,
pas un relevé architectural ou cadastral.
