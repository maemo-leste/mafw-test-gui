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
#include <gconf/gconf-client.h>

#include "gui.h"
#include "source-treeview.h"
#include "metadata-view.h"
#include "playlist-controls.h"
#include "playlist-treeview.h"
#include "renderer-combo.h"
#include "renderer-controls.h"
#include "playlist-controls.h"
#include "main.h"

#define GTK_BUILDER_FILE DATA_DIR "/mafw-test-gui.ui"
#define MTG_GC_KEY_METADATA_API "/apps/mafw/test-gui/use_metadata_api"

static GtkBuilder *builder;

GtkWidget *main_window;
static gboolean mwin_fullscreen;

/* Prototypes for gtk-builder signal connections */
gboolean on_delete_event(GtkWidget *widget,
                         GdkEvent *event,
                         gpointer user_data);
gboolean on_delete_uri_dialog_event (GtkWidget *widget,
		                     GdkEvent  *event,
		                     gpointer   user_data);
void on_playling_order_close_clicked(GtkButton *button, gpointer   user_data);

gboolean
on_delete_event (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   user_data)
{
        application_exit ();

        return TRUE;
}

gboolean
on_delete_uri_dialog_event (GtkWidget *widget,
		            GdkEvent  *event,
		            gpointer   user_data)
{
        gtk_widget_hide (widget);
        return TRUE;
}

/**
 * Returns the selected object-id, if it is not a container. The
 * selected renderer is returned too.
 **/
static gchar *get_renderer_and_non_container_oid(MafwRenderer **renderer)
{
	if (selected_is_container() == FALSE)
	{
		gchar* oid;

		oid = get_selected_object_id ();
		if (oid == NULL)
		{
			hildon_banner_show_information (NULL,
					"qgn_list_smiley_angry",
					"Current item has no object ID");
			return NULL;
		}

		*renderer = get_selected_renderer ();
		if (*renderer == NULL)
		{
			hildon_banner_show_information (NULL,
					"qgn_list_smiley_angry",
					"No renderer selected");
			g_free (oid);
			return NULL;
		}

		return oid;
	}
	return NULL;
}

static void
on_play_selected_object_activate (GtkMenuItem* item, gpointer user_data)
{
	gchar *oid;
	MafwRenderer *renderer = NULL;

	oid = get_renderer_and_non_container_oid(&renderer);
	if (oid)
	{
		set_current_oid(oid);
		mafw_renderer_play_object (renderer, oid, NULL, NULL);
		g_free(oid);
	}
}

/**
 * Calls get_metadata for the given object-id, for all-keys
 **/
static void get_metadata_from_oid(gchar *oid, MafwSourceMetadataResultCb cb,
				  gpointer udata)
{
	MafwSource *source;
	gchar *source_uuid;

	if (!mafw_source_split_objectid(oid, &source_uuid, NULL))
	{
		g_critical("Wrong object-id format: %s", oid);
		return;
	}

	g_assert(source_uuid);

	source = MAFW_SOURCE(mafw_registry_get_extension_by_uuid(
				MAFW_REGISTRY(mafw_registry_get_instance()),
				source_uuid));

	if (!source)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						"Source not available");
	}
	else
	{
		mafw_source_get_metadata(source, oid,
				MAFW_SOURCE_ALL_KEYS,
				cb,
				udata);
	}
	g_free(source_uuid);
}

/**
 * Plays an URI
 **/
static void mdata_res_cb_play_uri (MafwSource *self, const gchar *object_id,
			GHashTable *metadata, MafwRenderer *selected_renderer,
			const GError *error)
{
	GValue* value;
	const gchar* uri;

	value = mafw_metadata_first (metadata, MAFW_METADATA_KEY_URI);
	uri = g_value_get_string (value);

	g_assert(uri);
	mafw_renderer_play_uri(selected_renderer, uri, NULL, NULL);
}

static void
on_play_selected_uri_activate (GtkMenuItem* item, gpointer user_data)
{
	gchar *oid;
	MafwRenderer *renderer = NULL;

	oid = get_renderer_and_non_container_oid(&renderer);
	if (oid)
	{
		get_metadata_from_oid(oid,
			(MafwSourceMetadataResultCb)mdata_res_cb_play_uri,
			renderer);
		g_free(oid);
	}
}

