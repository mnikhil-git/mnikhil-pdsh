/*****************************************************************************\
 *
 *  $Id$
 *  $Source$
 *
 *  Copyright (C) 1998-2002 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick (garlick@llnl.gov>
 *  UCRL-CODE-980038
 *  
 *  This file is part of PDSH, a parallel remote shell program.
 *  For details, see <http://www.llnl.gov/linux/pdsh/>.
 *  
 *  PDSH is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  PDSH is distributed in the hope that it will be useful, but WITHOUT 
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 *  for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with PDSH; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "list.h"
#include "xmalloc.h"
#include "xstring.h"
#include "err.h"

/*
 * number of entries to allocate to a list initially and at subsequent
 * expansions
 */
#define LIST_CHUNK      16

#define SPACES          "\n\t "

/*
 * this completes the list_t type in list.h and prevents users of this
 * module from violating our abstraction.
 */
struct list_implementation {
#ifndef NDEBUG
#define LIST_MAGIC 	0xd00fd00f
        int magic;
#endif
        int size;
        int nitems;
        char **data;
};

/* 
 * Helper function for list_split(). Extract tokens from str.  
 * Return a pointer to the next token; at the same time, advance 
 * *str to point to the next separator.  
 *   sep (IN)	string containing list of separator characters
 *   str (IN)	double-pointer to string containing tokens and separators
 *   RETURN	next token
 */
static char *
_next_tok(char *sep, char **str)
{
	char *tok;

	/* push str past any leading separators */
	while (**str != '\0' && strchr(sep, **str) != '\0')
		(*str)++;

	if (**str == '\0')
		return NULL;

	/* assign token pointer */
	tok = *str;

	/* push str past token and leave pointing to first separator */
	while (**str != '\0' && strchr(sep, **str) == '\0')
		(*str)++;

	/* nullify consecutive separators and push str beyond them */
	while (**str != '\0' && strchr(sep, **str) != '\0')
		*(*str)++ = '\0';

	return tok;
}

/*
 * Create a new list with LIST_CHUNK elements allocated.
 *   RETURN	newly allocated list.
 */
list_t 
list_new(void)
{
	list_t new = (list_t)Malloc(sizeof(struct list_implementation));
#ifndef NDEBUG
	new->magic = LIST_MAGIC;
#endif
	new->data = (char **)Malloc(LIST_CHUNK * sizeof(char *));
	new->size = LIST_CHUNK;
	new->nitems = 0;

	return new;
}

/*
 * Expand a list to accomodate LIST_CHUNK more elements.
 *   l (in)	list which will be expanded
 */
void 
list_expand(list_t l)
{
	assert(l->magic == LIST_MAGIC);
	l->size += LIST_CHUNK;
	Realloc((void **)&(l->data), l->size * sizeof(char *));
}

/*
 * Free a list, including all the strings stored in elements, and the list
 * structure itself.  The casts to void are to shuddup the aix xlc_r compiler.
 *   l (IN)	pointer to list (list will be set to NULL)
 */
void 
list_free(list_t *l)
{
	int i;

	assert((*l)->magic == LIST_MAGIC);
	for (i = 0; i < (*l)->nitems; i++)
		Free((void **)&((*l)->data[i]));
	Free((void **)&((*l)->data));
	Free((void **)l);
}
	
/*
 * Push a word onto list.
 *   l (IN)	list 
 *   word (IN)	word to be added to list
 */
void 
list_push(list_t l, char *word)
{
	assert(l->magic == LIST_MAGIC);
	if (l->size == l->nitems)
		list_expand(l);
	assert(l->size > l->nitems);
	l->data[l->nitems++] = Strdup(word);
}

/*
 * Pop a word off of list (caller is responsible for freeing word)
 *   l (IN)	list
 *   RETURN	last entry of list
 */
char *
list_pop(list_t l)
{
	char *word = NULL;

	assert(l->magic == LIST_MAGIC);
	if (l->nitems > 0)	
		word = l->data[--(l->nitems)];

	return word;
}

/*
 * Shift a word off list (caller is responsible for freeing word)
 *   l (IN)	list
 *   RETURN	first entry of list
 */
char *
list_shift(list_t l)
{
	char *word = NULL;
	int i;

	assert(l->magic == LIST_MAGIC);
	if (l->nitems) {
		word = l->data[0];
		for (i = 0; i < l->nitems; i++)
			l->data[i] = l->data[i + 1];
		l->nitems--;
	}

	return word;
}


