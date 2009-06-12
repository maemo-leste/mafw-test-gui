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
#include "playlist-treeview.h"
#include <libmafw/mafw.h>
#include <libmafw-shared/mafw-playlist-manager.h>
#include <libmafw-shared/mafw-proxy-playlist.h>

#include "playlist-controls.h"
#include "renderer-combo.h"
#include "main.h"
#include "gui.h"

extern GtkWidget *main_window;

static GtkTreeModel *playlist_name_model;
static GtkWidget *playlist_name_combobox;
static GtkWidget *repeat_toggle;
static GtkWidget *shuffle_toggle;
static GtkWidget *lower_item;
static GtkWidget *raise_item;

static gboolean select_on_creation = TRUE;

enum {
	PLAYLIST_COMBO_COLUMN_NAME, /* Playlist name displayed in the combo */
	PLAYLIST_COMBO_COLUMN_ID,   /* Proxy PLS id (hidden) */
	PLAYLIST_COMBO_COLUMNS
};

/* Prototypes for gtk-builder signal connections */
void on_shuffle_button_toggled(GtkWidget *widget);
void on_repeat_button_toggled(GtkWidget *widget);
void on_playlist_name_combobox_changed(GtkWidget *widget);
void on_add_playlist_button_clicked(GtkWidget *widget);
void on_remove_playlist_button_clicked(GtkWidget *widget);

static void append_playlist_to_combo (MafwProxyPlaylist *playlist,
				      gpointer user_data);
static gchar* get_name_dialog (const gchar* text, guint pls_id);

static void on_mafw_playlist_notify(GObject *gobject, GParamSpec *arg1,
				     gpointer user_data);

/*****************************************************************************
 * Repeat
 *****************************************************************************/

/**
 * GTK signal handler for repeat button presses
 */
void
on_repeat_button_toggled (GtkWidget *widget)
{
	MafwPlaylist *playlist = MAFW_PLAYLIST(get_current_playlist());
	if (playlist == NULL)
		return;

	mafw_playlist_set_repeat(playlist,
				 gtk_toggle_button_get_active(
					 GTK_TOGGLE_BUTTON(widget)));
}

/**
 * Manually toggle the repeat button state
 */
void
toggle_repeat (gboolean new_state)
{
	/* Temporarily block the button's own signal sending so that we
	   don't end up in an endless loop */
	g_signal_handlers_block_by_func (repeat_toggle,
                                         on_repeat_button_toggled,
                                         NULL);

	/* Set the button's state */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(repeat_toggle),
				      new_state);

	/* Re-enable the button's signal sending */
	g_signal_handlers_unblock_by_func (repeat_toggle,
					   on_repeat_button_toggled,
					   NULL);
}

/*****************************************************************************
 * Shuffle
 *****************************************************************************/

/**
 * GTK signal handler for shuffle button presses
 */
void
on_shuffle_button_toggled (GtkWidget *widget)
{
	MafwPlaylist *playlist = MAFW_PLAYLIST(get_current_playlist());
	if (playlist == NULL)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						"No current playlist");
		return;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) == TRUE)
		mafw_playlist_shuffle(playlist, NULL);
	else
		mafw_playlist_unshuffle(playlist, NULL);
}

/**
 * Manually toggle the shuffle button state
 */
void
toggle_shuffle (gboolean new_state)
{
	/* Temporarily block the button's own signal sending so that we
	   don't end up in an endless loop */
	g_signal_handlers_block_by_func (shuffle_toggle,
                                         on_shuffle_button_toggled,
                                         NULL);

	/* Set the button's state */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_toggle),
				     new_state);

	/* Re-enable the button's signal sending */
	g_signal_handlers_unblock_by_func (shuffle_toggle,
					   on_shuffle_button_toggled,
					   NULL);
}

/*****************************************************************************
 * Playlists combo
 *****************************************************************************/

/**
 * GTK signal handler for playlist combo item selections
 */
