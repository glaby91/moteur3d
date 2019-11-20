'''''''''''''''''''''''''''''''''''''''''''''
'lecture fichier bmp
'Reduce /4
'Mirror texturing
'Inverse texturing
'
'''''''''''''''''''''''''''''''


PRINT HEX$(_RGBA32(red, grn, blu, inte))


DIM tableredf(100, 100)
DIM tablegrnf(100, 100)
DIM tablebluf(100, 100)
DIM tabletransf(100, 100)

DIM tablemirrorredf(100, 100)
DIM tablemirrorgrnf(100, 100)
DIM tablemirrorbluf(100, 100)
DIM tablemirrortransf(100, 100)

DIM EnteteID AS INTEGER, taillepfo AS INTEGER, taillepfa AS INTEGER
DIM offsetimpfa AS _UNSIGNED LONG
DIM offsetimpfo AS _UNSIGNED LONG


DIM RED AS _UNSIGNED _BYTE
DIM GRE AS _UNSIGNED _BYTE
DIM BLU AS _UNSIGNED _BYTE

DIM tablered(200, 200)
DIM tablegrn(200, 200)
DIM tableblu(200, 200)
DIM tabletrans(200, 200)

OPEN "text1.bmp" FOR BINARY AS #1
GET #1, 1, EnteteID

IF EnteteID = &H4D42 THEN PRINT "Detection Fichier bitmap ok " ELSE CLOSE #1: END

GET #1, 3, taillepfa
GET #1, 5, taillepfo
GET #1, 11, offsetimpfa
GET #1, 12, offsetimpfo

'PRINT "Taille du fichier : ";: PRINT HEX$(taillepfa);: PRINT HEX$(taillepfo):
'PRINT "Depart des donnees : ";: PRINT HEX$(offsetimpfo);: PRINT HEX$(offsetimpfa):


FOR y = 200 TO 1 STEP -1
    FOR x = 1 TO 200

        GET #1, offsetimpfa + 1, BLU
        GET #1, offsetimpfa + 2, GRE
        GET #1, offsetimpfa + 3, RED
        tablered(x, y) = RED
        tablegrn(x, y) = GRE
        tableblu(x, y) = BLU
        tabletrans(x, y) = 255
        offsetimpfa = offsetimpfa + 3
        ' PRINT RED, GRE, BLU
        ' PRINT offsetimpfa
    NEXT
NEXT


CLOSE #1




' Scale to table 100,100
xa = 0
ya = 0
FOR x = 0 TO 198 STEP 2

    FOR y = 0 TO 198 STEP 2
        tableredf(xa, ya) = ((tablered(x, y) + tablered(x + 1, y) + tablered(x + 1, y + 1) + tablered(x, y + 1))) / 4
        tablegrnf(xa, ya) = ((tablegrn(x, y) + tablegrn(x + 1, y) + tablegrn(x + 1, y + 1) + tablegrn(x, y + 1))) / 4
        tablebluf(xa, ya) = ((tableblu(x, y) + tableblu(x + 1, y) + tableblu(x + 1, y + 1) + tableblu(x, y + 1))) / 4
        tabletransf(xa, ya) = (tabletrans(x, y) + tabletrans(x + 1, y) + tabletrans(x + 1, y + 1) + tabletrans(x, y + 1)) / 4
        ya = ya + 1
    NEXT
    ya = 0
    xa = xa + 1
NEXT

' scale table to tablemirror

FOR y = 0 TO 100
    FOR x = 0 TO 100
        tablemirrorredf(x, y) = tableredf(100 - x, y)
        tablemirrorgrnf(x, y) = tablegrnf(100 - x, y)
        tablemirrorbluf(x, y) = tablebluf(100 - x, y)
        tablemirrortransf(x, y) = tabletransf(100 - x, y)

    NEXT
NEXT
'dessine

SCREEN _NEWIMAGE(800, 600, 32)
mode1& = _DEST
_DEST mode1&

FOR x = 0 TO 200
    FOR y = 0 TO 200
        LINE (x, y)-(x, y), _RGBA32(tablered(x, y), tablegrn(x, y), tableblu(x, y), tabletrans(x, y))
    NEXT
NEXT

t = 0

f = 1
u = 0
FOR y = 0 TO 100
    FOR x = 0 TO 100

        LINE (x + u + 150, y + 300)-(x + u + 150, y + 300), _RGBA32(tableredf(x, y), tablegrnf(x, y), tablebluf(x, y), tabletransf(x, y))
        LINE (x + 250, y + 400)-(x + 250, y + 400), _RGBA32(tableredf(x, y), tablegrnf(x, y), tablebluf(x, y), tabletransf(x, y))
    NEXT
    u = u + 1
NEXT
decyy = 0
FOR x = 0 TO 100
    decyy = decyy + 1

    FOR y = 0 TO 100
        LINE (500 + x, y)-(500 + x, y), _RGBA32(tableredf(x, y), tablegrnf(x, y), tablebluf(x, y), tabletransf(x, y))
        LINE (500 + x, y + 150)-(500 + x, y + 150), _RGBA32(tablemirrorredf(x, y), tablemirrorgrnf(x, y), tablemirrorbluf(x, y), tablemirrortransf(x, y))
        LINE (500 + x, y + 300)-(500 + x, y + 300), _RGBA32(255 - tablemirrorredf(x, y), 255 - tablemirrorgrnf(x, y), 255 - tablemirrorbluf(x, y), tablemirrortransf(x, y))
        decy = 3 * COS(6.28 * f * t)

        LINE (x, y + 300 + decy)-(x, y + 300 + decy), _RGBA32(tableredf(x, y), tablegrnf(x, y), tablebluf(x, y), tabletransf(x, y))


        LINE (x + 150, y + decyy + 300)-(x + 150, y + decyy + 300), _RGBA32(tableredf(x, y), tablegrnf(x, y), tablebluf(x, y), tabletransf(x, y))


        t = t + 1

    NEXT

NEXT


