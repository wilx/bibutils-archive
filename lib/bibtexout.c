/*
 * bibtexout.c
 *
 * Copyright (c) Chris Putnam 2003-2016
 *
 * Program and source code released under the GPL version 2
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "newstr.h"
#include "strsearch.h"
#include "utf8.h"
#include "xml.h"
#include "fields.h"
#include "name.h"
#include "title.h"
#include "url.h"
#include "bibformats.h"

static int  bibtexout_write( fields *in, FILE *fp, param *p, unsigned long refnum );
static void bibtexout_writeheader( FILE *outptr, param *p );

void
bibtexout_initparams( param *p, const char *progname )
{
	p->writeformat      = BIBL_BIBTEXOUT;
	p->format_opts      = 0;
	p->charsetout       = BIBL_CHARSET_DEFAULT;
	p->charsetout_src   = BIBL_SRC_DEFAULT;
	p->latexout         = 1;
	p->utf8out          = BIBL_CHARSET_UTF8_DEFAULT;
	p->utf8bom          = BIBL_CHARSET_BOM_DEFAULT;
	p->xmlout           = BIBL_XMLOUT_FALSE;
	p->nosplittitle     = 0;
	p->verbose          = 0;
	p->addcount         = 0;
	p->singlerefperfile = 0;

	p->headerf = bibtexout_writeheader;
	p->footerf = NULL;
	p->writef  = bibtexout_write;

	if ( !p->progname && progname )
		p->progname = strdup( progname );
}

enum {
	TYPE_UNKNOWN = 0,
	TYPE_ARTICLE,
	TYPE_INBOOK,
	TYPE_INPROCEEDINGS,
	TYPE_PROCEEDINGS,
	TYPE_INCOLLECTION,
	TYPE_COLLECTION,
	TYPE_BOOK,
	TYPE_PHDTHESIS,
	TYPE_MASTERSTHESIS,
	TYPE_REPORT,
	TYPE_MANUAL,
	TYPE_UNPUBLISHED,
	TYPE_ELECTRONIC,
	TYPE_MISC,
	NUM_TYPES
};

static int
bibtexout_type( fields *in, char *filename, int refnum, param *p )
{
	int type = TYPE_UNKNOWN, i, maxlevel, n, level;
	char *tag, *genre;

	/* determine bibliography type */
	for ( i=0; i<in->n; ++i ) {
		tag = fields_tag( in, i, FIELDS_CHRP );
		if ( strcasecmp( tag, "GENRE" ) && strcasecmp( tag, "NGENRE" ) ) continue;
		genre = fields_value( in, i, FIELDS_CHRP );
		level = in->level[i];
		if ( !strcasecmp( genre, "periodical" ) ||
		     !strcasecmp( genre, "academic journal" ) ||
		     !strcasecmp( genre, "magazine" ) ||
		     !strcasecmp( genre, "newspaper" ) ||
		     !strcasecmp( genre, "article" ) )
			type = TYPE_ARTICLE;
		else if ( !strcasecmp( genre, "instruction" ) )
			type = TYPE_MANUAL;
		else if ( !strcasecmp( genre, "unpublished" ) )
			type = TYPE_UNPUBLISHED;
		else if ( !strcasecmp( genre, "conference publication" ) ) {
			if ( level==0 ) type=TYPE_PROCEEDINGS;
			else type = TYPE_INPROCEEDINGS;
		} else if ( !strcasecmp( genre, "collection" ) ) {
			if ( level==0 ) type=TYPE_COLLECTION;
			else type = TYPE_INCOLLECTION;
		} else if ( !strcasecmp( genre, "report" ) )
			type = TYPE_REPORT;
		else if ( !strcasecmp( genre, "book chapter" ) )
			type = TYPE_INBOOK;
		else if ( !strcasecmp( genre, "book" ) ) {
			if ( level==0 ) type = TYPE_BOOK;
			else type = TYPE_INBOOK;
		} else if ( !strcasecmp( genre, "thesis" ) ) {
			if ( type==TYPE_UNKNOWN ) type=TYPE_PHDTHESIS;
		} else if ( !strcasecmp( genre, "Ph.D. thesis" ) )
			type = TYPE_PHDTHESIS;
		else if ( !strcasecmp( genre, "Masters thesis" ) )
			type = TYPE_MASTERSTHESIS;
		else  if ( !strcasecmp( genre, "electronic" ) )
			type = TYPE_ELECTRONIC;
	}
	if ( type==TYPE_UNKNOWN ) {
		for ( i=0; i<in->n; ++i ) {
			tag = fields_tag( in, i, FIELDS_CHRP );
			if ( strcasecmp( tag, "ISSUANCE" ) ) continue;
			genre = fields_value( in, i, FIELDS_CHRP );
			if ( !strcasecmp( genre, "monographic" ) ) {
				if ( in->level[i]==0 ) type = TYPE_BOOK;
				else if ( in->level[i]==1 ) type = TYPE_MISC;
			}
		}
	}

	/* default to TYPE_MISC */
	if ( type==TYPE_UNKNOWN ) {
		maxlevel = fields_maxlevel( in );
		if ( maxlevel > 0 ) type = TYPE_MISC;
		else {
			if ( p->progname ) fprintf( stderr, "%s: ", p->progname );
			fprintf( stderr, "Cannot identify TYPE in reference %d ", refnum+1 );
			n = fields_find( in, "REFNUM", LEVEL_ANY );
			if ( n!=-1 ) 
				fprintf( stderr, " %s", (char*) fields_value( in, n, FIELDS_CHRP ) );
			fprintf( stderr, " (defaulting to @Misc)\n" );
			type = TYPE_MISC;
		}
	}
	return type;
}

