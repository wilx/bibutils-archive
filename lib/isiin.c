/*
 * isiin.c
 *
 * Copyright (c) Chris Putnam 2004-2016
 *
 * Program and source code released under the GPL version 2
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "is_ws.h"
#include "newstr.h"
#include "newstr_conv.h"
#include "fields.h"
#include "name.h"
#include "title.h"
#include "serialno.h"
#include "reftypes.h"
#include "isiin.h"

/*****************************************************
 PUBLIC: void isiin_initparams()
*****************************************************/
void
isiin_initparams( param *p, const char *progname )
{
	p->readformat       = BIBL_ISIIN;
	p->charsetin        = BIBL_CHARSET_DEFAULT;
	p->charsetin_src    = BIBL_SRC_DEFAULT;
	p->latexin          = 0;
	p->xmlin            = 0;
	p->utf8in           = 0;
	p->nosplittitle     = 0;
	p->verbose          = 0;
	p->addcount         = 0;
	p->output_raw       = 0;

	p->readf    = isiin_readf;
	p->processf = isiin_processf;
	p->cleanf   = NULL;
	p->typef    = isiin_typef;
	p->convertf = isiin_convertf;
	p->all      = isi_all;
	p->nall     = isi_nall;

	list_init( &(p->asis) );
	list_init( &(p->corps) );

	if ( !progname ) p->progname = NULL;
	else p->progname = strdup( progname );
}

/*****************************************************
 PUBLIC: int isiin_readf()
*****************************************************/

/* ISI definition of a tag is strict:
 *   char 1 = uppercase alphabetic character
 *   char 2 = uppercase alphabetic character or digit
 */
static int
isiin_istag( char *buf )
{
	if ( ! (buf[0]>='A' && buf[0]<='Z') ) return 0;
	if ( ! (((buf[1]>='A' && buf[1]<='Z'))||(buf[1]>='0'&&buf[1]<='9')))
		return 0;
	return 1;
}

static int
readmore( FILE *fp, char *buf, int bufsize, int *bufpos, newstr *line )
{
	if ( line->len ) return 1;
	else return newstr_fget( fp, buf, bufsize, bufpos, line );
}

int
isiin_readf( FILE *fp, char *buf, int bufsize, int *bufpos, newstr *line, newstr *reference, int *fcharset )
{
	int haveref = 0, inref = 0;
	char *p;
	*fcharset = CHARSET_UNKNOWN;
	while ( !haveref && readmore( fp, buf, bufsize, bufpos, line ) ) {
		if ( !line->data ) continue;
		p = &(line->data[0]);
		/* Recognize UTF8 BOM */
		if ( line->len > 2 &&
				(unsigned char)(p[0])==0xEF &&
				(unsigned char)(p[1])==0xBB &&
				(unsigned char)(p[2])==0xBF ) {
			*fcharset = CHARSET_UNICODE;
			p += 3;
		}
		/* Each reference ends with 'ER ' */
		if ( isiin_istag( p ) ) {
			if ( !strncmp( p, "FN ", 3 ) ) {
				if (strncasecmp( p, "FN ISI Export Format",20)){
					fprintf( stderr, ": warning file FN type not '%s' not recognized.\n", /*r->progname,*/ p );
				}
			} else if ( !strncmp( p, "VR ", 3 ) ) {
				if ( strncasecmp( p, "VR 1.0", 6 ) ) {
					fprintf(stderr,": warning file version number '%s' not recognized, expected 'VR 1.0'\n", /*r->progname,*/ p );
				}
			} else if ( !strncmp( p, "ER", 2 ) ) haveref = 1;
			else {
				newstr_addchar( reference, '\n' );
				newstr_strcat( reference, p );
				inref = 1;
			}
			newstr_empty( line );
		}
		/* not a tag, but we'll append to the last values */
		else if ( inref ) {
			newstr_addchar( reference, '\n' );
			newstr_strcat( reference, p );
			newstr_empty( line );
		}
		else {
			newstr_empty( line );
		}
	}
	return haveref;
}

