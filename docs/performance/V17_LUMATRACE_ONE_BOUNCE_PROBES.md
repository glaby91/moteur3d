# LumaTrace — rebond diffus GPU léger

## But

Ajouter à LumaTrace un rebond diffus localisé à partir des surfaces réellement éclairées. Le calcul reste une approximation à un rebond ; il ne lance pas un path tracing complet et ne recalcule pas la lumière à chaque image.

## Calcul

- La radiance directe des triangles tient compte du rayon des lumières de scène et d'un test d'ombre BVH. Les deux lumières cyan/orange de démonstration, placées au centre de la scène, sont supprimées.
- LumaTrace construit un volume de 24 × 10 × 24 sondes. Huit rayons déterministes par sonde recherchent la première surface. Le shader récupère sa texture à l'UV d'impact, sa radiance directe et l'ombre du soleil, puis écrit le rebond dans une texture 3D.
- Le pixel shader interpole le volume dans l'espace et ajoute ce rebond à la couleur du matériau. Le cache est conservé tant que la scène, les textures ou l'état lumineux ne changent pas ; le déplacement de caméra ne relance pas les rayons.
- Le bouton « Rebonds indirects 3D » pilote ce calcul. Le mode LumaTrace reste sélectionné en cas d'échec et affiche son erreur habituelle ; aucun autre backend n'est choisi automatiquement.

## Pré-vérification sur V17.629

Le contrôle CPU géométrique de la scène publiée V17.629 a échantillonné 96 positions au-dessus du bassin avec huit directions par position. Après exclusion de l'auto-intersection immédiate avec l'eau, 33 des 768 rayons atteignent directement la fresque. Ce résultat confirme qu'un transfert fresque → piscine est géométriquement possible ; il ne prétend pas mesurer la luminosité finale.

La compilation LumaTrace ajoute un contrôle WARP dédié sur le package de scène le plus récent : il rend la vue piscine/fresque avec le rebond désactivé puis activé, vérifie que les images diffèrent, que le rendu GPU aboutit et que le cache donne ensuite une image identique. Ce contrôle valide le chemin HLSL et son exécution logicielle, pas les performances du GPU de l'utilisateur.

## Limites

- Une seule diffusion est calculée ; les reflets spéculaires, la réfraction et les rebonds multiples ne sont pas couverts.
- Les sondes donnent une illumination interpolée, pas une lightmap UV haute résolution. Les petits détails peuvent donc être lissés.
- Les huit rayons par sonde sont recalculés quand les paramètres lumineux changent, puis réutilisés pendant les déplacements de caméra.
