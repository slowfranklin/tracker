/*
 * Copyright (C) 2010, Codeminded BVBA <abustany@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include <libtracker-client/tracker-client.h>

static TrackerClient *client;
static GMainLoop *main_loop;

static void
query_cb (TrackerResultIterator *iterator,
          GError                *error,
          gpointer               user_data)
{
	if (!iterator) {
		g_printerr ("Query preparation failed, %s\n", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	while (tracker_result_iterator_has_next (iterator)) {
		int i;

		tracker_result_iterator_next (iterator);

		for (i = 0; i < tracker_result_iterator_n_columns (iterator); i++) {
			g_print ("%s", tracker_result_iterator_value (iterator, i));

			if (i != tracker_result_iterator_n_columns (iterator) - 1) {
				g_print (", ");
			}
		}

		g_print ("\n");
	}

	tracker_result_iterator_free (iterator);
	g_object_unref (client);

	g_main_loop_quit (main_loop);
}

int
main (int argc, char **argv) 
{
	const gchar *query;

	if (argc != 2) {
		g_printerr ("Usage: %s query\n", argv[0]);
		return EXIT_FAILURE;
	}

	query = argv[1];

	main_loop = g_main_loop_new (NULL, FALSE);

	client = tracker_client_new (0, 0);

	tracker_resources_sparql_query_iterate_async (client, query, query_cb, NULL);

	g_main_loop_run (main_loop);

	return EXIT_SUCCESS;
}
