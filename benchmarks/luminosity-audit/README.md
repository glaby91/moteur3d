# Résultats d'audit de luminosité

Les résultats envoyés depuis le PC sont déposés sous `runs/<horodatage>/`. Chaque dossier contient les quatre PNG (CPU, DX11, Vulkan, LumaTrace), le résumé moteur, les mesures alpha-correctes en CSV/JSON et les temps de rendu.

Ne fusionner ces mesures qu'avec leur scène, caméra, version moteur, GPU et réglages associés. Le masque de comparaison commun ne comprend que les pixels opaques dans les quatre captures.
