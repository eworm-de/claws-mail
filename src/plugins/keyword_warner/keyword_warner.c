/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2006-2022  Ricardo Mones and the Claws Mail Team
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "version.h"
#include "keyword_warner.h"
#include "keyword_warner_prefs.h"
#include "codeconv.h"
#include "prefs_common.h"

/** Identifier for the hook. */
static gulong hook_id = HOOK_NONE;

static KeywordWarnerMention *kw_matcherlist_string_match(MatcherList *matchers, gchar *str, gchar *sig_separator)
{
	MsgInfo info;
	int i = 0;
	gboolean ret = FALSE;
	gchar **lines = NULL;
	KeywordWarnerMention *kwm = NULL;

	if (str == NULL || *str == '\0') {
		return kwm;
	}
	
	lines = g_strsplit(str, "\n", -1);
	if (kwarnerprefs.skip_quotes
		&& *prefs_common_get_prefs()->quote_chars != '\0') {
		debug_print("checking without quotes\n");
		for (i = 0; lines[i] != NULL && ret == FALSE; i++) {
			if(kwarnerprefs.skip_signature
				&& sig_separator != NULL
				&& *sig_separator != '\0'
				&& strcmp(lines[i], sig_separator) == 0) {
				debug_print("reached signature delimiter at line %d\n", i);
				break;
			}
			if (line_has_quote_char(lines[i], 
				prefs_common_get_prefs()->quote_chars) == NULL) {
				debug_print("testing line %d\n", i);
				info.subject = lines[i];
				ret = matcherlist_match(matchers, &info);
				debug_print("line %d: %d\n", i, ret);
			}
		}
	} else {
		debug_print("checking with quotes\n");
		for (i = 0; lines[i] != NULL && ret == FALSE; i++) {
			if(kwarnerprefs.skip_signature
				&& sig_separator != NULL
				&& *sig_separator != '\0'
				&& strcmp(lines[i], sig_separator) == 0) {
				debug_print("reached signature delimiter at line %d\n", i);
				break;
			}
			debug_print("testing line %d\n", i);
			info.subject = lines[i];
			ret = matcherlist_match(matchers, &info);
			debug_print("line %d: %d\n", i, ret);
		}
	}
	if (ret != FALSE) {
		kwm = g_new0(KeywordWarnerMention, 1);
		kwm->line = i; /* usual humans count lines from 1 */
		kwm->context = g_strdup(lines[i - 1]);
		debug_print("found at line %d, context \"%s\"\n", kwm->line, kwm->context);
	}
	g_strfreev(lines);

	return kwm;
}

/**
 * Looks for keyword references in the composer text.
 *
 * @param compose The composer object to inspect.
 *
 * @return A pointer to an KeywordWarnerMention if keyword references
 * are found, or NULL otherwise.
 */
KeywordWarnerMention *is_keyword_mentioned(Compose *compose)
{
	GtkTextView *textview = NULL;
	GtkTextBuffer *textbuffer = NULL;
	GtkTextIter start, end;
	gchar *text = NULL;
	KeywordWarnerMention *mention = NULL;
	MatcherList *matchers = NULL;

	if (kwarnerprefs.match_strings != NULL
			&& kwarnerprefs.match_strings[0] != '\0') {
		matchers = matcherlist_new_from_lines(kwarnerprefs.match_strings, FALSE, kwarnerprefs.case_sensitive);

		if (matchers) {
			textview = GTK_TEXT_VIEW(compose->text);
	        textbuffer = gtk_text_view_get_buffer(textview);
			gtk_text_buffer_get_start_iter(textbuffer, &start);
			gtk_text_buffer_get_end_iter(textbuffer, &end);
			text = gtk_text_buffer_get_text(textbuffer, &start, &end, FALSE);

			debug_print("checking text for keyword mentions\n");
			if (text != NULL) {
				mention = kw_matcherlist_string_match(matchers, text, compose->account->sig_sep);
				g_free(text);
			}	
			matcherlist_free(matchers);
			debug_print("done\n");
		} else
			g_warning("couldn't allocate matcher");
	}            
	return mention;
}

/**
 * Check whether not check while redirecting or forwarding.
 *
 * @param mode The current compose->mode.
 *
 * @return TRUE for cancel further checking because it's being redirected or
 *         forwarded and user configured not to check, FALSE otherwise.
 */
