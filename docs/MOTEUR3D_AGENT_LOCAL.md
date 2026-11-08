# Agent local Moteur3D — prototype v1

Cet agent Python standard-library pilote l'exécutable Qt **sans cliquer dans l'interface**. Il écoute uniquement sur `127.0.0.1` et exige un jeton Bearer. Il ne fournit **pas encore** de connecteur ChatGPT distant : une passerelle sortante sécurisée et autorisée sera nécessaire.

## Prérequis

- Windows, Python 3.10+ et une distribution compilée de `moteur3d_qt.exe`.
- Les scènes `.bin`, `scene_pripyat_views.json` et ressources nécessaires doivent se trouver dans le dossier runtime (comme dans le paquet GitHub Actions).
- Pour Compute GPU, un pilote D3D11 compatible ; pour un test WARP, définir `M3D_COMPUTE_WARP=1` avant le lancement de l'agent.

## Démarrage (PowerShell)

Depuis la racine du dépôt, adapter le dossier `dist` :

```powershell
$env:M3D_AGENT_TOKEN = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
$env:M3D_COMPUTE_WARP = "1" # optionnel : forcer WARP plutôt que le GPU
python tools/moteur3d_local_agent.py --exe "C:\\Moteur3D\\dist\\moteur3d_qt.exe" --runtime "C:\\Moteur3D\\dist" --port 8765
```

Conserver le jeton dans le terminal, ne jamais le committer. Dans un **second** PowerShell :

```powershell
$token = "<recopier la valeur M3D_AGENT_TOKEN du premier terminal>"
$headers = @{ Authorization = "Bearer $token" }
Invoke-RestMethod http://127.0.0.1:8765/api/status -Headers $headers
$job = Invoke-RestMethod http://127.0.0.1:8765/api/render -Method Post -Headers $headers -ContentType "application/json" -Body '{"camera":12,"backend":"cpu"}'
Invoke-RestMethod "http://127.0.0.1:8765$($job.status_url)" -Headers $headers
# Quand status == completed :
Invoke-WebRequest "http://127.0.0.1:8765/api/jobs/$($job.job_id)/image" -Headers $headers -OutFile "capture.png"
```

Pour le rendu Compute, utiliser `{"camera":12,"backend":"d3d11_compute"}`. Les caméras actuellement admises vont de 1 à 13.

## API v1

| Méthode | Route | Fonction |
| --- | --- | --- |
| GET | `/api/status` | Statut de l'agent |
| POST | `/api/render` | Démarre un rendu asynchrone ; renvoie HTTP 202 et un ID |
| GET | `/api/jobs/{id}` | Statut, durée, erreur éventuelle |
| GET | `/api/jobs/{id}/image` | PNG si terminé |

Seules les options `--snapshot` et `--snapshot-compute` de l'exécutable sont utilisées. Pas d'exécution de commande libre, pas de chemin arbitraire, un seul rendu à la fois, durée maximale 15 minutes, 100 jobs par session. L'agent doit être arrêté pour vider son historique. Les captures sont écrites dans `dist/remote-renders/`.

**Sécurité :** ne pas ouvrir le port 8765 sur le routeur, ne pas remplacer `127.0.0.1` par `0.0.0.0`, ne pas exposer cet HTTP non chiffré sur Internet. Une future passerelle ChatGPT devra appliquer authentification, TLS, approbation des actions et limitation des accès.

**Limites :** la durée renvoyée est celle du processus entier, pas une mesure GPU isolée. L'API n'implémente pas encore les benchmarks ni le changement de résolution ; ces options exigent des points d'entrée natifs confirmés dans le moteur.
