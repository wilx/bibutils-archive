/*
 * url.h
 *
 * Copyright (c) Chris Putnam 2004-2016
 *
 * Source code released under the GPL version 2
 *
 */
#ifndef URL_H
#define URL_H

#include "list.h"
#include "fields.h"

int is_doi( char *s );
int is_uri_remote_scheme( char *p );
int is_embedded_link( char *s );

void doi_to_url( fields *info, int n, char *urltag, newstr *doi_url );
void pmid_to_url( fields *info, int n, char *urltag, newstr *pmid_url );
void pmc_to_url( fields *info, int n, char *urltag, newstr *pmid_url );
void arxiv_to_url( fields *info, int n, char *urltag, newstr *arxiv_url );
void jstor_to_url( fields *info, int n, char *urltag, newstr *jstor_url );
void mrnumber_to_url( fields *info, int n, char *urltag, newstr *jstor_url );

int urls_merge_and_add( fields *in, int lvl_in, fields *out, char *tag_out, int lvl_out, list *types );
int urls_split_and_add( char *value_in, fields *out, int lvl_out );


#endif
