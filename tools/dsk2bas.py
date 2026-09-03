#!/usr/bin/env python3
"""Pull the Applesoft programs out of DOS 3.3 disk images, as text.

Most surviving Apple II software is a .do or .dsk image, not a listing, and
the BASIC inside is tokenised. This walks the catalog, follows each type-A
file's track/sector list, and detokenises with the ROM's keyword table (the
same one token.c carries), so the result loads straight into the interpreter
or feeds tools/corpus.py.

usage: tools/dsk2bas.py image.do [more images ...]   (writes native/NAME.bas)
"""
import sys, os, re
KW = ["END","FOR","NEXT","DATA","INPUT","DEL","DIM","READ","GR","TEXT","PR#","IN#","CALL","PLOT","HLIN","VLIN",
"HGR2","HGR","HCOLOR=","HPLOT","DRAW","XDRAW","HTAB","HOME","ROT=","SCALE=","SHLOAD","TRACE","NOTRACE","NORMAL","INVERSE","FLASH",
"COLOR=","POP","VTAB","HIMEM:","LOMEM:","ONERR","RESUME","RECALL","STORE","SPEED=","LET","GOTO","RUN","IF","RESTORE","&",
"GOSUB","RETURN","REM","STOP","ON","WAIT","LOAD","SAVE","DEF","POKE","PRINT","CONT","LIST","CLEAR","GET","NEW",
"TAB(","TO","FN","SPC(","THEN","AT","NOT","STEP","+","-","*","/","^","AND","OR",">","=","<","SGN","INT","ABS","USR","FRE","SCRN(",
"PDL","POS","SQR","RND","LOG","EXP","COS","SIN","TAN","ATN","PEEK","LEN","STR$","VAL","ASC","CHR$","LEFT$","RIGHT$","MID$"]
def sec(d,t,s): return d[(t*16+s)*256:(t*16+s+1)*256]
def readfile(d, t, s):
    data=b''
    while t or s:
        tsl=sec(d,t,s)
        for i in range(12,256,2):
            dt,ds=tsl[i],tsl[i+1]
            if dt==0 and ds==0: continue
            data+=sec(d,dt,ds)
        t,s=tsl[1],tsl[2]
    return data
def detok(prog):
    n=len(prog); out=[]; i=0
    while i+4<=n:
        nxt=prog[i]|(prog[i+1]<<8); ln=prog[i+2]|(prog[i+3]<<8)
        if nxt==0: break
        i+=4; parts=[]
        while i<n and prog[i]!=0:
            b=prog[i]; i+=1
            if b>=0x80:
                k=KW[b-0x80] if b-0x80<len(KW) else '?'
                parts.append(' '+k+' ' if k.isalpha() or k[-1] in '=(#:$' else k)
            else: parts.append(chr(b))
        i+=1
        out.append('%d %s' % (ln, re.sub(r'  +',' ',''.join(parts)).strip()))
    return '\n'.join(out)+'\n'
for img in sys.argv[1:]:
    d=open(img,'rb').read()
    vtoc=sec(d,17,0); t,s=vtoc[1],vtoc[2]; seen=0
    while t and seen<40:
        c=sec(d,t,s); seen+=1
        for i in range(7):
            e=c[11+i*35:11+(i+1)*35]
            if e[0] in (0,0xff) or (e[2]&0x7f)!=2: continue
            name=bytes(b&0x7f for b in e[3:33]).decode('ascii','replace').rstrip()
            f=readfile(d,e[0],e[1]); L=f[0]|(f[1]<<8); prog=f[2:2+L]
            outname='native/%s-%s.bas' % (os.path.basename(img)[:-3], name.replace(' ','_').lower())
            open(outname,'w').write(detok(prog)); print('wrote', outname, len(prog),'bytes')
        t,s=c[1],c[2]
