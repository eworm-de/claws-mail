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
#include "folder.h"

#define PREFSBUFSIZE		1024


GSList * global_scoring = NULL;

ScoringProp * scoringprop_new(MatcherList * matchers, int score)
{
	ScoringProp * scoring;

	scoring = g_new0(ScoringProp, 1);
	scoring->matchers = matchers;
	scoring->score = score;

	return scoring;
}

ScoringProp * scoringprop_copy(ScoringProp *src)
{
	ScoringProp * new;
	GSList *tmp;
	
	new = g_new0(ScoringProp, 1);
	new->matchers = g_new0(MatcherList, 1);
	for (tmp = src->matchers->matchers; tmp != NULL && tmp->data != NULL;) {
		MatcherProp *matcher = (MatcherProp *)tmp->data;
		
		new->matchers->matchers = g_slist_append(new->matchers->matchers,
						   matcherprop_copy(matcher));
		tmp = tmp->next;
	}
	new->matchers->bool_and = src->matchers->bool_and;
	new->score = src->score;

	return new;
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

/*
  syntax for scoring config

  file ~/.sylpheed/scoringrc

  header "x-mailing" match "toto" score -10
  subject match "regexp" & to regexp "regexp" score 50
  subject match "regexp" | to regexpcase "regexp" | age_sup 5 score 30

  if score is = MIN_SCORE (-999), no more match is done in the list
  if score is = MAX_SCORE (-999), no more match is done in the list
 */


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

void prefs_scoring_free(GSList * prefs_scoring)
{
 	while (prefs_scoring != NULL) {
 		ScoringProp * scoring = (ScoringProp *) prefs_scoring->data;
 		scoringprop_free(scoring);
 		prefs_scoring = g_slist_remove(prefs_scoring, scoring);
 	}
}

static gboolean prefs_scoring_free_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;

	if(!item->prefs)
		return FALSE;

	prefs_scoring_free(item->prefs->scoring);
	item->prefs->scoring = NULL;

	return FALSE;
}

void prefs_scoring_clear()
{
	GList * cur;

	for (cur = folder_get_list() ; cur != NULL ; cur = g_list_next(cur)) {
		Folder *folder;

		folder = (Folder *) cur->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				prefs_scoring_free_func, NULL);
	}

	prefs_scoring_free(global_scoring);
	global_scoring = NULL;
}
