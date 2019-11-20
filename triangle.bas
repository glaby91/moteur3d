SCREEN 12
DIM trianglex(300)
DIM triangley(300)

'trace deux axes 640//2 320    480/2   240

LINE (0, 240)-(640, 240), 3
LINE (320, 0)-(320, 480), 3

'coordonnee decalage
decx = 320
decy = 240


trianglex(0) = 15
trianglex(1) = 15
trianglex(2) = 30
triangley(0) = 30
triangley(1) = 15
triangley(2) = 30
f = 15
zoom = 200

FOR rot = 3 TO 296 STEP 3

    trianglex(rot) = trianglex(0) + zoom * SIN(3.14 * f * rot)
    triangley(rot) = triangley(0) + zoom * COS(3.14 * f * rot)
    trianglex(rot + 1) = trianglex(1) + zoom * SIN(3.14 * f * rot)
    trianglex(rot + 2) = trianglex(2) + zoom * SIN(3.14 * f * rot)
    triangley(rot + 1) = triangley(1) + zoom * COS(3.14 * f * rot)
    triangley(rot + 2) = triangley(2) + zoom * COS(3.14 * f * rot)
NEXT

co = 15
GOSUB triangle

DO

    IF LEN(INKEY$) THEN EXIT DO
LOOP

END
'trace des triangles
triangle:
FOR x = 0 TO 296 STEP 3
    LINE (decx + trianglex(x), decy + triangley(x))-(decx + trianglex(x + 1), decy + triangley(x + 1)), co
    LINE (decx + trianglex(x + 1), decy + triangley(x + 1))-(decx + trianglex(x + 2), decy + triangley(x + 2)), co
    LINE (decx + trianglex(x), decy + triangley(x))-(decx + trianglex(x + 2), decy + triangley(x + 2)), co
NEXT
RETURN

