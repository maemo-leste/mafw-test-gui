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

#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <config.h>

#include <libmafw/mafw.h>

#include "source-treeview.h"
#include "playlist-treeview.h"
#include "metadata-view.h"
#include "renderer-combo.h"
#include "renderer-controls.h"
#include "gui.h"
#include "main.h"

/*****************************************************************************
 * Static widgets & variables
 *****************************************************************************/

extern GtkWidget *main_window;

/** GtkTreeView* that displays the model contents */
static GtkWidget *treeview;

/** A GtkListStore* that contains only the current container contents */
static GtkTreeModel *model;

/** A cache that receives the items initially and that is purged every once
    in a while to the real model. */
static GtkTreeModel *cache;

/** Button that is used to go up in the tree hierarchy */
static GtkWidget* up_button;

/** Browse view optimization mode */
static SourceModelBehaviour model_behaviour;

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/

static void
browse (MafwSource *source, const gchar *object_id, guint skip, guint count);

static void
cancel_browse (const gchar* objectid, guint browse_id);

static void
display_sources(void);

void
on_source_treeview_row_activated (GtkTreeView *treeview, GtkTreePath *path,
				  GtkTreeViewColumn *column, gpointer userdata);
void
on_source_up_button_clicked (GtkButton* button, gpointer user_data);

static gboolean
find_objectid (const gchar* objectid, GtkTreeIter* iter);

/*****************************************************************************
 * Source tree structure
 *****************************************************************************/

enum {
	COLUMN_TITLE,
	COLUMN_OBJECTID,
	COLUMN_MIME,
	COLUMNS
};

/*****************************************************************************
 * Performance measurement functions
 *****************************************************************************/

static struct timeval perf_start_time;
static struct timeval perf_end_time;
static guint          perf_num;

static void perf_start(void)
{
	memset(&perf_start_time, 0, sizeof(struct timeval));
	memset(&perf_end_time, 0, sizeof(struct timeval));
	perf_num = 0;

	gettimeofday(&perf_start_time, NULL);
}

static void perf_end(void)
{
	gettimeofday(&perf_end_time, NULL);

	printf("\nBrowsed %d items in %lu.%lu seconds\n", perf_num,
	       perf_end_time.tv_sec - perf_start_time.tv_sec,
	       perf_end_time.tv_usec - perf_start_time.tv_usec);
}

/*****************************************************************************
 * Hard key handlers
 *****************************************************************************/

/**
 * Give input focus to the source treeview
 */
gboolean source_tv_get_focus(gpointer data)
{
	gtk_widget_grab_focus(treeview);
	return FALSE;
}

/**
 * Check, whether the focus can move up in the source treeview
 */
