/*
 * list.h
 *
 * version: 2016-11-03
 *
 * Copyright (c) Chris Putnam 2004-2016
 *
 * Source code released under the GPL version 2
 *
 */

#ifndef LISTS_H
#define LISTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "newstr.h"

#define LIST_ERR (0)
#define LIST_ERR_CANNOTOPEN (-1)
#define LIST_OK  (1)

#define LIST_CHR (0)
#define LIST_STR (1)

typedef struct list {
	int n, max;
	newstr *str;
	unsigned char sorted;
} list;


void    lists_init( list *a, ... );
void    lists_free( list *a, ... );
void    lists_empty( list *a, ... );

void    list_init( list *a );
int     list_init_values ( list *a, ... );
int     list_init_valuesc( list *a, ... );
void    list_free( list *a );
void    list_empty( list *a );

list *  list_new( void );
void    list_delete( list * );

list*   list_dup( list *a );
int     list_copy( list *to, list *from );

newstr * list_addvp( list *a, unsigned char mode, void *vp );
newstr * list_addc( list *a, const char *value );
newstr * list_add( list *a, newstr *value );

int      list_addvp_all( list *a, unsigned char mode, ... );
int      list_addc_all( list *a, ... );
int      list_add_all( list *a, ... );

newstr * list_addvp_unique( list *a, unsigned char mode, void *vp );
newstr * list_addc_unique( list *a, const char *value );
newstr * list_add_unique( list *a, newstr *value );

int     list_append( list *a, list *toadd );
int     list_append_unique( list *a, list *toadd );

int     list_remove( list *a, int n );

newstr* list_get( list *a, int n );
char*   list_getc( list *a, int n );

newstr* list_set( list *a, int n, newstr *s );
newstr* list_setc( list *a, int n, const char *s );

void    list_sort( list *a );

void    list_swap( list *a, int n1, int n2 );

int     list_find( list *a, const char *searchstr );
int     list_findnocase( list *a, const char *searchstr );
int     list_match_entry( list *a, int n, char *s );
void    list_trimend( list *a, int n );

int     list_fill( list *a, const char *filename, unsigned char skip_blank_lines );
int     list_fillfp( list *a, FILE *fp, unsigned char skip_blank_lines );
int     list_tokenize( list *tokens, newstr *in, const char *delim, int merge_delim );
int     list_tokenizec( list *tokens, char *p, const char *delim, int merge_delim );

#endif
