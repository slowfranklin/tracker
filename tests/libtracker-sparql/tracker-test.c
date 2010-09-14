/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <tracker-sparql.h>

typedef struct {
	const gchar *input ;
	const gchar *output;
} ESCAPE_TEST_DATA;

ESCAPE_TEST_DATA test_data []  = {
	{ "SELECT \"a\"", "SELECT \\\"a\\\"" },
	{ "SELECT ?u \t \n \r \b \f", "SELECT ?u \\t \\n \\r \\b \\f" },
	{ NULL, NULL }
};

static TrackerSparqlConnection *connection;
static GMainLoop *main_loop;

#if HAVE_TRACKER_FTS

static GCancellable *cancellables[6] = { NULL, NULL, NULL, NULL };

/* OK:   query 0 with either query 4 or 5.
 * FAIL: query 4 and 5 together (requires data to exist)
 */
static const gchar *queries[6] = {
	/* #1 */
	"SELECT ?p WHERE { ?p tracker:indexed true }",
	/* #2 */
	"SELECT ?prefix ?ns WHERE { ?ns a tracker:Namespace ; tracker:prefix ?prefix }",
	/* #3 */
	"SELECT ?p WHERE { ?p tracker:fulltextIndexed true }",
	/* #4 */
	"SELECT"
	"  ?u nie:url(?u)"
	"  tracker:coalesce(nie:title(?u), nfo:fileName(?u), \"Unknown\")"
	"  nfo:fileLastModified(?u)"
	"  nfo:fileSize(?u)"
	"  nie:url(?c) "
	"WHERE { "
	"  ?u fts:match \"love\" . "
	"  ?u nfo:belongsToContainer ?c ; "
	"     tracker:available true . "
	"} "
	"ORDER BY DESC(fts:rank(?u)) "
	"OFFSET 0 LIMIT 100",
	/* #5 */
	"SELECT"
	"  ?song"
	"  nie:url(?song)"
	"  tracker:coalesce(nie:title(?song), nfo:fileName(?song), \"Unknown\")"
	"  fn:string-join((?performer, ?album), \" - \")"
	"  nfo:duration(?song)"
	"  ?tooltip "
	"WHERE {"
	"  ?match fts:match \"love\""
	"  {"
	"    ?song nmm:musicAlbum ?match"
	"  } UNION {"
	"    ?song nmm:performer ?match"
	"  } UNION {"
	"    ?song a nfo:Audio ."
	"    ?match a nfo:Audio"
	"    FILTER (?song = ?match)"
	"  }"
	"  ?song nmm:performer [ nmm:artistName ?performer ] ;"
	"        nmm:musicAlbum [ nie:title ?album ] ;"
	"        nfo:belongsToContainer [ nie:url ?tooltip ]"
	"} "
	"ORDER BY DESC(fts:rank(?song)) DESC(nie:title(?song)) "
	"OFFSET 0 LIMIT 100",
	NULL
};

#endif

static void
test_tracker_sparql_escape_string (void)
{
	gint i;
	gchar *result;

	for (i = 0; test_data[i].input != NULL; i++) {
		result = tracker_sparql_escape_string (test_data[i].input);
		g_assert_cmpstr (result, ==, test_data[i].output);
		g_free (result);
	}
}

static void
test_tracker_sparql_escape_uri_vprintf (void)
{
	gchar *result;

	result = tracker_sparql_escape_uri_printf ("test:uri:contact-%d", 14, NULL);
	g_assert_cmpstr (result, ==, "test:uri:contact-14");
	g_free (result);
}

#if HAVE_TRACKER_FTS

static void test_tracker_sparql_cursor_next_async_query (gint query);

static void
test_tracker_sparql_cursor_next_async_cb (GObject      *source,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gboolean success;
	static gint finished = 0;
	static gint next = 0;
	gint next_to_cancel = 1;
	gint query;

	query = GPOINTER_TO_INT(user_data);

	g_assert (result != NULL);
	success = tracker_sparql_cursor_next_finish (TRACKER_SPARQL_CURSOR (source),
	                                             result,
	                                             &error);

	if (finished == 1 && next == next_to_cancel) {
		g_assert (error != NULL);
		g_print ("Got Cancellation GError\n");
	} else {
		g_assert_no_error (error);
	}

	cursor = TRACKER_SPARQL_CURSOR (source);
	g_assert (cursor != NULL);

	g_print ("  %d: %s\n",
	         query,
	         tracker_sparql_cursor_get_string (cursor, 0, NULL));

	if (!success) {
		finished++;
		next = 0;

		g_print ("Finished %d\n", finished);

		if (finished == 1 || finished == 2) {
			test_tracker_sparql_cursor_next_async_query (finished);
		} else if (finished == 3) {
			g_main_loop_quit (main_loop);
		}
	} else {
		tracker_sparql_cursor_next_async (cursor,
		                                  cancellables[query],
		                                  test_tracker_sparql_cursor_next_async_cb,
		                                  user_data);

		next++;

		/* Random number here for next_count_to_cancel is "2",
		 * just want to do this mid-cursor iteration
		 */
		if (next == next_to_cancel && finished == 1) {
			/* Cancel */
			g_print ("Cancelling cancellable:%p at count:%d\n",
			         cancellables[query],
			         next);
			g_cancellable_cancel (cancellables[query]);
		}
	}
}

static void
test_tracker_sparql_cursor_next_async_query (gint query)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	g_print ("ASYNC query %d starting:\n", query);

	cancellables[query] = g_cancellable_new ();
	g_assert (cancellables[query] != NULL);

	cursor = tracker_sparql_connection_query (connection,
	                                          queries[query],
	                                          NULL,
	                                          &error);
	g_assert_no_error (error);
	g_assert (cursor != NULL);

	tracker_sparql_cursor_next_async (cursor,
	                                  cancellables[query],
	                                  test_tracker_sparql_cursor_next_async_cb,
	                                  GINT_TO_POINTER(query));
}

static void
test_tracker_sparql_cursor_next_async (void)
{
	/* So, the idea here:
	 * 1. Test async cursor_next() call.
	 * 2. Make sure we can cancel a cursor_next() call and start a new query (was failing)
	 * 3. Handle multiple async queries + async cursor_next() calls.
	 */
	test_tracker_sparql_cursor_next_async_query (0);
}

#endif

gint
main (gint argc, gchar **argv)
{
	int result;
	GError *error = NULL;

	g_thread_init (NULL);
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	main_loop = g_main_loop_new (NULL, FALSE);
	g_assert (main_loop != NULL);

	connection = tracker_sparql_connection_get (&error);

	g_assert_no_error (error);
	g_assert (connection != NULL);

	g_test_add_func ("/libtracker-sparql/tracker/tracker_sparql_escape_string", 
	                 test_tracker_sparql_escape_string);
	g_test_add_func ("/libtracker-sparql/tracker/tracker_sparql_escape_uri_vprintf",
	                 test_tracker_sparql_escape_uri_vprintf);
#if HAVE_TRACKER_FTS
	g_test_add_func ("/libtracker-sparql/tracker/tracker_sparql_cursor_next_async",
	                 test_tracker_sparql_cursor_next_async);
#endif

	result = g_test_run ();

#if HAVE_TRACKER_FTS
	g_main_loop_run (main_loop);
#endif

	g_object_unref (connection);

	return result;
}