static void insert_key_to_metadata_view (gpointer key, gpointer value,
					 gpointer user_data)
{
	GtkListStore* store;
	GtkTreeIter iter;
	gchar* str_value;

	store = GTK_LIST_STORE (user_data);
	g_assert (store != NULL);

	/* The actual contents type might be anything, so convert the value
	   always into a string that can be easily set to the model. */

	if (G_VALUE_HOLDS((GValue *)value, G_TYPE_STRING)) {
		str_value = g_strdup ((gchar *) g_value_get_string (value));
	} else {
		str_value = g_strdup_value_contents ((GValue*) value);
	}

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    0, (const gchar*) key, /* Key */
			    1, str_value,          /* Value */
			    -1);

	g_free (str_value);
}

static void
all_metadata_cb (MafwSource *self, const gchar *object_id, GHashTable *metadata,
		 gpointer user_data, const GError *error)
{
	if (metadata != NULL) {
		g_hash_table_foreach (metadata,
				      insert_key_to_metadata_view, user_data);
	}
}

static void
on_show_all_metadata_activate (GtkMenuItem* item, gpointer user_data)
{
	GtkWidget* dialog;
	GtkWidget* scroll;
	GtkWidget* view;
	GtkListStore* store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	MafwSource* src;
	gchar* oid;
	gchar* uuid;

	/* Get the currently selected object ID in source view */
	oid = get_selected_object_id ();
	if (oid == NULL || strlen (oid) == 0)
		return;

	/* New dialog */
	dialog = gtk_dialog_new_with_buttons ("Metadata",
					      GTK_WINDOW (main_window),
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_ACCEPT,
					      NULL);
	g_assert (dialog != NULL);
	gtk_widget_set_size_request (dialog, 600, 400);

	/* Create a list store for key & value columns */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	g_assert (store != NULL);

	/* Create a scrolled view and insert it into the dialog */
	scroll = gtk_scrolled_window_new (NULL, NULL);
	g_assert (scroll != NULL);
	gtk_container_add (GTK_CONTAINER (gtk_bin_get_child (GTK_BIN(dialog))),
			   scroll);

	/* Create a tree view and insert it into the scrolled view */
	view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	g_assert (view != NULL);
	gtk_container_add (GTK_CONTAINER (scroll), view);

	/* Create columns into the tree view for key & value */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Key", renderer,
							   "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
	column = gtk_tree_view_column_new_with_attributes ("Value", renderer,
							   "text", 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	/* Find the source that owns the selected object ID */
	mafw_source_split_objectid (oid, &uuid, NULL);
	src = MAFW_SOURCE (mafw_registry_get_extension_by_uuid (
				    mafw_registry_get_instance (),
				    uuid));
	g_assert (src != NULL);

	/* Get all metadata related to the selected object ID */
	mafw_source_get_metadata (src, oid, MAFW_SOURCE_ALL_KEYS,
				   all_metadata_cb, store);

	/* Show, run and destroy */
        gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (oid);
	g_free (uuid);
}

static void ob_created(MafwSource *self, const gchar *object_id,
			gpointer udata, const GError *error)
{
	/* container-changed signal should refresh */
	mtg_print_signal (MAFW_EXTENSION(self), "object-created",
			  "Object-id: %s", object_id);
}

static void add_to_source(MafwSource *self, const gchar *object_id,
			GHashTable *metadata, MafwSource *selected_source,
			const GError *error)
{
	const gchar *uuid =
                mafw_extension_get_uuid(MAFW_EXTENSION(selected_source));
	gchar *temp;

	if (error)
	{
		g_critical("Error on get-metadata cb");
		return;
	}
	temp = g_strconcat(uuid, "::", NULL);
	mafw_source_create_object(selected_source, temp,
					metadata,
			(MafwSourceObjectCreatedCb)ob_created,
					NULL);
	g_free(temp);
}
static void
on_create_object_activate (GtkMenuItem* item, gpointer user_data)
{
	MafwSource *selected_source = get_selected_source();
	gchar *selected_oid = playlist_get_selected_oid();

	if (selected_source == NULL)
	{
		g_free(selected_oid);
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						"No source selected");
		return;
	}
	else if (selected_oid == NULL)
	{
		g_free(selected_oid);
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						"No playlist item selected");
		return;
	}

	get_metadata_from_oid (selected_oid,
			       (MafwSourceMetadataResultCb) add_to_source,
			       selected_source);
	g_free(selected_oid);
}

