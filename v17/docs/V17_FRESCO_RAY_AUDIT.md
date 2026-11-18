# V17 — audit CPU des trajets lumineux de la fresque

Le workflow Windows produit un audit géométrique sur la scène finale du paquet, après l’installation de la coupole. Le script reconnaît les deux formats livrés de la fresque (un matériau unique ou huit panneaux triangulés), échantillonne sa face tournée vers la piscine, puis teste des trajets 3D vers l’eau et le vitrage. Les points d’eau sont pris au centre des couloirs, entre les lignes flottantes. Les rayons et premiers obstacles sont consignés dans un SVG, un CSV et un JSON téléchargeables comme artefact GitHub Actions.

Le SVG est une projection en coupe Y-Z ; les intersections, elles, sont calculées en 3D avec un BVH construit sur les boîtes et maillages triangulés du paquet. L’audit répond à une question précise : la géométrie laisse-t-elle des trajets de transfert entre la fresque, l’eau et la coupole ?

Il ne remplace pas le calcul d’éclairage du moteur : il ne simule pas le BRDF, le Fresnel, la transmission du verre ou de l’eau, ni le transport Monte-Carlo complet. Le rapport inspecte aussi le code de GI CPU. Dans le code actuellement audité, `m3d_gi_bake.cpp` obtient l’albédo avec un texel du mip le plus grossier, sans UV d’intersection ; les détails de couleur à l’intérieur de chaque matériau de fresque sont donc réduits à leur texel moyen pour les rebonds précalculés.

## Résultat de référence sur le paquet approuvé

Sur l’archive Windows approuvée V17.444, après installation CPU de la coupole V17.523 et sans compiler le moteur, le test a échantillonné huit panneaux de fresque et 36 points d’eau plus 24 points de vitrage par panneau. Il trouve 96/96 trajets géométriques libres vers l’eau et 146/192 vers la coupole. Les 46 trajets bloqués rencontrent principalement la toiture (28), puis la costière de la coupole (8), les poutres du plafond (6) et les lisses (4). Le choix de points d’eau au centre des couloirs évite de tester accidentellement les lignes flottantes.

Ce résultat montre que la géométrie offre des trajets directs depuis les panneaux échantillonnés. Il ne prouve pas que le calcul CPU donne la bonne énergie ou couleur au rebond ; le chemin d’albédo sans UV reste une piste distincte et concrète à corriger dans le moteur.

## Correctif PBR V17.541

Le CPU PBR traite désormais les matériaux albedo-image sans hériter des normales et de la rugosité procédurales du motif de génération. La Fresnel renforcée est réservée aux matériaux explicitement déclarés avec `surfaceModel: "water"`; un vitrage ne devient donc pas de l’eau du seul fait que son IOR est proche. Le générateur de la scène livrée marque explicitement l’eau du bassin.

## Exécution

```powershell
python tools/gi_fresco_ray_audit.py --self-test
python tools/gi_fresco_ray_audit.py --package dist-v17/benchmark-civic --output build-v17/fresco-ray-audit
```

Le test échoue si aucun trajet échantillonné n’atteint l’eau ou la coupole. Un trajet visible combiné à l’absence de rebond coloré dans l’image oriente ensuite l’enquête vers le traitement GI/PBR plutôt que vers une occultation géométrique totale.
