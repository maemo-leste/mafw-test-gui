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

#include <libmafw/mafw.h>
#include <hildon/hildon-banner.h>

#include "renderer-combo.h"
#include "renderer-controls.h"
#include "playlist-controls.h"
#include "playlist-treeview.h"
#include "source-treeview.h"
#include "metadata-view.h"
#include "main.h"

enum {
	RENDERER_COMBO_COLUMN_NAME,
	RENDERER_COMBO_COLUMN_RENDERER,
	RENDERER_COMBO_COLUMN_STATE,
	RENDERER_COMBO_COLUMN_VOLUME,
	RENDERER_COMBO_COLUMN_DURATION,
	RENDERER_COMBO_COLUMNS
};

static GtkWidget  *renderer_combo;
static GtkWidget  *fop_button;
static MafwRenderer   *selected_renderer;
static gchar      *state_labels[] = {"STOPPED",
				     "PLAYING",
				     "PAUSED",
				     "TRANSITIONING"};
static XID cur_xid = -1;

static void
toggle_fop (gboolean new_state);
void
on_frame_on_pause_button_toggled (GtkWidget *widget);

void
assign_playlist_to_current_renderer(MafwPlaylist *playlist)
{
	gboolean retval;
	if (selected_renderer == NULL)
		return;

	retval = mafw_renderer_assign_playlist(selected_renderer,
                                               playlist, NULL);
	if (!retval)
	{
		g_print ("Unable to assign playlist\n");
	}
}

guint
get_selected_renderer_volume (void)
{
        GtkComboBox  *combo;
        GtkTreeModel *model;
        GtkTreeIter   iter;
        guint         volume;

        combo = GTK_COMBO_BOX (renderer_combo);
        model = gtk_combo_box_get_model (combo);
        g_assert (model != NULL);

        if (!gtk_combo_box_get_active_iter (combo, &iter)) {
                return 0;
        }

	gtk_tree_model_get (model, &iter,
			    RENDERER_COMBO_COLUMN_VOLUME, &volume,
			    -1);

        return volume;
}

MafwPlayState
get_selected_renderer_state (void)
{
        MafwPlayState state;
        GtkComboBox  *combo;
        GtkTreeModel *model;
        GtkTreeIter   iter;

        combo = GTK_COMBO_BOX (renderer_combo);
        model = gtk_combo_box_get_model (combo);
        g_assert (model != NULL);

        if (!gtk_combo_box_get_active_iter (combo, &iter)) {
                return Stopped;
        }

	gtk_tree_model_get (model, &iter,
			    RENDERER_COMBO_COLUMN_STATE, &state,
			    -1);
        return state;
}

MafwRenderer *
get_selected_renderer()
{
	return selected_renderer;
}

void set_selected_renderer_volume (gfloat volume)
{
	MafwRenderer     *renderer;
	GValue        value ={0,};
	GtkTreeModel *model = NULL;
	GtkTreeIter   iter;
	GtkComboBox  *combo;

	combo = GTK_COMBO_BOX (renderer_combo);
        if (!gtk_combo_box_get_active_iter (combo, &iter)) {
                return;
        }

        model = gtk_combo_box_get_model (combo);
        g_assert (model != NULL);

	gtk_tree_model_get (model, &iter,
			    RENDERER_COMBO_COLUMN_RENDERER, &renderer,
			    -1);

	g_value_init (&value, G_TYPE_UINT);
	g_value_set_uint (&value, volume);
	mafw_extension_set_property (MAFW_EXTENSION(renderer),
                                     "volume",
                                     &value);

	gtk_list_store_set (GTK_LIST_STORE(model), &iter,
			    RENDERER_COMBO_COLUMN_VOLUME, (guint) volume,
			    -1);
}

