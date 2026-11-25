# Proposition — journal de diagnostic du moteur V17

> **Statut : proposition de conception uniquement.** Ce document n’ajoute aucun code et ne modifie pas le comportement du moteur.

## Objectif

Ajouter un mode de diagnostic activable qui rende les opérations du moteur traçables dans un journal exploitable. Pour chaque opération importante, l’équipe doit pouvoir distinguer une réussite, un avertissement, une erreur récupérable et un arrêt fatal, puis retrouver le contexte utile sans devoir reproduire immédiatement le problème.

Le mode diagnostic ne doit pas modifier le rendu, les paramètres de scène, ni la sélection du backend.

## État constaté avant cette proposition

Le moteur dispose déjà d’une trace ciblée liée à certains échecs de présentation native, écrite dans les diagnostics utilisateur. Les campagnes de benchmark produisent également leurs propres rapports et statuts. Ces sorties répondent à des besoins précis, mais elles ne constituent pas encore un journal unifié de l’application couvrant son démarrage et son fonctionnement.

La future implémentation devra reprendre ou intégrer cette trace existante afin d’éviter les journaux parallèles et les événements en double.

## Format et contenu

Utiliser un journal **JSON Lines (JSONL)** : un objet JSON complet par ligne, facile à lire à la main et à analyser automatiquement. Chaque événement devrait pouvoir contenir, selon le contexte :

- date et heure UTC avec précision suffisante ;
- identifiant de session, numéro d’événement et identifiant de corrélation ;
- niveau, sous-système et identifiant stable de l’événement ;
- action, résultat (started, success, warning, error, fatal) et durée si applicable ;
- version du moteur, révision Git, scène et révision de scène ;
- backend actif, périphérique GPU et pilote lorsqu’ils sont disponibles ;
- code d’erreur, message et contexte d’échec utiles à la résolution.

Les champs facultatifs doivent être absents ou explicitement nuls plutôt que renseignés avec des valeurs ambiguës. Les messages sont destinés aux humains ; les identifiants d’événements et les codes d’erreur restent stables pour permettre le filtrage et la comparaison entre versions.

Exemple de forme, à adapter à l’implémentation :

```json
{"ts":"2026-11-25T16:30:12.481Z","session":"…","seq":42,"level":"INFO","subsystem":"renderer","event":"backend.init","status":"success","backend":"Vulkan","duration_ms":184.2,"engine_version":"V17…","scene":"…"}
```

## Niveaux et activation

Niveaux proposés : DEBUG, INFO, WARNING, ERROR, FATAL. La réussite est enregistrée comme résultat (status: success) au niveau INFO, plutôt que comme niveau supplémentaire.

- Par défaut, conserver un journal léger, limité aux étapes importantes et aux avertissements/erreurs.
- Activer les détails supplémentaires via une option explicite, par exemple --debug ou --log-level DEBUG, et éventuellement --log-file <chemin>.
- Afficher dans la console ou l’interface les avertissements et erreurs avec un résumé compréhensible ; le fichier contient les champs structurés.
- Ne pas journaliser chaque image ou chaque pixel en mode normal. Les traces par image doivent être réservées à un niveau très détaillé et désactivé par défaut.

## Emplacement, rotation et robustesse

- Utiliser un répertoire de diagnostic propre à l’application via les chemins Qt (QStandardPaths::AppLocalDataLocation), dans un sous-dossier logs.
- Prévoir une rotation par taille ou par session, une limite de conservation configurable et un nom de fichier horodaté.
- Si l’écriture du journal échoue, le moteur doit continuer à fonctionner ; signaler l’échec une seule fois par un canal de secours, sans boucle de messages.
- Rendre l’écriture sûre en présence de plusieurs threads et préserver l’ordre avec un numéro de séquence ou un mécanisme équivalent.
- Ne jamais journaliser de secrets, jetons d’accès, données personnelles inutiles, ni le contenu complet de fichiers volumineux.

## Événements à instrumenter en priorité

1. Démarrage, lecture de configuration et arrêt propre.
2. Chargement de scène, ressources et textures : début, résultat, durée et ressource fautive.
3. Initialisation, sélection, changement et perte d’un backend ou périphérique.
4. Chargement/compilation de shaders et création des ressources GPU.
5. Début et fin des étapes de rendu utiles au diagnostic, sans produire de bruit à chaque frame.
6. Échec de présentation, récupération, bascule vers un autre backend et autres mécanismes de repli.
7. Campagnes de benchmark : identifiant d’essai, backend, scène, paramètres, résultats et erreurs, en conservant les rapports de benchmark dédiés.

Les avertissements doivent indiquer la décision prise ensuite (par exemple : ressource ignorée, nouvelle tentative, ou backend de repli choisi). Une erreur récupérable doit être distinguée d’un échec qui arrête réellement le moteur.

## Gestion des messages et des erreurs

Centraliser les messages Qt (qWarning, qCritical, etc.) dans le même système, en conservant leur catégorie et leur contexte. Les erreurs détectées dans le code doivent aussi être journalisées au point où elles sont comprises ; un message Qt seul ne fournit pas toujours assez d’informations pour diagnostiquer la cause.

Les exceptions et erreurs système ne doivent pas être masquées. Un éventuel rapport de crash ou minidump Windows serait une étape distincte, à étudier avec soin pour la confidentialité, la taille des fichiers et la fiabilité. Le journal de diagnostic ne doit pas prétendre remplacer un dump quand l’état mémoire est nécessaire à l’analyse.

## Critères d’acceptation de la future implémentation

- Le lancement et le rendu restent possibles si le fichier journal ne peut pas être créé ou écrit.
- Chaque ligne produite est un JSON valide et contient les champs obligatoires définis par le schéma.
- Plusieurs threads peuvent écrire sans lignes mélangées ni perte silencieuse.
- La rotation et la rétention sont vérifiables.
- Les succès, avertissements, erreurs récupérables et échecs fatals sont clairement différenciés.
- Un scénario d’erreur simulé permet de retrouver l’opération, le backend, la scène, la version et le message utile.
- Le coût du journal en mode normal est mesuré et reste négligeable ; l’activation de DEBUG est explicite.
- La trace de présentation native existante et les sorties du benchmark sont intégrées ou référencées sans duplication confuse.

## Découpage conseillé

1. Définir le schéma d’événement et le service de journalisation central.
2. Ajouter rotation, chemins, niveaux, options de lancement et capture des messages Qt.
3. Instrumenter le démarrage, le chargement des ressources et les backends.
4. Ajouter des scénarios de vérification contrôlés et mesurer le coût.
5. Documenter où récupérer le journal dans le paquet Windows et comment le joindre à un rapport d’anomalie.

Cette proposition ne préjuge pas de l’architecture finale ni des champs exacts : ils devront être confirmés lors de l’implémentation.
