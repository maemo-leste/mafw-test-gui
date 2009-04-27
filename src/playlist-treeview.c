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

#include "playlist-treeview.h"
#include "playlist-controls.h"
#include "renderer-controls.h"
#include "renderer-combo.h"
#include "source-treeview.h"
#include "main.h"
#include "gui.h"

#include <libmafw/mafw.h>
#include <libmafw-shared/mafw-playlist-manager.h>
#include <libmafw/mafw-registry.h>
#include <string.h>
#include <stdlib.h>

static GtkWidget *playlist_treeview = NULL;
static GtkTreeModel *playlist_model = NULL;

static gint playing_index = -1;
static gboolean update_current_idx;

/*****************************************************************************
 * Playlist structure
 *****************************************************************************/

enum {
	COLUMN_INDEX,
	COLUMN_CURRENT,
	COLUMN_TITLE,
	COLUMN_OBJECTID,
	COLUMNS
};

static GPtrArray *pl_get_mds;
static guint playlist_updater_id;

struct pl_get_mds_data {
	gint from;
	gint to;
	gpointer get_md_id;
};

/*****************************************************************************
 * Playlist contents displaying
 *****************************************************************************/

static gboolean playlist_updater(gpointer data);

static void cancel_all_get_mds(void)
{
	if (pl_get_mds)
	{
		guint i = 0;
		struct pl_get_mds_data *pldat;
		for (i = 0; (pldat = g_ptr_array_index(pl_get_mds, i)); i++)
		{
			mafw_playlist_cancel_get_items_md(pldat->get_md_id);
		}
	}
}

void
display_playlist_contents(MafwPlaylist *playlist)
{
	GError* error = NULL;
	guint size, i;
	GtkTreeIter iter;

	g_assert (playlist != NULL);

	/* Cancel any running requests */
	if (playlist_updater_id)
	{
		g_source_remove(playlist_updater_id);
		cancel_all_get_mds();
	}

	update_current_idx = FALSE;
	/* Clear playlist contents */
	clear_current_playlist_treeview ();

	/* Get playlist total size */
	size = mafw_playlist_get_size (playlist, &error);
	if (error != NULL)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						error->message);
		g_error_free (error);
		return;
	}
	
	if (size == 0)
		return;

	for (i = 0; i < size; i++)
	{
		gtk_list_store_append(GTK_LIST_STORE(playlist_model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (playlist_model), &iter,
					    COLUMN_INDEX, i,
					    -1);		
	}
	if (playlist_updater_id == 0)
	{
		playlist_updater_id = g_idle_add(playlist_updater, NULL);
	}
}

gchar *treeview_get_stored_title(guint index)
{
	gchar *title = NULL;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_nth_child(playlist_model,
						  &iter, NULL, index)
		    == TRUE)
	{
		gtk_tree_model_get (playlist_model, &iter,
				    COLUMN_TITLE, &title,
				    -1);
	}
	return title;
}

void
clear_current_playlist_treeview (void)
{
	gtk_list_store_clear (GTK_LIST_STORE (playlist_model));
}

static void pl_get_md_cb(MafwPlaylist *pls,
				       guint index,
				       const gchar *object_id,
				       GHashTable *metadata,
				       struct pl_get_mds_data *pldat)
{
	GtkTreeIter iter;
	GValue *value;
	gchar *title;
	gboolean from_uri = FALSE;

	if (!gtk_tree_model_iter_nth_child (playlist_model, &iter, NULL, index))
		return;

	/* Attempt to extract a sane title for the item */
        if (!metadata)
                title = g_strdup("Unknown");
        else
        {
                value = mafw_metadata_first(metadata, MAFW_METADATA_KEY_TITLE);
                if (value == NULL)
                {
                        value = mafw_metadata_first(metadata,
                                                    MAFW_METADATA_KEY_URI);
                        from_uri = TRUE;
                }
                if (value == NULL)
                        title = g_strdup("Unknown");
                else
                {
                        if (from_uri)
                                title = g_uri_unescape_string(
                                        g_value_get_string(value),
                                        NULL);
                        else
                                title = g_strdup(g_value_get_string(value));
                }
        }

