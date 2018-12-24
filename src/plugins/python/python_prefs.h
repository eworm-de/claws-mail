/* 
 * Claws Mail -- A GTK+ based, lightweight, and fast e-mail client
 * Copyright(C) 1999-2018 the Claws Mail Team
 * == Fancy Plugin ==
 * This file Copyright (C) 2009 -2014 Salvatore De Paolis
 * <iwkse@claws-mail.org> and the Claws Mail Team
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef PYTHON_PREFS_H
#define PYTHON_PREFS_H

typedef struct _PythonConfig PythonConfig;

struct _PythonConfig
{
	gint		console_win_width;
	gint		console_win_height;
};

extern PythonConfig python_config;

void python_prefs_init(void);
void python_prefs_done(void);

#endif