static gboolean
find_renderer (GtkTreeModel *model,
               const char   *uuid,
               GtkTreeIter  *iter)
{
        gboolean found = FALSE;
        gboolean more = TRUE;

        g_assert (uuid != NULL);
        g_assert (iter != NULL);

        more = gtk_tree_model_get_iter_first (model, iter);

        while (more) {
                MafwRenderer *info;

		gtk_tree_model_get (model, iter,
                                    RENDERER_COMBO_COLUMN_RENDERER, &info,
				    -1);

                if (info) {
                        const char *renderer_uuid;

                        renderer_uuid =
                                mafw_extension_get_uuid (MAFW_EXTENSION(info));

                        if (renderer_uuid &&
                            (strcmp (renderer_uuid, uuid) == 0))
                                found = TRUE;

                        g_object_unref (info);
                }

                if (found)
                        break;

                more = gtk_tree_model_iter_next (model, iter);
        }

        return found;
}

/* FIXME: is there a better implementation possible? */
static gboolean
is_iter_active (GtkComboBox *combo,
                GtkTreeIter *iter)
{
        GtkTreeModel *model;
        GtkTreeIter   active_iter;
        GtkTreePath  *active_path;
        GtkTreePath  *path;
        gboolean      ret;

        model = gtk_combo_box_get_model (combo);
        g_assert (model != NULL);

        path = gtk_tree_model_get_path (model, iter);
        g_assert (path != NULL);

        if (!gtk_combo_box_get_active_iter (combo, &active_iter)) {
                gtk_tree_path_free (path);

                return FALSE;
        }

        active_path = gtk_tree_model_get_path (model, &active_iter);
        g_assert (active_path != NULL);

        ret = (gtk_tree_path_compare (path, active_path) == 0);

        gtk_tree_path_free (path);
        gtk_tree_path_free (active_path);

        return ret;
}

static void
set_state (GtkTreeModel *model,
           GtkTreeIter  *iter,
           MafwPlayState state)
{
	gtk_list_store_set (GTK_LIST_STORE (model), iter,
                            RENDERER_COMBO_COLUMN_STATE, state,
			    -1);

        if (state == Playing ||
            state == Paused ||
            state == Transitioning) {
                gtk_widget_set_sensitive (renderer_combo, FALSE);
        } else {
                gtk_widget_set_sensitive (renderer_combo, TRUE);
        }

        if (is_iter_active (GTK_COMBO_BOX (renderer_combo), iter)) {
                prepare_controls_for_state (state);
        }
}

static const gchar *
state_to_string(MafwPlayState state)
{
	if (state >= Stopped && state <= Transitioning) {
		return state_labels[state];
	} else {
		return "UNKNOWN";
	}
}

static void
state_changed_cb(MafwRenderer *renderer,
		 MafwPlayState state,
		 gpointer user_data)
{
	const gchar  *uuid;
        GtkTreeModel *model;
        GtkTreeIter   iter;

	mtg_print_signal (MAFW_EXTENSION (renderer), "Renderer::state-changed",
			  "state:%s (%d)\n", state_to_string (state), state);

	uuid = mafw_extension_get_uuid(MAFW_EXTENSION(renderer));

        model = gtk_combo_box_get_model
                (GTK_COMBO_BOX (renderer_combo));
        g_assert (model != NULL);

        if (find_renderer (model, uuid, &iter)) {
                set_state (model, &iter, state);
	}
}

static void
media_changed_cb(MafwRenderer *renderer,
		 gint index,
		 gchar *object_id,
		 gpointer user_data)
{
	mtg_print_signal (MAFW_EXTENSION (renderer), "Renderer::media-changed",
			  "Index: %d, ObjectID:%s\n", index, object_id);

	set_current_oid(object_id);
	update_playing_item (index);
}


void
clear_selected_renderer_state (void)
{
        GtkComboBox  *combo;
        GtkTreeModel *model;
        GtkTreeIter   iter;

        combo = GTK_COMBO_BOX (renderer_combo);
        model = gtk_combo_box_get_model (combo);
        g_assert (model != NULL);

        if (gtk_combo_box_get_active_iter (combo, &iter)) {
                set_state (model, &iter, Stopped);
        }
}

