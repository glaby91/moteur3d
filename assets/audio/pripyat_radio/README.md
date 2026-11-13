# CSE WARP — Radio ukrainienne (ressource optionnelle)

Le module fabrique **une bande-son reconstituée de ~33 min 04 s**, avec voix en ukrainien
et un morceau chanté, à partir de **trois fichiers Ogg** de Wikimedia Commons.
Ce n'est **ni une émission complète enregistrée à la radio, ni une archive de 1970–1986**.
Il sert de prototype d'ambiance pour la scène Pripiat. Aucun enregistrement protégé
de Radio Kiev / Suspilne n'est inclus.

## Production (Windows, Linux)

1. Installer Python 3.9+ et FFmpeg (`ffmpeg` dans le PATH).
2. Depuis ce dossier :
   ```sh
   python build_radio.py --dry-run
   python build_radio.py
   ```
3. Le fichier est généré dans
   `generated/ukrainian_radio_reconstruction.ogg`.
   Les sources téléchargées sont mises en cache dans `cache/`.

`python build_radio.py --download-only` récupère les fichiers originaux sans traitement.
L'étape de téléchargement nécessite l'accès à Wikimedia Commons ; **l'exécution
du moteur 3D ne doit pas avoir besoin d'Internet**.

## Intégration prévue dans la scène

L'audio doit être traité comme une ressource **facultative**, chargée seulement si
`generated/ukrainian_radio_reconstruction.ogg` existe. Pour un émetteur radio spatial :

- Positionner une source mono dans la pièce près d'un poste radio.
- Boucle activable, lecture/pause et volume réglables (valeur initiale modérée).
- Atténuation avec la distance et les murs selon les fonctionnalités audio existantes.
- Pas d'ajout au chemin critique du CPU, au chargement des textures PBR ou au renderer DirectX11.
- Afficher dans l'interface ou les crédits : « Reconstitution sonore moderne,
  **pas un programme original des années 1970** ».

**État d'intégration :** l'outil de génération et le manifeste sont versionnés.
Le raccordement à un lecteur audio 3D dans l'exécutable CSE WARP n'est **pas encore codé**,
car il nécessite une adaptation au backend audio réel du programme.

## Redistribution

Lire [CREDITS.md](CREDITS.md) et conserver ce fichier avec tout montage distribué.
La bande-son adaptée doit satisfaire aux conditions CC BY-SA 3.0.
Les fichiers binaires (~59 Mo de sources) et les rendus ne sont **pas commités** :
ils sont récupérés/recréés à la demande pour ne pas alourdir GitHub.

## Vérifications

- `python -m py_compile build_radio.py` — syntaxe Python.
- `python build_radio.py --dry-run` — vérifie la playlist sans téléchargement.
- Vérifier le rendu et la durée via `ffprobe` après génération.
- Si le dépôt distribue l'audio publiquement, compléter au préalable l'attribution
  de la version Wikipédia historique mentionnée dans les crédits.