gboolean do_not_check_redirect_forward(int mode)
{
	switch (mode) {
	case COMPOSE_FORWARD:
	case COMPOSE_FORWARD_AS_ATTACH:
	case COMPOSE_FORWARD_INLINE:
	case COMPOSE_REDIRECT:
		if (kwarnerprefs.skip_forwards_and_redirections)
			return TRUE;
	default:
		return FALSE;
	}
}

/**
 * Callback function to be called before sending the mail.
 * 
 * @param source The composer to be checked.
 * @param data Additional data.
 *
 * @return TRUE if no keywords are mentioned,
 *         FALSE if keywords are mentioned.
 */
static gboolean kwarn_before_send_hook(gpointer source, gpointer data)
{
	Compose *compose = (Compose *)source;
	KeywordWarnerMention *mention = NULL;
	gboolean ret = FALSE; /* continue sending if FALSE */

	debug_print("KeywordWarner invoked\n");
	if (compose->batch)
		return FALSE;	/* do not check while queuing */

	if (do_not_check_redirect_forward(compose->mode))
		return FALSE;

	mention = is_keyword_mentioned(compose);
	if (mention != NULL) {
		AlertValue aval;
		gchar *message;
		gchar *bold_text;
		
		bold_text = g_strdup_printf("<span weight=\"bold\">%.20s</span>...",
				mention->context);
		message = g_strdup_printf(
				_("A keyword is used in the mail you are sending. "
				"The keyword appears on line %d, "
				"which begins with the text: %s\n\n%s"),
				mention->line,
				bold_text,
				compose->sending?_("Send it anyway?"):_("Queue it anyway?"));
		aval = alertpanel(_("Keyword warning"), message,
				  NULL, _("_Cancel"),
				  NULL, compose->sending ? _("_Send") : _("Queue"),
				  NULL, NULL, ALERTFOCUS_SECOND);
		g_free(message);
		g_free(bold_text);
		if (aval != G_ALERTALTERNATE)
			ret = TRUE;	
	}
	if (mention != NULL) {
		if (mention->context != NULL)
			g_free(mention->context);
		g_free(mention);
	}

	return ret;
}

/**
 * Initialize plugin.
 *
 * @param error  For storing the returned error message.
 *
 * @return 0 if initialization succeeds, -1 on failure.
 */
gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
			VERSION_NUMERIC, "Keyword Warner", error))
		return -1;

	hook_id = hooks_register_hook(COMPOSE_CHECK_BEFORE_SEND_HOOKLIST,
			kwarn_before_send_hook, NULL);

	if (hook_id == HOOK_NONE) {
		*error = g_strdup(_("Failed to register check before send hook"));
		return -1;
	}

	keyword_warner_prefs_init();

	debug_print("Keyword Warner plugin loaded\n");

	return 0;
}

/**
 * Destructor for the plugin.
 * Unregister the callback function and frees matcher.
 */
gboolean plugin_done(void)
{	
	hooks_unregister_hook(COMPOSE_CHECK_BEFORE_SEND_HOOKLIST, hook_id);
	keyword_warner_prefs_done();
	debug_print("Keyword Warner plugin unloaded\n");
	return TRUE;
}

/**
 * Get the name of the plugin.
 *
 * @return The plugin name (maybe translated).
 */
const gchar *plugin_name(void)
{
	return _("Keyword Warner");
}

/**
 * Get the description of the plugin.
 *
 * @return The plugin description (maybe translated).
 */
const gchar *plugin_desc(void)
{
	return _("Shows a warning when sending or queueing a message "
		 "and a reference to one or more keywords is found in the "
	         "message text.");
}

/**
 * Get the kind of plugin.
 *
 * @return The "GTK3" constant.
 */
const gchar *plugin_type(void)
{
	return "GTK3";
}

/**
 * Get the license acronym the plugin is released under.
 *
 * @return The "GPL" constant.
 */
const gchar *plugin_licence(void)
{
	return "GPL3+";
}

/**
 * Get the version of the plugin.
 *
 * @return The current version string.
 */
const gchar *plugin_version(void)
{
	return VERSION;
}

/**
 * Get the features implemented by the plugin.
 *
 * @return A constant PluginFeature structure with the features.
 */
struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_OTHER, N_("Keyword Warner")},
		  {PLUGIN_NOTHING, NULL}};

	return features;
}

