/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
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

#include "utils.h"

#define ENTITY_MAX_LEN 8
#define DECODED_MAX_LEN 6

static GHashTable *symbol_table = NULL;

typedef struct _EntitySymbol EntitySymbol;

struct _EntitySymbol
{
	gchar *const key;
	gchar *const value;
};

/* in alphabetical order with upper-case version first */
static EntitySymbol symbolic_entities[] = {
	/* A */
	{"Aacute", "Á"},
	{"aacute", "á"},
	{"Acirc", "Â"},
	{"acirc", "â"},
	{"acute", "´"},
	{"AElig", "Æ"},
	{"aelig", "æ"},
	{"Agrave", "À"},
	{"agrave", "à"},
	{"alefsym", "ℵ"},
	{"Alpha", "Α"},
	{"alpha", "α"},
	{"amp", "&"},
	{"and", "∧"},
	{"ang", "∠"},
	{"apos", "'"},
	{"Aring", "Å"},
	{"aring", "å"},
	{"asymp", "≈"},
	{"Atilde", "Ã"},
	{"atilde", "ã"},
	{"Auml", "Ä"},
	{"auml", "ä"},
	/* B */
	{"bdquo", "„"},
	{"Beta", "Β"},
	{"beta", "β"},
	{"brvbar", "¦"},
	{"bull", "•"},
	/* C */
	{"cap", "∩"},
	{"Ccedil", "Ç"},
	{"ccedil", "ç"},
	{"cedil", "¸"},
	{"cent", "¢"},
	{"Chi", "Χ"},
	{"chi", "χ"},
	{"circ", "ˆ"},
	{"clubs", "♣"},
	{"cong", "≅"},
	{"copy", "©"},
	{"crarr", "↵"},
	{"cup", "∪"},
	{"curren", "¤"},
	/* D */
	{"dagger", "†"},
	{"Dagger", "‡"},
	{"dArr", "⇓"},
	{"darr", "↓"},
	{"deg", "°"},
	{"Delta", "Δ"},
	{"delta", "δ"},
	{"diams", "♦"},
	{"divide", "÷"},
	/* E */
	{"Eacute", "É"},
	{"eacute", "é"},
	{"Ecirc", "Ê"},
	{"ecirc", "ê"},
	{"Egrave", "È"},
	{"egrave", "è"},
	{"empty", "∅"},
	{"emsp", "\xE2\x80\x83"},
	{"ensp", "\xE2\x80\x82"},
	{"Epsilon", "Ε"},
	{"epsilon", "ε"},
	{"equiv", "≡"},
	{"Eta", "Η"},
	{"eta", "η"},
	{"ETH", "Ð"},
	{"eth", "ð"},
	{"Euml", "Ë"},
	{"euml", "ë"},
	{"euro", "€"},
	{"exist", "∃"},
	/* F */
	{"fnof", "ƒ"},
	{"forall", "∀"},
	{"frac12", "½"},
	{"frac14", "¼"},
	{"frac34", "¾"},
	{"frasl", "⁄"},
	/* G */
	{"Gamma", "Γ"},
	{"gamma", "γ"},
	{"ge", "≥"},
	{"gt", ">"},
	/* H */
	{"hArr", "⇔"},
	{"harr", "↔"},
	{"hearts", "♥"},
	{"hellip", "…"},
	/* I */
	{"Iacute", "Í"},
	{"iacute", "í"},
	{"IArr", "⇐"},
	{"Icirc", "Î"},
	{"icirc", "î"},
	{"iexcl", "¡"},
	{"Igrave", "Ì"},
	{"igrave", "ì"},
	{"image", "ℑ"},
	{"infin", "∞"},
	{"int", "∫"},
	{"Iota", "Ι"},
	{"iota", "ι"},
	{"iquest", "¿"},
	{"isin", "∈"},
	{"Iuml", "Ï"},
	{"iuml", "ï"},
	/* K */
	{"Kappa", "Κ"},
	{"kappa", "κ"},
	/* L */
	{"Lambda", "Λ"},
	{"lambda", "λ"},
	{"lang", "〈"},
	{"laquo", "«"},
	{"larr", "←"},
	{"lceil", "⌈"},
	{"ldquo", "“"},
	{"le", "≤"},
	{"lfloor", "⌊"},
	{"lowast", "∗"},
	{"loz", "◊"},
	{"lrm", "\xE2\x80\x8E"},
	{"lsaquo", "‹"},
	{"lsquo", "‘"},
	{"lt", "<"},
	/* M */
	{"macr", "¯"},
	{"mdash", "—"},
	{"micro", "µ"},
	{"middot", "·"},
	{"minus", "−"},
	{"Mu", "Μ"},
	{"mu", "μ"},
	/* N */
	{"nabla", "∇"},
	{"nbsp", "\xC2\xA0"},
	{"ndash", "–"},
	{"ne", "≠"},
	{"ni", "∋"},
	{"not", "¬"},
	{"notin", "∉"},
	{"nsub", "⊄"},
	{"Ntilde", "Ñ"},
	{"ntilde", "ñ"},
	{"Nu", "Ν"},
	{"nu", "ν"},
	/* O */
	{"Oacute", "Ó"},
	{"oacute", "ó"},
	{"Ocirc", "Ô"},
	{"ocirc", "ô"},
	{"OElig", "Œ"},
	{"oelig", "œ"},
	{"Ograve", "Ò"},
	{"ograve", "ò"},
	{"oline", "‾"},
	{"Omega", "Ω"},
	{"omega", "ω"},
	{"Omicron", "Ο"},
	{"omicron", "ο"},
	{"oplus", "⊕"},
	{"or", "∨"},
	{"ordf", "ª"},
	{"ordm", "º"},
	{"Oslash", "Ø"},
	{"oslash", "ø"},
	{"Otilde", "Õ"},
	{"otilde", "õ"},
	{"otimes", "⊗"},
	{"Ouml", "Ö"},
	{"ouml", "ö"},
	/* P */
	{"para", "¶"},
	{"part", "∂"},
	{"permil", "‰"},
	{"perp", "⊥"},
	{"Phi", "Φ"},
	{"phi", "φ"},
	{"Pi", "Π"},
	{"pi", "π"},
	{"piv", "ϖ"},
	{"plusmn", "±"},
	{"pound", "£"},
	{"Prime", "″"},
	{"prime", "′"},
	{"prod", "∏"},
	{"prop", "∝"},
	{"Psi", "Ψ"},
	{"psi", "ψ"},
	/* Q */
	{"quot", "\""},
	/* R */
	{"radic", "√"},
	{"rang", "〉"},
	{"raquo", "»"},
	{"rArr", "⇒"},
	{"rarr", "→"},
	{"rceil", "⌉"},
	{"rdquo", "”"},
	{"real", "ℜ"},
	{"reg", "®"},
	{"rfloor", "⌋"},
	{"Rho", "Ρ"},
	{"rho", "ρ"},
	{"rlm", "\xE2\x80\x8F"},
	{"rsaquo", "›"},
	{"rsquo", "’"},
	/* S */
	{"sbquo", "‚"},
	{"Scaron", "Š"},
	{"scaron", "š"},
	{"sdot", "⋅"},
	{"sect", "§"},
	{"shy", "\xC2\xAD"},
	{"Sigma", "Σ"},
	{"sigma", "σ"},
	{"sigmaf", "ς"},
	{"sim", "∼"},
	{"spades", "♠"},
	{"sub", "⊂"},
	{"sube", "⊆"},
	{"sum", "∑"},
	{"sup", "⊃"},
	{"sup1", "¹"},
	{"sup2", "²"},
	{"sup3", "³"},
	{"supe", "⊇"},
	{"szlig", "ß"},
	/* T */
	{"Tau", "Τ"},
	{"tau", "τ"},
	{"there4", "∴"},
	{"Theta", "Θ"},
	{"theta", "θ"},
	{"thetasym", "ϑ"},
	{"thinsp", "\xE2\x80\x89"},
	{"THORN", "Þ"},
	{"thorn", "þ"},
	{"tilde", "˜"},
	{"times", "×"},
	{"trade", "™"},
	/* U */
	{"Uacute", "Ú"},
	{"uacute", "ú"},
	{"uArr", "⇑"},
	{"uarr", "↑"},
	{"Ucirc", "Û"},
	{"ucirc", "û"},
	{"Ugrave", "Ù"},
	{"ugrave", "ù"},
	{"uml", "¨"},
	{"upsih", "ϒ"},
	{"Upsilon", "Υ"},
	{"upsilon", "υ"},
	{"Uuml", "Ü"},
	{"uuml", "ü"},
	/* W */
	{"weierp", "℘"},
	/* X */
	{"Xi", "Ξ"},
	{"xi", "ξ"},
	/* Y */
	{"Yacute", "Ý"},
	{"yacute", "ý"},
	{"yen", "¥"},
	{"Yuml", "Ÿ"},
	{"yuml", "ÿ"},
	/* Z */
	{"Zeta", "Ζ"},
	{"zeta", "ζ"},
	{"zwj", "\xE2\x80\x8D"},
	{"zwnj", "\xE2\x80\x8C"},
	{NULL, NULL}
};