/*****************************************************
 PUBLIC: int isiin_processf()
*****************************************************/

static char *
process_isiline( newstr *tag, newstr *data, char *p )
{
	int i;

	/* collect tag and skip past it */
	i = 0;
	while ( i<2 && *p && *p!='\r' && *p!='\n') {
		newstr_addchar( tag, *p++ );
		i++;
	}
	while ( *p==' ' || *p=='\t' ) p++;
	while ( *p && *p!='\r' && *p!='\n' )
		newstr_addchar( data, *p++ );
	newstr_trimendingws( data );
	while ( *p=='\r' || *p=='\n' ) p++;
	return p;
}

int
isiin_processf( fields *isiin, char *p, char *filename, long nref )
{
	int status, n, ret = 1;
	newstr tag, data;
	newstrs_init( &tag, &data, NULL );
	while ( *p ) {
		newstrs_empty( &tag, &data, NULL );
		p = process_isiline( &tag, &data, p );
		if ( !data.len ) continue;
		if ( (tag.len>1) && isiin_istag( tag.data ) ) {
			status = fields_add( isiin, tag.data, data.data, 0 );
			if ( status!=FIELDS_OK ) {
				ret = 0;
				goto out;
			}
		} else {
			n = fields_num( isiin );
			if ( n>0 ) {
				/* only one AU or AF for list of authors */
				if ( !strcmp( isiin->tag[n-1].data,"AU") ){
					status = fields_add( isiin, "AU", data.data, 0);
					if ( status!=FIELDS_OK ) ret = 0;
				} else if ( !strcmp( isiin->tag[n-1].data,"AF") ){
					status = fields_add( isiin, "AF", data.data, 0);
					if ( status!=FIELDS_OK ) ret = 0;
				}
				/* otherwise append multiline data */
				else {
					newstr_addchar( &(isiin->data[n-1]),' ');
					newstr_strcat( &(isiin->data[n-1]), data.data );
					if ( newstr_memerr( &(isiin->data[n-1]) ) ) ret = 0;
				}
				if ( ret==0 ) goto out;
			}
		}
	}
out:
	newstrs_free( &data, &tag, NULL );
	return ret;
}

/*****************************************************
 PUBLIC: int isiin_typef()
*****************************************************/
int
isiin_typef( fields *isiin, char *filename, int nref, param *p, variants *all, int nall )
{
	char *refnum = "";
	int n, reftype, nrefnum;
	n = fields_find( isiin, "PT", 0 );
	nrefnum = fields_find ( isiin, "UT", 0 );
	if ( nrefnum!=-1 ) refnum = isiin->data[nrefnum].data;
	if ( n!=-1 )
		reftype = get_reftype( (isiin->data[n]).data, nref, p->progname, all, nall, refnum );
	else
		reftype = get_reftype( "", nref, p->progname, all, nall, refnum ); /* default */
	return reftype;
}

/*****************************************************
 PUBLIC: int isiin_convertf(), returns BIBL_OK or BIBL_ERR_MEMERR
*****************************************************/

static int
isiin_keyword_process( fields *info, char *newtag, char *p, int level )
{
	int fstatus, status = BIBL_OK;
	newstr keyword;
	newstr_init( &keyword );
	while ( *p ) {
		p = newstr_cpytodelim( &keyword, skip_ws( p ), ";", 1 );
		if ( newstr_memerr( &keyword ) ) { status = BIBL_ERR_MEMERR; goto out; }
		if ( keyword.len ) {
			fstatus = fields_add( info, newtag, keyword.data, level );
			if ( fstatus!=FIELDS_OK ) { status = BIBL_ERR_MEMERR; goto out; }
		}
	}
out:
	newstr_free( &keyword );
	return status;
}