	/* Update the item's title */
	gtk_list_store_set (GTK_LIST_STORE (playlist_model), &iter,
				    COLUMN_OBJECTID, object_id,
				    COLUMN_TITLE, title,
				    -1);
	g_free(title);
}

static void pl_get_md_finished(struct pl_get_mds_data *pldat)
{
	g_ptr_array_remove(pl_get_mds, pldat);
	g_free(pldat);
}

static void get_playlist_mds(MafwPlaylist *current_playlist, gint from, gint to)
{
	gpointer pl_get_md_id;
	struct pl_get_mds_data *pldat = g_new0(struct pl_get_mds_data, 1);

	pl_get_md_id = mafw_playlist_get_items_md(current_playlist,
							from, to,
				MAFW_SOURCE_LIST(MAFW_METADATA_KEY_TITLE,
						     MAFW_METADATA_KEY_URI),
					(MafwPlaylistGetItemsCB)pl_get_md_cb,
					pldat,
					(GDestroyNotify)pl_get_md_finished);
	if (!pl_get_mds)
		pl_get_mds = g_ptr_array_new();
	
	pldat->from = from;
	pldat->to = to;
	pldat->get_md_id = pl_get_md_id;
	g_ptr_array_add(pl_get_mds, pldat);

}

static gboolean playlist_updater(gpointer data)
{
	MafwPlaylist *current_playlist;
	GtkTreeIter iter;
	gint i = 0;
	gint from=-1, to=-1;

	current_playlist = MAFW_PLAYLIST(get_current_playlist ());
        if (current_playlist == NULL)
	{
		goto updater_exit;
	}
	
	if (!gtk_tree_model_get_iter_first(playlist_model, &iter))
		goto updater_exit;

	do
	{
		gchar *cur_oid;
		
		gtk_tree_model_get(playlist_model, &iter,
			    COLUMN_OBJECTID, &cur_oid,
			    -1);
		if (cur_oid)
		{
			if (from != -1)
			{
				to = i;
				get_playlist_mds(current_playlist, from, to);
				from = -1;
			}
			g_free(cur_oid);
		}
		else
		{
			if (from == -1)
				from = i;
		}
		i++;

	} while (gtk_tree_model_iter_next(playlist_model, &iter));
	if (from != -1)
	{
		to = i-1;
		get_playlist_mds(current_playlist, from, to);
	}
updater_exit:
	playlist_updater_id = 0;
	return FALSE;
}

/*****************************************************************************
 * Mafw signal handlers for the current playlist
 *****************************************************************************/

/**
 * Checks whether there is an ongoing playlist_get_items_md req, in the given range.
 */
static gboolean check_md_reqs(guint from, guint to)
{
	gint i = 0;
	
	if (!pl_get_mds)
		return FALSE;
	for (i = 0; pl_get_mds->len > i; i++)
	{
		struct pl_get_mds_data *pldat = g_ptr_array_index(pl_get_mds, i);
		
		if (!pldat)
			continue;
		if (pldat->from <= to && pldat->to >= from)
		{
			mafw_playlist_cancel_get_items_md(pldat->get_md_id);
			return TRUE;
		}
	}
	return FALSE;
}

