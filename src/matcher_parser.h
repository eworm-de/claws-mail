#ifndef MATCHER_PARSER_H

#define MATCHER_PARSER_H

#include "filtering.h"
#include "scoring.h"
#include <glib.h>

// #define MATCHER_BUF_SIZE 16384

extern FILE *matcher_parserin;
extern int matcher_parserlineno;


void matcher_parser_start_parsing(FILE * f);
int matcher_parserparse(void);
/* void matcher_parser_init();*/

MatcherList * matcher_parser_get_cond(gchar * str);
MatcherProp * matcher_parser_get_prop(gchar * str);
FilteringProp * matcher_parser_get_filtering(gchar * str);
ScoringProp * matcher_parser_get_scoring(gchar * str);
/* void matcher_parser_read_config(); */

#endif
