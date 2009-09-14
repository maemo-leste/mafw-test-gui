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

#include <libmafw/mafw-registry.h>

#include <gtk/gtk.h>

#include "metadata-view.h"
#include "source-treeview.h"
#include "renderer-combo.h"
#include "renderer-controls.h"
#include "fullscreen.h"
#include "gui.h"
#include "main.h"

static GtkWidget *metadata_treeview;
static GtkWidget *metadata_visual;
static gchar *current_oid;

enum {
	METADATA_COLUMN_NAME,
	METADATA_COLUMN_VALUE,
	METADATA_COLUMNS
};

/**
 * Clear the content of the mdata treeview
 */
static void clear_treeview(void)
{
	gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(
				GTK_TREE_VIEW(metadata_treeview))));
}

/**
 * Checks, whether the object id is the (stored) current object id
 */
static gboolean check_current_object_id(const gchar *object_id)
{
	if (current_oid != NULL && object_id != NULL)
		return !strcmp(object_id, current_oid);

	/* In case of play_object, media_changed returns NULL as object_id */
	return object_id == current_oid;
}

static void add_row(GtkListStore *list_store, GtkTreeIter *iter,
			const gchar *key)
{
	GtkTreeIter current_row;
	gchar *cur_key;

	/* Finding the row, whether it exists or not */
	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store),
					   &current_row))
	{
		gtk_list_store_append(list_store, iter);
		return;
	}

	do
	{
		gtk_tree_model_get(GTK_TREE_MODEL(list_store), &current_row,
				   METADATA_COLUMN_NAME, &cur_key, -1);

		if (strcmp(key, cur_key) == 0)
		{
			g_free(cur_key);
			*iter = current_row;
			return;
		}
		g_free(cur_key);
	}
	while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store),
					&current_row));

	if (!strcmp(key, MAFW_METADATA_KEY_TITLE) ||
	    !strcmp(key, MAFW_METADATA_KEY_ALBUM) ||
	    !strcmp(key, MAFW_METADATA_KEY_ARTIST) ||
	    !strcmp(key, MAFW_METADATA_KEY_DURATION))
	{
		/* Insert these as the topmost keys */
		gtk_list_store_insert(list_store, iter, 0);
	}
	else
	{
		/* Append everything else */
		gtk_list_store_append(list_store, iter);
	}
}

/**
 * Adds the key-value pars, to the mdata treeview. title, album, artist,
 * duration should be the first.
 */
static void add_to_mdata_view(const gchar *key, const gchar *value)
{
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE(
				gtk_tree_view_get_model(
					GTK_TREE_VIEW(metadata_treeview)));

	g_assert(list_store);

	add_row(list_store, &iter, key);

	gtk_list_store_set (list_store, &iter,
                      METADATA_COLUMN_NAME, key,
                      METADATA_COLUMN_VALUE, value,
                      -1);
}

/**
 * Transforms the metadata value to string, and it adds to the treeview
 */
static void add_metadata(const gchar *key, gpointer val, gpointer user_data)
{
	GValue strval;

	if (strcmp(key, MAFW_METADATA_KEY_MIME) == 0)
	{
		GValue* cur_val;
		const gchar* mime;

		cur_val = g_value_array_get_nth(val, 0);

		mime = g_value_get_string(cur_val);
		if (mime == NULL)
			return;
		else if (strstr(mime, "audio") != NULL)
		{
			if (is_fullscreen_open() == TRUE)
				close_fullscreen_window();

			gtk_widget_hide(metadata_visual);
		}
		else
		{
 			gtk_widget_show(metadata_visual);
		}
	}
	
	/* The source:browse_cb sends the metadata in different way, than the
	   renderer::metadata-changed signal sends it*/
	else if (!strcmp(key, MAFW_METADATA_KEY_DURATION))
	{
		GValue *cur_val;

		cur_val = g_value_array_get_nth(val,0);

		if (G_VALUE_HOLDS_INT64(cur_val))
			set_position_hscale_duration(g_value_get_int64(cur_val));
		else
			set_position_hscale_duration(g_value_get_int(cur_val));

	}

	else if (!strcmp(key, MAFW_METADATA_KEY_IS_SEEKABLE))
	{
		GValue *cur_val;

		cur_val = g_value_array_get_nth(val,0);

		if (g_value_get_boolean(cur_val) == TRUE)
		{
			enable_seek_buttons(TRUE);
			set_position_hscale_sensitive(TRUE);
		}
		else
		{
			enable_seek_buttons(FALSE);
			set_position_hscale_sensitive(FALSE);
		}
	}

	else if (!strcmp(key, MAFW_METADATA_KEY_VIDEO_CODEC) ||
		 !strcmp(key, MAFW_METADATA_KEY_VIDEO_FRAMERATE) ||
		 !strcmp(key, MAFW_METADATA_KEY_RES_X) ||
		 !strcmp(key, MAFW_METADATA_KEY_RES_Y)) {
		/* Sometimes we get these but no mime type key,
		   in this case, we have a video anyway */
		gtk_widget_show(metadata_visual);
	}

	memset(&strval, 0, sizeof(strval));

	guint i;
	GString *str;
	GValueArray *values;

	str = g_string_new(NULL);
	values = val;
	for (i = 0; i < values->n_values; i++) {
		g_value_init(&strval, G_TYPE_STRING);
		g_value_transform(g_value_array_get_nth(values, i),
				  &strval);
		if (i > 0)
			g_string_append(str, ", ");
		g_string_append_c(str, '`');
		g_string_append(str, g_value_get_string(&strval));
		g_string_append_c(str, '\'');
		g_value_unset(&strval);
	}

	add_to_mdata_view(key, str->str);

	g_string_free(str, TRUE);
}