static void ob_destroyed(MafwSource *self, const gchar *object_id,
					    gpointer user_data,
					    const GError *error)
{
	/* container-changed signal should refresh */
	mtg_print_signal (MAFW_EXTENSION(self), "object-destroyed",
			  "Object-id: %s", object_id);
}

static void
on_destroy_object_activate (GtkMenuItem* item, gpointer user_data)
{
	MafwSource *selected_source = get_selected_source();
	gchar *selected_oid = get_selected_object_id();

	if (selected_source == NULL)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						"No source selected");
		return;
	}
	else if (selected_oid == NULL)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						"No playlist item selected");
		return;
	}

	mafw_source_destroy_object (selected_source, selected_oid,
				 (MafwSourceObjectDestroyedCb) ob_destroyed,
				 NULL);

	g_free(selected_oid);
}

static void
on_detached_model_menu_toggled (GtkCheckMenuItem* item, gpointer userdata)
{
	source_treeview_set_model_behaviour (SourceModelDetached);
}

static void
on_cached_model_menu_toggled (GtkCheckMenuItem* item, gpointer userdata)
{
	source_treeview_set_model_behaviour (SourceModelCached);
}

static void
on_normal_model_menu_toggled (GtkCheckMenuItem* item, gpointer userdata)
{
	source_treeview_set_model_behaviour (SourceModelNormal);
}

void on_enter_uri_cancel_clicked(GtkButton *button, gpointer user_data);
void on_enter_uri_ok_clicked(GtkButton *button, gpointer user_data);

void on_enter_uri_cancel_clicked(GtkButton *button, gpointer   user_data)
{
	GtkWidget *dialog =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "enter-uri-dialog"));
	GtkWidget *entry =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "enter-uri-entry"));
	GtkWidget *title_entry =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "enter-uri-title"));
	gtk_widget_hide(dialog);
	gtk_entry_set_text(GTK_ENTRY(entry), "");
	gtk_entry_set_text(GTK_ENTRY(title_entry), "");
}

static void enter_uri_dialog_cb_bookmark_uri(const gchar *uri)
{
	MafwSource *selected_source = get_selected_source();
	GtkWidget *entry =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "enter-uri-title"));
	const gchar *title = gtk_entry_get_text(GTK_ENTRY(entry));
	GHashTable *metadata;

	if (!selected_source)
	{
		return;
	}

	metadata = mafw_metadata_new();
	mafw_metadata_add_str(metadata, MAFW_METADATA_KEY_URI, uri);

	if (title && title[0])
	{
		mafw_metadata_add_str(metadata, MAFW_METADATA_KEY_TITLE,
						title);;
	}

	add_to_source(NULL, NULL, metadata, selected_source, NULL);
}

static void enter_uri_dialog_cb_play_uri(const gchar *uri)
{
	MafwRenderer *selected_renderer = get_selected_renderer();

	if (!selected_renderer)
	{
		return;
	}

	mafw_renderer_play_uri(selected_renderer, uri, NULL, NULL);
}

static void enter_uri_dialog_cb_import_pls(const gchar *uri)
{
	playlist_import(uri);
}

void on_enter_uri_ok_clicked(GtkButton *button, gpointer   user_data)
{
	GtkWidget *entry =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "enter-uri-entry"));
	GtkWidget *title_entry =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "enter-uri-title"));
	GtkWidget *dialog =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "enter-uri-dialog"));
	const gchar *uri = gtk_entry_get_text(GTK_ENTRY(entry));
	void (*ok_cb)(const gchar*) = g_object_get_data(G_OBJECT(dialog), "cb");

	if (!uri || !uri[0])
	{
		return;
	}

	ok_cb(uri);

	gtk_widget_hide(dialog);

	gtk_entry_set_text(GTK_ENTRY(entry), "");
	gtk_entry_set_text(GTK_ENTRY(title_entry), "");
}

