# Rendu CPU hors-ligne

Le renderer CPU reste le chemin complet et autonome du moteur. Il ne nécessite ni DirectX, ni GPU. Le mode **CPU hors-ligne** ajouté dans V14 est un premier renderer stochastique : il échantillonne les pixels aléatoirement, prend en charge la profondeur de champ et estime l'éclairage indirect diffus par plusieurs rebonds. Il vise d’abord les images fixes. Le rendu hors-ligne des séquences d’animation reste à intégrer.

## État de cette première étape

- Le CPU calcule les intersections avec le BVH existant et peut utiliser les threads configurés.
- Le préréglage de qualité hors-ligne lance 64 échantillons par pixel et 3 rebonds diffus, avec arrêt anticipé par roulette russe.
- Le rendu principal, les reflets directs et les matériaux transparents continuent d'utiliser le shading V14 existant.
- Les textures actuelles restent en RGB 8 bits et le framebuffer de sortie reste en RGB 8 bits.
- Le modèle de matériaux et l'estimation de l'éclairage indirect ne constituent pas encore un renderer PBR photoréaliste. Le mode est expérimental et peut être lent en haute résolution.

## Détection DXR

Au lancement sous Windows, le moteur interroge D3D12 et le niveau DXR exposé par l'adaptateur. Cette détection ne déclenche pas à elle seule un rendu GPU : la sélection automatique reste CPU tant que le backend DXR propre au moteur n'existe pas. L'aperçu DirectX 11 raster reste une option séparée. Hors Windows, le renderer CPU reste disponible sans dépendance à DirectX.

## Étapes graphiques suivantes

1. Passer les textures et calculs d'éclairage dans un espace linéaire à haute dynamique et ajouter l'export HDR.
2. Définir un modèle PBR cohérent (albédo, métallicité, rugosité, normales et émission) partagé par tous les renderers.
3. Ajouter l'échantillonnage des sources lumineuses, une meilleure gestion de la réfraction et un débruiteur pour réduire le nombre d'échantillons nécessaires.
4. Ajouter un backend DXR qui consomme les mêmes scènes et paramètres, en gardant le renderer CPU comme référence et solution complète.
5. Rendre les images et animations par lots, avec progression, reprise et export d'images sans limite temps réel.

Le format de scène et le chemin CPU restent utilisables quand aucun GPU compatible n'est présent.