/*
 * Given a list of separators and a string, generate a list
 *   sep (IN)	string containing separater characters
 *   str (IN)	string containing tokens and separators
 *   RETURN 	new list containing all tokens
 */
list_t 
list_split(char *sep, char *str)
{
	list_t new = list_new();
	char *tok;

	if (sep == NULL)
		sep = SPACES;

	while ((tok = _next_tok(sep, &str)) != NULL) {
		if (strlen(tok) > 0)
			list_push(new, tok);
	} 
			
	return new;
}

/* 
 * Opposite of split (caller responsible for freeing result).  
 *   sep (IN)	string containing separater characters
 *   l (IN)	list
 */
char *
list_join(char *sep, list_t l)
{
	int i;
	char *result = NULL;

	assert(l->magic == LIST_MAGIC);
	for (i = 0; i < l->nitems; i++) {
		if (result != NULL)	/* add separator if not first token */
			xstrcat(&result, sep);
		xstrcat(&result, l->data[i]);
	}

	return result;
}	
	

/*
 * Dump a list, for debugging 
 *   l (IN)	list
 */
void 
list_dump(list_t l)
{
	int i;

	assert(l->magic == LIST_MAGIC);
	out("size   = %d\n", l->size);
	out("nitems = %d\n", l->nitems);
	for (i = 0; i < l->nitems; i++) 
		out("data[%d] = `%s'\n", i, l->data[i]);
}	

/* 
 * Push the contents of list l2 onto list l1.
 *   l1 (IN)	target list
 *   l2 (IN)	source list
 */
void 
list_pushl(list_t l1, list_t l2)
{
	int i;

	assert(l1->magic == LIST_MAGIC);
	assert(l2->magic == LIST_MAGIC);
	for (i = 0; i < l2->nitems; i++)
		list_push(l1, l2->data[i]);
}

/* 
 * Return true if item is found in list (not a substring--a complete match)
 *   l (IN)	list to be searched
 *   item (IN)	string to look for 
 */
int 
list_test(list_t l, char *item)
{
	int i, found = 0;

	assert(l->magic == LIST_MAGIC);
	for (i = 0; i < l->nitems; i++)	
		if (!strcmp(l->data[i], item)) {
			found = 1;
			break;	
		}
	return found;
}

/*
 * Remove the items in l2 from l1.
 *   l1 (IN/OUT)	first list
 *   l2 (IN)		list of items to be deleted from l1
 */
void
list_subtract(list_t l1, list_t l2)
{
	int i;

	assert(l1->magic == LIST_MAGIC);
	assert(l2->magic == LIST_MAGIC);
	for (i = 0; i < l1->nitems; i++)
		if (list_test(l2, l1->data[i]))
			list_del(l1, i--);
}

/* 
 * Similar to list_pushl(), but only items not already found in l1 are pushed
 * from l2 to l1.
 *   l1 (IN)	target list
 *   l2 (IN)	source list
 */
void 
list_merge(list_t l1, list_t l2)
{
	int i;

	assert(l1->magic == LIST_MAGIC);
	assert(l2->magic == LIST_MAGIC);
	for (i = 0; i < l2->nitems; i++)
		if (!list_test(l1, l2->data[i]))
			list_push(l1, l2->data[i]);
}

/*
 * Return the number of items in a list.
 *   l (IN)	target list
 *   RETURN	number of items
 */
int 
list_length(list_t l)
{
	assert(l->magic == LIST_MAGIC);
	return l->nitems;
}

/* 
 * Return the nth element of a list.
 *   l (IN)	list
 *   n (IN)	index into list
 *   RETURN	pointer to element
 */
char *
list_nth(list_t l, int n)
{
	assert(l->magic == LIST_MAGIC);
	assert(n < l->nitems && n >= 0);
	return l->data[n];
}

/* 
 * Delete the nth element of the list (freeing deleted element).
 *   l (IN)	list
 *   n (IN)	index into list
 */
void 
list_del(list_t l, int n)
{
	int i;
	assert(l->magic == LIST_MAGIC);
	assert(n < l->nitems && n >= 0);
	Free((void **)&(l->data[n]));		/* free the string */
	for (i = n; i < l->nitems - 1; i++)
		l->data[i] = l->data[i + 1];
	l->nitems--;
}