static gboolean is_up_possible(void)
{
	GtkTreeSelection *cur_selection;
	GtkTreePath *root, *selected_p;
	GtkTreeIter iter;
	gboolean retval = TRUE;

	g_assert (model != NULL);

	cur_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	if (!gtk_tree_selection_get_selected(cur_selection, &model, &iter))
	{
		/* Let the default handler select a tv element */
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

/**
 * Check, whether the focus can move down in the source treeview
 */
static gboolean is_down_possible(void)
{
	GtkTreeSelection *cur_selection;
	GtkTreeIter iter_selected, iter, iter_parent;
	gboolean retval = FALSE;

	g_assert (model != NULL);

	cur_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	if (!gtk_tree_selection_get_selected(cur_selection, &model, &iter))
	{
		/* Let the default handler select a tv element */
		return TRUE;
	}
	iter_selected = iter;

	if (gtk_tree_model_iter_next(model, &iter))
		return TRUE;
	iter = iter_selected;

	while (gtk_tree_model_iter_parent(model, &iter_parent, &iter))
	{
		iter = iter_parent;
		if (gtk_tree_model_iter_next(model, &iter))
		{
			retval = TRUE;
			break;
		}
		iter = iter_parent;
	}
	return retval;
}

/**
 * Hard key press handler for source treeview
 */
static gboolean
on_source_treeview_key_pressed (GtkWidget* widget, GdkEventKey* event,
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

	}

	return FALSE;
}

/*****************************************************************************
 * Current container ID and parent ID. These keep track of the container path.
 *****************************************************************************/

/** Store container ID's & names on the traversed folder path in this struct */
typedef struct _ContainerStackItem
{
	/** Container's object ID */
	gchar *objectid;

	/** Last browse ID */
	guint browseid;

} ContainerStackItem;

/** This queue keeps track of the current container path we are browsing in */
static GQueue *container_stack = NULL;

/**
 * container_stack_push:
 * @objectid: An object ID belonging to a container
 *
 * Push a new container ID on top of the container stack. Used when navigating
 * deeper in the folder hierarchy.
 */
static void container_stack_push(const gchar* objectid)
{
	ContainerStackItem *item = NULL;

	if (container_stack == NULL)
		container_stack = g_queue_new();

	item = g_new0(ContainerStackItem, 1);
	item->objectid = g_strdup(objectid);

	g_queue_push_head(container_stack, item);
}

/**
 * container_stack_pop:
 * @objectid: The current container ID (must be freed manually)
 * @browseid: The last browseid for the topmost container
 *
 * Take the topmost object ID & browse ID away from the container stack. Used
 * when navigating upwards in the folder hierarchy.
 *
 * Returns #FALSE when the stack is empty, otherwise #TRUE
 */
static gboolean container_stack_pop(gchar** objectid, guint* browseid)
{
	ContainerStackItem* item = NULL;

	if (container_stack == NULL)
		return FALSE;

	item = g_queue_pop_head(container_stack);
	if (item != NULL)
	{
		if (objectid != NULL)
			*objectid = item->objectid;
		if (browseid != NULL)
			*browseid = item->browseid;

		g_free(item);

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*****************************************************************************
 * Container stack poke functions
 *****************************************************************************/

/**
 * container_stack_poke_browseid:
 * @browseid: A new browse ID
 *
 * Replace the topmost container stack item's browse ID. Used when fetching
 * items incrementally or whenever a new browse action is initiated on the
 * current container.
 *
 * Returns #FALSE if the stack is empty, otherwise #TRUE
 */
static gboolean container_stack_poke_browseid(guint browseid)
{
	ContainerStackItem* item = NULL;

	g_return_val_if_fail(container_stack != NULL, FALSE);

	item = g_queue_peek_head(container_stack);
	if (item != NULL)
	{
		item->browseid = browseid;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*****************************************************************************
 * Container stack peek functions
 *****************************************************************************/

/**
 * container_stack_peek_objectid:
 * @objectid: A pointer to store the object ID to
 *
 * Get the current container's object ID from the top of the container stack.
 * The returned string must be freed.
 *
 * Returns #FALSE if the stack is empty, otherwise #TRUE
 */
static gboolean container_stack_peek_objectid(gchar** objectid)
{
	ContainerStackItem* item = NULL;

	if (container_stack == NULL)
		return FALSE;

	item = g_queue_peek_head(container_stack);
	if (item != NULL)
	{
		if (objectid != NULL)
			*objectid = g_strdup(item->objectid);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * container_stack_peek_browseid:
 * @browseid: A pointer to store the current browse ID to.
 *
 * Get the current container's latest browse ID from the top of the container
 * stack.
 *
 * Returns #FALSE if the stack is empty, otherwise #TRUE
 */
static gboolean container_stack_peek_browseid(guint* browseid)
{
	ContainerStackItem* item = NULL;

	if (container_stack == NULL)
		return FALSE;

	item = g_queue_peek_head(container_stack);
	if (item != NULL)
	{
		if (browseid != NULL)
			*browseid = item->browseid;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*****************************************************************************
 * Source model behaviour
 *****************************************************************************/

/**
 * Change the list model's behaviour when browse results are expected.
 *
 * If @behaviour == #SourceModelNormal, each browse result item is appended
 * to the list model when the item arrives.
 *
 * If @behaviour == #SourceModelCached, each browse result item is appended
 * to a cache list model when the item arrives. The cache contents are purged
 * to the actual list model in intervals of 20 items.
 *
 * If @behaviour == #SourceModelDetached, the actual list model is detached
 * from the tree view when browse() is called, items are appended to the model
 * one by one, and the model is then re-attached when the last browse result
 * arrives.
 */
void
source_treeview_set_model_behaviour(SourceModelBehaviour behaviour)
{
	/* Don't do it twice */
	if (model_behaviour == behaviour)
		return;

	if ((behaviour == SourceModelNormal || behaviour == SourceModelCached)
	    && gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)) == NULL)
	{
		/* Model is currently detached, and user wants to see the
		   attached model (cached or normal). Re-attach it. */
		gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);
		g_object_unref (model);
	}

	model_behaviour = behaviour;
}

SourceModelBehaviour
source_treeview_get_model_behaviour(void)
{
	return model_behaviour;
}

/*****************************************************************************
 * Item adding/updating
 *****************************************************************************/

/**
 * Update the metadata of the given item.
 */
static void
update_model_item (GtkTreeModel* tree_model, GtkTreeIter* iter,
		   const gchar *objectid, GHashTable* metadata)
{
	const gchar *title;
	const gchar *mime;
	GValue *mval;

	if (metadata != NULL)
	{
		/* Title */
		mval = mafw_metadata_first(metadata, MAFW_METADATA_KEY_TITLE);
		if (mval == NULL)
		{
			mval = mafw_metadata_first(metadata,
						    MAFW_METADATA_KEY_URI);
		}

		if (mval != NULL) {
			title = g_value_get_string(mval);
			if (title && strcmp(title, "") == 0) {
				title = "Unknown";
			}
		} else {
			title = objectid;
		}

		/* Mime type */
		mval = mafw_metadata_first(metadata, MAFW_METADATA_KEY_MIME);
		if (mval != NULL)
			mime = g_value_get_string(mval);
		else
			mime = "unknown";
	}
	else
	{
		g_warning ("Got NULL metadata for objectid %s.\n", objectid);

		title = objectid;
		mime = "unknown";
	}

	/* Append the item */
	gtk_list_store_set (GTK_LIST_STORE (tree_model), iter,
			    COLUMN_TITLE, title,
			    COLUMN_OBJECTID, objectid,
			    COLUMN_MIME, mime,
			    -1);
}

/**
 * Append the given object ID and its metadata to the end of the tree model
 */
static void
append_model_item (const gchar* objectid, GHashTable* metadata)
{
	GtkTreeModel* tree_model;
	GtkTreeIter iter;

	if (model_behaviour == SourceModelCached)
		tree_model = cache;
	else
		tree_model = model;

	gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);
	update_model_item (tree_model, &iter, objectid, metadata);
}

/**
 * Bounce one item from the cache model into the real model
 */
static gboolean
purge_foreach (GtkTreeModel *cache, GtkTreePath *path, GtkTreeIter *iter,
	       gpointer data)
{
	GtkTreeIter realiter;
	gchar* title;
	gchar* objectid;
	gchar* mime;

	gtk_tree_model_get (cache, iter,
			    COLUMN_TITLE, &title,
			    COLUMN_OBJECTID, &objectid,
			    COLUMN_MIME, &mime,
			    -1);

	gtk_list_store_append (GTK_LIST_STORE (model), &realiter);
	gtk_list_store_set (GTK_LIST_STORE (model), &realiter,
			    COLUMN_TITLE, title,
			    COLUMN_OBJECTID, objectid,
			    COLUMN_MIME, mime,
			    -1);

	g_free(title);
	g_free(objectid);
	g_free(mime);

	return FALSE;
}

/**
 * Bounce the complete contents of the cache model into the real model
 */
static gboolean
purge_cache_idle (gpointer data)
{
	gtk_tree_model_foreach (cache, purge_foreach, NULL);
	gtk_list_store_clear (GTK_LIST_STORE (cache));
	return FALSE;
}

/*****************************************************************************
 * Browse
 *****************************************************************************/

#ifndef G_DEBUG_DISABLE
static void
print_metadata (gpointer key, gpointer value, gpointer user_data)
{
	gchar* contents = g_strdup_value_contents ((GValue*) value);
	g_debug ("\t%s = %s\n", (gchar*) key, contents);
	g_free (contents);
}
#endif

/**
 * Browse result callback
 */
static void
browse_cb (MafwSource *source, guint browseid, gint remaining_count,
	   guint index, const gchar *objectid, GHashTable *metadata,
	   gpointer user_data, const GError *error)
{
	#ifndef G_DEBUG_DISABLE
	mtg_print_signal (MAFW_EXTENSION(source), "browse-result",
			  "BrowseID: %u, Remaining count: %d, Index: %u "
			  "ObjectID: %s\n", browseid, remaining_count, index,
			  objectid);
	#endif

	if (error != NULL)
	{
		/* The model should be detached here, but in case we end up in
		   this callback twice because of some glitch, make sure that
		   we don't try to re-attach twice and end up unreffing the
		   model completely. */
		if (browseid != MAFW_SOURCE_INVALID_BROWSE_ID &&
			model_behaviour == SourceModelDetached &&
		    gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)) == NULL)
		{
			gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
						 model);
			g_object_unref (model);
		}
		hildon_banner_show_information(
				main_window,
				NULL,
				error->message);

		perf_end();
	}
	else
	{
		/* Normal browse results. */
		guint current_browseid = 0;

                #ifndef G_DEBUG_DISABLE
		if (metadata != NULL)
			g_hash_table_foreach (metadata, print_metadata, NULL);
                #endif

		perf_num++;

		/* Append results only if they belong to the current action */
		if (container_stack_peek_browseid (&current_browseid) == TRUE)
		{
			if (current_browseid == browseid)
				append_model_item (objectid, metadata);
		}

		if (remaining_count == 0)
		{
			/* The model should be detached here, but in case we end
			   up in this callback twice because of some glitch,
			   make sure that we don't try to re-attach twice and
			   end up unreffing the model completely. */
			if (model_behaviour == SourceModelDetached &&
			    gtk_tree_view_get_model (GTK_TREE_VIEW (treeview))
			    == NULL)
			{
				gtk_tree_view_set_model (
					GTK_TREE_VIEW (treeview), model);
				g_object_unref (model);
			}
			else if (model_behaviour == SourceModelCached)
			{
				g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10,
						 purge_cache_idle, NULL, NULL);
			}

			/* Termination signal */
			perf_end();

			/* Invalidate the current browse ID */
			container_stack_poke_browseid(
				MAFW_SOURCE_INVALID_BROWSE_ID);
		}
		else if ((perf_num % 20) == 0 &&
			 model_behaviour == SourceModelCached)
		{
			g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10,
					 purge_cache_idle, NULL, NULL);
		}
	}
}

