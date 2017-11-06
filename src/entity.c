/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2017 Ricardo Mones and the Claws Mail team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"
#include "utils.h"
#include "entity.h"

#define ENTITY_MAX_LEN 8
#define DECODED_MAX_LEN 6

static GHashTable *symbol_table = NULL;

typedef struct _EntitySymbol EntitySymbol;

struct _EntitySymbol
{
	gchar *const key;
	gchar *const value;
};

static EntitySymbol symbolic_entities[] = {
	/* in alphabetical order with upper-case version first */
	{"Aacute", "\303\201"},
	{"aacute", "\303\241"},
	{"Acirc", "\303\202"},
	{"acirc", "\303\242"},
	{"acute", "\302\264"},
	{"AElig", "\303\206"},
	{"aelig", "\303\246"},
	{"Agrave", "\303\200"},
	{"agrave", "\303\240"},
	{"amp", "&" },
	{"apos", "'" },
	{"Aring", "\303\205"},
	{"aring", "\303\245"},
	{"Atilde", "\303\203"},
	{"atilde", "\303\243"},
	{"Auml", "\303\204"},
	{"auml", "\303\244"},
	{"bdquo", "\342\200\236"},
	{"brvbar", "\302\246"},
	{"bull", "\342\200\242"},
	{"Ccedil", "\303\207"},
	{"ccedil", "\303\247"},
	{"cedil", "\302\270"},
	{"cent", "\302\242"},
	{"circ", "\313\206"},
	{"copy", "©" },
	{"curren", "\302\244"},
	{"dagger", "\342\200\240"},
	{"Dagger", "\342\200\241"},
	{"deg", "\302\260"},
	{"divide", "\303\267"},
	{"Eacute", "\303\211"},
	{"eacute", "\303\251"},
	{"Ecirc", "\303\212"},
	{"ecirc", "\303\252"},
	{"Egrave", "\303\210"},
	{"egrave", "\303\250"},
	{"emsp", "\342\200\203"},
	{"ensp", "\342\200\202"},
	{"ETH", "\303\220"},
	{"eth", "\303\260"},
	{"Euml", "\303\213"},
	{"euml", "\303\253"},
	{"euro", "€" },
	{"frac12", "\302\275"},
	{"frac14", "\302\274"},
	{"frac34", "\302\276"},
	{"gt", ">" },
	{"hellip", "…" },
	{"Iacute", "\303\215"},
	{"iacute", "\303\255"},
	{"Icirc", "\303\216"},
	{"icirc", "\303\256"},
	{"iexcl", "\302\241"},
	{"Igrave", "\303\214"},
	{"igrave", "\303\254"},
	{"iquest", "\302\277"},
	{"Iuml", "\303\217"},
	{"iuml", "\303\257"},
	{"laquo", "\302\253"},
	{"ldquo",  "“" },
	{"lsaquo", "\342\200\271"},
	{"lsquo",  "‘" },
	{"lt", "<" },
	{"macr", "\302\257"},
	{"mdash", "—" },
	{"micro", "\302\265"},
	{"middot", "\302\267"},
	{"nbsp", " " },
	{"ndash", "\342\200\223"},
	{"not", "\302\254"},
	{"Ntilde", "\303\221"},
	{"ntilde", "\303\261"},
	{"Oacute", "\303\223"},
	{"oacute", "\303\263"},
	{"Ocirc", "\303\224"},
	{"ocirc", "\303\264"},
	{"OElig", "\305\222"},
	{"oelig", "\305\223"},
	{"Ograve", "\303\222"},
	{"ograve", "\303\262"},
	{"ordf", "\302\252"},
	{"ordm", "\302\272"},
	{"Oslash", "\303\230"},
	{"oslash", "\303\270"},
	{"Otilde", "\303\225"},
	{"otilde", "\303\265"},
	{"Ouml", "\303\226"},
	{"ouml", "\303\266"},
	{"para", "\302\266"},
	{"permil", "\342\200\260"},
	{"plusmn", "\302\261"},
	{"pound", "\302\243"},
	{"quot", "\"" },
	{"raquo", "\302\273"},
	{"rdquo",  "”" },
	{"reg", "®" },
	{"rsaquo", "\342\200\272"},
	{"rsquo",  "’" },
	{"sbquo", "\342\200\232"},
	{"Scaron", "\305\240"},
	{"scaron", "\305\241"},
	{"sect", "\302\247"},
	{"shy", "\302\255"},
	{"squot", "\47"},
	{"sup1", "\302\271"},
	{"sup2", "\302\262"},
	{"sup3", "\302\263"},
	{"szlig", "\303\237"},
	{"thinsp", "\342\200\211"},
	{"THORN", "\303\236"},
	{"thorn", "\303\276"},
	{"tilde", "\313\234"},
	{"times", "\303\227"},
	{"trade", "™" },
	{"Uacute", "\303\232"},
	{"uacute", "\303\272"},
	{"Ucirc", "\303\233"},
	{"ucirc", "\303\273"},
	{"Ugrave", "\303\231"},
	{"ugrave", "\303\271"},
	{"uml", "\302\250"},
	{"Uuml", "\303\234"},
	{"uuml", "\303\274"},
	{"Yacute", "\303\235"},
	{"yacute", "\303\275"},
	{"yen", "\302\245"},
	{"yuml", "\303\277"},
	{"Yuml", "\305\270"},
	{NULL, NULL}
};

static gchar* entity_extract_to_buffer(gchar *p, gchar b[])
{
	gint i = 0;

	while (*p != '\0' && *p != ';' && i < ENTITY_MAX_LEN) {
		b[i] = *p;
		++i, ++p;
	}
	if (*p != ';' || i == ENTITY_MAX_LEN)
		return NULL;
	b[i] = '\0';

	return b;
}

static gchar *entity_decode_numeric(gchar *str)
{
	gchar b[ENTITY_MAX_LEN];
	gchar *p = str, *res;
	gboolean hex = FALSE;
	gunichar c;

	++p;
	if (*p == '\0')
		return NULL;

	if (*p == 'x') {
		hex = TRUE;
		++p;
		if (*p == '\0')
			return NULL;
	}

	if (entity_extract_to_buffer (p, b) == NULL)
		return NULL;

	c = g_ascii_strtoll (b, NULL, (hex? 16: 10));
	res = g_malloc0 (DECODED_MAX_LEN + 1);
	g_unichar_to_utf8 (c, res);

	return res;
}

static gchar *entity_decode_symbol(gchar *str)
{
	gchar b[ENTITY_MAX_LEN];
	gchar *decoded;

	if (entity_extract_to_buffer (str, b) == NULL)
		return NULL;

	if (symbol_table == NULL) {
		gint i;

		symbol_table = g_hash_table_new (g_str_hash, g_str_equal);
		for (i = 0; symbolic_entities[i].key != NULL; ++i) {
			g_hash_table_insert (symbol_table,
				symbolic_entities[i].key, symbolic_entities[i].value);
		}
		debug_print("initialized entities table with %d symbols\n", i);
	}

	decoded = g_hash_table_lookup (symbol_table, b);
	if (decoded != NULL)
		return g_strdup (decoded);

	return NULL;
}

gchar *entity_decode(gchar *str)
{
	gchar *p = str;
	if (p == NULL || *p != '&')
		return NULL;
	++p;
	if (*p == '\0')
		return NULL;
	if (*p == '#')
		return entity_decode_numeric(p);
	else
		return entity_decode_symbol(p);
}