static gboolean get_renderer_iter(MafwExtension *self, GtkTreeModel *model,
				GtkTreeIter *iter)
{
	gpointer *stored_extension;
	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;

	do
	{
		gtk_tree_model_get(model, iter, RENDERER_COMBO_COLUMN_RENDERER,
					&stored_extension, -1);
		if ((MafwExtension*)stored_extension == self)
			return TRUE;
	}
	while(gtk_tree_model_iter_next(model, iter));

	return FALSE;
}

static void renderer_error_cb(GObject *render, guint domain, gint code,
                              gchar *message)
{
	hildon_banner_show_information (NULL,
					"qgn_list_smiley_angry",
					message);
}

static void update_volume(MafwExtension *self, const gchar *name, GValue *value,
					 gpointer udata, const GError *error)
{
	GtkComboBox *combo = GTK_COMBO_BOX (renderer_combo);
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	guint vol;

	if (error)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						error->message);
		return;
	}
	model = gtk_combo_box_get_model (combo);

	vol = g_value_get_uint(value);
	if (get_renderer_iter(self, model, &iter))
	{
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
			    RENDERER_COMBO_COLUMN_VOLUME, (guint) vol,
			    -1);
	}

	if (get_selected_renderer() == (MafwRenderer*)self)
		set_volume_vscale(vol);

}

static void
append_media_renderer_to_tree (MafwRenderer          *renderer,
                               const char        *uuid)
{
        MafwRenderer         *info;
        GtkComboBox      *combo;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        char             *name;
        gboolean          was_empty;

        info = MAFW_RENDERER (renderer);
        combo = GTK_COMBO_BOX (renderer_combo);

        name = g_strdup(mafw_extension_get_name (MAFW_EXTENSION(info)));
        if (name == NULL)
                name = g_strdup (uuid);
	/* Hack to set true the current-frame-on-pause property */
	else if (g_strcmp0(name, "Mafw-Gst-Renderer") == 0) {
		mafw_extension_set_property_boolean (MAFW_EXTENSION(info),
						     "current-frame-on-pause",
						     TRUE);
	}

	mafw_extension_get_property (MAFW_EXTENSION(info),
                                     "volume", update_volume,
                                     renderer);

        model = gtk_combo_box_get_model (combo);

        if (gtk_combo_box_get_active (combo) == -1)
                was_empty = TRUE;
        else
                was_empty = FALSE;

        memset (&iter, 0, sizeof (iter));
        gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter, -1,
					   RENDERER_COMBO_COLUMN_NAME, name,
					   RENDERER_COMBO_COLUMN_RENDERER,
                                           renderer,
					   RENDERER_COMBO_COLUMN_STATE, Stopped,
					   -1);

        if (was_empty)
                gtk_combo_box_set_active_iter (combo, &iter);

        g_free (name);
}

static void property_changed_cb(MafwExtension *object, const gchar *name,
				const GValue *value)
{
	gchar* contents = g_strdup_value_contents (value);
	mtg_print_signal (object, "SiSo::property-changed",
			  "Name: %s, Value:%s\n", name, contents);
	g_free (contents);

	if ( MAFW_RENDERER(object) == get_selected_renderer())
	{
		if (!strcmp(name, MAFW_PROPERTY_RENDERER_MUTE))
			toggle_mute_button(g_value_get_boolean(value));
                else if(!strcmp(name, MAFW_PROPERTY_RENDERER_VOLUME))
                        set_volume_vscale(g_value_get_uint(value));
		else if (!strcmp(name, "current-frame-on-pause"))
			toggle_fop(g_value_get_boolean(value));
		else if (!strcmp(name, MAFW_PROPERTY_RENDERER_XID))
			cur_xid = g_value_get_uint(value);
	}
}