static void show_uri_dialog(const gchar *title, gpointer cb)
{
	GtkWidget *dialog =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "enter-uri-dialog"));
        g_signal_connect(G_OBJECT (dialog), "delete-event",
                         G_CALLBACK(on_delete_uri_dialog_event), NULL);

	gtk_window_set_title(GTK_WINDOW(dialog), title);
	g_object_set_data(G_OBJECT(dialog), "cb", cb);
	gtk_widget_show_all(dialog);
}

static void uri_dialog_hide_title_fields(void)
{
	GtkWidget *wid = GTK_WIDGET(gtk_builder_get_object(builder,
                                                           "enter-uri-title"));

	gtk_widget_hide(wid);

	wid = GTK_WIDGET(gtk_builder_get_object(builder,
                                                "enter-uri-title-label"));
	gtk_widget_hide(wid);
}

static void
on_bookmark_uri_event(GtkMenuItem* item, gpointer user_data)
{
	show_uri_dialog("Bookmark URI", enter_uri_dialog_cb_bookmark_uri);
}

static void
on_play_uri_event(GtkMenuItem* item, gpointer user_data)
{
	if (get_selected_renderer())
	{
		show_uri_dialog("Play URI", enter_uri_dialog_cb_play_uri);
		uri_dialog_hide_title_fields();
	}
}

static void import_oid_mdat_cb(MafwSource *self,
					   const gchar *object_id,
					   GHashTable *metadata,
					   gpointer user_data,
					   const GError *error)
{
	if (error)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						error->message);
	}
	else
	{
		GValue *cur_value = NULL;

		if (metadata)
			cur_value = mafw_metadata_first(metadata,
                                                        MAFW_METADATA_KEY_URI);
		if (!cur_value ||
			!(G_IS_VALUE(cur_value) &&
                          G_VALUE_HOLDS_STRING(cur_value)))
		{/* No URI?? */
			hildon_banner_show_information (
                                NULL,
				"qgn_list_smiley_angry",
				"Metadata does not contain "
                                "any URI information");
		}
		else
		{
			playlist_import(g_value_get_string(cur_value));
		}
	}
}

static void
on_import_file(GtkMenuItem* item, gpointer user_data)
{
	show_uri_dialog("Import playlist", enter_uri_dialog_cb_import_pls);
	uri_dialog_hide_title_fields();
}

static void
on_import_oid(GtkMenuItem* item, gpointer user_data)
{
	gchar *selected_oid = get_selected_object_id();
	if (selected_oid)
	{
		playlist_import(selected_oid);
		g_free(selected_oid);
	}
}

static void
on_import_src_uri(GtkMenuItem* item, gpointer user_data)
{
	gchar *selected_oid = get_selected_object_id();
	MafwSource *selected_source = get_selected_source();

	if (selected_oid && selected_source)
	{
		mafw_source_get_metadata(selected_source, selected_oid,
				MAFW_SOURCE_LIST(MAFW_METADATA_KEY_URI),
				import_oid_mdat_cb, NULL);
	}
	g_free(selected_oid);
}

void on_playling_order_close_clicked(GtkButton *button, gpointer   user_data)
{
	GtkWidget *dialog =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "playing-order-dialog"));
	gtk_widget_hide(dialog);
}

static GtkTreeModel *plorder_model;


static void
on_show_playing_order(GtkMenuItem* item, gpointer user_data)
{
	GtkWidget *dialog =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "playing-order-dialog"));
	MafwPlaylist *pl = MAFW_PLAYLIST(get_current_playlist());
	GtkWidget *plorder_treeview =
                GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "playing-order-treeview"));
	GtkCellRenderer *text_renderer;
	guint cur_index = -1, starting_idx;;
	GtkTreeIter iter;
	gchar *cur_title;
	GtkTreeViewColumn *column;

	if (!pl)
		return;

	gtk_widget_reparent(dialog, main_window);
	if (!plorder_model)
	{
		plorder_model = GTK_TREE_MODEL(
		gtk_list_store_new(2,
				   G_TYPE_UINT,
				   G_TYPE_STRING));

		gtk_tree_view_set_model (GTK_TREE_VIEW (plorder_treeview),
				 plorder_model);

		/* Cell renderer */
		text_renderer = gtk_cell_renderer_text_new ();

		/* Visual index column */
		column = gtk_tree_view_column_new_with_attributes (
			"Index",
			text_renderer,
			"text",
			0,
			NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW (plorder_treeview),
                                             column);

		/* Object ID column */
		column = gtk_tree_view_column_new_with_attributes (
			"Title",
			text_renderer,
			"text",
			1,
			NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW (plorder_treeview),
                                             column);
	}

	gtk_list_store_clear (GTK_LIST_STORE (plorder_model));

	mafw_playlist_get_starting_index(pl, &cur_index, NULL, NULL);
	starting_idx = cur_index;
	if (cur_index != -1)
	do
	{
		cur_title = treeview_get_stored_title(cur_index);

		gtk_list_store_append (GTK_LIST_STORE(plorder_model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (plorder_model), &iter,
				    0, cur_index,
				    1, cur_title,
				    -1);
		g_free(cur_title);
	} while (mafw_playlist_get_next(pl, &cur_index, NULL,NULL) &&
						cur_index != starting_idx);
	gtk_widget_show_all(dialog);



}

