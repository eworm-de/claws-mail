/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto & The Sylpheed-Claws Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_ASPELL

#include <windows.h>
#include "w32_aspell_init.h"
#include "prefs_common.h"
#include "utils.h"

HMODULE hMod_aspell_common;
HMODULE hMod_aspell;

void w32_aspell_assign(void);
int w32_aspell_null(void);

#define W32_ASPELL_ASSIGN(nam) nam = \
	(w32_aspell_loaded()) \
	? (t##nam)GetProcAddress(hMod_aspell,#nam) \
	: (t##nam)w32_aspell_null \
	;

int w32_aspell_init(void) {
  	gchar *aspell_dll;
	gchar *path, *ver;

	g_return_val_if_fail(prefs_common.enable_aspell, 0);

	ver  = read_w32_registry_string("HKLM", "SOFTWARE\\Aspell", "AspellVersion");
	path = read_w32_registry_string("HKLM", "SOFTWARE\\Aspell", "Path");
	g_return_val_if_fail(ver || path, 0);

	aspell_dll = g_strdup_printf("%s%c%s%d%s",
	    path, G_DIR_SEPARATOR, "aspell-", ver[0], ".dll");
	hMod_aspell = LoadLibrary(aspell_dll);
	g_free(ver);
	g_free(path);
	g_free(aspell_dll);

	w32_aspell_assign();
	if (!w32_aspell_loaded()) {
		FreeLibrary(hMod_aspell);
		return 0 ;
	} else if (!prefs_common.aspell_path) {
		/* get dict-dir from registry */
		gchar * tmppath;
		tmppath = g_strdup_printf("%s%c%s%c",
			read_w32_registry_string("HKLM", "Software\\Aspell", NULL),
			G_DIR_SEPARATOR, "dict", G_DIR_SEPARATOR);
		subst_char(tmppath, G_DIR_SEPARATOR, '/');
		prefs_common.aspell_path=tmppath;
	}
	return 1 ;
}

int w32_aspell_loaded(void) {
	if (hMod_aspell)
		return 1;
	else
		return 0;
}

int w32_aspell_null(void) {
	return 0;
}

void w32_aspell_assign(void) {
W32_ASPELL_ASSIGN(  aspell_mutable_container_add                                          )
W32_ASPELL_ASSIGN(  aspell_mutable_container_remove                                       )
W32_ASPELL_ASSIGN(  aspell_mutable_container_clear                                        )
//W32_ASPELL_ASSIGN(  spell_mutable_container_to_mutable_container                          )
W32_ASPELL_ASSIGN(  aspell_key_info_enumeration_next                                      )
W32_ASPELL_ASSIGN(  delete_aspell_key_info_enumeration                                    )
W32_ASPELL_ASSIGN(  aspell_key_info_enumeration_clone                                     )
W32_ASPELL_ASSIGN(  aspell_key_info_enumeration_assign                                    )
W32_ASPELL_ASSIGN(  new_aspell_config                                                     )
W32_ASPELL_ASSIGN(  delete_aspell_config                                                  )
W32_ASPELL_ASSIGN(  aspell_config_clone                                                   )
W32_ASPELL_ASSIGN(  aspell_config_assign                                                  )
W32_ASPELL_ASSIGN(  aspell_config_error_number                                            )
W32_ASPELL_ASSIGN(  aspell_config_error_message                                           )
W32_ASPELL_ASSIGN(  aspell_config_error                                                   )
W32_ASPELL_ASSIGN(  aspell_config_set_extra                                               )
W32_ASPELL_ASSIGN(  aspell_config_keyinfo                                                 )
W32_ASPELL_ASSIGN(  aspell_config_possible_elements                                       )
W32_ASPELL_ASSIGN(  aspell_config_get_default                                             )
W32_ASPELL_ASSIGN(  aspell_config_elements                                                )
W32_ASPELL_ASSIGN(  aspell_config_replace                                                 )
W32_ASPELL_ASSIGN(  aspell_config_remove                                                  )
W32_ASPELL_ASSIGN(  aspell_config_have                                                    )
W32_ASPELL_ASSIGN(  aspell_config_retrieve                                                )
W32_ASPELL_ASSIGN(  aspell_config_retrieve_list                                           )
W32_ASPELL_ASSIGN(  aspell_config_retrieve_bool                                           )
W32_ASPELL_ASSIGN(  aspell_config_retrieve_int                                            )
W32_ASPELL_ASSIGN(  aspell_error_is_a                                                     )
W32_ASPELL_ASSIGN(  aspell_error_number                                                   )
W32_ASPELL_ASSIGN(  aspell_error_message                                                  )
W32_ASPELL_ASSIGN(  aspell_error                                                          )
W32_ASPELL_ASSIGN(  delete_aspell_can_have_error                                          )
W32_ASPELL_ASSIGN(  new_aspell_speller                                                    )
W32_ASPELL_ASSIGN(  to_aspell_speller                                                     )
W32_ASPELL_ASSIGN(  delete_aspell_speller                                                 )
W32_ASPELL_ASSIGN(  aspell_speller_error_number                                           )
W32_ASPELL_ASSIGN(  aspell_speller_error_message                                          )
W32_ASPELL_ASSIGN(  aspell_speller_error                                                  )
W32_ASPELL_ASSIGN(  aspell_speller_config                                                 )
W32_ASPELL_ASSIGN(  aspell_speller_check                                                  )
W32_ASPELL_ASSIGN(  aspell_speller_add_to_personal                                        )
W32_ASPELL_ASSIGN(  aspell_speller_add_to_session                                         )
W32_ASPELL_ASSIGN(  aspell_speller_personal_word_list                                     )
W32_ASPELL_ASSIGN(  aspell_speller_session_word_list                                      )
W32_ASPELL_ASSIGN(  aspell_speller_main_word_list                                         )
W32_ASPELL_ASSIGN(  aspell_speller_save_all_word_lists                                    )
W32_ASPELL_ASSIGN(  aspell_speller_clear_session                                          )
W32_ASPELL_ASSIGN(  aspell_speller_suggest                                                )
W32_ASPELL_ASSIGN(  aspell_speller_store_replacement                                      )
W32_ASPELL_ASSIGN(  delete_aspell_filter                                                  )
W32_ASPELL_ASSIGN(  aspell_filter_error_number                                            )
W32_ASPELL_ASSIGN(  aspell_filter_error_message                                           )
W32_ASPELL_ASSIGN(  aspell_filter_error                                                   )
W32_ASPELL_ASSIGN(  to_aspell_filter                                                      )
W32_ASPELL_ASSIGN(  delete_aspell_document_checker                                        )
W32_ASPELL_ASSIGN(  aspell_document_checker_error_number                                  )
W32_ASPELL_ASSIGN(  aspell_document_checker_error_message                                 )
W32_ASPELL_ASSIGN(  aspell_document_checker_error                                         )
W32_ASPELL_ASSIGN(  new_aspell_document_checker                                           )
W32_ASPELL_ASSIGN(  to_aspell_document_checker                                            )
W32_ASPELL_ASSIGN(  aspell_document_checker_reset                                         )
W32_ASPELL_ASSIGN(  aspell_document_checker_process                                       )
W32_ASPELL_ASSIGN(  aspell_document_checker_next_misspelling                              )
W32_ASPELL_ASSIGN(  aspell_document_checker_filter                                        )
W32_ASPELL_ASSIGN(  aspell_word_list_empty                                                )
W32_ASPELL_ASSIGN(  aspell_word_list_size                                                 )
W32_ASPELL_ASSIGN(  aspell_word_list_elements                                             )
W32_ASPELL_ASSIGN(  delete_aspell_string_enumeration                                      )
W32_ASPELL_ASSIGN(  aspell_string_enumeration_clone                                       )
W32_ASPELL_ASSIGN(  aspell_string_enumeration_assign                                      )
W32_ASPELL_ASSIGN(  aspell_string_enumeration_at_end                                      )
W32_ASPELL_ASSIGN(  aspell_string_enumeration_next                                        )
W32_ASPELL_ASSIGN(  get_aspell_module_info_list                                           )
W32_ASPELL_ASSIGN(  aspell_module_info_list_empty                                         )
W32_ASPELL_ASSIGN(  aspell_module_info_list_size                                          )
W32_ASPELL_ASSIGN(  aspell_module_info_list_elements                                      )
W32_ASPELL_ASSIGN(  get_aspell_dict_info_list                                             )
W32_ASPELL_ASSIGN(  aspell_dict_info_list_empty                                           )
W32_ASPELL_ASSIGN(  aspell_dict_info_list_size                                            )
W32_ASPELL_ASSIGN(  aspell_dict_info_list_elements                                        )
W32_ASPELL_ASSIGN(  aspell_module_info_enumeration_at_end                                 )
W32_ASPELL_ASSIGN(  aspell_module_info_enumeration_next                                   )
W32_ASPELL_ASSIGN(  delete_aspell_module_info_enumeration                                 )
W32_ASPELL_ASSIGN(  aspell_module_info_enumeration_clone                                  )
W32_ASPELL_ASSIGN(  aspell_module_info_enumeration_assign                                 )
W32_ASPELL_ASSIGN(  aspell_dict_info_enumeration_at_end                                   )
W32_ASPELL_ASSIGN(  aspell_dict_info_enumeration_next                                     )
W32_ASPELL_ASSIGN(  delete_aspell_dict_info_enumeration                                   )
W32_ASPELL_ASSIGN(  aspell_dict_info_enumeration_clone                                    )
W32_ASPELL_ASSIGN(  aspell_dict_info_enumeration_assign                                   )
W32_ASPELL_ASSIGN(  new_aspell_string_list                                                )
W32_ASPELL_ASSIGN(  aspell_string_list_empty                                              )
W32_ASPELL_ASSIGN(  aspell_string_list_size                                               )
W32_ASPELL_ASSIGN(  aspell_string_list_elements                                           )
W32_ASPELL_ASSIGN(  aspell_string_list_add                                                )
W32_ASPELL_ASSIGN(  aspell_string_list_remove                                             )
W32_ASPELL_ASSIGN(  aspell_string_list_clear                                              )
W32_ASPELL_ASSIGN(  aspell_string_list_to_mutable_container                               )
W32_ASPELL_ASSIGN(  delete_aspell_string_list                                             )
W32_ASPELL_ASSIGN(  aspell_string_list_clone                                              )
W32_ASPELL_ASSIGN(  aspell_string_list_assign                                             )
W32_ASPELL_ASSIGN(  new_aspell_string_map                                                 )
W32_ASPELL_ASSIGN(  aspell_string_map_add                                                 )
W32_ASPELL_ASSIGN(  aspell_string_map_remove                                              )
W32_ASPELL_ASSIGN(  aspell_string_map_clear                                               )
W32_ASPELL_ASSIGN(  aspell_string_map_to_mutable_container                                )
W32_ASPELL_ASSIGN(  delete_aspell_string_map                                              )
W32_ASPELL_ASSIGN(  aspell_string_map_clone                                               )
W32_ASPELL_ASSIGN(  aspell_string_map_assign                                              )
W32_ASPELL_ASSIGN(  aspell_string_map_empty                                               )
W32_ASPELL_ASSIGN(  aspell_string_map_size                                                )
W32_ASPELL_ASSIGN(  aspell_string_map_elements                                            )
W32_ASPELL_ASSIGN(  aspell_string_map_insert                                              )
W32_ASPELL_ASSIGN(  aspell_string_map_replace                                             )
W32_ASPELL_ASSIGN(  aspell_string_map_lookup                                              )
W32_ASPELL_ASSIGN(  aspell_string_pair_enumeration_at_end                                 )
W32_ASPELL_ASSIGN(  aspell_string_pair_enumeration_next                                   )
W32_ASPELL_ASSIGN(  delete_aspell_string_pair_enumeration                                 )
W32_ASPELL_ASSIGN(  aspell_string_pair_enumeration_clone                                  )
W32_ASPELL_ASSIGN(  aspell_string_pair_enumeration_assign                                 )
}

#endif /* USE_ASPELL */