static void renderer_metadata_changed_cb(MafwRenderer *self, const gchar *key,
		GValueArray *value)
{
	gint i;
	for (i = 0; i < value->n_values; i++)
	{
		gchar* contents = g_strdup_value_contents (&(value->values[i]));
		mtg_print_signal (MAFW_EXTENSION (self),
                                  "Renderer::metadata-changed",
				  "Key: %s, Value:%s\n", key, contents);
		g_free (contents);
	}

	mdata_view_update(key, value);
}

static void renderer_playlist_changed_cb(MafwRenderer *self,
					 MafwPlaylist *playlist,
					 gpointer user_data)
{
	on_renderer_assigned_playlist_changed(playlist);
}

void
add_media_renderer (MafwRenderer *renderer)
{
        GtkTreeModel      *model;
        GtkTreeIter        iter;
        const char        *uuid;

        uuid = mafw_extension_get_uuid(MAFW_EXTENSION(renderer));
        if (uuid == NULL)
                return;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (renderer_combo));

        if (!find_renderer (model, uuid, &iter)) {
		g_signal_connect(renderer, "state-changed",
				 G_CALLBACK(state_changed_cb), NULL);
		g_signal_connect(renderer, "media-changed",
				 G_CALLBACK(media_changed_cb), NULL);
		g_signal_connect(renderer, "property-changed",
				 G_CALLBACK(property_changed_cb), NULL);
		g_signal_connect(renderer, "metadata-changed",
				 G_CALLBACK(renderer_metadata_changed_cb),
                                 NULL);
		g_signal_connect(renderer, "error",
				 G_CALLBACK(renderer_error_cb), NULL);
		g_signal_connect(renderer, "playlist-changed",
				 G_CALLBACK(renderer_playlist_changed_cb),
				 NULL);
                append_media_renderer_to_tree (renderer, uuid);
	}
}

void
remove_media_renderer (MafwRenderer *renderer)
{
        MafwRenderer        *info;
        GtkComboBox     *combo;
        GtkTreeModel    *model;
        GtkTreeIter      iter;
        const char      *uuid;

        info = MAFW_RENDERER (renderer);
        combo = GTK_COMBO_BOX (renderer_combo);

        uuid = mafw_extension_get_uuid (MAFW_EXTENSION(info));
        if (uuid == NULL)
                return;

        model = gtk_combo_box_get_model (combo);

        if (find_renderer (model, uuid, &iter)) {
                if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
                	gtk_combo_box_set_active (combo, 0);
        }
}

static void
selected_renderer_status_cb (MafwRenderer *renderer, MafwPlaylist *playlist,
			 guint index, MafwPlayState state,
			 const gchar *object_id, gpointer user_data,
			 const GError *error)
{
	if (error != NULL) {
		g_print ("Unable to get renderer status: %s\n", error->message);
	} else {
		if (MAFW_IS_PROXY_PLAYLIST(playlist))
			select_playlist(MAFW_PROXY_PLAYLIST(playlist));
		else
			g_print ("Renderer does not have a Proxy Playlist\n");
	}
}

static void fop_status_cb(MafwExtension *self,
					 const gchar *name,
					 GValue *value,
					 gpointer udata,
					 const GError *error)
{
	if (!error && value)
	{
		gtk_widget_set_sensitive(fop_button, TRUE);
		toggle_fop(g_value_get_boolean(value));
	}
	else
	{
		gtk_widget_set_sensitive(fop_button, FALSE);
	}
}