static void
output( FILE *fp, fields *out, int format_opts )
{
	int i, j, len, nquotes;
	char *tag, *value, ch;

	/* ...output type information "@article{" */
	value = ( char * ) fields_value( out, 0, FIELDS_CHRP );
	if ( !(format_opts & BIBL_FORMAT_BIBOUT_UPPERCASE) ) fprintf( fp, "@%s{", value );
	else {
		len = strlen( value );
		fprintf( fp, "@" );
		for ( i=0; i<len; ++i )
			fprintf( fp, "%c", toupper((unsigned char)value[i]) );
		fprintf( fp, "{" );
	}

	/* ...output refnum "Smith2001" */
	value = ( char * ) fields_value( out, 1, FIELDS_CHRP );
	fprintf( fp, "%s", value );

	/* ...rest of the references */
	for ( j=2; j<out->n; ++j ) {
		nquotes = 0;
		tag   = ( char * ) fields_tag( out, j, FIELDS_CHRP );
		value = ( char * ) fields_value( out, j, FIELDS_CHRP );
		fprintf( fp, ",\n" );
		if ( format_opts & BIBL_FORMAT_BIBOUT_WHITESPACE ) fprintf( fp, "  " );
		if ( !(format_opts & BIBL_FORMAT_BIBOUT_UPPERCASE ) ) fprintf( fp, "%s", tag );
		else {
			len = strlen( tag );
			for ( i=0; i<len; ++i )
				fprintf( fp, "%c", toupper((unsigned char)tag[i]) );
		}
		if ( format_opts & BIBL_FORMAT_BIBOUT_WHITESPACE ) fprintf( fp, " = \t" );
		else fprintf( fp, "=" );

		if ( format_opts & BIBL_FORMAT_BIBOUT_BRACKETS ) fprintf( fp, "{" );
		else fprintf( fp, "\"" );

		len = strlen( value );
		for ( i=0; i<len; ++i ) {
			ch = value[i];
			if ( ch!='\"' ) fprintf( fp, "%c", ch );
			else {
				if ( format_opts & BIBL_FORMAT_BIBOUT_BRACKETS || ( i>0 && value[i-1]=='\\' ) )
					fprintf( fp, "\"" );
				else {
					if ( nquotes % 2 == 0 )
						fprintf( fp, "``" );
					else    fprintf( fp, "\'\'" );
					nquotes++;
				}
			}
		}

		if ( format_opts & BIBL_FORMAT_BIBOUT_BRACKETS ) fprintf( fp, "}" );
		else fprintf( fp, "\"" );
	}

	/* ...finish reference */
	if ( format_opts & BIBL_FORMAT_BIBOUT_FINALCOMMA ) fprintf( fp, "," );
	fprintf( fp, "\n}\n\n" );

	fflush( fp );
}

