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

#include <string.h>
#include <stdlib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <hildon/hildon.h>

#include <libmafw/mafw.h>

#include "source-treeview.h"
#include "metadata-view.h"
#include "playlist-treeview.h"
#include "renderer-controls.h"
#include "playlist-controls.h"
#include "renderer-combo.h"
#include "fullscreen.h"

#include "config.h"

#define SEC_PER_MIN 60
#define SEC_PER_HOUR 3600

typedef enum {
	PLAY_TYPE_SINGLE = 1,
	PLAY_TYPE_PLAYLIST
} PlayType;


static GtkWidget *volume_vscale;
static GtkWidget *position_hscale;
static GtkWidget *position_label;
static GtkWidget *prev_button;
static GtkWidget *seek_backwards_button;
static GtkWidget *play_button;
static GtkWidget *stop_button;
static GtkWidget *seek_forwards_button;
static GtkWidget *next_button;
static GtkWidget *mute_toggle;

static guint    timeout_id = 0;

static gboolean in_state_stopped;

/* Prototypes for gtk-builder signal connections */
void on_volume_vscale_value_changed(GtkWidget *widget);
void on_mute_button_toggled(GtkToggleButton *togglebutton, gpointer user_data);
void on_previous_button_clicked(GtkButton *button, gpointer user_data);
void on_seek_backwards_button_clicked (GtkButton *button, gpointer user_data);
void on_play_button_clicked(GtkButton *button, gpointer user_data);
void on_stop_button_clicked(GtkButton *button, gpointer user_data);
void on_seek_forwards_button_clicked (GtkButton *button, gpointer user_data);
void on_next_button_clicked(GtkButton *button, gpointer user_data);
void on_clear_state_button_clicked(GtkButton *button, gpointer user_data);
gboolean on_position_hscale_value_changed(GtkRange *range, gpointer user_data);

static void get_position_info_cb (MafwRenderer *renderer, gint position,
				  gpointer user_data, const GError *error);

static gboolean
update_position (gpointer data)
{
	MafwRenderer *renderer = NULL;
	MafwPlayState state;

	renderer = get_selected_renderer ();
	if (!renderer)
	{
		timeout_id = 0;
		return FALSE;
	}

	state = get_selected_renderer_state ();
	if (state == Stopped) {
		timeout_id = 0;
		return FALSE;
	}

	mafw_renderer_get_position (renderer, get_position_info_cb,  NULL);
        return TRUE;
}

static void
add_timeout (void)
{
        if (timeout_id == 0) {
                timeout_id = g_timeout_add (250,
                                            update_position,
                                            NULL);
        }
}

static void
remove_timeout (void)
{
        if (timeout_id != 0) {
                g_source_remove (timeout_id);
                timeout_id = 0;
        }
}



/* FIXME let the user know of the error too */
static void
play_error_cb(MafwRenderer *renderer, gpointer user_data, const GError *error)
{
	if (error != NULL)
		hildon_banner_show_information (GTK_WIDGET (user_data),
						"chat_smiley_angry",
						error->message);
}

static void
set_position_cb (MafwRenderer   *renderer,
		 gint       position,
		 gpointer    user_data,
		 const GError     *error)
{
	if (error != NULL)
		hildon_banner_show_information (GTK_WIDGET (user_data),
						"chat_smiley_angry",
						error->message);
}

void
set_current_renderer_index (guint index)
{
	MafwRenderer *renderer;
	renderer = get_selected_renderer();
	if (renderer == NULL)
		return;

	mafw_renderer_goto_index(renderer, index, play_error_cb, NULL);
}

void
resume (void)
{
	MafwRenderer *renderer;

	renderer = get_selected_renderer();
	if (renderer == NULL)
		return;

	mafw_renderer_resume(renderer, play_error_cb, NULL);
}

void
pause (void)
{
	MafwRenderer *renderer;
        MafwPlayState state;

	renderer = get_selected_renderer ();
	if (renderer == NULL)
		return;

        state = get_selected_renderer_state ();
        if (state == Playing) {
		mafw_renderer_pause(renderer, play_error_cb, NULL);

        } else {
		g_warning ("Tried to Pause when renderer state "
			   "is not Playing. Skipped.");
	}
}

