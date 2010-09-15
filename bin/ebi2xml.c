/*
 * ebi2xml.c
 * 
 * Copyright (c) Chris Putnam 2004-2010
 *
 * Source code and program released under the GPL
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include "bibutils.h"
#include "tomods.h"
#include "bibprog.h"

char help1[] =  "Converts an EBI XML file into MODS XML\n\n";
char help2[] = "ebi_file";

int
main( int argc, char *argv[] )
{
	param p;
	bibl_initparams( &p, BIBL_EBIIN, BIBL_MODSOUT, "ebi2xml" );
	tomods_processargs( &argc, argv, &p, help1, help2 );
	bibprog( argc, argv, &p );
	bibl_freeparams( &p );
	return EXIT_SUCCESS;
}