void
on_playlist_name_combobox_changed (GtkWidget *widget)
{
	MafwPlaylist *playlist;

	playlist = MAFW_PLAYLIST(get_current_playlist ());
        if (playlist == NULL)
	{
		clear_current_playlist_treeview ();
		return;
        }

	toggle_shuffle (mafw_playlist_is_shuffled (playlist));
	toggle_repeat (mafw_playlist_get_repeat (playlist));

	display_playlist_contents (playlist);
	assign_playlist_to_current_renderer (playlist);
}

void
on_add_playlist_button_clicked (GtkWidget *widget)
{
	gchar* name;

	name = get_name_dialog("New playlist", ~0);
	if (name != NULL)
	{
		MafwPlaylistManager *manager;
		MafwProxyPlaylist *playlist;
		GError *error = NULL;

		manager = mafw_playlist_manager_get ();
		g_assert (manager != NULL);

		playlist = mafw_playlist_manager_create_playlist (manager,
								  name,
								  &error);

		if (error != NULL)
		{
			hildon_banner_show_information (main_window,
							"qgn_list_gene_invalid",
							error->message);
			g_error_free(error);
		}
		else if (playlist != NULL)
		{
			/*check if playlist already exists, select the
                         * same & show info*/
			GtkTreeIter iter;
			if (find_playlist_iter(playlist, &iter)) {
				gtk_combo_box_set_active_iter(
				GTK_COMBO_BOX(playlist_name_combobox), &iter);
				hildon_banner_show_information (
					main_window,
					"qgn_note_infoprint",
					"Playlist already exists");
			}
		}
		else
		{
			hildon_banner_show_information (
				main_window,
				"qgn_list_gene_invalid",
				"Unable to create new playlist");
		}

		g_free(name);
	}
}

void
on_remove_playlist_button_clicked (GtkWidget *widget)
{
	GtkWidget *dialog;
	GtkTreeIter iter;
	gchar* name = NULL;
	guint id = 0;

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(playlist_name_combobox),
					  &iter) == FALSE) {
		return;
	}

	gtk_tree_model_get (playlist_name_model, &iter,
			    PLAYLIST_COMBO_COLUMN_NAME, &name,
			    PLAYLIST_COMBO_COLUMN_ID, &id,
			    -1);

	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO,
					 "Delete playlist: '%s' (%u)?",
					 name, id);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
		MafwPlaylistManager *manager;
		MafwProxyPlaylist *playlist;
		GError *error = NULL;

		manager = mafw_playlist_manager_get ();
		g_assert (manager != NULL);

		playlist = mafw_playlist_manager_get_playlist(manager, id,
							      &error);
		if (error != NULL) {
			g_print("Cannot find playlist proxy with ID:%u: %s",
				id, error->message);
			g_error_free(error);
			return;
		}

		/* Unassign the playlist before destroying it. Delete a playlist
		   that is assigned to some renderer is not allowed. */
		assign_playlist_to_current_renderer(NULL);
		mafw_playlist_manager_destroy_playlist(manager, playlist,
						       &error);
	}

	g_free (name);

	gtk_widget_destroy (dialog);
}

