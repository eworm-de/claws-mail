void (*outascii)(), (*outkanji)(), (*outgaiji)(), (*outkana)();
/*---------------------------------------------------------------------
    NAME
	outsjis
 ---------------------------------------------------------------------*/
void outsjis(c1, c2)
    register int c1, c2;
{
    register int c;

    c = c1 * 2 - (c1 <= 0x9f ? 0x00e1 : (c1 < 0xf0 ? 0x0161 : 0x01bf));
    if (c2 < 0x9f)
	c2 = c2 - (c2 > 0x7f ? 0x20 : 0x1f);
    else {
	c2 = c2 - 0x7e;
	c++;
    }
    (*(c1 <= 0xef ? outkanji : outgaiji))(c, c2);
}
