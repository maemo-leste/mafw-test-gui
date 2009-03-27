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

#ifndef __PLAYLISTTREEVIEW_H__
#define __PLAYLISTTREEVIEW_H__

#include <config.h>
#include <gtk/gtk.h>

#include <libmafw/mafw.h>

void playlist_treeview_set_use_metadata_api(gboolean state);
void display_playlist_contents(MafwPlaylist *playlist);
gchar *treeview_get_stored_title(guint index);

void on_add_item_button_clicked(GtkWidget *widget);
void on_remove_item_button_clicked(GtkWidget *widget);
void on_raise_item_button_clicked(GtkWidget *widget);
void on_lower_item_button_clicked(GtkWidget *widget);
void on_clear_playlist_button_clicked(GtkWidget *widget);

void clear_current_playlist_treeview (void);

void update_playing_item (int index);
guint get_stored_visual_index(void);

void update_current_playlist_treeview(MafwPlaylist *playlist, guint from,
				      guint nremoved, guint nreplaced);

gint get_current_playlist_index (void);

void playlist_tw_move_item(guint from, guint to);

void playlist_menu_changed(GtkCheckMenuItem *checkmenuitem, gpointer user_data);
void playlist_get_focus(void);
gboolean playlist_has_focus(void);
gchar *playlist_get_selected_oid(void);
void update_playing_index_column(void);
void select_playing_sort(gboolean select_it);
void setup_playlist_treeview (GtkBuilder *builder);

void on_mafw_playlist_contents_changed(MafwPlaylist *playlist,
					       guint from, guint nremoved,
					       guint nreplaced);
void on_mafw_playlist_item_moved (MafwPlaylist *playlist, guint from,
					  guint to);


#endif /* __PLAYLISTTREEVIEW_H__ */
