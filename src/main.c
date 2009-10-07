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

#include <gconf/gconf-client.h>

#include <libmafw/mafw.h>
#include <libmafw/mafw-log.h>
#include <libmafw/mafw-registry.h>
#include <libmafw-shared/mafw-shared.h>
#include <sys/resource.h>

#include "main.h"
#include "gui.h"
#include "renderer-combo.h"
#include "renderer-controls.h"
#include "source-treeview.h"
#include "playlist-controls.h"
#include "playlist-treeview.h"

static MafwRegistry *registry = NULL;


void activate_all(gboolean make_active)
{
	GList *sources = mafw_registry_get_sources(registry);

	while(sources)
	{
		mafw_extension_set_property_boolean(MAFW_EXTENSION(sources->data),
						MAFW_PROPERTY_EXTENSION_ACTIVATE, make_active);
		sources = g_list_next(sources);
	}
}

void mtg_print_signal_gen (const gchar* origin, const gchar* signal,
			   const gchar* format, ...)
{
#ifndef G_DEBUG_DISABLE
	va_list varargs;

	g_print ("SIGNAL [%s] from [%s]: ", signal, origin);
	va_start (varargs, format);
	vfprintf (stderr, format, varargs);
	va_end (varargs);
#endif
}

void mtg_print_signal (MafwExtension* origin, const gchar* signal,
		       const gchar* format, ...)
{
#ifndef G_DEBUG_DISABLE
	const gchar* name;
	const gchar* uuid;
	va_list varargs;

	if (origin != NULL) {
		name = mafw_extension_get_name (origin);
		uuid = mafw_extension_get_uuid (origin);
	} else {
		name = "N/A";
		uuid = "N/A";
	}

	g_print ("SIGNAL [%s] from [%s][%s]: ", signal, name, uuid);
	va_start (varargs, format);
	vfprintf (stderr, format, varargs);
	va_end (varargs);
#endif
}

static void 
source_added_cb(MafwRegistry * registry, GObject *source, gpointer user_data)
{
	/* Get a reference to the available source */
	if (MAFW_IS_SOURCE(source)) {
		g_print("Source %s available\n", 
			mafw_extension_get_name(MAFW_EXTENSION(source)));
                /* Do not add metadata resolver source */
                if (strcmp(mafw_extension_get_uuid(MAFW_EXTENSION(source)), "gnomevfs") != 0)
                        add_source(MAFW_SOURCE(source));

		mtg_print_signal (MAFW_EXTENSION(source),
				  "Registry::source-added", "\n");
	}
}

static void 
source_removed_cb(MafwRegistry * registry, GObject *source, gpointer user_data)
{
	/* Get a reference to the unavailable source */
	if (MAFW_IS_SOURCE(source)) {
		g_print("Removing source %s \n", 
			mafw_extension_get_name(MAFW_EXTENSION(source)));
                remove_source(MAFW_SOURCE(source));

		mtg_print_signal (MAFW_EXTENSION(source),
				  "Registry::source-removed", "\n");
	}
}

static void 
renderer_added_cb(MafwRegistry * registry, GObject *renderer, gpointer user_data)
{
	/* Get a reference to the available renderer */
	if (MAFW_IS_RENDERER(renderer)) {
		g_print("Renderer %s available\n", 
			mafw_extension_get_name(MAFW_EXTENSION(renderer)));
		add_media_renderer(MAFW_RENDERER(renderer));

		mtg_print_signal (MAFW_EXTENSION(renderer),
				  "Registry::renderer-added", "\n");
	}
}

static void
renderer_removed_cb(MafwRegistry * registry, GObject *renderer, gpointer user_data)
{
	/* Get a reference to the available renderer */
	if (MAFW_IS_RENDERER(renderer)) {
		g_print("Renderer %s removed\n", 
			mafw_extension_get_name(MAFW_EXTENSION(renderer)));
		remove_media_renderer(MAFW_RENDERER(renderer));

		mtg_print_signal (MAFW_EXTENSION(renderer),
				  "Registry::renderer-removed", "\n");
	}
}