void
on_rename_playlist_button_clicked (GtkWidget *widget)
{
	MafwProxyPlaylist *playlist;
	guint old_id;
	const gchar* old_name;
	gchar* new_name;

	playlist = get_current_playlist();
	if (playlist == NULL)
		return;

	old_name = mafw_playlist_get_name (MAFW_PLAYLIST (playlist));
	old_id = mafw_proxy_playlist_get_id (playlist);
	new_name = get_name_dialog (old_name, old_id);
	if (new_name != NULL)
	{
		mafw_playlist_set_name (MAFW_PLAYLIST (playlist), new_name);

		old_name = mafw_playlist_get_name (MAFW_PLAYLIST (playlist));
		if (strcmp(old_name, new_name) != 0)
		{
			hildon_banner_show_information(
				main_window,
				"qgn_list_gene_invalid",
				"Unable to rename playlist");
		}

		g_free(new_name);
	}
}
void
on_save_playlist_button_clicked (GtkWidget *widget)
{
	MafwPlaylistManager *manager;
        MafwProxyPlaylist *playlist, *newpl;
        guint old_id;
        gchar* old_name;
        gchar* new_name;
        GError *error = NULL;

	manager = mafw_playlist_manager_get();
        playlist = get_current_playlist();
        if (playlist == NULL)
                return;

        old_name = mafw_playlist_get_name (MAFW_PLAYLIST (playlist));
        old_id = mafw_proxy_playlist_get_id (playlist);
        new_name = get_name_dialog (old_name, old_id);
        if (new_name != NULL)
        {
                if((newpl = mafw_playlist_manager_dup_playlist(manager,
                                                               playlist,
                                                               new_name,
                                                               &error)) == NULL)
                {
                         hildon_banner_show_information (
                                 main_window,
                                 "qgn_list_gene_invalid",
                                 error?error->message:"Unable to duplicate "
                                 "playlist");

                }
                if(error)
                {
                         hildon_banner_show_information (
                                 main_window,
                                 "qgn_list_gene_invalid",
                                 error->message);
                         g_error_free(error);
                }

                g_free(new_name);
		g_free(old_name);
		if (newpl) {
                        /* This time, prevent selecting the new playlist when
                           playlist is added with 'playlist-created' signal */
                        select_on_creation = FALSE;
			g_object_unref(newpl);
                }
        }
}

void on_renderer_assigned_playlist_changed(MafwPlaylist *playlist)
{
	MafwPlaylist *current_playlist;
	gchar *current_pls_name = NULL;
	gchar *pls_name = NULL;
	GtkTreeIter iter;

	if (!playlist) {
		return;
	}

	/* Check if the recently assigned playlist is already the current one.
	 If so, do nothing. */
	current_playlist = MAFW_PLAYLIST(get_current_playlist());
	if (current_playlist) {
		current_pls_name = mafw_playlist_get_name(current_playlist);
		pls_name = mafw_playlist_get_name(playlist);
		if (g_strcmp0(current_pls_name, pls_name) == 0) {
			g_free(current_pls_name);
			g_free(pls_name);
			return;
		}
	}

	if ((find_playlist_iter(MAFW_PROXY_PLAYLIST(playlist), &iter) == TRUE)) {
		gtk_combo_box_set_active_iter(
			GTK_COMBO_BOX(playlist_name_combobox), &iter);
	}

	g_free(current_pls_name);
	g_free(pls_name);
}

/*****************************************************************************
 * Renaming dialog
 *****************************************************************************/

static gchar*
get_name_dialog(const gchar* text, guint pls_id)
{
	GtkWidget *dialog;
	GtkWidget *entry;
	gchar *name;

	dialog = gtk_dialog_new_with_buttons("Playlist name",
					     NULL,
					     GTK_DIALOG_MODAL,
					     GTK_STOCK_OK,
					     GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL,
					     GTK_RESPONSE_REJECT,
					     NULL);

	entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(gtk_bin_get_child(GTK_BIN(dialog))),
			  entry);
	gtk_entry_set_text(GTK_ENTRY(entry), text);

show_dlg:
	gtk_widget_show_all(dialog);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		name = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
		name = g_strstrip(name);
		if (name == NULL || strlen(name) == 0)
		{
			hildon_banner_show_information(
				main_window,
				"qgn_list_gene_invalid",
				"Empty names are not allowed");
			g_free(name);
			goto show_dlg;
		}
	}
	else
	{
		name = NULL;
	}

	gtk_widget_destroy(dialog);

	return name;
}