/**
 * Browse for a source's items under the given container object ID.
 */
static void
browse (MafwSource* source, const gchar *object_id, guint skip, guint count)
{
	const gchar *const *metadata_keys;
	guint browse_id;

	metadata_keys = MAFW_SOURCE_LIST (MAFW_METADATA_KEY_TITLE,
					   MAFW_METADATA_KEY_URI,
					   MAFW_METADATA_KEY_MIME);

	if (model_behaviour == SourceModelDetached)
	{
		g_object_ref (model);
		gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), NULL);
	}

	browse_id = mafw_source_browse(source, object_id,
					FALSE, /* Recursive */
					NULL,  /* Filter */
					"",    /* Sorting */
					metadata_keys,
					skip,
					count,
					browse_cb,
					NULL);

	if (browse_id == MAFW_SOURCE_INVALID_BROWSE_ID)
	{
		container_stack_pop(NULL, NULL);
		if (model_behaviour == SourceModelDetached)
		{
			gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
						 model);
			g_object_unref (model);
		}

	}
	else
	{
		/* Clear the contents of the model so that new browse results
		   can be placed to it. */
		gtk_list_store_clear (GTK_LIST_STORE (model));


		/* Put the new browse ID to the top of the container stack */
		container_stack_poke_browseid(browse_id);
	}
}

