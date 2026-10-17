' OpenGCSE v0.4 - moteur 3D + 2D (evolution de moteur.bas v0.1)
' Implementation experimentale QB64 / QB64-PE
' Geometrie parametrique -> triangles de rendu -> projection -> Z-buffer
' Sources historiques des textures conservees dans rgb.bas / lecteur bmp.bas
OPTION _EXPLICIT
CONST W = 800
CONST H = 600
' Spatial SSAA 4x (2x2). For 16x set AA_SAMPLES=16 and AA_SCALE=4.
CONST AA_SAMPLES = 4
CONST AA_SCALE = 2
CONST RW = W * AA_SCALE
CONST RH = H * AA_SCALE
CONST NEARPLANE = .1
CONST FOCAL = 560
CONST MAXOBJECTS = 10000
CONST MAXTRIANGLES = 120000
CONST FARPLANE = 180
CONST LODDISTANCE = 65
TYPE Vertex3D
    x AS SINGLE
    y AS SINGLE
    z AS SINGLE
END TYPE
TYPE Triangle3D
    a AS Vertex3D
    b AS Vertex3D
    c AS Vertex3D
    color AS _UNSIGNED LONG
END TYPE
DECLARE SUB AddTriangle (a AS Vertex3D, b AS Vertex3D, c AS Vertex3D, color AS _UNSIGNED LONG)
DECLARE SUB AddBox (cx AS SINGLE, cy AS SINGLE, cz AS SINGLE, sx AS SINGLE, sy AS SINGLE, sz AS SINGLE, color AS _UNSIGNED LONG)
DECLARE SUB DrawTriangle (t AS Triangle3D)
DECLARE SUB AddSceneBox (cx AS SINGLE, cy AS SINGLE, cz AS SINGLE, sx AS SINGLE, sy AS SINGLE, sz AS SINGLE, color AS _UNSIGNED LONG)
DECLARE FUNCTION BoxVisible% (cx AS SINGLE, cy AS SINGLE, cz AS SINGLE, sx AS SINGLE, sy AS SINGLE, sz AS SINGLE)
DECLARE SUB Raster (x1 AS SINGLE, y1 AS SINGLE, z1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE, z2 AS SINGLE, x3 AS SINGLE, y3 AS SINGLE, z3 AS SINGLE, color AS _UNSIGNED LONG)
DECLARE FUNCTION Edge! (ax AS SINGLE, ay AS SINGLE, bx AS SINGLE, by AS SINGLE, px AS SINGLE, py AS SINGLE)
DIM SHARED triangles(1 TO MAXTRIANGLES) AS Triangle3D
DIM SHARED objectsVisible AS LONG, objectsCulled AS LONG, objectsLOD AS LONG, facesCulled AS LONG
DIM SHARED count AS LONG
DIM SHARED depth(0 TO RW - 1, 0 TO RH - 1) AS SINGLE
DIM screenHandle AS LONG, renderHandle AS LONG
DIM frameStart AS DOUBLE, frameMs AS DOUBLE
DIM i AS LONG, x AS LONG, y AS LONG, gx AS LONG, gy AS LONG, gz AS LONG
DIM cx AS SINGLE, cy AS SINGLE, cz AS SINGLE, sz AS SINGLE
screenHandle = _NEWIMAGE(W, H, 32)
SCREEN screenHandle
renderHandle = _NEWIMAGE(RW, RH, 32)
_TITLE "OpenGCSE - moteur.bas - Z-buffer (experimental)"
' 1000 objets mathematiques : grille 10 x 10 x 10, tailles variables.
' Les objets restent des boites parametriques jusqu'a la preparation du rendu.
FOR gz = 0 TO 9
    FOR gy = 0 TO 9
        FOR gx = 0 TO 9
            cx = (gx - 4.5) * 1.6
            cy = (gy - 4.5) * 1.35
            cz = 8 + gz * 5.5
            sz = .65 + ((gx + gy + gz) MOD 4) * .16
            AddSceneBox cx, cy, cz, sz, sz * 1.2, sz, _RGB32(55 + gx * 18, 65 + gy * 16, 80 + gz * 14)
        NEXT
    NEXT