void
play (void)
{
	MafwPlayState state;

	state = get_selected_renderer_state ();
	if (state == Stopped) {
		MafwRenderer *renderer;

		renderer = get_selected_renderer();
		if (renderer == NULL)
			return;
		if (!is_fullscreen_open())
			set_selected_renderer_xid (get_metadata_visual_xid ());
		else
			set_selected_renderer_xid (get_fullscreen_xid ());
		mafw_renderer_play(renderer, play_error_cb, NULL);
	} else if (state == Paused) {
		resume ();
	} else if (state == Playing) {
		pause ();
	}
}

void
force_play (void)
{
	MafwRenderer *renderer;
	renderer = get_selected_renderer();
	if (renderer == NULL)
		return;
	mafw_renderer_play(renderer, play_error_cb, NULL);
}

void
stop (void)
{
	MafwRenderer *renderer;
        MafwPlayState state;

	renderer = get_selected_renderer();
	if (renderer == NULL)
		return;

        state = get_selected_renderer_state ();
        if (state != Stopped) {
		remove_timeout();
		mafw_renderer_stop(renderer, play_error_cb, NULL);
	} else {
		g_warning ("Tried to Stop when renderer state "
			   "is already Stopped. Skipped.");
	}

}

void
next (void)
{
	MafwRenderer *renderer;

	renderer = get_selected_renderer();
	if (renderer == NULL)
		return;

	mafw_renderer_next (renderer, play_error_cb, NULL);
}

void
prev (void)
{
	MafwRenderer *renderer;

	renderer = get_selected_renderer();
	if (renderer == NULL)
		return;

	mafw_renderer_previous (renderer, play_error_cb, NULL);
}

/**
 * Called, when the user toggles the mute button. Sets the selected
 * renderer's state according to that
 */
void on_mute_button_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	MafwRenderer *renderer = get_selected_renderer();

	if (renderer != NULL) {
		gboolean mute = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(togglebutton));

		mafw_extension_set_property_boolean(MAFW_EXTENSION(renderer),
                                                    MAFW_PROPERTY_RENDERER_MUTE,
                                                    mute);
	}
}

static void mute_cb(MafwExtension *self, const gchar *name, GValue *value,
		    gpointer user_data, const GError *error)
{
	if (!error && MAFW_RENDERER(self) == get_selected_renderer() &&
		!strcmp(name, MAFW_PROPERTY_RENDERER_MUTE))
	{
		toggle_mute_button(g_value_get_boolean(value));
	}

	if (error != NULL)
		hildon_banner_show_information (GTK_WIDGET (user_data),
						"chat_smiley_angry",
						error->message);
}

/**
 * Sets the mute toggle button's state, according to the current renderer state
 */
void
update_mute_button (void)
{
	MafwRenderer *renderer = get_selected_renderer();

	mafw_extension_get_property(MAFW_EXTENSION(renderer),
                                    MAFW_PROPERTY_RENDERER_MUTE,
                                    mute_cb, NULL);
}

/**
 * Sets the mute toggle button's state, according to the selected renderer's
 * current mute state
 */
void
toggle_mute_button (gboolean new_state)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_toggle), new_state);
}

void
set_volume_vscale (guint volume)
{
        g_signal_handlers_block_by_func (volume_vscale,
                                         on_volume_vscale_value_changed,
                                         NULL);

        gtk_range_set_value (GTK_RANGE (volume_vscale), volume);

        g_signal_handlers_unblock_by_func (volume_vscale,
                                           on_volume_vscale_value_changed,
                                           NULL);
}

void
on_previous_button_clicked (GtkButton *button,
                            gpointer   user_data)
{
	prev ();
}

void
on_seek_backwards_button_clicked (GtkButton *button,
				  gpointer   user_data)
{
	MafwRenderer *renderer;

	renderer = get_selected_renderer ();
	if (renderer == NULL)
		return;

	mafw_renderer_set_position (renderer, SeekRelative, -20,
                                    set_position_cb, NULL);
}

void
on_play_button_clicked (GtkButton *button,
                        gpointer   user_data)
{
	play ();
}

void
on_stop_button_clicked (GtkButton *button,
                        gpointer   user_data)
{
	stop ();
}

void
on_seek_forwards_button_clicked (GtkButton *button,
				 gpointer   user_data)
{
	MafwRenderer *renderer;

	renderer = get_selected_renderer ();
	if (renderer == NULL)
		return;

	mafw_renderer_set_position (renderer, SeekRelative, 20,
                                    set_position_cb, NULL);
}

void
on_next_button_clicked (GtkButton *button,
                        gpointer   user_data)
{
	next ();
}