/**
 * Cancel an ongoing browse action with given browse ID and object ID
 */
static void
cancel_browse (const gchar* objectid, guint browseid)
{
	MafwRegistry* registry;
	MafwExtension* extension;
	gchar* uuid;
	mafw_source_split_objectid (objectid, &uuid, NULL);
	if (uuid == NULL)
		return;

	/* Get a registry instance */
	registry = mafw_registry_get_instance ();
	g_assert (registry != NULL);

	/* Re-attach the model only if it is currently detached */
	if (model_behaviour == SourceModelDetached &&
	    gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)) == NULL)
	{
		gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);
		g_object_unref (model);
	}

	/* Find a source matching the UUID */
	extension = mafw_registry_get_extension_by_uuid (registry, uuid);
	if (extension == NULL)
	{
		/* Unable to find a source. Go to top level. */
		gchar* msg;
		msg = g_strdup_printf("No source for UUID %s", uuid);
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						msg);
		g_free(msg);
	}
	else
	{
		GError* error = NULL;

		if (mafw_source_cancel_browse (MAFW_SOURCE (extension),
                                               browseid,
                                               &error) == FALSE)
		{
			hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						error->message);
			g_error_free (error);
		}
	}

	g_free (uuid);
}

/*****************************************************************************
 * GTK signal handlers
 *****************************************************************************/

/**
 * GTK signal handler for treeview row activations (double clicks)
 */
void
on_source_treeview_row_activated (GtkTreeView *treeview,
				  GtkTreePath *path,
				  GtkTreeViewColumn *column,
				  gpointer user_data)
{
	if (selected_is_container () == FALSE)
	{
		/* An item was clicked. Add it to the playlist. */
		on_add_item_button_clicked (NULL);
	}
	else
	{
		/* A container was clicked. Go inside it and start browsing. */
		MafwSource *source = NULL;
		gchar *object_id = NULL;

		/* Get the currently selected source */
		source = get_selected_source ();
		if (source == NULL)
			return;

		/* Get the currently selected object ID */
		object_id = get_selected_object_id ();
		if (object_id == NULL)
			return;

		/* TODO: Cancel previous browse. */

		/* Push the to-be-browsed object ID to the top of the
		   container stack so that we can go back again. */
		container_stack_push(object_id);

		/* Start performance measurement */
		perf_start ();

		/* Browse the selected container */
		browse (source, object_id, 0, 0);

		g_free (object_id);
	}
}