gboolean
select_playlist(MafwProxyPlaylist *playlist)
{
	GtkTreeIter iter;
	if (find_playlist_iter(playlist, &iter) == TRUE) {
		/* Select the playlist */
		gtk_combo_box_set_active_iter(
			GTK_COMBO_BOX(playlist_name_combobox), &iter);
		return TRUE;
	} else {
		/* Unable to select a non-existing or invalid playlist */
		gtk_combo_box_set_active(
			GTK_COMBO_BOX(playlist_name_combobox), -1);
		return FALSE;
	}
}

MafwProxyPlaylist*
get_current_playlist ()
{
	MafwPlaylistManager *manager;
	MafwProxyPlaylist *playlist;
	guint id;
	GError *error = NULL;

	id = get_current_playlist_id();

	manager = mafw_playlist_manager_get ();
	g_assert (manager != NULL);

	playlist = mafw_playlist_manager_get_playlist(manager, id, &error);
	if (error != NULL) {
		g_print("Cannot find playlist proxy with ID:%u: %s",
			id, error->message);
		g_error_free(error);
		return NULL;
	} else {
		return playlist;
	}
}

guint
get_current_playlist_id (void)
{
        GtkTreeIter   iter;
        guint         id;

        if (!gtk_combo_box_get_active_iter (
				GTK_COMBO_BOX(playlist_name_combobox), &iter)) {
                return 0;
        }

        gtk_tree_model_get (playlist_name_model,
                            &iter,
                            PLAYLIST_COMBO_COLUMN_ID, &id,
                            -1);

        return id;
}

gboolean
find_playlist_iter(MafwProxyPlaylist *playlist, GtkTreeIter *iter)
{
	guint id;

	g_return_val_if_fail(iter != NULL, FALSE);

	/* If the combo is empty, there is nothing to do */
	if (gtk_tree_model_get_iter_first (playlist_name_model, iter) == FALSE)
		return FALSE;

	do {
		gtk_tree_model_get (playlist_name_model, iter,
				    PLAYLIST_COMBO_COLUMN_ID, &id,
				    -1);

		if (mafw_proxy_playlist_get_id (playlist) == id) {
			return TRUE;
		}

	} while (gtk_tree_model_iter_next(playlist_name_model, iter) == TRUE);

	return FALSE;
}

static void
append_playlist_to_combo(MafwProxyPlaylist *playlist, gpointer user_data)
{
	GtkTreeIter iter;
	gchar* name;
	guint id;
	gboolean select = (gboolean) GPOINTER_TO_INT(user_data);

	g_assert (playlist != NULL);
	g_assert (MAFW_IS_PROXY_PLAYLIST(playlist));

	name = mafw_playlist_get_name (MAFW_PLAYLIST(playlist));
	id = mafw_proxy_playlist_get_id (playlist);

	gtk_list_store_append (GTK_LIST_STORE (playlist_name_model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (playlist_name_model), &iter,
			    PLAYLIST_COMBO_COLUMN_NAME, name,
			    PLAYLIST_COMBO_COLUMN_ID, id,
			    -1);

	if (select == TRUE)
	{
		gtk_combo_box_set_active_iter (
			GTK_COMBO_BOX (playlist_name_combobox), &iter);
	}

	/* Start listening to playlist property notifications */
	g_signal_connect (playlist, "notify",
			  (GCallback) on_mafw_playlist_notify, NULL);

	/* Connect to the signals about the changes too */
	g_signal_connect (playlist, "contents-changed",
			  (GCallback) on_mafw_playlist_contents_changed, NULL);
	g_signal_connect (playlist, "item-moved",
			  (GCallback) on_mafw_playlist_item_moved, NULL);
	
	g_free(name);

}

/*****************************************************************************
 * Import
 *****************************************************************************/

static void import_cb(MafwPlaylistManager *self,
					  guint import_id,
					  MafwProxyPlaylist *playlist,
					  gpointer user_data,
					  const GError *error)
{
	#ifndef G_DEBUG_DISABLE
	g_print ("SIGNAL import-cb with import-id: %d ", import_id);
	if (playlist)
	{
		gchar *name = mafw_playlist_get_name(MAFW_PLAYLIST(playlist));
		fprintf (stderr, "with new Playlist: (%d) %s",
				mafw_proxy_playlist_get_id(playlist),
				name);
		g_free(name);
	}
	else
		fprintf (stderr, "with error: %s", error->message);
	#endif
	if (error)
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						error->message);
}