static void
crawler_state_changed (GConfClient *client,
		       guint conn_id,
		       GConfEntry *entry,
		       gpointer user_data)
{
	const GConfValue *value;
	gchar *str_value;
	MafwPlaylist *playlist;
		
	/* Reload current playlist when crawler is done 
	   indexing local content */

	value = gconf_entry_get_value(entry);
	str_value = gconf_value_to_string(value);
	
	if (str_value && 
	    g_ascii_strcasecmp (str_value, "IDLE") == 0) {
		g_print ("Crawler is done indexing."  \
			 "Reloading current playlist\n");
		playlist = MAFW_PLAYLIST(get_current_playlist());
		if (playlist != NULL) {
			display_playlist_contents(playlist);
		}
	}
}

static void 
register_crawler_watch (void)
{
	GConfClient *client = gconf_client_get_default();
	gconf_client_add_dir(client,
			     "/apps/osso/mafw-metalayer-crawler",
			     GCONF_CLIENT_PRELOAD_NONE,
			     NULL);
	gconf_client_notify_add(client, 
				"/apps/osso/mafw-metalayer-crawler/state",
				crawler_state_changed, 
				NULL, NULL, NULL);
}

static gboolean
init_app (void)
{
        GError *error = NULL;
	GList *extension_list;
	gchar **plugin_list;

        g_type_init ();

	/* Register plugins */
	registry = MAFW_REGISTRY(mafw_registry_get_instance());
	if (registry == NULL) {
		g_error("init_app: failed to get MafwRegistry object\n");
		return FALSE;
	}

	mafw_shared_init(registry, &error);
	if (error != NULL)
	{
		hildon_banner_show_information (NULL,
						"chat_smiley_angry",
						error->message);
		g_error_free(error);
		error = NULL;
	} 

	g_signal_connect(registry,
			 "renderer_added", G_CALLBACK(renderer_added_cb), NULL);

	g_signal_connect(registry,
			 "renderer_removed", G_CALLBACK(renderer_removed_cb), NULL);

	g_signal_connect(registry,
			 "source_added", G_CALLBACK(source_added_cb), NULL);

	g_signal_connect(registry,
			 "source_removed", G_CALLBACK(source_removed_cb), NULL);

	extension_list = mafw_registry_get_renderers(registry);
	while (extension_list)
	{
		renderer_added_cb(registry, G_OBJECT(extension_list->data), NULL);
		extension_list = g_list_next(extension_list);
	}
	
	extension_list = mafw_registry_get_sources(registry);
	while (extension_list)
	{
		source_added_cb(registry, G_OBJECT(extension_list->data), NULL);
		extension_list = g_list_next(extension_list);
	}

	
	/* Check if we need to load any plugins directly */
	if (NULL != g_getenv("MAFW_TG_PLUGINS")) {
		plugin_list = g_strsplit(g_getenv("MAFW_TG_PLUGINS"),
					 G_SEARCHPATH_SEPARATOR_S, 0);
		gchar **plugin;
		plugin = plugin_list;
		for (; NULL != *plugin; plugin++) {
			mafw_registry_load_plugin(
					MAFW_REGISTRY(registry),
					*plugin, &error);
			g_debug("Plugin '%s' loaded\n", *plugin);
			if (error != NULL) {
				gchar* msg;
				msg = g_strdup_printf("Unable to load plugin"
						      "%s: %s", *plugin,
						      error->message);
				hildon_banner_show_information (NULL,
						"chat_smiley_angry", msg);
				g_free(msg);
				g_error_free(error);
				error = NULL;
			} 
		}
		g_strfreev(plugin_list);
	}

	/* Hook to crawler status */
	register_crawler_watch ();

	return TRUE;
}

static void
deinit_app (void)
{
	/* Stop any ongoing playback */
	stop ();
}

void
application_exit (void)
{
        deinit_app ();
        deinit_ui ();
        gtk_main_quit ();
}

gint
main (gint   argc,
      gchar *argv[])
{
	setpriority (PRIO_PROCESS, 0, 10);
	mafw_log_init(NULL);
        if (!init_ui (&argc, &argv)) {
           return -2;
        }

        if (!init_app ()) {
           return -3;
        }

        gtk_main ();

        return 0;
}
