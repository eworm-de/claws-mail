
/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "jconv.h"

#ifdef HAVE_CODESET
#include <langinfo.h>
#endif

#define DEBUG_DO(x)

#define JCONV_DEFAULT_CONFFILE "/etc/libjconv/default.conf"

typedef struct {
	char *name;
	char *codeset;
	int num_pref_codesets;
	char **pref_codesets;
} jconv_locale_info;


static jconv_locale_info *jconv_locales = NULL;
static int num_jconv_locales = 0;
static int current_locale = -1;

static void
jconv_info_clear_config (void)
{
	int i, j;
	for (i = 0; i < num_jconv_locales; i++) {
		jconv_locale_info *p;
		p = jconv_locales + i;
		free (p->name);
		free (p->codeset);
		for (j = 0; j < p->num_pref_codesets; j++) {
			free(p->pref_codesets[j]);
		}
		if (p->pref_codesets)
			free (p->pref_codesets);
	}
	if (jconv_locales)
		free (jconv_locales);
}

static char *
jconv_gettok (char **strp)
{
	const char *spc = " \t\n";
	char *p, *tok;
	p = *strp;
	p += strspn(p, spc);
	if (*p == 0) return NULL;
	tok = p;
	p += strcspn(p, spc);
	if (*p)
		*(p++) = 0;
	*strp = p;
	return tok;
}

static int
jconv_parse_line (char *buffer)
{
	/* locale_name : codeset_of_the_locale pref_codeset ... */
	char *locname, *colon, *codeset, **codesets, *c;
	int num_codesets;

	locname = jconv_gettok(&buffer);
	if (locname == NULL || locname[0] == '#') return -1;
	colon = jconv_gettok(&buffer);
	if (colon == NULL || strcmp(colon, ":") != 0) return -1;
	codeset = jconv_gettok(&buffer);
	if (codeset == NULL) return -1;
	DEBUG_DO(printf("jconv_parse_line: %s:%s", locname, codeset));
	
	codesets = NULL;
	num_codesets = 0;
	while ((c = jconv_gettok(&buffer)) != NULL) {
		codesets = realloc(codesets,
				   (num_codesets + 1) * sizeof(*codesets));
		codesets[num_codesets] = strdup(c);
		DEBUG_DO(printf(" %s", c));
		num_codesets++;
	}
	DEBUG_DO(printf("\n"));
	
	jconv_locales = realloc(jconv_locales,
				(num_jconv_locales + 1) *
				sizeof(*jconv_locales));
	jconv_locales[num_jconv_locales].name = strdup(locname);
	jconv_locales[num_jconv_locales].codeset = strdup(codeset);
	jconv_locales[num_jconv_locales].num_pref_codesets = num_codesets;
	jconv_locales[num_jconv_locales].pref_codesets = codesets;
	num_jconv_locales++;
	return 0;
}

static int
jconv_info_load_config (const char *conffile)
{
	FILE *fp;
	char buffer[1024];
	jconv_info_clear_config();
	fp = fopen(conffile, "r");
	if (fp == NULL)
		return -1;
	while (fgets(buffer, 1024, fp) != NULL) {
		jconv_parse_line(buffer);
	}
	fclose(fp);
	return 0;
}

static int
jconv_info_query (const char *name, size_t name_len)
{
	int i;
	DEBUG_DO(printf("query %d %s\n", name_len, name));
	for (i = 0; i < num_jconv_locales; i++) {
		if (strlen(jconv_locales[i].name) == name_len &&
		    strncasecmp(jconv_locales[i].name, name, name_len) == 0)
			return i;
	}
	return -1;
}

void
jconv_info_set_locale (void)
{
	char *locale_name;
	
	locale_name = setlocale(LC_CTYPE, NULL);
	if (current_locale >= 0 &&
	    strcasecmp(jconv_locales[current_locale].name, locale_name) == 0)
		return;
	current_locale = -1;
	/* 1st try */
	current_locale = jconv_info_query(locale_name, strlen(locale_name));
	if (current_locale >= 0) return;
	/* 2nd try */
	current_locale = jconv_info_query(locale_name,
					  strcspn(locale_name, "@"));
	if (current_locale >= 0) return;
	/* 3rd try */
	current_locale = jconv_info_query(locale_name,
					  strcspn(locale_name, "@.+,"));
	if (current_locale >= 0) return;
	/* 4th try */
	current_locale = jconv_info_query(locale_name,
					  strcspn(locale_name, "@.+,_"));
	if (current_locale >= 0) return;
	/* a;sldkjfaslf;kjsaf;lsakjf;alksjdf;laskdjf;a */
	current_locale = 0;
}

void
jconv_info_init (const char *conffile)
{
	jconv_info_load_config(conffile ?
			       conffile : JCONV_DEFAULT_CONFFILE);
	jconv_info_set_locale();
	DEBUG_DO(printf("current_locale: %s %s\n",
			jconv_locales[current_locale].name,
			jconv_locales[current_locale].codeset));
}

void
jconv_info_maybe_init (void)
{
	if (current_locale < 0)
		jconv_info_init(NULL);
}

const char *
jconv_info_get_current_codeset (void)
{
	jconv_info_maybe_init();
#ifdef HAVE_CODESET
	return nl_langinfo(CODESET);
#else
	return jconv_locales[current_locale].codeset;
#endif
}

const char *const *
jconv_info_get_pref_codesets (int *num_codesets_r)
{
	const char *const *codes;
	jconv_info_maybe_init();
	codes = (const char **)jconv_locales[current_locale].pref_codesets;
	if (codes == NULL) {
		if (num_codesets_r)
			*num_codesets_r = 1;
		return (const char **)&jconv_locales[current_locale].codeset;
	}
	if (num_codesets_r)
		*num_codesets_r =
			jconv_locales[current_locale].num_pref_codesets;
	return codes;
}

#if 0
int
main (void)
{
	int n;
	const char *const *codes;
	setlocale(LC_ALL, "");
	jconv_info_init(NULL);
	codes = jconv_info_get_pref_codesets (&n);
	printf("codes[0] %s\n", codes[0]);
	/*
	codes[0] = NULL;
	free (codes[0]);
	*/
	return 0;
}
#endif

#if 0
int
main (void)
{
	char *src, *dest;
	size_t len;
	setlocale(LC_ALL, "");
	for (len = 0, src = NULL; !feof(stdin); ) {
		src = realloc(src, len + 4096);
		len += fread(src + len, 1, 4096, stdin);
	}
	src[len] = 0;
	dest = jconv_strdup_conv_fullauto(src);
	fwrite(dest, strlen(dest), 1, stdout);
	return 0;
}
#endif