void playlist_import(const gchar *oid)
{
	MafwPlaylistManager *manager;
	guint import_id;
	GError *err = NULL;

	manager = mafw_playlist_manager_get ();
	g_assert (manager != NULL);	

	import_id = mafw_playlist_manager_import(manager, oid, NULL, import_cb,
						 NULL, &err);

	if (import_id == MAFW_PLAYLIST_MANAGER_INVALID_IMPORT_ID)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						err->message);
	}
}

/*****************************************************************************
 * Mafw signal handlers
 *****************************************************************************/

/**
 * Handler for Mafw playlist name changes
 */
static void
on_mafw_playlist_name_changed (MafwProxyPlaylist *playlist)
{
	GtkTreeIter iter;
	const gchar *name;

	g_return_if_fail(playlist != NULL);

	if (find_playlist_iter(playlist, &iter) == TRUE)
	{
		name = mafw_playlist_get_name (MAFW_PLAYLIST(playlist));
		gtk_list_store_set (GTK_LIST_STORE(playlist_name_model), &iter,
				    PLAYLIST_COMBO_COLUMN_NAME, name,
				    -1);
	}
}

/**
 * Handler for Mafw playlist property changes
 */
static void
on_mafw_playlist_notify(GObject *gobject, GParamSpec *arg1, gpointer user_data)
{
	mtg_print_signal_gen (mafw_playlist_get_name(MAFW_PLAYLIST (gobject)),
			      "Playlist::notify", "Property: %s\n", arg1->name);

	if (strcmp(arg1->name, "is-shuffled") == 0)
	{
		if (MAFW_PLAYLIST (gobject) ==
		    MAFW_PLAYLIST (get_current_playlist ()))
		{
			MafwPlaylist *playlist;

			playlist = MAFW_PLAYLIST (gobject);
			g_assert(playlist != NULL);

			/* Toggle shuffle button state */
			toggle_shuffle (mafw_playlist_is_shuffled(playlist));

			/* TODO: Update current playing index */
		}
	}
	else if (strcmp(arg1->name, "repeat") == 0)
	{
		if (MAFW_PLAYLIST (gobject) ==
		    MAFW_PLAYLIST( get_current_playlist ()))
		{
			MafwPlaylist *playlist;

			playlist = MAFW_PLAYLIST (gobject);
			g_assert(playlist != NULL);

			/* Toggle repeat button state */
			toggle_repeat (mafw_playlist_get_repeat(playlist));
		}
	}
	else if (strcmp(arg1->name, "name") == 0)
	{
		/* Update a changed playlist name */
		on_mafw_playlist_name_changed (get_current_playlist ());
	}
	else
	{
		g_warning ("Ignoring property: %s", arg1->name);
	}
}

/**
 * Handler for Mafw playlist creations (to get them into the combo)
 */
static void
on_mafw_playlist_created (MafwPlaylistManager* manager,
			   MafwProxyPlaylist *playlist, gpointer user_data)
{
	g_return_if_fail(manager != NULL);
	g_return_if_fail(playlist != NULL);

	/* Append and select */
	append_playlist_to_combo (playlist,
                                  GINT_TO_POINTER(select_on_creation));

        /* Reset select */
        if (select_on_creation == FALSE) {
                select_on_creation = TRUE;
        }
}

/**
 * Handler for playlist destruction failures
 */
static void
on_mafw_playlist_destruction_failed(MafwPlaylistManager* manager,
				     MafwProxyPlaylist *playlist,
				     gpointer user_data)
{
	hildon_banner_show_information (main_window,
					"qgn_list_gene_invalid",
					"The playlist cannot be destroyed.");
}

