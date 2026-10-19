' QB64 RAY CASTING ENGINE — branch raycasting — version 1.0.0 (2026-10-19)
' ONE primary ray per INTERNAL 16:10 pixel; first analytic hit only.
' True sphere / AABB / infinite-plane intersections, no triangle rasterizer.
' ESC quits; A/D camera, +/- zoom; H toggles 320x200 and 640x400.
' Educational, stand-alone subset of the JavaScript V9 (no BVH/shadow atlas).
OPTION _EXPLICIT

TYPE Ball
    x AS SINGLE
    y AS SINGLE
    z AS SINGLE
    radius AS SINGLE
    mat AS INTEGER
END TYPE
TYPE Cuboid
    x0 AS SINGLE
    y0 AS SINGLE
    z0 AS SINGLE
    x1 AS SINGLE
    y1 AS SINGLE
    z1 AS SINGLE
    mat AS INTEGER
END TYPE
DECLARE SUB BuildScene ()
DECLARE SUB TraceOne (dx AS SINGLE, dy AS SINGLE, dz AS SINGLE, hitID AS INTEGER, depth AS SINGLE, nx AS SINGLE, ny AS SINGLE, nz AS SINGLE)
DECLARE SUB RenderFrame ()
DECLARE FUNCTION Pigment& (mat AS INTEGER, px AS SINGLE, py AS SINGLE, pz AS SINGLE, nx AS SINGLE, ny AS SINGLE, nz AS SINGLE, dist AS SINGLE)
DECLARE FUNCTION SkyColour& (dy AS SINGLE, dx AS SINGLE, dz AS SINGLE)

DIM SHARED spheres(1 TO 64) AS Ball, cubes(1 TO 32) AS Cuboid
DIM SHARED sphereCount AS INTEGER, cubeCount AS INTEGER
DIM SHARED camX AS SINGLE, camY AS SINGLE, camZ AS SINGLE
DIM SHARED fwX AS SINGLE, fwY AS SINGLE, fwZ AS SINGLE
DIM SHARED rtX AS SINGLE, rtY AS SINGLE, rtZ AS SINGLE
DIM SHARED upX AS SINGLE, upY AS SINGLE, upZ AS SINGLE
DIM SHARED yaw AS SINGLE, camRadius AS SINGLE
DIM SHARED rw AS INTEGER, rh AS INTEGER
DIM SHARED screenPage AS LONG, offscreen AS LONG, totalRays AS LONG
DIM keyPress AS STRING, frameStart AS DOUBLE
CONST PI = 3.141592653589793#

screenPage = _NEWIMAGE(640, 400, 32)
SCREEN screenPage
_TITLE "Moteur3D QB64 — RAY CASTING analytique / 16:10"
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
    RenderFrame
    _LIMIT 25
LOOP
_FREEIMAGE offscreen
SYSTEM

SUB BuildScene
    DIM i AS INTEGER, angle AS SINGLE
    sphereCount = 0: cubeCount = 0
    ' Trunk: overlapping sphere segments form a softly tapered round trunk.
    FOR i = 0 TO 8
        sphereCount = sphereCount + 1
        spheres(sphereCount).x = -1.7: spheres(sphereCount).y = .25 + i * .29
        spheres(sphereCount).z = .4: spheres(sphereCount).radius = .32 - .012 * i
        spheres(sphereCount).mat = 2
    NEXT i
    FOR i = 0 TO 14
        angle = i * 2.39996
        sphereCount = sphereCount + 1
        spheres(sphereCount).x = -1.7 + COS(angle) * .78
        spheres(sphereCount).y = 3.07 + .27 * SIN(angle * 2)
        spheres(sphereCount).z = .4 + SIN(angle) * .66
        spheres(sphereCount).radius = .68 + .10 * (i MOD 3)
        spheres(sphereCount).mat = 3
    NEXT i
    ' Shrubs / decorative foliage
    FOR i = 0 TO 7
        angle = i * 2.39996
        sphereCount = sphereCount + 1
        spheres(sphereCount).x = 3 + COS(angle) * .82
        spheres(sphereCount).y = .83 + (i MOD 3) * .19
        spheres(sphereCount).z = -1.6 + SIN(angle) * .72
        spheres(sphereCount).radius = .36
        spheres(sphereCount).mat = 3
    NEXT i
    ' A stone back wall and a wooden bench made of exact boxes
    cubeCount = 1
    cubes(1).x0 = -6: cubes(1).y0 = 0: cubes(1).z0 = -3.3
    cubes(1).x1 = 6: cubes(1).y1 = 1.4: cubes(1).z1 = -3
    cubes(1).mat = 4
    cubeCount = 2
    cubes(2).x0 = 1.05: cubes(2).y0 = .75: cubes(2).z0 = 1.2
    cubes(2).x1 = 3.25: cubes(2).y1 = .92: cubes(2).z1 = 1.85
    cubes(2).mat = 2
    cubeCount = 3
    cubes(3).x0 = 1.05: cubes(3).y0 = .93: cubes(3).z0 = 1.75
    cubes(3).x1 = 3.25: cubes(3).y1 = 1.60: cubes(3).z1 = 1.93
    cubes(3).mat = 2