static void
on_renderer_combo_changed (GtkComboBox *widget,
		       gpointer     user_data)
{
        GtkComboBox  *combo;
        GtkTreeModel *model;
        GtkTreeIter   iter;
        MafwPlayState state;
        guint         volume;

        combo = GTK_COMBO_BOX (renderer_combo);
        model = gtk_combo_box_get_model (combo);
        g_assert (model != NULL);

        if (!gtk_combo_box_get_active_iter (combo, &iter)) {
                return;
        }

	if (selected_renderer != NULL) {
		stop();
	}

        gtk_tree_model_get (model, &iter,
			    RENDERER_COMBO_COLUMN_RENDERER, &selected_renderer,
                            RENDERER_COMBO_COLUMN_STATE, &state,
			    RENDERER_COMBO_COLUMN_VOLUME, &volume,
                            -1);

        set_volume_vscale (volume);
        prepare_controls_for_state (state);
	update_mute_button();

	if (selected_renderer != NULL) {
		/* Get the selected renderer's playlist */
		mafw_renderer_get_status(selected_renderer,
                                         selected_renderer_status_cb,
                                         NULL);
		set_selected_renderer_xid (get_metadata_visual_xid ());
		mafw_extension_get_property(MAFW_EXTENSION(selected_renderer),
						"current-frame-on-pause",
						fop_status_cb, NULL);
	}
}

void
set_selected_renderer_xid (XID xid)
{
	GValue value = { 0 };
	
	
	if (xid == cur_xid)
		return;
	
	cur_xid = xid;

	/* Set the visual widget's XID to the selected renderer for
         * playing video */
	g_value_init (&value, G_TYPE_UINT);
	g_value_set_uint (&value, xid);
	mafw_extension_set_property (MAFW_EXTENSION (selected_renderer),
                                     "xid",
                                     &value);
}

/**
 * GTK signal handler for frame-on-pause button presses
 */
void
on_frame_on_pause_button_toggled (GtkWidget *widget)
{
	if (selected_renderer)
		mafw_extension_set_property_boolean(
                        MAFW_EXTENSION (selected_renderer),
                        "current-frame-on-pause",
                        gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON(widget)));
}

/**
 * Manually toggle the frame-on-pause button state
 */
static void
toggle_fop (gboolean new_state)
{
	/* Temporarily block the button's own signal sending so that we
	   don't end up in an endless loop */
	g_signal_handlers_block_by_func (fop_button,
                                         on_frame_on_pause_button_toggled,
                                         NULL);

	/* Set the button's state */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(fop_button),
				      new_state);

	/* Re-enable the button's signal sending */
	g_signal_handlers_unblock_by_func (fop_button,
					   on_frame_on_pause_button_toggled,
					   NULL);
}


void
setup_renderer_combo (GtkBuilder *builder)
{
        GtkListStore *store;
        GtkCellRenderer *renderer;

	/* Create the renderer combo from gtk-builder */
	renderer_combo =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "renderer-combobox"));
	g_assert (renderer_combo != NULL);

	/* Catch selection changes from the renderer combo */
	g_signal_connect (renderer_combo, "changed",
			  G_CALLBACK (on_renderer_combo_changed), NULL);

	/* Create a list store for renderers and set it to the combo */
	store = gtk_list_store_new (RENDERER_COMBO_COLUMNS,
				    G_TYPE_STRING, /* Name     */
				    G_TYPE_OBJECT, /* Renderer     */
				    G_TYPE_UINT,   /* State    */
				    G_TYPE_UINT,   /* Volume   */
				    G_TYPE_UINT);  /* Duration */
	gtk_combo_box_set_model (GTK_COMBO_BOX (renderer_combo),
				 GTK_TREE_MODEL(store));
	g_object_unref (store);

	/* Create a text cell renderer for renderer name to the combo */
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (renderer_combo),
				    renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (renderer_combo),
				       renderer, "text",
				       RENDERER_COMBO_COLUMN_NAME);

	/* Get the frame-on-pause button from gtk-builder */
	fop_button =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "frame-on-pause-button"));
	g_assert (fop_button != NULL);

	/* Catch selection changes from the renderer combo */
	g_signal_connect (fop_button, "changed",
			  G_CALLBACK (on_renderer_combo_changed), NULL);
}