/**
 * Handler for playlist destructions (to remove them from the combo)
 */
static void
on_mafw_playlist_destroyed (MafwPlaylistManager* manager,
			     MafwProxyPlaylist *playlist, gpointer user_data)
{
	GtkTreeIter iter;

	g_return_if_fail(manager != NULL);
	g_return_if_fail(playlist != NULL);

	if (find_playlist_iter(playlist, &iter) == TRUE)
	{

		GtkTreeIter tmp_iter = iter;

		/* Set the next active iter before removing the iter from the
		   combo. */
		if (gtk_tree_model_iter_next(playlist_name_model,
					     &tmp_iter) == TRUE) {
			/* Set the next iter as the active one.*/
			gtk_combo_box_set_active_iter(
				GTK_COMBO_BOX(playlist_name_combobox),
				&tmp_iter);
		} else {
			/* The iter is the last one in the combo so, set the
			   first iter as the active one. */
			gtk_tree_model_get_iter_first(playlist_name_model,
                                                      &tmp_iter);
			gtk_combo_box_set_active_iter(
				GTK_COMBO_BOX(playlist_name_combobox),
                                &tmp_iter);
		}

		/* Remove the iter. */
		gtk_list_store_remove(GTK_LIST_STORE (playlist_name_model),
				      &iter);

	}
}

/*****************************************************************************
 * Initialization
 *****************************************************************************/

static void
setup_playlist_name_combo (void)
{
	MafwPlaylistManager *manager;
	GPtrArray *playlists;
	GError *error = NULL;
	GtkCellRenderer *renderer;

	/* Create a list store model */
	playlist_name_model = GTK_TREE_MODEL(
		gtk_list_store_new (PLAYLIST_COMBO_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_UINT));

	gtk_combo_box_set_model (GTK_COMBO_BOX(playlist_name_combobox),
				 playlist_name_model);

	/* Create a text cell renderer to display text in the combo from
	   the model's _NAME column. */
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(playlist_name_combobox),
				   renderer, TRUE);

	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(playlist_name_combobox),
				       renderer,
				       "text",
				       PLAYLIST_COMBO_COLUMN_NAME,
				       NULL);

	/* Get a handle to the playlist manager singleton */
	manager = mafw_playlist_manager_get ();
	g_assert (manager != NULL);

	/* Listen to playlist manager's playlist creation/destruction signals */
	g_signal_connect(manager, "playlist-created",
			 G_CALLBACK(on_mafw_playlist_created),
                         &select_on_creation);
	g_signal_connect(manager, "playlist-destroyed",
			 G_CALLBACK(on_mafw_playlist_destroyed), NULL);

	g_signal_connect(manager, "playlist-destruction-failed",
			 G_CALLBACK(on_mafw_playlist_destruction_failed),
			 NULL);

	/* Get all existing playlists and append them to the playlists model */
	playlists = mafw_playlist_manager_get_playlists (manager, &error);
	if (error != NULL) {
		g_print ("Unable to get playlists: %s\n", error->message);
		g_error_free (error);
		return;
	}

	/* Append playlists but don't select anything */
	g_ptr_array_foreach (playlists, (GFunc) append_playlist_to_combo,
			     GINT_TO_POINTER(FALSE));
}