END SUB

SUB TraceOne (dx AS SINGLE, dy AS SINGLE, dz AS SINGLE, hitID AS INTEGER, depth AS SINGLE, nx AS SINGLE, ny AS SINGLE, nz AS SINGLE)
    DIM i AS INTEGER, b AS SINGLE, cc AS SINGLE, disc AS SINGLE, d AS SINGLE
    DIM qx AS SINGLE, qy AS SINGLE, qz AS SINGLE, near AS SINGLE, far AS SINGLE
    DIM ta AS SINGLE, tb AS SINGLE, origin AS SINGLE, direction AS SINGLE
    DIM lowBound AS SINGLE, highBound AS SINGLE, j AS INTEGER
    DIM px AS SINGLE, py AS SINGLE, pz AS SINGLE
    hitID = 0: depth = 999999: nx = 0: ny = 1: nz = 0
    ' The infinite ground plane is candidate number 1.
    IF dy < -.00001 THEN
        d = -camY / dy
        IF d > .001 THEN depth = d: hitID = 1
    END IF
    FOR i = 1 TO sphereCount
        qx = camX - spheres(i).x: qy = camY - spheres(i).y: qz = camZ - spheres(i).z
        b = qx * dx + qy * dy + qz * dz
        cc = qx * qx + qy * qy + qz * qz - spheres(i).radius * spheres(i).radius
        disc = b * b - cc
        IF disc >= 0 THEN
            d = -b - SQR(disc)
            IF d <= .001 THEN d = -b + SQR(disc)
            IF d > .001 AND d < depth THEN
                hitID = i + 1: depth = d
                px = camX + dx * d - spheres(i).x
                py = camY + dy * d - spheres(i).y
                pz = camZ + dz * d - spheres(i).z
                nx = px / spheres(i).radius: ny = py / spheres(i).radius: nz = pz / spheres(i).radius
            END IF
        END IF
    NEXT i
    FOR i = 1 TO cubeCount
        near = -999999: far = 999999
        FOR j = 1 TO 3
            SELECT CASE j
                CASE 1: origin = camX: direction = dx: lowBound = cubes(i).x0: highBound = cubes(i).x1
                CASE 2: origin = camY: direction = dy: lowBound = cubes(i).y0: highBound = cubes(i).y1
                CASE 3: origin = camZ: direction = dz: lowBound = cubes(i).z0: highBound = cubes(i).z1
            END SELECT
            IF ABS(direction) < .0000001 THEN
                IF origin < lowBound OR origin > highBound THEN near = 999999: far = -999999: EXIT FOR
            ELSE
                ta = (lowBound - origin) / direction: tb = (highBound - origin) / direction
                IF ta > tb THEN SWAP ta, tb
                IF ta > near THEN near = ta
                IF tb < far THEN far = tb
            END IF
            IF near > far THEN EXIT FOR
        NEXT j
        IF near <= 0 THEN near = far
        IF near > .001 AND near < depth AND near <= far THEN
            depth = near: hitID = 100 + i
            px = camX + dx * near: py = camY + dy * near: pz = camZ + dz * near
            nx = 0: ny = 0: nz = 0
            IF ABS(px - cubes(i).x0) < .005 THEN nx = -1
            IF ABS(px - cubes(i).x1) < .005 THEN nx = 1
            IF ABS(py - cubes(i).y0) < .005 THEN ny = -1
            IF ABS(py - cubes(i).y1) < .005 THEN ny = 1
            IF ABS(pz - cubes(i).z0) < .005 THEN nz = -1
            IF ABS(pz - cubes(i).z1) < .005 THEN nz = 1
        END IF
    NEXT i
END SUB

FUNCTION SkyColour& (dy AS SINGLE, dx AS SINGLE, dz AS SINGLE)
    DIM k AS SINGLE, cloud AS SINGLE, r AS SINGLE, g AS SINGLE, b AS SINGLE
    k = .45 + dy
    IF k < 0 THEN k = 0
    IF k > 1 THEN k = 1
    cloud = SIN(dx * 19 + dz * 7) * SIN(dz * 13 - dy * 21)
    IF cloud < .55 THEN cloud = 0 ELSE cloud = (cloud - .55) * .65
    r = 180 - 108 * k + cloud * 75
    g = 207 - 76 * k + cloud * 65
    b = 226 - 29 * k + cloud * 55
    SkyColour& = _RGB32(r, g, b)
END FUNCTION