void
on_clear_state_button_clicked (GtkButton *button,
                               gpointer   user_data)
{
        clear_selected_renderer_state ();
}

gboolean
on_position_hscale_value_changed (GtkRange *range,
                                  gpointer  user_data)
{
        gint seconds;
	MafwRenderer *renderer = NULL;

	renderer = get_selected_renderer ();
	if (!renderer) {
		return TRUE;
	}

        seconds = (gint) gtk_range_get_value (range);
	mafw_renderer_set_position (renderer, SeekAbsolute, seconds,
                                    set_position_cb, NULL);

        return TRUE;
}


void set_position_hscale_sensitive(gboolean sensitive)
{
	if (sensitive && in_state_stopped)
	{
		return;
	}
	gtk_widget_set_sensitive(position_hscale, sensitive);
}

void
set_position_hscale_duration (gint duration)
{
        if (duration > 0) {
		g_signal_handlers_block_by_func
			(position_hscale,
			 on_position_hscale_value_changed,
			 NULL);
                gtk_range_set_range (GTK_RANGE (position_hscale),
                                     0.0,
                                     duration+1);
		g_signal_handlers_unblock_by_func
			(position_hscale,
			 on_position_hscale_value_changed,
			 NULL);

        }
}

static void
set_position_hscale_position (gint position)
{
	if (is_fullscreen_open())
		return;
        if (position >= 0) {
		guint seconds, mins;
		gchar *label_text;

		seconds = position % 60;
		mins = position / 60;

		label_text = g_strdup_printf("%02d:%02d", mins, seconds);
		gtk_label_set_text(GTK_LABEL(position_label), label_text);
		g_free(label_text);

		g_signal_handlers_block_by_func
					(position_hscale,
					 on_position_hscale_value_changed,
					 NULL);

		gtk_range_set_value (GTK_RANGE (position_hscale),
                                     position);

		g_signal_handlers_unblock_by_func
					(position_hscale,
					 on_position_hscale_value_changed,
					 NULL);
	}

}

void move_position(gint multipler)
{
	gint new_pos = (gint)gtk_range_get_value(GTK_RANGE (position_hscale));
	GtkAdjustment *range_adjust = gtk_range_get_adjustment(
					GTK_RANGE (position_hscale));

	if (!GTK_WIDGET_IS_SENSITIVE(position_hscale))
		return;

	new_pos += 10*multipler;

	if (new_pos < 0)
		new_pos = 0;
	if (new_pos > (gint)range_adjust->upper)
		new_pos = (gint)range_adjust->upper;

	gtk_range_set_value (GTK_RANGE (position_hscale), new_pos);
}

static void
get_position_info_cb (MafwRenderer   *renderer,
                      gint        position,
                      gpointer    user_data,
	              const GError     *error)
{
	MafwPlayState state;

	if (error == NULL) {
		set_position_hscale_position (position);
	} else {
		/* If we are stopped, it is normal that we got the error,
		   just drop a warning in this case and do not bother the 
		   user */
		state = get_selected_renderer_state ();
		if (state == Stopped) {
			g_warning ("%s", error->message);
		} else {
			hildon_banner_show_information (GTK_WIDGET (user_data),
							"qgn_list_smiley_angry",
							error->message);
		}
	}
}

