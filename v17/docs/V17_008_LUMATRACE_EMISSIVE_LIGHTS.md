# LumaTrace V17.008 — éclairage des surfaces lumineuses

LumaTrace récupère maintenant les surfaces émissives de la scène comme sources de lumière locales : bandes lumineuses de la piscine, néons, lampes et matériaux dont l’émission est explicitement définie.

Pour limiter le coût, les triangles émissifs proches sont regroupés en cellules de 4 m, puis ramenés à au plus 16 sources d’appoint réparties le long des luminaires. Chaque source réutilise les rayons d’ombre BVH et les quatre rayons diffus déterministes déjà calculés par triangle. Le résultat reste dans le cache de radiance GPU tant que la scène, l’heure ou les lumières ne changent pas. Aucun parcours de rayons supplémentaire n’est fait à chaque image.

Les scènes qui déclarent déjà des lumières ponctuelles gardent ces sources. Les anciennes lumières de test sont conservées uniquement comme dernier recours, quand la scène ne contient ni surface émissive ni lumière déclarée.

La solution traite l’émission visible et le rebond diffus. Elle ne calcule pas de réfraction de caustiques ni de path tracing complet.