void
on_mafw_playlist_contents_changed(MafwPlaylist *playlist, guint from,
				   guint nremoved, guint nreplaced)
{
	MafwPlaylist *current_playlist;
	GtkTreeIter iter;
	guint i;

	mtg_print_signal_gen (mafw_playlist_get_name (
				      MAFW_PLAYLIST (playlist)),
			      "Playlist::contents-changed",
			      "From: %d, Nremoved: %d, Nreplaced: %d\n",
			      from, nremoved, nreplaced);

	current_playlist = MAFW_PLAYLIST(get_current_playlist ());
        if (current_playlist == NULL)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						"No playlist selected");
		return;
	}

	/* Updating only visible playlist */
	if (mafw_proxy_playlist_get_id (MAFW_PROXY_PLAYLIST(playlist)) !=
	    mafw_proxy_playlist_get_id (MAFW_PROXY_PLAYLIST(current_playlist)))
	{
		g_debug("Non-visible playlist updated, doing nothing.\n");
		return;
	}

	if (nremoved && check_md_reqs(from, from + nremoved) &&
			playlist_updater_id == 0)
		playlist_updater_id = g_idle_add(playlist_updater, NULL);
	if (nreplaced && check_md_reqs(from, from + nreplaced) &&
			playlist_updater_id == 0)
		playlist_updater_id = g_idle_add(playlist_updater, NULL);

	/* Remove first */
	if (gtk_tree_model_iter_nth_child (playlist_model, &iter, NULL, from))
	{
		for (i = 0; i < nremoved; i++)
		{
			if (gtk_list_store_remove (
				    GTK_LIST_STORE (playlist_model), &iter)
			    == FALSE)
			{
				if (i != nremoved - 1)
					g_warning ("%d != %d -1, probably " \
					   "the playlist was cleared before " \
					   "populating all its contents ", i, nremoved);
				playing_index = -1;
				break;
			}

			if (playing_index >= from + i)
				playing_index--;
		}
	}

	if (nreplaced > 0)
	{
		/* Then insert */
		for (i = 0; i < nreplaced; i++)
		{
			/* Make it the $from + $i:th item in the list. */
			gtk_list_store_insert (GTK_LIST_STORE(playlist_model),
					       &iter, from + i);
			gtk_list_store_set (GTK_LIST_STORE (playlist_model), &iter,
					    COLUMN_INDEX, from + i,
					    -1);
	
			/* If the insertion happened before the currently playing
			   index, it must be incremented. */
			if (playing_index >= from + i)
				playing_index++;
	
		}
		if (playlist_updater_id == 0)
		{
			playlist_updater_id = g_idle_add(playlist_updater, NULL);
		}
	}
	/* Refresh the indicies */
	if (gtk_tree_model_iter_nth_child (playlist_model, &iter, NULL, from))
	{
		i = 0;
		do
		{
			gtk_list_store_set (GTK_LIST_STORE (playlist_model),
                                            &iter,
                                            COLUMN_INDEX, from + i,
                                            -1);
			i++;
		} while (gtk_tree_model_iter_next(playlist_model, &iter));
	}
}

void
on_mafw_playlist_item_moved (MafwPlaylist *playlist, guint from, guint to)
{
	GtkTreeIter it_from;
	GtkTreeIter it_to;

	mtg_print_signal_gen (mafw_playlist_get_name (
				      MAFW_PLAYLIST (playlist)),
			      "Playlist::item-moved",
			      "From: %u, To: %u\n",
			      from, to);

	if (MAFW_PLAYLIST(get_current_playlist()) != playlist)
		return;

	/* Get the affected items and check that they exist */
	g_assert(gtk_tree_model_iter_nth_child(playlist_model,
					       &it_from, NULL, from));
	g_assert(gtk_tree_model_iter_nth_child(playlist_model,
					       &it_to, NULL, to));
	/* Refresh the indicies */
	gtk_list_store_set(GTK_LIST_STORE (playlist_model), &it_from,
				    COLUMN_INDEX, to,
				    -1);
	gtk_list_store_set(GTK_LIST_STORE (playlist_model), &it_to,
				    COLUMN_INDEX, from,
				    -1);

	/* Move the affected item from index $from to index $to */
	if (from < to)
	{
		gtk_list_store_move_after(GTK_LIST_STORE(playlist_model),
					  &it_from, &it_to);
	}
	else
	{
		gtk_list_store_move_before(GTK_LIST_STORE(playlist_model),
					   &it_from, &it_to);
	}

	/* Position of the currently playing item might have changed too */
	if ((playing_index > from && playing_index > to) ||
	    (playing_index < from && playing_index < to))
	{
		/* The move occurred completely before or after the current
		   item; the moved item didn't skip over the current item
		   one way or the other. */
	}
	else if (playing_index >= to)
	{
		playing_index--;
	}
	else if (playing_index < to)
	{
		playing_index++;
	}
}