/**
 * GTK signal handler for up button clicks (for going upwards in the directory
 * hierarchy).
 */
void
on_source_up_button_clicked (GtkButton* button, gpointer user_data)
{
	gchar* objectid = NULL;
	guint browseid;

	if (container_stack_pop (&objectid, &browseid) == TRUE)
	{
		/* Cancel the current browse operation */
		if (browseid != MAFW_SOURCE_INVALID_BROWSE_ID)
			cancel_browse (objectid, browseid);

		g_free (objectid);
		objectid = NULL;

		if (container_stack_peek_objectid (&objectid) == FALSE)
		{
			/* We are back at the top level. Display all sources. */
			display_sources ();
		}
		else
		{
			MafwRegistry* registry;
			MafwExtension* extension;
			gchar* uuid;

			/* We ended up on a regular parent container. Need to
			   browse its contents. */

			mafw_source_split_objectid (objectid, &uuid, NULL);
			if (uuid == NULL) {
				g_free(objectid);
				return;
			}

			/* Get a registry instance */
			registry = mafw_registry_get_instance ();
			g_assert (registry != NULL);

			/* Find a source matching the UUID */
			extension =
                                mafw_registry_get_extension_by_uuid(registry,
                                                                    uuid);
			if (extension == NULL)
			{
				/* Unable to find a source. Go to top level. */

				display_sources ();
			}
			else
			{
				/* Clear the contents of the model so that new
				   browse results can be placed to it. */
				gtk_list_store_clear (GTK_LIST_STORE (model));

				/* Start performance measurement */
				perf_start ();

				/* Browse the container */
				browse (MAFW_SOURCE (extension), objectid,
                                        0, 0);
			}

			g_free (uuid);
		}
	}
	else
	{
		/* Already on top level. Nothing to do, except refresh
		   sources list. */
		display_sources ();
	}
	
	g_free (objectid);

}

/*****************************************************************************
 * Container changed signal handling
 *****************************************************************************/

/**
 * Listen to sources'  container changed signals and act accordingly
 */
static void
on_source_container_changed (MafwSource* source, const gchar* objectid)
{
	gchar* current_oid = NULL;

	if (container_stack_peek_objectid (&current_oid) == FALSE)
	{
		/* On top level, nothing to do. Containers can't change
		   here, because we're displaying extensions. */
		return;
	}
	else if (current_oid != NULL && objectid != NULL &&
		 strcmp (current_oid, objectid) == 0)
	{
		/* The container that we are currently in has changed.
		   Clear the container's contents and fetch them again.
		   There is no need to create a new entry to container stack,
		   because we are already in the container that we need to
		   browse. */
		gtk_list_store_clear (GTK_LIST_STORE (model));
		perf_start ();
		browse (source, objectid, 0, 0);
	}
	else
	{
		/* Some other container changed than the one that we're
		   currently in. Not interested. */
	}

	g_free (current_oid);
}

/*****************************************************************************
 * Metadata changed signal handling
 *****************************************************************************/

/**
 * Metadata result callback. Called after changed metadata is requested from
 * on_source_metadata_changed().
 */
static void
metadata_cb (MafwSource* self, const gchar* objectid, GHashTable *metadata,
	     gpointer user_data, const GError* error)
{
	GtkTreeIter iter;

	if (error != NULL)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						error->message);
		return;
	}

	/* Find the item that we fetched metadata for and update it */
	if (find_objectid (objectid, &iter) == TRUE)
	{
		update_model_item (model, &iter, objectid, metadata);
	}
	else
	{
		/* Too late, the item is not in the view now, no need 
                   to update anything. */
	}
}

static void
on_source_metadata_changed (MafwSource* source, const gchar* objectid)
{
	GtkTreeIter iter;

	if (find_objectid (objectid, &iter) == TRUE)
	{
		const gchar *const *keys;

		keys = MAFW_SOURCE_LIST (MAFW_METADATA_KEY_TITLE,
					  MAFW_METADATA_KEY_URI,
					  MAFW_METADATA_KEY_MIME);


		/* The changed item is present in the current model.
		   We need to get its info. Don't save the iter until
		   metadata_cb is called, since it might be already invalid
		   because get_metadata() is async. */
		mafw_source_get_metadata (source, objectid, keys, metadata_cb,
					   NULL);
	}
}

/*****************************************************************************
 * Top-level source add/remove
 *****************************************************************************/