static gchar* entity_extract_to_buffer(gchar *p, gchar b[])
{
	gint i = 0;

	while (*p != '\0' && *p != ';' && i < ENTITY_MAX_LEN) {
		b[i] = *p;
		++i, ++p;
	}
	if (*p != ';' || i == 0 || i == ENTITY_MAX_LEN)
		return NULL;
	b[i] = '\0';

	return b;
}

static gchar *entity_decode_numeric(gchar *str)
{
	gchar b[ENTITY_MAX_LEN];
	gchar *p = str, *res;
	gboolean hex = FALSE;
	gunichar c = 0;
	gint ret;

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

	if (strlen(b) > 0)
		c = g_ascii_strtoll (b, NULL, (hex ? 16 : 10));

	if (c < 32)
		/* An unprintable character; return the Unicode replacement symbol */
		return g_strdup("\xef\xbf\xbd");

	if (!g_unichar_validate(c)) {
		/* Make sure the character is valid Unicode */
		debug_print("Numeric reference '&#%s;' is invalid in Unicode codespace\n", b);
		return NULL;
	}

	res = g_malloc0 (DECODED_MAX_LEN + 1);
	ret = g_unichar_to_utf8 (c, res);
	if (ret == 0) {
		debug_print("Failed to convert unicode character %u to UTF-8\n", c);
		g_free(res);
		res = NULL;
	}

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
