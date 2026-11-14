# Rendu CPU hors-ligne

Le renderer CPU reste autonome et complet. Il ne nécessite ni DirectX, ni GPU. Le mode **CPU hors-ligne PBR/HDR** cible d’abord les images fixes ; l’export par lots des animations reste à intégrer.

## Fonctions intégrées

- 64 échantillons stochastiques par pixel, jusqu’à 3 rebonds diffus, avec roulette russe.
- BRDF PBR de première passe : distribution GGX, Fresnel Schlick, rugosité issue de la carte existante et métallicité estimée depuis `reflectivityPercent`.
- Décodage sRGB vers linéaire pour les couleurs des textures et accumulation flottante HDR pendant le calcul.
- Tone mapping ACES et encodage sRGB à l’affichage.
- Filtre bilatéral CPU 5×5 après rendu, activé uniquement pour ce mode.

## Limites actuelles

- Les textures de couleur source restent RGB565/RGB 8 bits ; il n’y a pas encore de carte de métallicité dédiée.
- La métallicité, l’émission et certaines propriétés de matière sont encore estimées à partir des noms de matériaux et des anciens champs de scène.
- Le framebuffer affiché est 8 bits après tone mapping ; l’export OpenEXR ou HDR n’est pas encore ajouté.
- Le débruiteur utilise couleur et luminance, sans guides de normales ou d’albédo. Il peut lisser les détails fins.
- Le modèle reste expérimental. Il ne garantit pas encore une qualité photo sur toutes les scènes ; les textures, les lumières surfaciques, les matériaux et l’échantillonnage demandent encore du travail.

## Détection DXR

Au lancement sous Windows, le moteur interroge D3D12 et le niveau DXR exposé par l’adaptateur. Cette détection ne sélectionne pas un renderer GPU tant que le backend DXR du moteur n’existe pas : le mode Auto utilise donc le CPU. L’aperçu DirectX 11 raster est une option séparée. Le renderer CPU fonctionne aussi hors Windows.

## Étapes suivantes

1. Ajouter des paramètres matériaux explicites et des cartes métallicité/rugosité haute résolution.
2. Échantillonner les lumières surfaciques et l’environnement avec une stratégie de bruit faible.
3. Améliorer le débruiteur avec des guides albédo/normales.
4. Ajouter un export HDR/EXR et le rendu hors-ligne par lots des séquences d’animation.
5. Ajouter un backend DXR partageant les mêmes matériaux, en gardant le CPU comme solution complète.