/**
 * Append a single source to the current list store
 */
static void
append_source (MafwSource *source)
{
	const gchar *name;
	GtkTreeIter iter;
	gchar *oid;

	name = mafw_extension_get_name (MAFW_EXTENSION (source));
	g_print ("Appending source: %s\n", name);

	/* Do not add metadata resolver source */
        if (strcmp(mafw_extension_get_uuid(
                           MAFW_EXTENSION(source)), "gnomevfs") == 0)
                        return;
	/* Construct a top-level object ID for the source so that it can be
	   browsed just like a regular container. */
	oid = g_strconcat(mafw_extension_get_uuid (
                                  MAFW_EXTENSION(source)), "::", NULL);

	/* Append the source to the list store */
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    COLUMN_TITLE, name,
			    COLUMN_OBJECTID, oid,
			    COLUMN_MIME, NULL,
			    -1);

	g_free(oid);
}

/**
 * Clear the current list store contents and display all available sources
 */
static void
display_sources(void)
{
	MafwRegistry* registry;
	GList* node;

	while (container_stack_pop (NULL, NULL) == TRUE)
	{
		/* Just pop the container stack until it is empty. */
	}

	/* Clear the contents of the current list store */
	gtk_list_store_clear (GTK_LIST_STORE (model));

	registry = MAFW_REGISTRY(mafw_registry_get_instance());
	if (registry == NULL)
	{
		g_error("Unable to get MafwRegistry object");
		return;
	}

	node = mafw_registry_get_sources(registry);
	if (node == NULL)
	{
		hildon_banner_show_information (NULL,
						"qgn_list_smiley_angry",
						"No sources available");
		return;
	}

	/* Append all known sources to the model */
	for (node = node; node != NULL; node = node->next)
		append_source (MAFW_SOURCE (node->data));
}

/**
 * Called from a Mafw signal handler. If the current view is on top level,
 * all newly-found sources are appended to view. Otherwise they are ignored.
 */
void
add_source (MafwSource *source)
{
	/* If the container stack is empty, we are on top level and can
	   append all newly-found sources into view. Mafw shouldn't signal
	   any sources twice, so no need to check for existing sources. */
	if (container_stack_peek_objectid(NULL) == FALSE)
	{
		append_source (source);

		/* Listen to container change signals */
		g_signal_connect(source, "container-changed",
				 (GCallback) on_source_container_changed, NULL);

		/* Listen to metadata change signals */
		g_signal_connect(source, "metadata-changed",
				 (GCallback) on_source_metadata_changed, NULL);
	}
	else
	{
		/* We are inside some other server's container. No need to
		   do anything with the newly-found source. */
	}
}

/**
 * Called from a Mafw signal handler. If the current view is on top level,
 * all destroyed sources are removed from view. Otherwise they are ignored.
 */
void
remove_source (MafwSource *source)
{
	gchar *oid;

	/* If the container stack is empty, we are on top level and can
	   remove all destroyed sources from the view. Otherwise, if we are
	   inside the destroyed source, we must return to top level. */
	if (container_stack_peek_objectid(&oid) == TRUE)
	{
		const gchar *source_oid;
		gchar *uuid;

		source_oid = mafw_extension_get_uuid (MAFW_EXTENSION (source));

		/* Check, whether we are inside the destroyed source */
		mafw_source_split_objectid(oid, &uuid, NULL);
		if (oid != NULL && source_oid != NULL &&
		    strcmp (oid, source_oid) == 0)
		{
			/* We are inside the destroyed source. Get out! */
			/* TODO: Cancel browse? */
			display_sources ();
		}

		g_free(uuid);
		g_free(oid);
	}
	else
	{
		GtkTreeIter iter;

		/* We are on top level. Find the source and remove it. */
		oid = g_strconcat(mafw_extension_get_uuid (
                                          MAFW_EXTENSION(source)),
				  "::", NULL);

		if (find_objectid (oid, &iter) == TRUE)
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

		g_free(oid);
	}
}

/*****************************************************************************
 * Treeview item helpers
 *****************************************************************************/

/**
 * Find the given object ID from the current list of items and return a
 * GtkTreeIter pointing to that object.
 */
