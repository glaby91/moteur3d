# V17 — audit CPU des trajets lumineux de la fresque

Le workflow Windows produit un audit géométrique sur la scène finale du paquet, après l’installation de la coupole. Le script échantillonne six points sur la face de la fresque tournée vers la piscine, puis teste des trajets 3D vers des points de la surface de l’eau et du vitrage. Les rayons visibles et les premiers obstacles sont consignés dans un SVG, un CSV et un JSON téléchargeables comme artefact GitHub Actions.

Le SVG est une projection en coupe Y-Z ; les intersections, elles, sont calculées en 3D avec les boîtes et maillages triangulés du paquet. L’audit répond à une question précise : la géométrie laisse-t-elle des trajets de transfert entre la fresque, l’eau et la coupole ?

Il ne remplace pas le calcul d’éclairage du moteur : il ne simule pas le BRDF, le Fresnel, la transmission du verre ou de l’eau, ni le transport Monte-Carlo complet. Le rapport inspecte aussi le code de GI CPU. Dans le code actuellement audité, `m3d_gi_bake.cpp` obtient l’albédo avec un texel du mip le plus grossier, sans UV d’intersection ; les motifs colorés détaillés de la fresque sont donc moyennés dans les rebonds précalculés.

## Exécution

```powershell
python tools/gi_fresco_ray_audit.py --self-test
python tools/gi_fresco_ray_audit.py --package dist-v17/benchmark-civic --output build-v17/fresco-ray-audit
```

Le test échoue si aucun trajet échantillonné n’atteint l’eau ou la coupole. Un trajet visible combiné à l’absence de rebond coloré dans l’image oriente ensuite l’enquête vers le traitement GI/PBR plutôt que vers une occultation géométrique totale.