static void
append_type( int type, fields *out, int *status )
{
	char *typenames[ NUM_TYPES ] = {
		[ TYPE_ARTICLE       ] = "Article",
		[ TYPE_INBOOK        ] = "Inbook",
		[ TYPE_PROCEEDINGS   ] = "Proceedings",
		[ TYPE_INPROCEEDINGS ] = "InProceedings",
		[ TYPE_BOOK          ] = "Book",
		[ TYPE_PHDTHESIS     ] = "PhdThesis",
		[ TYPE_MASTERSTHESIS ] = "MastersThesis",
		[ TYPE_REPORT        ] = "TechReport",
		[ TYPE_MANUAL        ] = "Manual",
		[ TYPE_COLLECTION    ] = "Collection",
		[ TYPE_INCOLLECTION  ] = "InCollection",
		[ TYPE_UNPUBLISHED   ] = "Unpublished",
		[ TYPE_ELECTRONIC    ] = "Electronic",
		[ TYPE_MISC          ] = "Misc",
	};
	int fstatus;
	char *s;

	if ( type < 0 || type >= NUM_TYPES ) type = TYPE_MISC;
	s = typenames[ type ];

	fstatus = fields_add( out, "TYPE", s, LEVEL_MAIN );
	if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
}

static void
append_citekey( fields *in, fields *out, int format_opts, int *status )
{
	int n, fstatus;
	newstr s;
	char *p;

	n = fields_find( in, "REFNUM", LEVEL_ANY );
	if ( ( format_opts & BIBL_FORMAT_BIBOUT_DROPKEY ) || n==-1 ) {
		fstatus = fields_add( out, "REFNUM", "", LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}

	else {
		newstr_init( &s );
		p = fields_value( in, n, FIELDS_CHRP );
		while ( p && *p && *p!='|' ) {
			if ( format_opts & BIBL_FORMAT_BIBOUT_STRICTKEY ) {
				if ( isdigit((unsigned char)*p) || (*p>='A' && *p<='Z') ||
				     (*p>='a' && *p<='z' ) ) {
					newstr_addchar( &s, *p );
				}
			}
			else {
				if ( *p!=' ' && *p!='\t' ) {
					newstr_addchar( &s, *p );
				}
			}
			p++;
		}
		fstatus = fields_add( out, "REFNUM", newstr_cstr( &s ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
		newstr_free( &s );
	}
}

static void
append_simple( fields *in, char *intag, char *outtag, fields *out, int *status )
{
	int n, fstatus;

	n = fields_find( in, intag, LEVEL_ANY );
	if ( n!=-1 ) {
		fields_setused( in, n );
		fstatus = fields_add( out, outtag, fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
}

static void
append_simpleall( fields *in, char *intag, char *outtag, fields *out, int *status )
{
	int i, fstatus;

	for ( i=0; i<in->n; ++i ) {
		if ( fields_match_tag( in, i, intag ) ) {
			fields_setused( in, i );
			fstatus = fields_add( out, outtag, fields_value( in, i, FIELDS_CHRP ), LEVEL_MAIN );
			if ( fstatus!=FIELDS_OK ) {
				*status = BIBL_ERR_MEMERR;
				return;
			}
		}
	}
}

static void
append_keywords( fields *in, fields *out, int *status )
{
	newstr keywords, *word;
	int i, fstatus;
	vplist a;

	newstr_init( &keywords );
	vplist_init( &a );

	fields_findv_each( in, LEVEL_ANY, FIELDS_STRP, &a, "KEYWORD" );

	if ( a.n ) {

		for ( i=0; i<a.n; ++i ) {
			word = vplist_get( &a, i );
			if ( i>0 ) newstr_strcat( &keywords, "; " );
			newstr_newstrcat( &keywords, word );
		}

		fstatus = fields_add( out, "keywords", keywords.data, LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			goto out;
		}


	}

out:
	newstr_free( &keywords );
	vplist_free( &a );
}

static void
append_fileattach( fields *in, fields *out, int *status )
{
	int i, fstatus;
	newstr data;
	char *tag;

	newstr_init( &data );
	for ( i=0; i<in->n; ++i ) {
		tag = fields_tag( in, i, FIELDS_CHRP );
		if ( strcasecmp( tag, "FILEATTACH" ) ) continue;
		newstr_strcpy( &data, ":" );
		newstr_newstrcat( &data, &(in->data[i]) );
		if ( strsearch( in->data[i].data, ".pdf" ) )
			newstr_strcat( &data, ":PDF" );
		else if ( strsearch( in->data[i].data, ".html" ) )
			newstr_strcat( &data, ":HTML" );
		else newstr_strcat( &data, ":TYPE" );
		fields_setused( in, i );
		fstatus = fields_add( out, "file", data.data, LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			goto out;
		}
		newstr_empty( &data );
	}
out:
	newstr_free( &data );
}

static void
append_people( fields *in, char *tag, char *ctag, char *atag,
		char *bibtag, int level, fields *out, int format_opts )
{
	newstr allpeople, oneperson;
	int i, npeople, person, corp, asis;

	newstrs_init( &allpeople, &oneperson, NULL );

	/* primary citation authors */
	npeople = 0;
	for ( i=0; i<in->n; ++i ) {
		if ( level!=LEVEL_ANY && in->level[i]!=level ) continue;
		person = ( strcasecmp( in->tag[i].data, tag ) == 0 );
		corp   = ( strcasecmp( in->tag[i].data, ctag ) == 0 );
		asis   = ( strcasecmp( in->tag[i].data, atag ) == 0 );
		if ( person || corp || asis ) {
			if ( npeople>0 ) {
				if ( format_opts & BIBL_FORMAT_BIBOUT_WHITESPACE )
					newstr_strcat(&allpeople,"\n\t\tand ");
				else newstr_strcat( &allpeople, "\nand " );
			}
			if ( corp ) {
				newstr_addchar( &allpeople, '{' );
				newstr_strcat( &allpeople, fields_value( in, i, FIELDS_CHRP ) );
				newstr_addchar( &allpeople, '}' );
			} else if ( asis ) {
				newstr_addchar( &allpeople, '{' );
				newstr_strcat( &allpeople, fields_value( in, i, FIELDS_CHRP ) );
				newstr_addchar( &allpeople, '}' );
			} else {
				name_build_withcomma( &oneperson, fields_value( in, i, FIELDS_CHRP ) );
				newstr_newstrcat( &allpeople, &oneperson );
			}
			npeople++;
		}
	}
	if ( npeople ) {
		fields_add( out, bibtag, allpeople.data, LEVEL_MAIN );
	}

	newstrs_free( &allpeople, &oneperson, NULL );
}

static int
append_title_chosen( fields *in, char *bibtag, fields *out, int nmainttl, int nsubttl )
{
	newstr fulltitle, *mainttl = NULL, *subttl = NULL;
	int status, ret = BIBL_OK;

	newstr_init( &fulltitle );

	if ( nmainttl!=-1 ) {
		mainttl = fields_value( in, nmainttl, FIELDS_STRP );
		fields_setused( in, nmainttl );
	}

	if ( nsubttl!=-1 ) {
		subttl = fields_value( in, nsubttl, FIELDS_STRP );
		fields_setused( in, nsubttl );
	}

	title_combine( &fulltitle, mainttl, subttl );

	if ( fulltitle.len ) {
		status = fields_add( out, bibtag, newstr_cstr( &fulltitle ), LEVEL_MAIN );
		if ( status!=FIELDS_OK ) ret = BIBL_ERR_MEMERR;
	}

	newstr_free( &fulltitle );

	return ret;
}

static int
append_title( fields *in, char *bibtag, int level, fields *out, int format_opts )
{
	int title = -1,     short_title = -1;
	int subtitle = -1,  short_subtitle = -1;
	int use_title = -1, use_subtitle = -1;

	title          = fields_find( in, "TITLE",         level );
	short_title    = fields_find( in, "SHORTTITLE",    level );
	subtitle       = fields_find( in, "SUBTITLE",      level );
	short_subtitle = fields_find( in, "SHORTSUBTITLE", level );

	if ( title==-1 || ( ( format_opts & BIBL_FORMAT_BIBOUT_SHORTTITLE ) && level==1 ) ) {
		use_title    = short_title;
		use_subtitle = short_subtitle;
	}

	else {
		use_title    = title;
		use_subtitle = subtitle;
	}

	return append_title_chosen( in, bibtag, out, use_title, use_subtitle );
}

static void
append_titles( fields *in, int type, fields *out, int format_opts, int *status )
{
	/* item=main level title */
	*status = append_title( in, "title", 0, out, format_opts );
	if ( *status!=BIBL_OK ) return;

	switch( type ) {

		case TYPE_ARTICLE:
		*status = append_title( in, "journal", 1, out, format_opts );
		break;

		case TYPE_INBOOK:
		*status = append_title( in, "bookTitle", 1, out, format_opts );
		if ( *status!=BIBL_OK ) return;
		*status = append_title( in, "series",    2, out, format_opts );
		break;

		case TYPE_INCOLLECTION:
		case TYPE_INPROCEEDINGS:
		*status = append_title( in, "booktitle", 1, out, format_opts );
		if ( *status!=BIBL_OK ) return;
		*status = append_title( in, "series",    2, out, format_opts );
		break;

		case TYPE_PHDTHESIS:
		case TYPE_MASTERSTHESIS:
		*status = append_title( in, "series", 1, out, format_opts );
		break;

		case TYPE_BOOK:
		case TYPE_REPORT:
		case TYPE_COLLECTION:
		case TYPE_PROCEEDINGS:
		*status = append_title( in, "series", 1, out, format_opts );
		if ( *status!=BIBL_OK ) return;
		*status = append_title( in, "series", 2, out, format_opts );
		break;

		default:
		/* do nothing */
		break;

	}
}

static int
find_date( fields *in, char *date_element )
{
	char date[100], partdate[100];
	int n;

	sprintf( date, "DATE:%s", date_element );
	n = fields_find( in, date, LEVEL_ANY );

	if ( n==-1 ) {
		sprintf( partdate, "PARTDATE:%s", date_element );
		n = fields_find( in, partdate, LEVEL_ANY );
	}

	return n;
}

static void
append_date( fields *in, fields *out, int *status )
{
	char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	int n, month, fstatus;

	n = find_date( in, "YEAR" );
	if ( n!=-1 ) {
		fields_setused( in, n );
		fstatus = fields_add( out, "year", in->data[n].data, LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			return;
		}
	}

	n = find_date( in, "MONTH" );
	if ( n!=-1 ) {
		fields_setused( in, n );
		month = atoi( in->data[n].data );
		if ( month>0 && month<13 )
			fstatus = fields_add( out, "month", months[month-1], LEVEL_MAIN );
		else
			fstatus = fields_add( out, "month", in->data[n].data, LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			return;
		}
	}

	n = find_date( in, "DAY" );
	if ( n!=-1 ) {
		fields_setused( in, n );
		fstatus = fields_add( out, "day", in->data[n].data, LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			return;
		}
	}

}

static void
append_arxiv( fields *in, fields *out, int *status )
{
	int n, fstatus1, fstatus2;
	newstr url;

	n = fields_find( in, "ARXIV", LEVEL_ANY );
	if ( n==-1 ) return;

	fields_setused( in, n );

	/* ...write:
	 *     archivePrefix = "arXiv",
	 *     eprint = "#####",
	 * ...for people in high energy physics
	 */
	fstatus1 = fields_add( out, "archivePrefix", "arXiv", LEVEL_MAIN );
	fstatus2 = fields_add( out, "eprint", fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
	if ( fstatus1!=FIELDS_OK || fstatus2!=FIELDS_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	}

	/* ...also write:
	 *     url = "http://arxiv.org/abs/####",
	 * ...to maximize compatibility
	 */
	newstr_init( &url );
	arxiv_to_url( in, n, "URL", &url );
	if ( url.len ) {
		fstatus1 = fields_add( out, "url", newstr_cstr( &url ), LEVEL_MAIN );
		if ( fstatus1!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
	newstr_free( &url );
}

static void
append_urls( fields *in, fields *out, int *status )
{
	int lstatus;
	list types;

	lstatus = list_init_valuesc( &types, "URL", "PMID", "PMC", "JSTOR", NULL );
	if ( lstatus!=LIST_OK ) {
		*status = BIBL_ERR_MEMERR;
		return;
	}

	*status = urls_merge_and_add( in, LEVEL_ANY, out, "url", LEVEL_MAIN, &types );

	list_free( &types );
}

static void
append_isi( fields *in, fields *out, int *status )
{
	int n, fstatus;

	n = fields_find( in, "ISIREFNUM", LEVEL_ANY );
	if ( n!=-1 ) {
		fstatus = fields_add( out, "note", fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
}

static int
append_articlenumber( fields *in, fields *out )
{
	int n, fstatus;

	n = fields_find( in, "ARTICLENUMBER", LEVEL_ANY );
	if ( n==-1 ) return BIBL_OK;

	fields_setused( in, n );
	fstatus = fields_add( out, "pages", fields_value( in, n, FIELDS_CHRP ), LEVEL_MAIN );
	if ( fstatus!=FIELDS_OK ) return BIBL_ERR_MEMERR;
	return BIBL_OK;
}

static void
append_pages( fields *in, fields *out, int format_opts, int *status )
{
	int sn, en, fstatus;
	newstr pages;

	sn = fields_find( in, "PAGES:START", LEVEL_ANY );
	en = fields_find( in, "PAGES:STOP",  LEVEL_ANY );
	if ( sn==-1 && en==-1 ) {
		*status = append_articlenumber( in, out );
		return;
	}
	newstr_init( &pages );
	if ( sn!=-1 ) {
		newstr_newstrcat( &pages, fields_value( in, sn, FIELDS_STRP ) );
		fields_setused( in, sn );
	}
	if ( sn!=-1 && en!=-1 ) {
		if ( format_opts & BIBL_FORMAT_BIBOUT_SINGLEDASH )
			newstr_strcat( &pages, "-" );
		else
			newstr_strcat( &pages, "--" );
	}
	if ( en!=-1 ) {
		newstr_newstrcat( &pages, fields_value( in, en, FIELDS_STRP ) );
		fields_setused( in, en );
	}
	fstatus = fields_add( out, "pages", newstr_cstr( &pages ), LEVEL_MAIN );
	if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	newstr_free( &pages );
}

/*
 * from Tim Hicks:
 * I'm no expert on bibtex, but those who know more than I on our mailing 
 * list suggest that 'issue' isn't a recognised key for bibtex and 
 * therefore that bibutils should be aliasing IS to number at some point in 
 * the conversion.
 *
 * Therefore prefer outputting issue/number as number and only keep
 * a distinction if both issue and number are present for a particular
 * reference.
 */

static void
append_issue_number( fields *in, fields *out, int *status )
{
	int nissue  = fields_find( in, "ISSUE",  LEVEL_ANY );
	int nnumber = fields_find( in, "NUMBER", LEVEL_ANY );
	int fstatus;

	if ( nissue!=-1 && nnumber!=-1 ) {
		fields_setused( in, nissue );
		fields_setused( in, nnumber );
		fstatus = fields_add( out, "issue", fields_value( in, nissue, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) {
			*status = BIBL_ERR_MEMERR;
			return;
		}
		fstatus = fields_add( out, "number", fields_value( in, nnumber, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	} else if ( nissue!=-1 ) {
		fields_setused( in, nissue );
		fstatus = fields_add( out, "number", fields_value( in, nissue, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	} else if ( nnumber!=-1 ) {
		fields_setused( in, nnumber );
		fstatus = fields_add( out, "number", fields_value( in, nnumber, FIELDS_CHRP ), LEVEL_MAIN );
		if ( fstatus!=FIELDS_OK ) *status = BIBL_ERR_MEMERR;
	}
}

static int
append_data( fields *in, fields *out, param *p, unsigned long refnum )
{
	int type, status = BIBL_OK;

	type = bibtexout_type( in, "", refnum, p );

	append_type        ( type, out, &status );
	append_citekey     ( in, out, p->format_opts, &status );
	append_people      ( in, "AUTHOR",     "AUTHOR:CORP",     "AUTHOR:ASIS",     "author", 0, out, p->format_opts );
	append_people      ( in, "EDITOR",     "EDITOR:CORP",     "EDITOR:ASIS",     "editor", -1, out, p->format_opts );
	append_people      ( in, "TRANSLATOR", "TRANSLATOR:CORP", "TRANSLATOR:ASIS", "translator", -1, out, p->format_opts );
	append_titles      ( in, type, out, p->format_opts, &status );
	append_date        ( in, out, &status );
	append_simple      ( in, "EDITION",            "edition",   out, &status );
	append_simple      ( in, "PUBLISHER",          "publisher", out, &status );
	append_simple      ( in, "ADDRESS",            "address",   out, &status );
	append_simple      ( in, "VOLUME",             "volume",    out, &status );
	append_issue_number( in, out, &status );
	append_pages       ( in, out, p->format_opts, &status );
	append_keywords    ( in, out, &status );
	append_simple      ( in, "CONTENTS",           "contents",  out, &status );
	append_simple      ( in, "ABSTRACT",           "abstract",  out, &status );
	append_simple      ( in, "LOCATION",           "location",  out, &status );
	append_simple      ( in, "DEGREEGRANTOR",      "school",    out, &status );
	append_simple      ( in, "DEGREEGRANTOR:ASIS", "school",    out, &status );
	append_simple      ( in, "DEGREEGRANTOR:CORP", "school",    out, &status );
	append_simpleall   ( in, "NOTES",              "note",      out, &status );
	append_simpleall   ( in, "ANNOTE",             "annote",    out, &status );
	append_simple      ( in, "ISBN",               "isbn",      out, &status );
	append_simple      ( in, "ISSN",               "issn",      out, &status );
	append_simple      ( in, "MRNUMBER",           "mrnumber",  out, &status );
	append_simple      ( in, "CODEN",              "coden",     out, &status );
	append_simple      ( in, "DOI",                "doi",       out, &status );
	append_urls        ( in, out, &status );
	append_fileattach  ( in, out, &status );
	append_arxiv       ( in, out, &status );
	append_isi         ( in, out, &status );
	append_simple      ( in, "LANGUAGE",           "language",  out, &status );

	return status;
}

static int
bibtexout_write( fields *in, FILE *fp, param *p, unsigned long refnum )
{
	int status;
	fields out;

	fields_init( &out );

	status = append_data( in, &out, p, refnum );
	if ( status==BIBL_OK ) output( fp, &out, p->format_opts );

	fields_free( &out );

	return status;
}

static void
bibtexout_writeheader( FILE *outptr, param *p )
{
	if ( p->utf8bom ) utf8_writebom( outptr );
}