void
setup_playlist_controls (GtkBuilder *builder)
{
	GtkWidget* image;
	GtkWidget* widget;

	/* Add playlist */
	widget = GTK_WIDGET(gtk_builder_get_object(builder,
                                                   "add-playlist-button"));
        g_assert (widget != NULL);

	gtk_button_set_label (GTK_BUTTON(widget), NULL);
	image = gtk_image_new_from_icon_name ("qgn_widg_mplayer_add",
					      HILDON_ICON_SIZE_SMALL);
	gtk_button_set_image (GTK_BUTTON(widget), image);

	/* Remove playlist */
	widget = GTK_WIDGET(gtk_builder_get_object(builder,
                                                   "remove-playlist-button"));
        g_assert (widget != NULL);

	gtk_button_set_label (GTK_BUTTON(widget), NULL);
	image = gtk_image_new_from_icon_name ("qgn_widg_mplayer_remove",
					      HILDON_ICON_SIZE_SMALL);
	gtk_button_set_image (GTK_BUTTON(widget), image);

	/* Add item */
	widget = GTK_WIDGET(gtk_builder_get_object(builder,
                                                   "add-item-button"));
        g_assert (widget != NULL);

	gtk_button_set_label (GTK_BUTTON(widget), NULL);
	image = gtk_image_new_from_icon_name ("qgn_list_gene_forward",
					      HILDON_ICON_SIZE_SMALL);
	gtk_button_set_image (GTK_BUTTON(widget), image);

	/* Remove item */
	widget = GTK_WIDGET(gtk_builder_get_object(builder,
                                                   "remove-item-button"));
        g_assert (widget != NULL);

	gtk_button_set_label (GTK_BUTTON(widget), NULL);
	image = gtk_image_new_from_icon_name ("qgn_list_gene_back",
					      HILDON_ICON_SIZE_SMALL);
	gtk_button_set_image (GTK_BUTTON(widget), image);

	/* Remove item */
	widget = GTK_WIDGET(gtk_builder_get_object(builder,
                                                    "clear-playlist-button"));
        g_assert (widget != NULL);

	gtk_button_set_label (GTK_BUTTON(widget), NULL);
	image = gtk_image_new_from_icon_name ("qgn_toolb_gene_deletebutton",
					      HILDON_ICON_SIZE_SMALL);
	gtk_button_set_image (GTK_BUTTON(widget), image);

	/* Raise */
	raise_item = GTK_WIDGET(gtk_builder_get_object(builder,
                                                       "raise-item-button"));
        g_assert (raise_item != NULL);

	gtk_button_set_label (GTK_BUTTON (raise_item), NULL);
	image = gtk_image_new_from_icon_name ("qgn_indi_arrow_up",
					      HILDON_ICON_SIZE_SMALL);
	gtk_button_set_image (GTK_BUTTON (raise_item), image);

	/* Lower */
	lower_item = GTK_WIDGET(gtk_builder_get_object(builder,
                                                       "lower-item-button"));
        g_assert (lower_item != NULL);

	gtk_button_set_label (GTK_BUTTON (lower_item), NULL);
	image = gtk_image_new_from_icon_name ("qgn_indi_arrow_down",
					      HILDON_ICON_SIZE_SMALL);
	gtk_button_set_image (GTK_BUTTON (lower_item), image);

	/* Repeat */
	repeat_toggle = GTK_WIDGET(gtk_builder_get_object(builder,
                                                          "repeat-button"));
        g_assert (repeat_toggle != NULL);

	gtk_button_set_label (GTK_BUTTON(repeat_toggle), NULL);
	image = gtk_image_new_from_icon_name ("qgn_medi_random_off_repeat_on",
					      HILDON_ICON_SIZE_SMALL);
	gtk_button_set_image (GTK_BUTTON(repeat_toggle), image);

	/* Shuffle */
	shuffle_toggle = GTK_WIDGET(gtk_builder_get_object(builder,
                                                           "shuffle-button"));
        g_assert (shuffle_toggle != NULL);

	gtk_button_set_label (GTK_BUTTON (shuffle_toggle), NULL);
	image = gtk_image_new_from_icon_name ("qgn_medi_random_on_repeat_off",
					      HILDON_ICON_SIZE_SMALL);
	gtk_button_set_image (GTK_BUTTON (shuffle_toggle), image);

	/* Playlist combo */
        playlist_name_combobox =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "playlist-name-combobox"));
        g_assert (playlist_name_combobox != NULL);

	setup_playlist_name_combo ();
}
