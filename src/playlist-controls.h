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

#ifndef __PLAYLIST_CONTROLS_H__
#define __PLAYLIST_CONTROLS_H__

#include <config.h>
#include <gtk/gtk.h>

#include <libmafw/mafw.h>
#include <libmafw-shared/mafw-proxy-playlist.h>

void setup_playlist_controls (GtkBuilder *builder);

void toggle_shuffle (gboolean new_state);
void toggle_repeat (gboolean new_state);
void enable_playlist_buttons(gboolean enable);
void on_rename_playlist_button_clicked(GtkWidget *widget);
void on_save_playlist_button_clicked(GtkWidget *widget);

MafwProxyPlaylist* get_current_playlist (void);
gboolean select_playlist(MafwProxyPlaylist *playlist);

guint get_current_playlist_id (void);

gboolean find_playlist_iter(MafwProxyPlaylist *playlist, GtkTreeIter *iter);

void playlist_import(const gchar *oid);

#endif
