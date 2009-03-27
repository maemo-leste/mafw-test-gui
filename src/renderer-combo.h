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

#ifndef __RENDERERCOMBO_H__
#define __RENDERERCOMBO_H__

#include <config.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <libmafw/mafw.h>

void
assign_playlist_to_current_renderer(MafwPlaylist *playlist);

guint
get_selected_renderer_volume    (void);

MafwPlayState
get_selected_renderer_state     (void);

MafwRenderer *
get_selected_renderer           (void);

void
set_selected_renderer_volume    (gfloat volume);

void
clear_selected_renderer_state   (void);

void
add_media_renderer              (MafwRenderer         *renderer);

void
remove_media_renderer           (MafwRenderer         *renderer);

void
set_selected_renderer_xid           (XID               xid);

void
setup_renderer_combo            (GtkBuilder           *builder);

#endif /* __RENDERERCOMBO_H__ */
