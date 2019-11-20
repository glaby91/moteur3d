'''''''''''''''''''''''''''''''''''''''''''''''''''''
'
'   Moteur 3D + 2D
'
'  V0.1   OpenGCSE
'
''''''''''''''''''''''''''''''''''''''''''''
'
'
'La methode employe est celle des triangles pour definir des surfaces
'en 3D
'
'On pose ici une limitation au nombre de triangles
'pour les tests on passe nbtrianglemax= 100
nbtrianglemax = 100
'On definit alors 3 tables pour les coordonees
' un trinagle est defini par les positions x,y,z; x+1,y+1,z+1;x+2,y+2,z+2

DIM triax(3 * nbtrianglemax)
DIM triay(3 * nbtrianglemax)
DIM triaz(3 * nbtiranglemax)
'Chaque triangle peut se voir appliquer une texture diffrente
'On va limiter le nombre de texture en taille et en nombe
'une texture doit etre former par 200x200 pixels en 32bits BMP
'et limiter a 15
nblimtext = 15
DIM texture(nblimtext * 200, nblimtext * 200)
'On va devevoir aussi avoir 4 niveaux de zoom sur la texture selon l'eloignement
'de l'objet
DIM texture2(nblimtext * 100, nblimtext * 100)
DIM texture3(nblimtext * 50, nblimtext * 50)
DIM texture4(nblimtext * 25, nblimtext * 25)

'
'Le but etant de projeter des triangles 3D dans un espace 2D
'on doit alors dimensionner une table de rendu en 2D dependant
'de la resolution
'On doit alors definir un ecran type ici en fullhd
resox = 1920
resoy = 1080
'On definit la table de rendu
'Comme le rendu est defini par R,V,B
'On definit trois table
DIM renduR(reosx, resoy)
DIM renduV(reosx, resoy)
DIM renduB(reosx, resoy)
DO

    IF LEN(INKEY$) THEN EXIT DO

LOOP

''''''''''''''''''''''''''''''''''''''''''
'Parti rendu des triangles 3D->2D
'Pour rendre des objets a l'ecran et dans le cas de superposition on doit afficher
'l'element devant nos yeux. On utilise la technique du Z buffer chaque point de la scene
'on doit definir un champs de vision maximale et une boite de renduy pour comparer chaque objet
profchamp = 3000
DIM renduZ(resox, resoy)
DIM triaxzbuffer(3 * nbtrianglemax)
DIM triayzbuffer(3 * nbtrianglemax)
DIM triazzbuffer(3 * nbtiranglemax)

