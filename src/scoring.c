#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include "intl.h"
#include "utils.h"
#include "defs.h"
#include "procheader.h"
#include "matcher.h"
#include "scoring.h"

#define PREFSBUFSIZE		1024

GSList * prefs_scoring = NULL;

ScoringProp * scoringprop_parse(gchar ** str)
{
	gchar * tmp;
	gchar * save;
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

	if (key != SCORING_SCORE) {
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

#ifdef 0
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

		if (*buf != '#') {
			tmp = buf;
			scoring = scoringprop_parse(&tmp);
			if (tmp != NULL) {
				prefs_scoring = g_slist_append(prefs_scoring,
							       scoring);
			}
			else {
				// debug
				g_warning(_("syntax error : %s\n"), buf);
			}
		}
 	}

 	fclose(fp);
}
