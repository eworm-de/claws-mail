#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include "defs.h"
#include "intl.h"
#include "utils.h"
#include "procheader.h"
#include "matcher.h"
#include "scoring.h"
#include "prefs.h"

#define PREFSBUFSIZE		1024

GSList * prefs_scoring = NULL;

ScoringProp * scoringprop_parse(gchar ** str)
{
	gchar * tmp;
	gint key;
	ScoringProp * scoring;
	gint score;
	MatcherList * matchers;

	tmp = * str;

	matchers = matcherlist_parse(&tmp);
	if (tmp == NULL) {
		* str = NULL;
		return NULL;
	}

	key = matcher_parse_keyword(&tmp);

	if (tmp == NULL) {
		matcherlist_free(matchers);
		* str = NULL;
		return NULL;
	}

	if (key != MATCHING_SCORE) {
		matcherlist_free(matchers);
		* str = NULL;
		return NULL;
	}

	score = matcher_parse_number(&tmp);

	if (tmp == NULL) {
		matcherlist_free(matchers);
		* str = NULL;
		return NULL;
	}

	scoring = scoringprop_new(matchers, score);

	* str = tmp;
	return scoring;
}


ScoringProp * scoringprop_new(MatcherList * matchers, int score)
{
	ScoringProp * scoring;

	scoring = g_new0(ScoringProp, 1);
	scoring->matchers = matchers;
	scoring->score = score;

	return scoring;
}

void scoringprop_free(ScoringProp * prop)
{
	matcherlist_free(prop->matchers);
	g_free(prop);
}

gint scoringprop_score_message(ScoringProp * scoring, MsgInfo * info)
{
	if (matcherlist_match(scoring->matchers, info))
		return scoring->score;
	else
		return 0;
}

gint score_message(GSList * scoring_list, MsgInfo * info)
{
	gint score = 0;
	gint add_score;
	GSList * l;

	for(l = scoring_list ; l != NULL ; l = g_slist_next(l)) {
		ScoringProp * scoring = (ScoringProp *) l->data;
		
		add_score = (scoringprop_score_message(scoring, info));
		if (add_score == MAX_SCORE || add_score == MIN_SCORE) {
			score = add_score;
			break;
		}
		score += add_score;
	}
	return score;
}

#if 0
static void scoringprop_print(ScoringProp * prop)
{
	GSList * l;

	if (prop == NULL) {
		printf("no scoring\n");
		return;
	}

	printf("----- scoring ------\n");
	for(l = prop->matchers ; l != NULL ; l = g_slist_next(l)) {
		matcherprop_print((MatcherProp *) l->data);
	}
	printf("cond: %s\n", prop->bool_and ? "and" : "or");
	printf("score: %i\n", prop->score);
}
#endif

/*
  syntax for scoring config

  file ~/.sylpheed/scoringrc

  header "x-mailing" match "toto" score -10
  subject match "regexp" & to regexp "regexp" score 50
  subject match "regexp" | to regexpcase "regexp" | age_sup 5 score 30

  if score is = MIN_SCORE (-999), no more match is done in the list
  if score is = MAX_SCORE (-999), no more match is done in the list
 */

void prefs_scoring_read_config(void)
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];

	debug_print(_("Reading headers configuration...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, SCORING_RC, NULL);
	if ((fp = fopen(rcpath, "r")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		prefs_scoring = NULL;
		return;
	}
	g_free(rcpath);

 	/* remove all scoring */
 	while (prefs_scoring != NULL) {
 		ScoringProp * scoring = (ScoringProp *) prefs_scoring->data;
 		scoringprop_free(scoring);
 		prefs_scoring = g_slist_remove(prefs_scoring, scoring);
 	}

 	while (fgets(buf, sizeof(buf), fp) != NULL) {
 		ScoringProp * scoring;
		gchar * tmp;

 		g_strchomp(buf);

		if ((*buf != '#') && (*buf != '\0')) {
			tmp = buf;
			scoring = scoringprop_parse(&tmp);
			if (tmp != NULL) {
				prefs_scoring = g_slist_append(prefs_scoring,
							       scoring);
			}
			else {
				/* debug */
				g_warning(_("syntax error : %s\n"), buf);
			}
		}
 	}

 	fclose(fp);
}

gchar * scoringprop_to_string(ScoringProp * prop)
{
	gchar * list_str;
	gchar * score_str;
	gchar * scoring_str;

	list_str = matcherlist_to_string(prop->matchers);

	if (list_str == NULL)
		return NULL;

	score_str = itos(prop->score);
	scoring_str = g_strconcat(list_str, " score ", score_str, NULL);
	g_free(list_str);

	return scoring_str;
}

void prefs_scoring_write_config(void)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;
	ScoringProp * prop;

	debug_print(_("Writing scoring configuration...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, SCORING_RC, NULL);

	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_warning(_("failed to write configuration to file\n"));
		g_free(rcpath);
		return;
	}

	for (cur = prefs_scoring; cur != NULL; cur = cur->next) {
		gchar *scoring_str;

		prop = (ScoringProp *) cur->data;
		scoring_str = scoringprop_to_string(prop);
		if (fputs(scoring_str, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_write_close_revert(pfile);
			g_free(rcpath);
			g_free(scoring_str);
			return;
		}
		g_free(scoring_str);
	}

	g_free(rcpath);

	if (prefs_write_close(pfile) < 0) {
		g_warning(_("failed to write configuration to file\n"));
		return;
	}
}