FUNCTION Pigment& (mat AS INTEGER, px AS SINGLE, py AS SINGLE, pz AS SINGLE, nx AS SINGLE, ny AS SINGLE, nz AS SINGLE, dist AS SINGLE)
    DIM r AS SINGLE, g AS SINGLE, b AS SINGLE, uv AS SINGLE
    DIM light AS SINGLE, shadow AS SINGLE, fog AS SINGLE
    uv = SIN(px * 21 + py * 9 + pz * 11) * 9
    SELECT CASE mat
        CASE 1: r = 89 + uv: g = 129 + uv: b = 68 + uv / 2
        CASE 2: r = 130 + uv: g = 89 + uv: b = 55 + uv / 2
        CASE 3: r = 70 + uv: g = 141 + uv: b = 67 + uv / 2
        CASE 4: r = 170 + uv: g = 166 + uv: b = 147 + uv / 2
        CASE ELSE: r = 178: g = 178: b = 178
    END SELECT
    light = nx * -.45 + ny * .8 + nz * .37
    IF light < 0 THEN light = 0
    light = .30 + .70 * light
    ' Deliberately pre-baked fake contact shadow: NO additional light ray.
    shadow = 1
    IF mat = 1 THEN
        IF (px + 1.7) ^ 2 + (pz - .4) ^ 2 < 3.2 THEN shadow = .69
    END IF
    light = light * shadow
    fog = 1 - EXP(-dist * .027)
    r = r * light * (1 - fog) + 184 * fog
    g = g * light * (1 - fog) + 207 * fog
    b = b * light * (1 - fog) + 219 * fog
    Pigment& = _RGB32(r, g, b)
END FUNCTION

SUB RenderFrame
    DIM xx AS INTEGER, yy AS INTEGER, id AS INTEGER
    DIM fx AS SINGLE, fy AS SINGLE, fz AS SINGLE, dx AS SINGLE, dy AS SINGLE, dz AS SINGLE
    DIM side AS SINGLE, height AS SINGLE, lenR AS SINGLE, length AS SINGLE, t AS SINGLE
    DIM nx AS SINGLE, ny AS SINGLE, nz AS SINGLE, px AS SINGLE, py AS SINGLE, pz AS SINGLE
    DIM mat AS INTEGER, ratio AS SINGLE
    camX = SIN(yaw) * camRadius: camY = 4: camZ = COS(yaw) * camRadius
    fwX = -camX: fwY = 1.55 - camY: fwZ = -camZ
    length = SQR(fwX * fwX + fwY * fwY + fwZ * fwZ)
    fwX = fwX / length: fwY = fwY / length: fwZ = fwZ / length
    rtX = -fwZ: rtY = 0: rtZ = fwX
    upX = rtY * fwZ - rtZ * fwY
    upY = rtZ * fwX - rtX * fwZ
    upZ = rtX * fwY - rtY * fwX
    _DEST offscreen
    totalRays = 0
    FOR yy = 0 TO rh - 1
        height = (1 - 2 * (yy + .5) / rh) * .58 * rh / rw
        FOR xx = 0 TO rw - 1
            side = (2 * (xx + .5) / rw - 1) * .58
            fx = fwX + rtX * side + upX * height
            fy = fwY + rtY * side + upY * height
            fz = fwZ + rtZ * side + upZ * height
            lenR = SQR(fx * fx + fy * fy + fz * fz)
            dx = fx / lenR: dy = fy / lenR: dz = fz / lenR
            TraceOne dx, dy, dz, id, t, nx, ny, nz
            totalRays = totalRays + 1
            IF id = 0 THEN
                PSET (xx, yy), SkyColour&(dy, dx, dz)
            ELSE
                px = camX + dx * t: py = camY + dy * t: pz = camZ + dz * t
                IF id = 1 THEN
                    mat = 1
                    nx = 0: ny = 1: nz = 0
                ELSEIF id < 100 THEN
                    mat = spheres(id - 1).mat
                ELSE
                    mat = cubes(id - 100).mat
                END IF
                PSET (xx, yy), Pigment&(mat, px, py, pz, nx, ny, nz, t)
            END IF
        NEXT xx
    NEXT yy
    _DEST screenPage
    _PUTIMAGE (0, 0)-(639, 399), offscreen, screenPage
    COLOR _RGB32(255, 255, 255)
    _PRINTSTRING (10, 9), "RAYCAST QB64  /  1 RAYON PAR PIXEL  /  " + LTRIM$(STR$(rw)) + "x" + LTRIM$(STR$(rh))
    _PRINTSTRING (10, 27), "Rayons: " + LTRIM$(STR$(totalRays)) + "  Objets: " + LTRIM$(STR$(sphereCount + cubeCount)) + "   A D tourner   +/- zoom   H qualite   ESC sortir"
    _DISPLAY
END SUB