static void
setup_menu (void)
{
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *sub_menu;
	GtkWidget *sub_item;
	GSList *group = NULL;

	menu = gtk_menu_new();

	/* Playlist view sub-menu */
	item = gtk_menu_item_new_with_label("Playlist view");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	sub_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), sub_menu);

	/* Rename current playlist */
	sub_item = gtk_menu_item_new_with_label ("Rename playlist");
	gtk_menu_shell_append (GTK_MENU_SHELL(sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_rename_playlist_button_clicked), NULL);

	/* Save current playlist as*/
	sub_item = gtk_menu_item_new_with_label ("Save playlist as");
	gtk_menu_shell_append (GTK_MENU_SHELL(sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_save_playlist_button_clicked), NULL);

	sub_item = gtk_menu_item_new_with_label ("Show playing order");
	gtk_menu_shell_append (GTK_MENU_SHELL(sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_show_playing_order), NULL);

	/**********************************************************************/

	/* Browse view sub-menu */
	item = gtk_menu_item_new_with_label ("Browse view");
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	sub_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), sub_menu);

	/* Play selected object */
	sub_item = gtk_menu_item_new_with_label ("Play selected object");
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_play_selected_object_activate), NULL);

	/* Play selected URI */
	sub_item = gtk_menu_item_new_with_label ("Play selected URI");
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_play_selected_uri_activate), NULL);

	/* Display selected item's metadata */
	sub_item = gtk_menu_item_new_with_label ("Show item's metadata");
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_show_all_metadata_activate), NULL);

	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu),
			       gtk_separator_menu_item_new ());

	/* Create object */
	sub_item =
                gtk_menu_item_new_with_label("Bookmark selected playlist item");
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_create_object_activate), NULL);

	/* Destroy object */
	sub_item = gtk_menu_item_new_with_label ("Destroy object");
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_destroy_object_activate), NULL);
	gtk_widget_set_sensitive (sub_item, TRUE);

	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu),
			       gtk_separator_menu_item_new ());

	/* Attached/detached list model */
	group = NULL;
	sub_item = gtk_radio_menu_item_new_with_label (group, "Detached model");
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (sub_item));
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "toggled",
			  G_CALLBACK (on_detached_model_menu_toggled), NULL);

	/* Set the most optimized mode active */
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (sub_item), TRUE);

	sub_item = gtk_radio_menu_item_new_with_label (group, "Cached model");
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (sub_item));
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "toggled",
			  G_CALLBACK (on_cached_model_menu_toggled), NULL);

	sub_item = gtk_radio_menu_item_new_with_label (group,
                                                       "Non-optimized model");
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (sub_item));
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "toggled",
			  G_CALLBACK (on_normal_model_menu_toggled), NULL);

	/**********************************************************************/

	/* Import sub-menu */
	item = gtk_menu_item_new_with_label ("Import");
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	sub_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), sub_menu);

	/* Import selected object-id */
	sub_item = gtk_menu_item_new_with_label ("Import selected object-id");
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_import_oid), NULL);

	/* Import selected URI */
	sub_item = gtk_menu_item_new_with_label ("Import selected URI");
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_import_src_uri), NULL);

	/* Import file */
	sub_item = gtk_menu_item_new_with_label ("Import from file");
	gtk_menu_shell_append (GTK_MENU_SHELL (sub_menu), sub_item);
	g_signal_connect (G_OBJECT (sub_item), "activate",
			  G_CALLBACK (on_import_file), NULL);

	/**********************************************************************/

	item = gtk_menu_item_new_with_label("Bookmark URI");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
			 GTK_SIGNAL_FUNC(on_bookmark_uri_event), NULL);

	item = gtk_menu_item_new_with_label("Play URI");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
			 GTK_SIGNAL_FUNC(on_play_uri_event), NULL);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu),
			       gtk_separator_menu_item_new ());

	/* Close */
	item = gtk_menu_item_new_with_label("Close");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
			 GTK_SIGNAL_FUNC(on_delete_event), NULL);

	hildon_window_set_menu (HILDON_WINDOW (main_window), GTK_MENU (menu));
}