/**
 * source_get_metadata cb
 */
void mdata_view_mdata_result(MafwSource *self,
			   const gchar *object_id,
			   GHashTable *metadata,
			   gpointer user_data,
			   const GError *error)
{
	if (error)
	{
		g_critical("Error in metadata results: %s", error->message);
		return;
	}

	if (!metadata)
		return;

	if (!check_current_object_id(object_id))
		return;

	g_hash_table_foreach(metadata, (GHFunc)add_metadata, NULL);
}

void mdata_view_update(const gchar *key, GValueArray *value)
{
	add_metadata(key, value, NULL);
}

/**
 * Updates the metadata
 */
void set_current_oid(const gchar *obj_id)
{
	MafwSource *source;
	gchar *source_uuid;

	/* Clear metadata treeview */
	clear_treeview();

	/* Get rid of the current object ID, if any */
	if (current_oid != NULL)
	{
		g_free(current_oid);
		current_oid = NULL;
	}

	/* Set the new object ID, if any */
	if (obj_id != NULL)
		current_oid = g_strdup(obj_id);
	else
		return;

	/* Extract the source-part from the object ID */
	if (!mafw_source_split_objectid(current_oid, &source_uuid, NULL))
	{
		g_critical("Wrong object-id format: %s", current_oid);
		clear_treeview();
		return;
	}

	/* Find the actual source component, identified by the UUID */
	g_assert(source_uuid != NULL);
	source = MAFW_SOURCE(mafw_registry_get_extension_by_uuid(
			      MAFW_REGISTRY(mafw_registry_get_instance()),
			      source_uuid));

	/* Get some metadata for the object ID */
	if (source == NULL)
	{
		hildon_banner_show_information (NULL,
						"chat_smiley_angry",
						"Source not available");
	}
	else
	{
		/* Disable seekbar, and enable only if we 
		   get seekble=TRUE metadata. Waiting to do this in
		   mdata_view_mdata_result might end up in
		   race-condition with the renderer metadata
		   in certain scenarios */
		set_position_hscale_sensitive(FALSE);

		mafw_source_get_metadata(
			 source, current_oid,
			 MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI,
					   MAFW_METADATA_KEY_MIME,
					   MAFW_METADATA_KEY_TITLE,
					   MAFW_METADATA_KEY_ALBUM,
					   MAFW_METADATA_KEY_ARTIST,
					   MAFW_METADATA_KEY_DURATION,
					   MAFW_METADATA_KEY_IS_SEEKABLE),
			 mdata_view_mdata_result, NULL);
	}

	g_free(source_uuid);
}

XID
get_metadata_visual_xid (void)
{
	return GDK_WINDOW_XID (metadata_visual->window);
}

static void
on_metadata_visual_button_press_event (GtkWidget      *widget,
				       GdkEventButton *event,
				       gpointer        user_data)
{
	open_fullscreen_window ();
}

static void
setup_treeview_columns (GtkWidget *treeview)

{
        GtkTreeSelection  *selection;
        GtkTreeViewColumn *column;
        GtkCellRenderer   *renderer;
        int                i;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        g_assert (selection != NULL);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);

	for (i=0; i<2; i++)
	{
		column = gtk_tree_view_column_new ();
		gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
		renderer = gtk_cell_renderer_text_new();
		gtk_tree_view_column_pack_start(column, renderer, TRUE);
		gtk_tree_view_column_set_sizing(column,
                                                GTK_TREE_VIEW_COLUMN_AUTOSIZE);
		gtk_tree_view_column_add_attribute (column,
                                                    renderer,
                                                    "text", i);
	}
}

static gboolean
mdataview_expose_cb (GtkWidget * widget, GdkEventExpose * event, gpointer data)
{
	if (!is_fullscreen_open() && !GTK_WIDGET_NO_WINDOW (widget)
		&& GTK_WIDGET_REALIZED (widget)) {
		set_selected_renderer_xid (GDK_WINDOW_XID(
						GDK_WINDOW(widget->window)));
	}
	return FALSE;
}

void
setup_metadata_treeview (GtkBuilder *builder)
{
	GdkEventMask mask;

	/* Metadata tree view */
        metadata_treeview =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "metadata-treeview"));
        g_assert (metadata_treeview != NULL);

        gtk_tree_view_set_model (GTK_TREE_VIEW (metadata_treeview),
					GTK_TREE_MODEL(gtk_list_store_new (
							METADATA_COLUMNS,
							/* Name */
							G_TYPE_STRING,
							/* Value */
							G_TYPE_STRING)));
        setup_treeview_columns (metadata_treeview);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW (metadata_treeview), TRUE);

	/* Visual component */
        metadata_visual = GTK_WIDGET(gtk_builder_get_object(builder,
                                                            "metadata-visual"));
	/* Resize the visual widget to something reasonable */
	gtk_widget_set_size_request (metadata_visual, 120, 100);

	gtk_widget_set_double_buffered (metadata_visual, FALSE);
        g_assert (metadata_visual != NULL);

	/* Create the X/GDK Window for the visual component so that we can pass
	   the window ID to renderer. */
	gtk_widget_realize (metadata_visual);

	g_signal_connect (metadata_visual, "expose-event",
			G_CALLBACK (mdataview_expose_cb), NULL);

	mask = gdk_window_get_events (metadata_visual->window);
	gdk_window_set_events (metadata_visual->window,
			       mask | GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK);

	g_signal_connect (metadata_visual, "button-press-event",
			  G_CALLBACK(on_metadata_visual_button_press_event),
			  NULL);
	
}