NEXT
DO
    frameStart = TIMER(.001)
    _DEST renderHandle
    CLS , _RGB32(17, 21, 32)
    FOR y = 0 TO RH - 1
        FOR x = 0 TO RW - 1
            depth(x, y) = 1E+30
        NEXT
    NEXT
    FOR i = count TO 1 STEP -1
        DrawTriangle triangles(i)
    NEXT
    _DEST screenHandle
    _PUTIMAGE (0, 0)-(W - 1, H - 1), renderHandle, screenHandle
    frameMs = (TIMER(.001) - frameStart) * 1000
    IF frameMs < 0 THEN frameMs = frameMs + 86400000#
    _PRINTSTRING (12, 12), "OpenGCSE | 1000 objets | SSAA 4x | Z-buffer"
    _PRINTSTRING (12, 30), "Triangles: " + LTRIM$(STR$(count)) + " | visibles: " + LTRIM$(STR$(objectsVisible)) + " | hors champ: " + LTRIM$(STR$(objectsCulled))
    _PRINTSTRING (12, 48), "Temps CPU/image: " + LTRIM$(STR$(INT(frameMs))) + " ms | ESC quitter"
    _DISPLAY
    _LIMIT 30
LOOP UNTIL _KEYDOWN(27)
_FREEIMAGE renderHandle
SYSTEM

' Culling d'objet par sphere englobante conservative en espace camera.
FUNCTION BoxVisible% (cx AS SINGLE, cy AS SINGLE, cz AS SINGLE, sx AS SINGLE, sy AS SINGLE, sz AS SINGLE)
    DIM radius AS SINGLE, zmin AS SINGLE, zmax AS SINGLE, halfW AS SINGLE, halfH AS SINGLE
    radius = SQR(sx * sx + sy * sy + sz * sz) / 2
    zmin = cz - radius: zmax = cz + radius
    IF zmax <= NEARPLANE OR zmin >= FARPLANE THEN EXIT FUNCTION
    IF zmax <= 0 THEN EXIT FUNCTION
    halfW = (W / 2) * zmax / FOCAL
    halfH = (H / 2) * zmax / FOCAL
    IF cx - radius > halfW OR cx + radius < -halfW THEN EXIT FUNCTION
    IF cy - radius > halfH OR cy + radius < -halfH THEN EXIT FUNCTION
    BoxVisible = -1
END FUNCTION

SUB AddSceneBox (cx AS SINGLE, cy AS SINGLE, cz AS SINGLE, sx AS SINGLE, sy AS SINGLE, sz AS SINGLE, color AS _UNSIGNED LONG)
    IF NOT BoxVisible(cx, cy, cz, sx, sy, sz) THEN
        objectsCulled = objectsCulled + 1
        EXIT SUB
    END IF
    objectsVisible = objectsVisible + 1
    ' LOD : les objets eloignes ont une geometrie simplifiee.
    ' Une boite ne possede deja que 12 triangles : la simplification
    ' conserve donc ses 12 triangles afin de ne pas changer sa silhouette.
    IF cz > LODDISTANCE THEN objectsLOD = objectsLOD + 1
    AddBox cx, cy, cz, sx, sy, sz, color
END SUB

SUB AddTriangle (a AS Vertex3D, b AS Vertex3D, c AS Vertex3D, color AS _UNSIGNED LONG)


SUB AddTriangle (a AS Vertex3D, b AS Vertex3D, c AS Vertex3D, color AS _UNSIGNED LONG)
    IF count >= UBOUND(triangles) THEN EXIT SUB
    count = count + 1
    triangles(count).a = a
    triangles(count).b = b
    triangles(count).c = c
    triangles(count).color = color
END SUB

SUB AddBox (cx AS SINGLE, cy AS SINGLE, cz AS SINGLE, sx AS SINGLE, sy AS SINGLE, sz AS SINGLE, color AS _UNSIGNED LONG)
    DIM p(0 TO 7) AS Vertex3D
    DIM face(0 TO 5, 0 TO 3) AS LONG
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO 7
        IF (i AND 1) THEN p(i).x = cx + sx / 2 ELSE p(i).x = cx - sx / 2
        IF (i AND 2) THEN p(i).y = cy + sy / 2 ELSE p(i).y = cy - sy / 2
        IF (i AND 4) THEN p(i).z = cz + sz / 2 ELSE p(i).z = cz - sz / 2
    NEXT
    ' Chaque face est un quad decompose en deux triangles pour la rasterisation.
    face(0, 0) = 0: face(0, 1) = 1: face(0, 2) = 3: face(0, 3) = 2
    face(1, 0) = 4: face(1, 1) = 6: face(1, 2) = 7: face(1, 3) = 5
    face(2, 0) = 0: face(2, 1) = 4: face(2, 2) = 5: face(2, 3) = 1
    face(3, 0) = 2: face(3, 1) = 3: face(3, 2) = 7: face(3, 3) = 6
    face(4, 0) = 0: face(4, 1) = 2: face(4, 2) = 6: face(4, 3) = 4
    face(5, 0) = 1: face(5, 1) = 5: face(5, 2) = 7: face(5, 3) = 3
    FOR i = 0 TO 5
        AddTriangle p(face(i, 0)), p(face(i, 1)), p(face(i, 2)), color
        AddTriangle p(face(i, 0)), p(face(i, 2)), p(face(i, 3)), color
    NEXT
