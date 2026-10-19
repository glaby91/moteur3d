' QB64 RASTER ENGINE — branch raster — version 1.0.0 (2026-10-19)
' True triangle rasterization, perspective UV, camera Z buffer, 16:10.
' Standalone QB64/ QB64-PE code. ESC quits, A/D rotates, +/- zooms, H toggles quality.
' This is a compact functional port of the algorithm, NOT a byte-for-byte V8.1 port.
OPTION _EXPLICIT

TYPE Vertex
    x AS SINGLE
    y AS SINGLE
    z AS SINGLE
    u AS SINGLE
    v AS SINGLE
END TYPE
TYPE Triangle
    a AS Vertex
    b AS Vertex
    c AS Vertex
    material AS INTEGER
END TYPE
DECLARE SUB MakeVtx (dst AS Vertex, x AS SINGLE, y AS SINGLE, z AS SINGLE, u AS SINGLE, v AS SINGLE)
DECLARE SUB PushTri (a AS Vertex, b AS Vertex, c AS Vertex, mat AS INTEGER)
DECLARE SUB PushQuad (a AS Vertex, b AS Vertex, c AS Vertex, d AS Vertex, mat AS INTEGER)
DECLARE SUB BuildScene ()
DECLARE SUB SphereMesh (x AS SINGLE, y AS SINGLE, z AS SINGLE, rx AS SINGLE, ry AS SINGLE, rz AS SINGLE, mat AS INTEGER)
DECLARE SUB Project (p AS Vertex, sx AS SINGLE, sy AS SINGLE, iz AS SINGLE)
DECLARE SUB RasterTri (tri AS Triangle)
DECLARE SUB Render ()
DECLARE FUNCTION SurfaceColour& (material AS INTEGER, u AS SINGLE, v AS SINGLE, shade AS SINGLE)

DIM SHARED faces(1 TO 1400) AS Triangle
DIM SHARED countFaces AS INTEGER, zBuffer(0 TO 639, 0 TO 399) AS SINGLE
DIM SHARED camX AS SINGLE, camY AS SINGLE, camZ AS SINGLE
DIM SHARED fwX AS SINGLE, fwY AS SINGLE, fwZ AS SINGLE
DIM SHARED rtX AS SINGLE, rtY AS SINGLE, rtZ AS SINGLE
DIM SHARED upX AS SINGLE, upY AS SINGLE, upZ AS SINGLE
DIM SHARED yaw AS SINGLE, camRadius AS SINGLE
DIM SHARED rw AS INTEGER, rh AS INTEGER, screenPage AS LONG, offscreen AS LONG
DIM keyPress AS STRING, done AS INTEGER, oldW AS INTEGER
CONST PI = 3.141592653589793#

screenPage = _NEWIMAGE(640, 400, 32)
SCREEN screenPage
_TITLE "Moteur3D QB64 — Raster triangles Z / 16:10"
rw = 320: rh = 200: yaw = .42: camRadius = 12
offscreen = _NEWIMAGE(rw, rh, 32)
BuildScene
DO
    keyPress = LCASE$(INKEY$)
    IF keyPress = CHR$(27) THEN EXIT DO
    SELECT CASE keyPress
        CASE "a": yaw = yaw - .11
        CASE "d": yaw = yaw + .11
        CASE "+", "=": camRadius = camRadius - 1
        CASE "-": camRadius = camRadius + 1
        CASE "h"
            IF rw = 320 THEN rw = 640: rh = 400 ELSE rw = 320: rh = 200
            _FREEIMAGE offscreen
            offscreen = _NEWIMAGE(rw, rh, 32)
    END SELECT
    IF camRadius < 5 THEN camRadius = 5
    IF camRadius > 28 THEN camRadius = 28
    Render
    _LIMIT 25
LOOP
_FREEIMAGE offscreen
SYSTEM

SUB MakeVtx (dst AS Vertex, x AS SINGLE, y AS SINGLE, z AS SINGLE, u AS SINGLE, v AS SINGLE)
    dst.x = x: dst.y = y: dst.z = z: dst.u = u: dst.v = v
END SUB

SUB PushTri (a AS Vertex, b AS Vertex, c AS Vertex, mat AS INTEGER)
    IF countFaces >= UBOUND(faces) THEN EXIT SUB
    countFaces = countFaces + 1
    faces(countFaces).a = a: faces(countFaces).b = b
    faces(countFaces).c = c: faces(countFaces).material = mat
