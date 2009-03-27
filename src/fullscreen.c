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

#include <hildon/hildon.h>

#include "fullscreen.h"
#include "metadata-view.h"
#include "renderer-combo.h"
#include "renderer-controls.h"

static GtkWidget *window = NULL;
static GtkWidget *layout = NULL;
static GtkWidget *visual = NULL;
static gboolean fullscreen = FALSE;

/*****************************************************************************
 * Private API
 *****************************************************************************/

static gboolean
on_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	set_selected_renderer_xid (get_metadata_visual_xid ());

	window = NULL;
	layout = NULL;
	visual = NULL;

	return FALSE;
}

static void
on_visual_button_press_event (GtkWidget *widget, GdkEventButton *event,
			      gpointer user_data)
{
	close_fullscreen_window ();
}

static gboolean
on_key_press_event (GtkWidget* widget, GdkEventKey* event, gpointer user_data)
{
	gboolean retval = FALSE;

	switch(event->keyval)
	{
	case HILDON_HARDKEY_ESC:
		close_fullscreen_window ();
		retval = TRUE;
		break;

	case HILDON_HARDKEY_MENU:
		/* Let hildon take care of this button */
		break;

	case HILDON_HARDKEY_HOME:
		/* Let hildon take care of this button */
		break;

	case HILDON_HARDKEY_FULLSCREEN:
		if (fullscreen == TRUE)
			gtk_window_unfullscreen (GTK_WINDOW(window));
		else
			gtk_window_fullscreen (GTK_WINDOW(window));
		fullscreen = !fullscreen;
		retval = TRUE;
		break;

	case HILDON_HARDKEY_SELECT:
		/* renderer_controls::play() switches between play/resume */
		play ();
		break;

	case HILDON_HARDKEY_UP:
		move_position(1);
		break;

	case HILDON_HARDKEY_DOWN:
		move_position(-1);
		break;

	case HILDON_HARDKEY_LEFT:
		prev();
		break;

	case HILDON_HARDKEY_RIGHT:
		next();
		break;

	case HILDON_HARDKEY_INCREASE:
		change_volume(1);
		break;

	case HILDON_HARDKEY_DECREASE:
		change_volume(-1);
		break;

	default:
		retval = FALSE;
		break;
	}

	return retval;
}

/*****************************************************************************
 * Public API
 *****************************************************************************/

gboolean
is_fullscreen_open (void)
{
	if (window == NULL)
		return FALSE;
	else
		return GTK_WIDGET_VISIBLE(window);
}

static gboolean
fs_expose_cb (GtkWidget * widget, GdkEventExpose * event, gpointer data)
{
	if (fullscreen && !GTK_WIDGET_NO_WINDOW (widget) &&
		GTK_WIDGET_REALIZED (widget)) {
		set_selected_renderer_xid (GDK_WINDOW_XID (GDK_WINDOW (widget->window)));
	}
	return FALSE;
}

void
open_fullscreen_window (void)
{
	GdkEventMask mask;

	if (window == NULL)
	{
		window = hildon_window_new ();
		layout = gtk_hbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (window), layout);
		
		visual = gtk_drawing_area_new ();
		gtk_container_add (GTK_CONTAINER (layout), visual);

		gtk_widget_realize (window);
		gtk_widget_realize (visual);
		gtk_widget_set_double_buffered (visual, FALSE);

		mask = gdk_window_get_events (visual->window);
		gdk_window_set_events (visual->window, mask |
				       GDK_BUTTON_PRESS_MASK |
				       GDK_BUTTON_RELEASE_MASK);

		g_signal_connect (visual, "button-press-event",
				  G_CALLBACK(on_visual_button_press_event),
				  NULL);

		g_signal_connect(G_OBJECT(window), "key-press-event",
				 G_CALLBACK(on_key_press_event), NULL);

		g_signal_connect(window, "delete-event",
				 G_CALLBACK (on_delete_event), NULL);
		g_signal_connect (G_OBJECT (visual), "expose-event",
			G_CALLBACK (fs_expose_cb), NULL);
	}

	
	gtk_window_fullscreen (GTK_WINDOW (window));
	gtk_widget_show_all (window);
	fullscreen = TRUE;
}

void
close_fullscreen_window (void)
{
	g_assert (window != NULL);
	fullscreen = FALSE;

	gtk_widget_hide (window);
	set_selected_renderer_xid (get_metadata_visual_xid());
}

XID
get_fullscreen_xid (void)
{
	return GDK_WINDOW_XID (visual->window);
}
