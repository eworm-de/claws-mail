#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <glib.h>

#define BUFSIZE 1024
#define STDIN 0
#define STDOUT 1

int main(int argc, char** argv) {
	FILE *fin,*fout;
	int nread;
	gsize c_in,c_out;
	gchar *inbuf = g_malloc(BUFSIZE);
	gchar *outbuf = g_malloc(BUFSIZE);

	if (argc > 1) fin=fopen(argv[1],"rb");
	else fin=fdopen(STDIN,"rb");

	if (argc > 2) fout=fopen(argv[2],"wb");
	else fout=fdopen(STDOUT,"wb");

	if (fin && fout) {
		while ( fgets(inbuf, BUFSIZE, fin) ) {
			outbuf=g_locale_to_utf8(inbuf,-1,&c_in,&c_out,NULL);
			outbuf[c_out]=0;
			fputs(outbuf, fout);
		}
		fclose(fout);
		fclose(fin);
	}
	g_free(inbuf);
	g_free(outbuf);
}