static gboolean
find_objectid (const gchar* objectid, GtkTreeIter* iter)
{
	g_return_val_if_fail (objectid != NULL, FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	if (gtk_tree_model_get_iter_first (model, iter) == FALSE)
		return FALSE;

	do
	{
		gchar* oid;
		gtk_tree_model_get (model, iter,
				    COLUMN_OBJECTID, &oid,
				    -1);

		if (oid != NULL && strcmp (oid, objectid) == 0)
		{
			g_free (oid);
			return TRUE;
		}
		else
		{
			g_free (oid);
		}

	} while (gtk_tree_model_iter_next (model, iter) == TRUE);

	return FALSE;
}

/**
 * Check, whether the currently selected item is a container
 */
gboolean
selected_is_container (void)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	gchar* mime = NULL;
	GtkTreeIter iter;

	/* Get the selection object */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        g_assert (selection != NULL);

	/* Check, if something is selected */
	if (gtk_tree_selection_get_selected (selection, &model, &iter) == FALSE)
		return FALSE;

	/* Get the selected item's MIME type */
	gtk_tree_model_get (model, &iter,
			    COLUMN_MIME, &mime,
			    -1);

	/* Accept either NULL mime type (top-level sources) or a container
	   mimetype (normal containers) */
	if (mime == NULL ||
	    strcmp (mime, MAFW_METADATA_VALUE_MIME_CONTAINER) == 0)
	{
		g_free(mime);
		return TRUE;
	}
	else
	{
		g_free(mime);
		return FALSE;
	}
}

/**
 * Get the currently selected item's object ID. The returned string must be
 * freed after use.
 */
gchar*
get_selected_object_id (void)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection = NULL;
	gchar *object_id = NULL;

	/* Get the selection object */
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        g_assert (selection != NULL);

	/* Check, if something is selected */
        if (gtk_tree_selection_get_selected (selection, &model, &iter) == TRUE)
        {
		/* Get the selected item's object ID */
                gtk_tree_model_get (model, &iter,
                                    COLUMN_OBJECTID, &object_id,
                                    -1);
        }

        return object_id;
}

/**
 * Get the currently selected source (or NULL if there isn't one)
 */
MafwSource*
get_selected_source (void)
{
	gchar* object_id;
	MafwExtension *extension;
	gchar* uuid;

	/* First try getting the object ID from the current container we're
	   in. If that fails, try to get the object ID from a selected item. */
	if (container_stack_peek_objectid (&object_id) == FALSE)
	{
		object_id = get_selected_object_id ();
		if (object_id == NULL)
			return NULL;
	}

	/* Extract UUID from the selected object ID */
	mafw_source_split_objectid (object_id, &uuid, NULL);
	if (uuid != NULL)
	{
		MafwRegistry* registry;

		registry = mafw_registry_get_instance ();
		g_assert (registry != NULL);

		/* Get the SiSo that has the matching UUID */
		extension = mafw_registry_get_extension_by_uuid (registry,
                                                                 uuid);
	}
	else
	{
		extension = NULL;
	}

	g_free (object_id);
	g_free (uuid);

	return MAFW_SOURCE (extension);
}

/**
 * Move the selection down once, to the current item's closest sibling.
 */
void
source_treeview_select_next (void)
{
	GtkTreeSelection* selection;
	GtkTreeModel* model;
	GtkTreeIter iter;
	GtkTreePath *path;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	g_assert (selection != NULL);

	if (gtk_tree_selection_get_selected (selection, &model, &iter) == TRUE)
	{
		if (gtk_tree_model_iter_next (model, &iter) == TRUE)
		{
			gtk_tree_selection_select_iter (selection, &iter);
			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW (treeview), path,
							NULL, FALSE);
			gtk_tree_path_free(path);
		}
	}
}

/*****************************************************************************
 * Source tree view construction
 *****************************************************************************/

enum {
	qgn_list_gene_fldr_cls,
	qgn_list_gene_music_file,
	qgn_list_gene_video_file,
	qgn_list_gene_image_file,
	qgn_list_gene_notsupported,
	qgn_list_filemanager,
	qgn_list_MAX
};

static GdkPixbuf* mimeimages[qgn_list_MAX];