void
update_playing_item (int index)
{
	GtkTreeIter iter;
	MafwPlaylist *current_playlist;
	GError *error = NULL;
	guint pls_size;

	current_playlist = MAFW_PLAYLIST(get_current_playlist());
	if (current_playlist == NULL)
		return;

	pls_size = mafw_playlist_get_size(current_playlist, &error);
	if (error != NULL)
	{
		g_print ("Error on getting vis_idx %d: %s", index,
			 error->message);
		g_error_free(error);
		return;
	}

	if ((index + 1) > pls_size)
		return;


	if (playing_index >= 0)
	{
		if (gtk_tree_model_iter_nth_child(playlist_model,
						  &iter, NULL, playing_index)
		    == TRUE)
		{
			gtk_list_store_set (GTK_LIST_STORE (playlist_model),
					    &iter,
					    COLUMN_CURRENT, "",
					    -1);
		}
	}

	playing_index = index;

	if (playing_index >= 0)
	{
		if (gtk_tree_model_iter_nth_child(playlist_model,
						  &iter, NULL, playing_index)
		    == TRUE)
		{
			gtk_list_store_set (GTK_LIST_STORE (playlist_model),
					    &iter,
					    COLUMN_CURRENT,
					    GTK_STOCK_GO_FORWARD,
					    -1);
			update_current_idx = FALSE;
		}
		else
		{
			update_current_idx = TRUE;
		}
	}
}

/*****************************************************************************
 * GTK signal handlers
 *****************************************************************************/

/**
 * Handler for playlist tree view double clicks
 */
static void
on_playlist_treeview_row_activated (GtkWidget *widget, GtkTreePath *path,
				    GtkTreeViewColumn *column,
				    gpointer user_data)
{
	GtkTreeIter iter;
	guint index = 0;

	if (path == NULL)
		return;

	/* Get the index of the currently selected item */
	gtk_tree_model_get_iter (playlist_model, &iter, path);
	gtk_tree_model_get (playlist_model, &iter,
			    COLUMN_INDEX, &index,
			    -1);

	/* Stop playback and move to selected index */
	stop ();
	set_current_renderer_index (index);

	/* Force playback start (renderer must be in stopped state now)
	   We cannot use play() because it checks the cached state of the
	   renderer (which is playing) and it would try to pause playback */
	force_play ();
}

/**
 * Handler for adding the current item from source view to selected playlist
 *
 * TODO: Move this to source view!
 */
void
on_add_item_button_clicked (GtkWidget *widget)
{
	MafwPlaylist *playlist;
	GError *error = NULL;
	gint index;
	gchar* oid;

	playlist = MAFW_PLAYLIST (get_current_playlist ());
	if (playlist == NULL)
	{
		hildon_banner_show_information (widget,
						"qgn_list_smiley_angry",
						"No current playlist");
		return;
	}

	/* Get insertion index */
	index = get_current_playlist_index();
	if (index < 0)
		index = mafw_playlist_get_size(playlist, NULL);
	else
		index++;

	/* Don't try to add containers to playlist */
	if (selected_is_container() == TRUE)
		return;

	/* Get the currently selected object ID from source view */
	oid = get_selected_object_id();
	if (oid == NULL)
	{
		hildon_banner_show_information (widget,
						"qgn_list_smiley_angry",
						"Item has no object ID");
		return;
	}

	/* Perform the insertion */
	mafw_playlist_insert_item(playlist, index, oid, &error);
	g_free(oid);

	if (error != NULL)
	{
		hildon_banner_show_information (widget,
						"qgn_list_smiley_angry",
						error->message);
		g_error_free(error);
	}
	else
	{
		/* Select item below current in source view */
		source_treeview_select_next();
	}
}

/**
 * Handler for removing the selected item from playlist view
 */
void
on_remove_item_button_clicked (GtkWidget *widget)
{
	MafwPlaylist *playlist;
	gint index;
	GError *error = NULL;

	/* Get the currently selected playlist */
	playlist = MAFW_PLAYLIST (get_current_playlist ());
	if (playlist == NULL)
	{
		hildon_banner_show_information (widget,
						"qgn_list_smiley_angry",
						"No current playlist");
		return;
	}

	/* Get the index of the currently selected playlist item */
	index = get_current_playlist_index ();
	if (index < 0)
		return;

	/* Remove the selected index from playlist */
	mafw_playlist_remove_item (playlist, (guint) index, &error);
	if (error != NULL)
	{
		hildon_banner_show_information (widget,
						"qgn_list_smiley_angry",
						error->message);
		g_error_free(error);
	}
}

