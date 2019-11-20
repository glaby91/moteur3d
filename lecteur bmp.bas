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

PRINT "Taille du fichier : ";: PRINT HEX$(taillepfa);: PRINT HEX$(taillepfo):
PRINT "Depart des donnees : ";: PRINT HEX$(offsetimpfo);: PRINT HEX$(offsetimpfa):


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
        PRINT RED, GRE, BLU
        PRINT offsetimpfa
    NEXT
NEXT


CLOSE #1

DO

    IF LEN(INKEY$) THEN EXIT DO
LOOP


SCREEN _NEWIMAGE(800, 600, 32)
mode1& = _DEST
_DEST mode1&

FOR x = 0 TO 200
    FOR y = 0 TO 200
        LINE (x, y)-(x, y), _RGBA32(tablered(x, y), tablegrn(x, y), tableblu(x, y), tabletrans(x, y))
    NEXT
NEXT