/* pull off authors first--use AF before AU */
static int
isiin_addauthors( fields *isiin, fields *info, int reftype, variants *all, int nall, list *asis, list *corps )
{
	char *newtag, *authortype, use_af[]="AF", use_au[]="AU";
	int level, i, n, has_af=0, has_au=0, nfields, ok;
	newstr *t, *d;

	nfields = fields_num( isiin );
	for ( i=0; i<nfields && has_af==0; ++i ) {
		t = fields_tag( isiin, i, FIELDS_STRP );
		if ( !strcasecmp( t->data, "AU" ) ) has_au++;
		if ( !strcasecmp( t->data, "AF" ) ) has_af++;
	}
	if ( has_af ) authortype = use_af;
	else authortype = use_au;
	for ( i=0; i<nfields; ++i ) {
		t = fields_tag( isiin, i, FIELDS_STRP );
		if ( !strcasecmp( t->data, "AU" ) ) has_au++;
		if ( strcasecmp( t->data, authortype ) ) continue;
		d = fields_value( isiin, i, FIELDS_STRP );
		n = process_findoldtag( authortype, reftype, all, nall );
		level = ((all[reftype]).tags[n]).level;
		newtag = all[reftype].tags[n].newstr;
		ok = name_add( info, newtag, d->data, level, asis, corps );
		if ( !ok ) return BIBL_ERR_MEMERR;
	}
	return BIBL_OK;
}

static void
isiin_report_notag( param *p, char *tag )
{
	if ( p->verbose && strcmp( tag, "PT" ) ) {
		if ( p->progname ) fprintf( stderr, "%s: ", p->progname );
		fprintf( stderr, "Did not identify ISI tag '%s'\n", tag );
	}
}

static int
isiin_simple( fields *info, char *tag, char *value, int level )
{
	int fstatus = fields_add( info, tag, value, level );
	if ( fstatus==FIELDS_OK ) return BIBL_OK;
	else return BIBL_ERR_MEMERR;
}

int
isiin_convertf( fields *isiin, fields *info, int reftype, param *p, variants *all, int nall )
{
	int process, level, i, n, nfields, ok, status;
	newstr *t, *d;
	char *newtag;

	status = isiin_addauthors( isiin, info, reftype, all, nall, &(p->asis), &(p->corps) );
	if ( status!=BIBL_OK ) return status;

	nfields = fields_num( isiin );
	for ( i=0; i<nfields; ++i ) {

		t = fields_tag( isiin, i, FIELDS_STRP );
		if ( !strcasecmp( t->data, "AU" ) || !strcasecmp( t->data, "AF" ) )
			continue;

		n = translate_oldtag( t->data, reftype, all, nall, &process, &level, &newtag );
		if ( n==-1 ) {
			isiin_report_notag( p, t->data );
			continue;
		}
		if ( process == ALWAYS ) continue; /* add in core code */

		d = fields_value( isiin, i, FIELDS_STRP );

		switch ( process ) {

		case SIMPLE:
		case DATE:
			status = isiin_simple( info, newtag, d->data, level );
			break;

		case PERSON:
			ok = name_add( info, newtag, d->data, level, &(p->asis), &(p->corps) );
			if ( ok ) status = BIBL_OK;
			else status = BIBL_ERR_MEMERR;
			break;

		case TITLE:
			ok = title_process( info, newtag, d->data, level, p->nosplittitle );
			if ( ok ) status = BIBL_OK;
			else status = BIBL_ERR_MEMERR;
			break;

		case KEYWORD:
			status = isiin_keyword_process( info, newtag, d->data, level );
			break;

		case SERIALNO:
			ok = addsn( info, d->data, level );
			if ( ok ) status = BIBL_OK;
			else status = BIBL_ERR_MEMERR;
			break;

		/* do nothing if process==TYPE || process==ALWAYS */
		default:
			status = BIBL_OK;
			break;

		}

		if ( status!=BIBL_OK ) return status;
	}

	return status;
}