/**
 * Handler for raising the selected item in playlist view by one
 */
void
on_raise_item_button_clicked (GtkWidget *widget)
{
	MafwPlaylist *playlist;
	GError *error = NULL;
	gint index;

	/* Get the selected playlist */
	playlist = MAFW_PLAYLIST (get_current_playlist ());
	if (playlist == NULL)
	{
		hildon_banner_show_information (widget,
						"qgn_list_smiley_angry",
						"No current playlist");
		return;
	}

	/* Get the selected playlist item */
	index = get_current_playlist_index ();
	if (index <= 0 || index > (mafw_playlist_get_size(playlist, NULL) - 1))
		return; /* Don't allow raising an item beyond limits */

	/* Perform the move */
	mafw_playlist_move_item (playlist, (guint) index, (guint) index - 1,
				  &error);
	if (error != NULL)
	{
		hildon_banner_show_information (widget,
						"qgn_list_smiley_angry",
						error->message);
		g_error_free(error);
	}
}

/**
 * Handler for lowering the selected item in playlist view by one
 */
void
on_lower_item_button_clicked (GtkWidget *widget)
{
	MafwPlaylist *playlist;
	gint index;
	GError *error = NULL;

	/* Get the selected playlist */
	playlist = MAFW_PLAYLIST (get_current_playlist ());
	if (playlist == NULL)
	{
		hildon_banner_show_information (widget,
						"qgn_list_smiley_angry",
						"No current playlist");
		return;
	}

	/* Get the selected playlist item */
	index = get_current_playlist_index ();
	if (index < 0 || index >= (mafw_playlist_get_size(playlist, NULL) - 1))
		return; /* Don't allow lowering an item beyond limits */

	/* Perform the move */
	mafw_playlist_move_item (playlist, (guint) index, (guint) index + 1,
				  &error);
	if (error != NULL)
	{
		hildon_banner_show_information (widget,
						"qgn_list_smiley_angry",
						error->message);
		g_error_free(error);
	}
}

/**
 * Handler for clearing the currently selected playlist's contents
 */
void
on_clear_playlist_button_clicked (GtkWidget *widget)
{
	MafwPlaylist *playlist;
	GError* error = NULL;

	playlist = MAFW_PLAYLIST (get_current_playlist ());
	if (playlist == NULL)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						"No current playlist");
		return;
	}

	/* Perform the clearing */
	mafw_playlist_clear (playlist, &error);
	if (error != NULL)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						error->message);
		g_error_free(error);
	}
}

/*****************************************************************************
 * Hard key handlers
 *****************************************************************************/

gboolean
playlist_has_focus (void)
{
	return GTK_WIDGET_HAS_FOCUS(playlist_treeview);
}

void
playlist_get_focus (void)
{
	gtk_widget_grab_focus(playlist_treeview);
}

static gboolean
is_up_possible (void)
{
	GtkTreeSelection *cur_selection;
	GtkTreeModel *model;
	GtkTreePath *root, *selected_p;
	GtkTreeIter iter;
	gboolean retval = TRUE;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_treeview));
	cur_selection = gtk_tree_view_get_selection(
		GTK_TREE_VIEW(playlist_treeview));
	if (!gtk_tree_selection_get_selected(cur_selection, &model, &iter))
	{// let the default handler select a tv element
		return TRUE;
	}

	root = gtk_tree_path_new_first();
	selected_p = gtk_tree_model_get_path(model, &iter);

	if (!gtk_tree_path_compare(root, selected_p))
		retval = FALSE;

	gtk_tree_path_free(root);
	gtk_tree_path_free(selected_p);
	return retval;
}