END SUB

SUB DrawTriangle (t AS Triangle3D)
    DIM x1 AS SINGLE, y1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE, x3 AS SINGLE, y3 AS SINGLE
    ' Limite v0.2 : triangles traversant le plan proche ignores ; clipping a ajouter.
    IF t.a.z <= NEARPLANE OR t.b.z <= NEARPLANE OR t.c.z <= NEARPLANE THEN EXIT SUB
    x1 = RW / 2 + (FOCAL * AA_SCALE) * t.a.x / t.a.z
    y1 = RH / 2 - (FOCAL * AA_SCALE) * t.a.y / t.a.z
    x2 = RW / 2 + (FOCAL * AA_SCALE) * t.b.x / t.b.z
    y2 = RH / 2 - (FOCAL * AA_SCALE) * t.b.y / t.b.z
    x3 = RW / 2 + (FOCAL * AA_SCALE) * t.c.x / t.c.z
    y3 = RH / 2 - (FOCAL * AA_SCALE) * t.c.y / t.c.z
    ' Back-face culling : les faces visibles des boites ont une aire positive
    ' avec l'ordre de sommets historique de AddBox et l'axe Y ecran inverse.
    IF Edge(x1, y1, x2, y2, x3, y3) <= 0 THEN
        facesCulled = facesCulled + 1
        EXIT SUB
    END IF
    Raster x1, y1, t.a.z, x2, y2, t.b.z, x3, y3, t.c.z, t.color
END SUB

FUNCTION Edge! (ax AS SINGLE, ay AS SINGLE, bx AS SINGLE, by AS SINGLE, px AS SINGLE, py AS SINGLE)
    Edge = (px - ax) * (by - ay) - (py - ay) * (bx - ax)
END FUNCTION

SUB Raster (x1 AS SINGLE, y1 AS SINGLE, z1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE, z2 AS SINGLE, x3 AS SINGLE, y3 AS SINGLE, z3 AS SINGLE, color AS _UNSIGNED LONG)
    DIM area AS SINGLE, e1 AS SINGLE, e2 AS SINGLE, e3 AS SINGLE
    DIM w1 AS SINGLE, w2 AS SINGLE, w3 AS SINGLE, invz AS SINGLE, z AS SINGLE
    DIM xmin AS LONG, xmax AS LONG, ymin AS LONG, ymax AS LONG, x AS LONG, y AS LONG
    area = Edge(x1, y1, x2, y2, x3, y3)
    IF ABS(area) < .00001 THEN EXIT SUB
    xmin = INT(x1): IF x2 < xmin THEN xmin = INT(x2)
    IF x3 < xmin THEN xmin = INT(x3)
    xmax = INT(x1) + 1: IF x2 > xmax THEN xmax = INT(x2) + 1
    IF x3 > xmax THEN xmax = INT(x3) + 1
    ymin = INT(y1): IF y2 < ymin THEN ymin = INT(y2)
    IF y3 < ymin THEN ymin = INT(y3)
    ymax = INT(y1) + 1: IF y2 > ymax THEN ymax = INT(y2) + 1
    IF y3 > ymax THEN ymax = INT(y3) + 1
    IF xmin < 0 THEN xmin = 0
    IF ymin < 0 THEN ymin = 0
    IF xmax >= RW THEN xmax = RW - 1
    IF ymax >= RH THEN ymax = RH - 1
    IF xmin > xmax OR ymin > ymax THEN EXIT SUB
    FOR y = ymin TO ymax
        FOR x = xmin TO xmax
            e1 = Edge(x2, y2, x3, y3, x + .5, y + .5)
            e2 = Edge(x3, y3, x1, y1, x + .5, y + .5)
            e3 = Edge(x1, y1, x2, y2, x + .5, y + .5)
            IF (e1 >= 0 AND e2 >= 0 AND e3 >= 0) OR (e1 <= 0 AND e2 <= 0 AND e3 <= 0) THEN
                w1 = e1 / area: w2 = e2 / area: w3 = e3 / area
                ' Interpolation perspective-correcte de la profondeur via 1/z.
                invz = w1 / z1 + w2 / z2 + w3 / z3
                IF invz > 0 THEN
                    z = 1 / invz
                    IF z < depth(x, y) THEN
                        depth(x, y) = z
                        PSET (x, y), color
                    END IF
                END IF
            END IF
        NEXT
    NEXT
END SUB
