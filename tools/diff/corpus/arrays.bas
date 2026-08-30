10 A(7) = 3.5
20 PRINT A(7)
30 PRINT A(0)
40 DIM B(3,3)
50 FOR I = 0 TO 3
60 FOR J = 0 TO 3
70 B(I,J) = I * 4 + J
80 NEXT J,I
90 PRINT B(3,3);" ";B(0,3);" ";B(2,1)
100 DIM C$(2)
110 C$(1) = "MID"
120 PRINT C$(1);"[";C$(0);"]"
