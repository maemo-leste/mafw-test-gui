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

#ifndef __RENDERER_CONTROL_H__
#define __RENDERER_CONTROL_H__

#include <config.h>
#include <libmafw/mafw.h>

void prev (void);
void next (void);
void stop (void);
void play (void);
void force_play (void);
void pause (void);
void resume (void);

void set_current_renderer_index (guint index);
void setup_renderer_controls(GtkBuilder *builder);
void prepare_controls_for_state(MafwPlayState state);
void toggle_mute_button(gboolean new_state);
void update_mute_button(void);
void set_volume_vscale(guint volume);
void change_volume(gint multipler);
void move_position(gint multipler);
void stop(void);
void set_position_hscale_duration(gint duration);
void enable_seek_buttons(gboolean enable);

#endif /* __RENDERER_CONTROL_H__ */