END SUB

SUB PushQuad (a AS Vertex, b AS Vertex, c AS Vertex, d AS Vertex, mat AS INTEGER)
    PushTri a, b, c, mat
    PushTri a, c, d, mat
END SUB

SUB SphereMesh (x AS SINGLE, y AS SINGLE, z AS SINGLE, rx AS SINGLE, ry AS SINGLE, rz AS SINGLE, mat AS INTEGER)
    DIM i AS INTEGER, j AS INTEGER, p0 AS SINGLE, p1 AS SINGLE, t0 AS SINGLE, t1 AS SINGLE
    DIM a AS Vertex, b AS Vertex, c AS Vertex, d AS Vertex
    FOR j = 0 TO 7
        p0 = -PI / 2 + PI * j / 8: p1 = -PI / 2 + PI * (j + 1) / 8
        FOR i = 0 TO 11
            t0 = 2 * PI * i / 12: t1 = 2 * PI * (i + 1) / 12
            MakeVtx a, x + rx * COS(p0) * COS(t0), y + ry * SIN(p0), z + rz * COS(p0) * SIN(t0), i / 12, j / 8
            MakeVtx b, x + rx * COS(p0) * COS(t1), y + ry * SIN(p0), z + rz * COS(p0) * SIN(t1), (i + 1) / 12, j / 8
            MakeVtx c, x + rx * COS(p1) * COS(t1), y + ry * SIN(p1), z + rz * COS(p1) * SIN(t1), (i + 1) / 12, (j + 1) / 8
            MakeVtx d, x + rx * COS(p1) * COS(t0), y + ry * SIN(p1), z + rz * COS(p1) * SIN(t0), i / 12, (j + 1) / 8
            PushQuad a, b, c, d, mat
        NEXT i
    NEXT j
END SUB

SUB BuildScene
    DIM q1 AS Vertex, q2 AS Vertex, q3 AS Vertex, q4 AS Vertex
    DIM x AS INTEGER, z AS INTEGER, j AS INTEGER
    DIM a AS SINGLE, b AS SINGLE, lo AS SINGLE, hi AS SINGLE, r1 AS SINGLE, r2 AS SINGLE
    DIM ax AS SINGLE, bx AS SINGLE, az AS SINGLE, bz AS SINGLE
    countFaces = 0
    ' Ground: true triangles, no Canvas tricks; UV coordinates are world-aligned.
    FOR x = -8 TO 7
        FOR z = -8 TO 7
            MakeVtx q1, x, 0, z, x / 2, z / 2
            MakeVtx q2, x + 1, 0, z, (x + 1) / 2, z / 2
            MakeVtx q3, x + 1, 0, z + 1, (x + 1) / 2, (z + 1) / 2
            MakeVtx q4, x, 0, z + 1, x / 2, (z + 1) / 2
            PushQuad q1, q2, q3, q4, 1
        NEXT z
    NEXT x
    ' Genuine polygon mesh trunk with rings (cylinder as triangles).
    FOR j = 0 TO 11
        lo = 2 * PI * j / 12: hi = 2 * PI * (j + 1) / 12
        ax = -1.7 + .28 * COS(lo): az = .4 + .28 * SIN(lo)
        bx = -1.7 + .28 * COS(hi): bz = .4 + .28 * SIN(hi)
        MakeVtx q1, ax, 0, az, j / 12, 0
        MakeVtx q2, bx, 0, bz, (j + 1) / 12, 0
        MakeVtx q3, -1.7 + .18 * COS(hi), 2.7, .4 + .18 * SIN(hi), (j + 1) / 12, 1
        MakeVtx q4, -1.7 + .18 * COS(lo), 2.7, .4 + .18 * SIN(lo), j / 12, 1
        PushQuad q1, q2, q3, q4, 2
    NEXT j
    SphereMesh -1.7, 3.1, .4, 1.28, 1.16, 1.1, 3
    SphereMesh -2.35, 2.76, .35, .73, .74, .72, 3
    SphereMesh -1.04, 2.85, .66, .82, .74, .79, 3
    ' Stone wall and a flat wooden bench.
    MakeVtx q1, -6, 0, -3, 0, 0
    MakeVtx q2, 6, 0, -3, 4, 0
    MakeVtx q3, 6, 1.5, -3, 4, 1
    MakeVtx q4, -6, 1.5, -3, 0, 1
    PushQuad q1, q2, q3, q4, 4
    MakeVtx q1, 1.1, .8, 1.2, 0, 0
    MakeVtx q2, 3.4, .8, 1.2, 1, 0
    MakeVtx q3, 3.4, .8, 1.8, 1, 1
    MakeVtx q4, 1.1, .8, 1.8, 0, 1
    PushQuad q1, q2, q3, q4, 2
    MakeVtx q1, 1.1, .8, 1.8, 0, 0
    MakeVtx q2, 3.4, .8, 1.8, 1, 0
    MakeVtx q3, 3.4, 1.55, 1.8, 1, 1
    MakeVtx q4, 1.1, 1.55, 1.8, 0, 1
    PushQuad q1, q2, q3, q4, 2
