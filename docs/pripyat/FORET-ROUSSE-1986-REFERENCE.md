# Forêt rouge de 1986 — cahier de référence pour Moteur3D

**Décision du projet :** reconstruire **la forêt roussie de l'été 1986**, et NON la repousse forestière actuelle.
La Forêt rouge est une zone proche de la centrale, **hors des limites de la ville de Pripyat**.

## Faits établis

- Un massif de conifères a été gravement affecté par l'irradiation suivant l'accident du 26 avril 1986.
- Les aiguilles de pins ont pris une couleur orangée à brun rougeâtre avant la mort des arbres.
- L'étendue exacte dépend de la classe de dommage ; la zone de forêt tuée est de l'ordre de **4–6 km²** selon le UK Centre for Ecology & Hydrology.
- Après l'accident, des arbres et des sols contaminés ont été enfouis ou évacués ; il ne faut pas placer de tranchées de réhabilitation **dans l'instantané 1986 avant les travaux**.
- Des photographies légendées de 1986 montrant les pins rougis existent dans les archives du Centre scientifique de Komi.

## Cadrage 3D / reconstruction visuelle

- **Couche temporelle :** été 1986 juste après les dégâts ; l'ajouter comme biotope autonome et géographiquement séparé de la place centrale.
- **Silhouette :** pinèdes de hauteur variable ; arbres morts portant encore leurs aiguilles décolorées, arbres partiellement atteints, bordure de forêt verte restante.
- **Palette de matériau envisagée** (proposition artistique, à étalonner sur les photographies) : brun cuivré, ocre orangé, brun sombre, bois grisé ; **aucun vert saturé sur les arbres tués**.
- **Objets rendus :** troncs, branches fines multiples, aiguilles en houppier, sol sableux et aiguilles tombées.
- **Rendu :** commencer par la visibilité lointaine d'une masse forestière et des cimes roussies ; conserver le modèle CPU/GPU light.
- **Géoréférence :** aucune frontière polygonale exacte fournie ici ; utiliser une couche cartographique séparée, pas un rectangle arbitraire sur la ville.
- **Cohérence historique :** si l'on conserve des façades dégradées de 2010–2026 dans le même panorama, étiqueter ce rendu comme **composition multi-époques** (ville contemporaine + forêt 1986), non comme une photographie documentaire datée 1986.

## Sources probantes

1. UKCEH — [TREE Red Fire](https://www.ceh.ac.uk/our-science/projects/tree-red-fire-radioactive-environment-damaged-fire) : dommages et aire affectée.
2. AIEA — [TECDOC-1240](https://www-pub.iaea.org/MTCD/Publications/PDF/te_1240_prn.pdf) : mortalité des pins et dégâts écologiques.
3. Centre scientifique de Komi — [photo d'archives « Рыжий лес », 1986](https://komisc.ru/home/news/komisc-news-2/2641-radionuklidnaya-anomaliya-ob-uchastii-instituta-biologii-v-likvidatsii-posledstvij-avarii-na-chernobylskoj-aes).
4. [National Academies Press — Wormwood Forest](https://www.nationalacademies.org/read/11318/chapter/3).

**État du projet :** références vérifiées et cahier établi ; **aucun nouveau biome n'est prétendu intégré ni rendu dans la scène tant que le code correspondant n'a pas été écrit et compilé.**