static gboolean
on_main_win_keypress (GtkWidget* widget, GdkEventKey* event, gpointer user_data)
{
	switch (event->keyval)
	{
		case HILDON_HARDKEY_FULLSCREEN:
			if (mwin_fullscreen)
				gtk_window_unfullscreen (
						GTK_WINDOW(main_window));
			else
				gtk_window_fullscreen (GTK_WINDOW(main_window));
			mwin_fullscreen = !mwin_fullscreen;
			return TRUE;

		case HILDON_HARDKEY_INCREASE:
			change_volume(1);
			break;

		case HILDON_HARDKEY_DECREASE:
			change_volume(-1);
			break;

		case HILDON_HARDKEY_LEFT:
		case HILDON_HARDKEY_RIGHT:
			if (playlist_has_focus())
				source_tv_get_focus(NULL);
			else
				playlist_get_focus();
			return TRUE;
	}

	return FALSE;
}

gboolean
init_ui (gint   *argc,
         gchar **argv[])
{
        GtkWidget *builder_window;
	GtkWidget *widgets;
	HildonProgram *program;

        gtk_init (argc, argv);
        g_thread_init (NULL);

        /* Load gtk-builder file */
        builder = gtk_builder_new();
        if (!gtk_builder_add_from_file(builder, GTK_BUILDER_FILE, NULL)) {
                g_critical ("Unable to load the GUI file %s", GTK_BUILDER_FILE);
                return FALSE;
        }

	/* Create a new hildon program and add the hildon window to be the main
	   program window */
	program = HILDON_PROGRAM (hildon_program_get_instance ());
	main_window = hildon_window_new ();
	hildon_program_add_window (program, HILDON_WINDOW (main_window));
	g_set_application_name("MAFW Test GUI");

        /* Create the main window from the gtk-builder file */
        builder_window = GTK_WIDGET(gtk_builder_get_object(builder,
                                                           "main-window"));
        g_assert (builder_window != NULL);

	/* Connect all signals specified in the gtk-builder file */
        gtk_builder_connect_signals(builder, NULL);

	/* Reparent all widgets from the created gtk-builder window to
	   our hildon window */
	widgets = gtk_bin_get_child(GTK_BIN(builder_window));
	g_object_ref(widgets);
	gtk_container_remove(GTK_CONTAINER(builder_window), widgets);
	gtk_container_add(GTK_CONTAINER(main_window), widgets);
	gtk_widget_destroy(builder_window);
	g_object_unref(widgets);

	/* Gtk-builder has connection to this event, but since the
	   gtk-builder created window is destroyed, we need to connect
	   the hildon window here */
	g_signal_connect(main_window, "delete-event",
			 G_CALLBACK(on_delete_event), NULL);
	g_signal_connect(main_window, "key-press-event",
			 G_CALLBACK(on_main_win_keypress), NULL);

	/* Setup UI components */
	setup_menu ();
        setup_source_treeview (builder);
	setup_playlist_treeview (builder);
	setup_playlist_controls (builder);
        setup_renderer_controls (builder);
        setup_renderer_combo (builder);
	setup_metadata_treeview(builder);

        gtk_widget_show_all (main_window);

	g_idle_add((GSourceFunc)source_tv_get_focus, NULL);

        return TRUE;
}

void
deinit_ui (void)
{
        g_object_unref (builder);
        gtk_widget_destroy (main_window);
}