END SUB

SUB Project (p AS Vertex, sx AS SINGLE, sy AS SINGLE, iz AS SINGLE)
    DIM dx AS SINGLE, dy AS SINGLE, dz AS SINGLE, side AS SINGLE, height AS SINGLE, deep AS SINGLE
    dx = p.x - camX: dy = p.y - camY: dz = p.z - camZ
    side = dx * rtX + dy * rtY + dz * rtZ
    height = dx * upX + dy * upY + dz * upZ
    deep = dx * fwX + dy * fwY + dz * fwZ
    IF deep < .15 THEN iz = -1: EXIT SUB
    iz = 1 / deep
    sx = rw / 2 + side * (rw * .85) * iz
    sy = rh / 2 - height * (rw * .85) * iz
END SUB

SUB RasterTri (tri AS Triangle)
    DIM x1 AS SINGLE, x2 AS SINGLE, x3 AS SINGLE, y1 AS SINGLE, y2 AS SINGLE, y3 AS SINGLE
    DIM z1 AS SINGLE, z2 AS SINGLE, z3 AS SINGLE, area AS SINGLE, w1 AS SINGLE, w2 AS SINGLE, w3 AS SINGLE
    DIM xx AS INTEGER, yy AS INTEGER, leftX AS INTEGER, rightX AS INTEGER, topY AS INTEGER, bottomY AS INTEGER
    DIM depth AS SINGLE, texU AS SINGLE, texV AS SINGLE
    DIM e1x AS SINGLE, e1y AS SINGLE, e1z AS SINGLE, e2x AS SINGLE, e2y AS SINGLE, e2z AS SINGLE
    DIM nx AS SINGLE, ny AS SINGLE, nz AS SINGLE, nl AS SINGLE, shade AS SINGLE
    Project tri.a, x1, y1, z1: Project tri.b, x2, y2, z2: Project tri.c, x3, y3, z3
    IF z1 <= 0 OR z2 <= 0 OR z3 <= 0 THEN EXIT SUB ' near-plane clipping omitted in QB64 demo
    area = (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1)
    IF ABS(area) < .0001 THEN EXIT SUB
    leftX = INT(x1): IF INT(x2) < leftX THEN leftX = INT(x2)
    IF INT(x3) < leftX THEN leftX = INT(x3)
    rightX = INT(x1 + 1): IF INT(x2 + 1) > rightX THEN rightX = INT(x2 + 1)
    IF INT(x3 + 1) > rightX THEN rightX = INT(x3 + 1)
    topY = INT(y1): IF INT(y2) < topY THEN topY = INT(y2)
    IF INT(y3) < topY THEN topY = INT(y3)
    bottomY = INT(y1 + 1): IF INT(y2 + 1) > bottomY THEN bottomY = INT(y2 + 1)
    IF INT(y3 + 1) > bottomY THEN bottomY = INT(y3 + 1)
    IF leftX < 0 THEN leftX = 0
    IF rightX >= rw THEN rightX = rw - 1
    IF topY < 0 THEN topY = 0
    IF bottomY >= rh THEN bottomY = rh - 1
    IF leftX > rightX OR topY > bottomY THEN EXIT SUB
    e1x = tri.b.x - tri.a.x: e1y = tri.b.y - tri.a.y: e1z = tri.b.z - tri.a.z
    e2x = tri.c.x - tri.a.x: e2y = tri.c.y - tri.a.y: e2z = tri.c.z - tri.a.z
    nx = e1y * e2z - e1z * e2y
    ny = e1z * e2x - e1x * e2z
    nz = e1x * e2y - e1y * e2x
    nl = SQR(nx * nx + ny * ny + nz * nz)
    IF nl > 0 THEN nx = nx / nl: ny = ny / nl: nz = nz / nl
    shade = .34 + .66 * ABS(nx * -.55 + ny * .78 + nz * .29)
    FOR yy = topY TO bottomY
        FOR xx = leftX TO rightX
            w1 = ((x2 - xx - .5) * (y3 - yy - .5) - (y2 - yy - .5) * (x3 - xx - .5)) / area
            w2 = ((x3 - xx - .5) * (y1 - yy - .5) - (y3 - yy - .5) * (x1 - xx - .5)) / area
            w3 = 1 - w1 - w2
            IF w1 >= -.00001 AND w2 >= -.00001 AND w3 >= -.00001 THEN
                depth = w1 * z1 + w2 * z2 + w3 * z3
                IF depth > zBuffer(xx, yy) THEN
                    zBuffer(xx, yy) = depth
                    texU = (w1 * tri.a.u * z1 + w2 * tri.b.u * z2 + w3 * tri.c.u * z3) / depth
                    texV = (w1 * tri.a.v * z1 + w2 * tri.b.v * z2 + w3 * tri.c.v * z3) / depth
                    PSET (xx, yy), SurfaceColour&(tri.material, texU, texV, shade)
                END IF
            END IF
        NEXT xx
    NEXT yy
