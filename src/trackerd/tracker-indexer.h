/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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



#ifndef _TRACKER_INDEXER_H
#define _TRACKER_INDEXER_H


#include <stdlib.h>
#include <glib.h>

#include "tracker-utils.h"


typedef struct {                         	 
	guint32 	service_id;              /* Service ID of the document */
	guint32 	service_type_id;         /* Service type ID of the document */
	guint32 	score;            	 /* Ranking score */
} SearchHit;


typedef enum {
	WordNormal,
	WordWildCard,
	WordExactPhrase
} WordType;


typedef struct {                        
	char	 	*word;    
	int		hit_count;
	float		idf;
	WordType	word_type;
} SearchWord;

typedef struct {                        
	Indexer 	*indexer;
	int 		*service_array;    
	int 		service_array_count;
	int 		hit_count;
	GSList	        *hits;
	GSList		*words;
	GSList		*duds;
	int		offset;
	int		limit;
} SearchQuery;


typedef enum {
	BoolAnd,
	BoolOr,
	BoolNot
} BoolOp;

SearchQuery * 	tracker_create_query 			(Indexer *indexer, int *service_array, int service_array_count, int offset, int limit);
void		tracker_free_query 			(SearchQuery *query);

void		tracker_add_query_word 			(SearchQuery *query, const char *word, WordType word_type);

guint32		tracker_indexer_calc_amalgamated 	(int service, int score);
void		tracker_index_free_hit_list		(GSList *hit_list);

Indexer * 	tracker_indexer_open 			(const char *name);
void		tracker_indexer_close 			(Indexer *indexer);
gboolean	tracker_indexer_optimize		(Indexer *indexer);
void		tracker_indexer_sync 			(Indexer *indexer);

/* Indexing api */
gboolean	tracker_indexer_append_word_chunk 	(Indexer *indexer, const char *word, WordDetails *details, int word_detail_count);
gboolean	tracker_indexer_append_word 		(Indexer *indexer, const char *word, guint32 id, int service, int score);
gboolean	tracker_indexer_update_word 		(Indexer *indexer, const char *word, guint32 id, int service, int score, gboolean remove_word);

gboolean	tracker_indexer_get_hits 		(SearchQuery *query);
char ***	tracker_get_hit_counts 			(SearchQuery *query);
int		tracker_get_hit_count 			(SearchQuery *query);


#endif
