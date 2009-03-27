/*
 * This file is a part of MAFW
 *
 * Copyright (C) 2007, 2008, 2009 Nokia Corporation, all rights reserved.
 *
 * Contact: Visa Smolander <visa.smolander@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __SOURCETREEVIEW_H__
#define __SOURCETREEVIEW_H__

#include <config.h>
#include <libmafw/mafw.h>

#include <gtk/gtk.h>

typedef enum _SourceModelBehaviour {
	SourceModelDetached,
	SourceModelCached,
	SourceModelNormal
} SourceModelBehaviour;

gboolean selected_is_container (void);
void source_treeview_select_next(void);
gboolean source_tv_get_focus(gpointer data);
void setup_source_treeview(GtkBuilder *builder);
void add_source(MafwSource *source);
void remove_source(MafwSource *source);
char *get_selected_object_id(void);
void source_treeview_set_cancel_browse(gboolean state);
MafwSource *get_selected_source(void);

void source_treeview_set_model_behaviour(SourceModelBehaviour behaviour);
SourceModelBehaviour source_treeview_get_model_behaviour(void);

#endif /* __SOURCETREEVIEW_H__ */