END SUB

FUNCTION SurfaceColour& (material AS INTEGER, u AS SINGLE, v AS SINGLE, shade AS SINGLE)
    DIM r AS SINGLE, g AS SINGLE, b AS SINGLE, grain AS SINGLE, pattern AS INTEGER
    pattern = (INT(u * 16) + INT(v * 16)) AND 1
    grain = SIN(u * 84 + SIN(v * 27) * 2) * 11
    SELECT CASE material
        CASE 1: r = 75 + pattern * 14: g = 128 + pattern * 13: b = 71
        CASE 2: r = 120 + grain: g = 82 + grain * .7: b = 49 + grain * .4
        CASE 3: r = 57 + pattern * 12: g = 142 + pattern * 18: b = 63 + pattern * 9
        CASE 4: r = 162 + pattern * 20: g = 158 + pattern * 17: b = 146 + pattern * 18
        CASE ELSE: r = 180: g = 190: b = 180
    END SELECT
    SurfaceColour& = _RGB32(r * shade, g * shade, b * shade)
END FUNCTION

SUB Render
    DIM xx AS INTEGER, yy AS INTEGER, i AS INTEGER, length AS SINGLE
    camX = SIN(yaw) * camRadius: camY = 4: camZ = COS(yaw) * camRadius
    fwX = -camX: fwY = 1.55 - camY: fwZ = -camZ
    length = SQR(fwX * fwX + fwY * fwY + fwZ * fwZ)
    fwX = fwX / length: fwY = fwY / length: fwZ = fwZ / length
    rtX = -fwZ: rtY = 0: rtZ = fwX
    upX = -fwY * rtZ: upY = fwZ * rtX - fwX * rtZ: upZ = fwY * rtX
    ' Orthonormal up = cross(right, forward)
    upX = rtY * fwZ - rtZ * fwY
    upY = rtZ * fwX - rtX * fwZ
    upZ = rtX * fwY - rtY * fwX
    _DEST offscreen
    FOR yy = 0 TO rh - 1
        LINE (0, yy)-(rw - 1, yy), _RGB32(87 + yy * .28, 139 + yy * .24, 192 + yy * .12)
    NEXT yy
    FOR yy = 0 TO rh - 1
        FOR xx = 0 TO rw - 1
            zBuffer(xx, yy) = 0
        NEXT xx
    NEXT yy
    FOR i = 1 TO countFaces
        RasterTri faces(i)
    NEXT i
    _DEST screenPage
    _PUTIMAGE (0, 0)-(639, 399), offscreen, screenPage
    COLOR _RGB32(244, 246, 243)
    _PRINTSTRING (10, 9), "RASTER QB64  /  Z BUFFER  /  " + LTRIM$(STR$(rw)) + "x" + LTRIM$(STR$(rh)) + " -> 640x400"
    _PRINTSTRING (10, 27), "Triangles: " + LTRIM$(STR$(countFaces)) + "    A D tourner   +/- zoom   H resolution   ESC sortir"
    _DISPLAY
END SUB