static gboolean
is_down_possible (void)
{
	GtkTreeSelection *cur_selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_treeview));
	cur_selection = gtk_tree_view_get_selection(
					GTK_TREE_VIEW(playlist_treeview));
	if (!gtk_tree_selection_get_selected(cur_selection, &model, &iter))
	{// let the default handler select a tv element
		return TRUE;
	}
	return gtk_tree_model_iter_next(model, &iter);
}

static gboolean
play_selected (void)
{
	GtkTreeSelection *cur_selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	gint *indicies;
	gboolean retval = TRUE;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_treeview));
	cur_selection = gtk_tree_view_get_selection(
					GTK_TREE_VIEW(playlist_treeview));
	if (!gtk_tree_selection_get_selected(cur_selection, &model, &iter))
	{
		return FALSE;
	}
	path = gtk_tree_model_get_path(model, &iter);
	indicies = gtk_tree_path_get_indices(path);

	if (playing_index != *indicies)
		retval = FALSE;
	else
		play();

	gtk_tree_path_free(path);

	return retval;
}

static gboolean
on_playlist_key_pressed (GtkWidget* widget, GdkEventKey* event,
			 gpointer user_data)
{
	switch (event->keyval)
	{
		case HILDON_HARDKEY_UP:
			if (!is_up_possible())
				return TRUE;
			break;
		case HILDON_HARDKEY_DOWN:
			if (!is_down_possible())
				return TRUE;
			break;
		case HILDON_HARDKEY_SELECT:
			return play_selected();
		case HILDON_HARDKEY_ESC:
			on_remove_item_button_clicked(NULL);
			return TRUE;
	}

	return FALSE;
}

/*****************************************************************************
 * Current selection helpers
 *****************************************************************************/

gchar*
playlist_get_selected_oid (void)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *oid = NULL;

	/* Get the tree view's selection object */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (
							 playlist_treeview));
	g_assert (selection != NULL);

	/* Check if there is a current selection */
	if (gtk_tree_selection_get_selected (selection, &model, &iter) == TRUE)
	{
		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
				   COLUMN_OBJECTID, &oid,
				   -1);
	}

	return oid;
}

gint
get_current_playlist_index (void)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint index = -1;

	/* Get the tree view's selection object */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (
							 playlist_treeview));
	g_assert (selection != NULL);

	/* Check if there is a current selection */
	if (gtk_tree_selection_get_selected (selection, &model, &iter) == TRUE)
	{
		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
				   COLUMN_INDEX, &index,
				   -1);
	}

	return index;
}

/*****************************************************************************
 * Initialization
 *****************************************************************************/

void
setup_playlist_treeview (GtkBuilder *builder)
{
	GtkCellRenderer *text_renderer;
	GtkCellRenderer *pxb_renderer;
	GtkTreeViewColumn *column;

	playlist_treeview =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "playlist-treeview"));;
	g_assert (playlist_treeview != NULL);

	g_signal_connect (playlist_treeview, "row-activated",
			  G_CALLBACK(on_playlist_treeview_row_activated), NULL);
	g_signal_connect (playlist_treeview,
                          "key-press-event",
                          G_CALLBACK (on_playlist_key_pressed),
                          NULL);

	/* Create a list store model for playlist contents */
	playlist_model = GTK_TREE_MODEL(
		gtk_list_store_new(COLUMNS,
				   G_TYPE_UINT,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING));

	gtk_tree_view_set_model (GTK_TREE_VIEW (playlist_treeview),
				 playlist_model);

	/* Cell renderers */
	text_renderer = gtk_cell_renderer_text_new ();
	pxb_renderer = gtk_cell_renderer_pixbuf_new ();

	/* Visual index column */
	column = gtk_tree_view_column_new_with_attributes (
		"Index",
		text_renderer,
		"text",
		COLUMN_INDEX,
		NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (playlist_treeview), column);

	/* Current item pixbuf */
	column = gtk_tree_view_column_new_with_attributes (
		"Current",
		pxb_renderer,
		"stock-id",
		COLUMN_CURRENT,
		NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (playlist_treeview), column);

	/* Object ID column */
	column = gtk_tree_view_column_new_with_attributes (
		"Title",
		text_renderer,
		"text",
		COLUMN_TITLE,
		NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (playlist_treeview), column);
}
