#ifndef __INTL_H__
#define __INTL_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef ENABLE_NLS
#  include <libintl.h>
#  ifndef TEXTDOMAIN
#    define _(String) gettext(String)
#  else
#    define _(String) dgettext(TEXTDOMAIN, String)
#  endif /* TEXTDOMAIN */
#  ifdef gettext_noop
#    define N_(String) gettext_noop(String)
#  else
#    define N_(String) (String)
#  endif /* gettext_noop */
#  ifndef HAVE_BIND_TEXTDOMAIN_CODESET
#    define bind_textdomain_codeset(Domain, Codeset) (Codeset)
#  endif /* HAVE_BIND_TEXTDOMAIN_CODESET */
#else
#  define _(String) (String)
#  define N_(String) (String)
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,String) (String)
#  define dcgettext(Domain,String,Type) (String)
#  define bindtextdomain(Domain,Directory) (Domain) 
#  define bind_textdomain_codeset(Domain, Codeset) (Codeset)
#endif /* ENABLE_NLS */

#endif /* __INTL_H__ */