static void mimeimage_init(void)
{
        GtkIconTheme* theme = NULL;

        /* Get theme */
        theme = gtk_icon_theme_get_default();
        g_return_if_fail(theme != NULL);

	mimeimages[qgn_list_gene_fldr_cls] =
		gtk_icon_theme_load_icon(theme,
					 "qgn_list_filesys_common_fldr",
					 HILDON_ICON_PIXEL_SIZE_SMALL,
					 GTK_ICON_LOOKUP_NO_SVG,
					 NULL);
	mimeimages[qgn_list_gene_music_file] =
		gtk_icon_theme_load_icon(theme,
					 "qgn_list_gene_music_file",
					 HILDON_ICON_PIXEL_SIZE_SMALL,
					 GTK_ICON_LOOKUP_NO_SVG,
					 NULL);
	mimeimages[qgn_list_gene_video_file] =
		gtk_icon_theme_load_icon(theme,
					 "qgn_list_gene_video_file",
					 HILDON_ICON_PIXEL_SIZE_SMALL,
					 GTK_ICON_LOOKUP_NO_SVG,
					 NULL);
	mimeimages[qgn_list_gene_image_file] =
		gtk_icon_theme_load_icon(theme,
					 "qgn_list_gene_image_file",
					 HILDON_ICON_PIXEL_SIZE_SMALL,
					 GTK_ICON_LOOKUP_NO_SVG,
					 NULL);
	mimeimages[qgn_list_gene_notsupported] =
		gtk_icon_theme_load_icon(theme,
					 "qgn_list_gene_notsupported",
					 HILDON_ICON_PIXEL_SIZE_SMALL,
					 GTK_ICON_LOOKUP_NO_SVG,
					 NULL);
	mimeimages[qgn_list_filemanager] =
		gtk_icon_theme_load_icon(theme,
					 "qgn_list_filemanager",
					 HILDON_ICON_PIXEL_SIZE_SMALL,
					 GTK_ICON_LOOKUP_NO_SVG,
					 NULL);
}

static void render_mimeimage_datafunc(GtkTreeViewColumn *column,
				      GtkCellRenderer *renderer,
				      GtkTreeModel *model,
				      GtkTreeIter *iter,
				      gpointer data)
{
        GdkPixbuf *pixbuf = NULL;
        gchar *mime = NULL;

        g_return_if_fail(renderer != NULL);
        g_return_if_fail(model != NULL);

        /* If it is expander, then it is a container */
        gtk_tree_model_get(model, iter,
			   COLUMN_MIME, &mime, -1);

        if (mime != NULL)
        {
                if (strcmp(mime, MAFW_METADATA_VALUE_MIME_CONTAINER) == 0)
                {
			pixbuf = mimeimages[qgn_list_gene_fldr_cls];
		}
		else if (strstr(mime, "audio") != NULL)
		{
			pixbuf = mimeimages[qgn_list_gene_music_file];
                }
		else if (strstr(mime, "video") != NULL)
		{
			pixbuf = mimeimages[qgn_list_gene_video_file];
		}
		else if (strstr(mime, "image") != NULL)
		{
			pixbuf = mimeimages[qgn_list_gene_image_file];
		}
                else
                {
                        pixbuf = mimeimages[qgn_list_gene_notsupported];
                }
        }
        else
        {
                /* The node didn't have class, expect it to be device.
                 * No need to check device type because only CDS-capable
                 * devices are in our list */
                pixbuf = mimeimages[qgn_list_filemanager];
        }

        g_free(mime);

        if (pixbuf != NULL)
        {
                /* Set pixbuf */
                g_object_set(G_OBJECT(renderer),
                             "pixbuf", pixbuf,
                             NULL);
        }
}

static void
create_playlist_treemodel (void)
{
	model = GTK_TREE_MODEL (gtk_list_store_new (COLUMNS,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_STRING));

	cache = GTK_TREE_MODEL (gtk_list_store_new (COLUMNS,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_STRING));
}

static void
setup_treeview_columns (GtkWidget *treeview)

{
	GtkTreeViewColumn *column;
	GtkCellRenderer *pxb_renderer;
	GtkCellRenderer *text_renderer;

	/* Icon */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	pxb_renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	gtk_tree_view_column_pack_start(column, pxb_renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func(column, pxb_renderer,
						render_mimeimage_datafunc,
						NULL, NULL);

	/* Title */
	text_renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (
		"Title",
		text_renderer,
		"text",
		COLUMN_TITLE,
		NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
}

void
setup_source_treeview (GtkBuilder *builder)
{
	/* Default to detached model, since it's the fastest */
	model_behaviour = SourceModelDetached;

	/* Get the up button from gtk-builder */
	up_button = GTK_WIDGET(gtk_builder_get_object(builder,
                                                      "source-up-button"));
	g_assert (up_button != NULL);

	/* Get the treeview from gtk-builder */
	treeview = GTK_WIDGET(gtk_builder_get_object(builder,
                                                     "source-treeview"));
	g_assert (treeview != NULL);

	/* Create a model for the view */
	create_playlist_treemodel ();

	/* Attach the model to the treeview */
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);
	g_object_unref (model);

	/* Setup appropriate columns for displaying the model contents */
	setup_treeview_columns (treeview);

	/* Receive hard key presses */
	g_signal_connect (treeview,
			  "key-press-event",
			  G_CALLBACK (on_source_treeview_key_pressed),
			  NULL);

	mimeimage_init();
}