void
prepare_controls_for_state (MafwPlayState state)
{
        gboolean play_possible;
        gboolean pause_possible;
        gboolean stop_possible;

        switch (state) {
        case Stopped:
		/* Disable the seekbar when the state is stopped */
		gtk_widget_set_sensitive(position_hscale, FALSE);
                play_possible = TRUE;
                pause_possible = FALSE;
                stop_possible = TRUE;
		in_state_stopped = TRUE;

                remove_timeout ();
		g_signal_handlers_block_by_func (G_OBJECT(position_hscale),
						on_position_hscale_value_changed, NULL);
		gtk_range_set_value (GTK_RANGE (position_hscale), 0.0);
		gtk_label_set_text(GTK_LABEL(position_label), "00:00");
		g_signal_handlers_unblock_by_func(G_OBJECT(position_hscale),
						  on_position_hscale_value_changed, NULL);
                break;

        case Paused:
                play_possible = TRUE;
                pause_possible = FALSE;
                stop_possible = TRUE;
		in_state_stopped = FALSE;

                remove_timeout ();

                update_position (NULL);

                break;

        case Playing:
                play_possible = FALSE;
                pause_possible = TRUE;
                stop_possible = TRUE;
		in_state_stopped = FALSE;

                /* Start tracking media position in playing state */
                add_timeout ();

                break;

        case Transitioning:
                play_possible = FALSE;
                pause_possible = FALSE;
                stop_possible = TRUE;
		in_state_stopped = FALSE;

                remove_timeout ();

                break;

       default:
                play_possible = TRUE;
                pause_possible = FALSE;
                stop_possible = TRUE;
		in_state_stopped = FALSE;

                remove_timeout ();

                break;
        }

        if (!stop_possible) {
                g_signal_handlers_block_by_func
                                        (position_hscale,
                                         on_position_hscale_value_changed,
                                         NULL);

                gtk_range_set_value (GTK_RANGE (position_hscale), 0.0);
		gtk_label_set_text(GTK_LABEL(position_label), "00:00");

                g_signal_handlers_unblock_by_func
                                        (position_hscale,
                                         on_position_hscale_value_changed,
                                         NULL);
        }

        if (play_possible == TRUE) {
                GtkWidget *image;
                image = gtk_image_new_from_icon_name("camera_playback",
                                                     HILDON_ICON_SIZE_SMALL);
                gtk_button_set_image(GTK_BUTTON(play_button), image);
        }

        if (pause_possible == TRUE) {
                GtkWidget *image;
                image = gtk_image_new_from_icon_name("camera_video_pause",
                                                     HILDON_ICON_SIZE_SMALL);
                gtk_button_set_image(GTK_BUTTON(play_button), image);
        }

	if (play_possible == FALSE && pause_possible == FALSE) {
		gtk_widget_set_sensitive (play_button, FALSE);
        } else {
		gtk_widget_set_sensitive (play_button, TRUE);
        }

        gtk_widget_set_sensitive (stop_button, stop_possible);
}

void
on_volume_vscale_value_changed (GtkWidget *widget)
{
	set_selected_renderer_volume (gtk_range_get_value (GTK_RANGE(widget)));
}

void enable_seek_buttons(gboolean enable)
{
	gtk_widget_set_sensitive(seek_backwards_button, enable);
	gtk_widget_set_sensitive(seek_forwards_button, enable);
}

void
setup_renderer_controls (GtkBuilder *builder)
{
	GtkWidget* image;

        volume_vscale = GTK_WIDGET(gtk_builder_get_object(builder,
                                                          "volume-vscale"));
        g_assert (volume_vscale != NULL);

        position_hscale = GTK_WIDGET(gtk_builder_get_object(builder,
                                                            "position-hscale"));
        g_assert (position_hscale != NULL);

	position_label = GTK_WIDGET(gtk_builder_get_object(builder,
                                                           "position-label"));
        g_assert (position_label != NULL);

	/* Previous */
        prev_button = GTK_WIDGET(gtk_builder_get_object(builder,
                                                        "previous-button"));
        g_assert (prev_button != NULL);

	/* Seek backwards */
        seek_backwards_button =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "seek-backwards-button"));
        g_assert (seek_backwards_button != NULL);

	/* Play */
        play_button = GTK_WIDGET(gtk_builder_get_object(builder,
                                                        "play-button"));
        g_assert (play_button != NULL);

        gtk_button_set_label(GTK_BUTTON(play_button), NULL);
        image = gtk_image_new_from_icon_name("camera_playback",
                                             HILDON_ICON_SIZE_SMALL);
        gtk_button_set_image(GTK_BUTTON(play_button), image);

	/* Stop */
        stop_button = GTK_WIDGET(gtk_builder_get_object(builder,
                                                        "stop-button"));
        g_assert (stop_button != NULL);

	/* Seek forwards */
        seek_forwards_button =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "seek-forwards-button"));
        g_assert (seek_forwards_button != NULL);

	/* Next */
        next_button = GTK_WIDGET(gtk_builder_get_object(builder,
                                                        "next-button"));
        g_assert (next_button != NULL);

	/* Mute */
	mute_toggle = GTK_WIDGET(gtk_builder_get_object(builder,
                                                        "mute-button"));
        g_assert (mute_toggle != NULL);

#ifndef MAFW_TEST_GUI_ENABLE_MUTE
	gtk_widget_set_sensitive(mute_toggle, FALSE);
#endif

        timeout_id = 0;

	/* Seeking is disabled by default */
	enable_seek_buttons(FALSE);

        g_object_weak_ref (G_OBJECT (position_hscale),
                           (GWeakNotify) remove_timeout,
                           NULL);
}
