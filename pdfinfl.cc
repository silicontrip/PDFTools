/*
Copyright (c) 1996-2007 Han The Thanh, <thanh@pdftex.org>

This file is part of pdfTeX.

pdfTeX is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

pdfTeX is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with pdfTeX; if not, write to the Free Software Foundation, Inc., 51
Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef POPPLER_VERSION
#define GString GooString
#define xpdfVersion POPPLER_VERSION
#include <dirent.h>
#include <goo/GooString.h>
#include <goo/gmem.h>
#include <goo/gfile.h>
#else
#include <aconf.h>
#include <assert.h>
#include <GString.h>
#include <gmem.h>
#include <gfile.h>
#endif

#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "GfxFont.h"
#include "PDFDoc.h"
#include "GlobalParams.h"
#include "Error.h"

static XRef *xref = 0;

int main(int argc, char *argv[])
{
    char *p, buf[1024];
    PDFDoc *doc;
    GString *fileName;
    Stream *s;
	Dict *d;
    Object srcStream, srcName, catalogDict,streamDict;
    FILE *outfile = stdout;
    char *outname;
    int objnum = 0, objgen = 0;
    bool extract_xref_table = false;
    int c;

    if (argc < 2) {
        fprintf(stderr,
                "Usage: pdftosrc <PDF-file>\n");
        exit(1);
    }
    fileName = new GString(argv[1]);
    globalParams = new GlobalParams();
    doc = new PDFDoc(fileName);
    if (!doc->isOk()) {
        fprintf(stderr, "Invalid PDF file\n");
        exit(1);
    }
    if (argc >= 3) {
        objnum = atoi(argv[2]);
        if (argc >= 4)
            objgen = atoi(argv[3]);
    }
    xref = doc->getXRef();
    catalogDict.initNull();
    xref->getCatalog(&catalogDict);
    if (!catalogDict.isDict("Catalog")) {
        fprintf(stderr, "No Catalog found\n");
        exit(1);
    }

    srcStream.initNull();

	fprintf (outfile,"%%PDF-1.7\n%%íì¦'\n");
	for(objnum=1; objnum<=xref->getNumObjects();objnum++) {

		xref->fetch(objnum,objgen,&srcStream);
		if (!srcStream.isNull()) {
			fprintf (outfile,"%d %d obj\n",objnum,objgen);


			if (srcStream.isStream()) {
				s = srcStream.getStream();
				s->reset();
				streamDict.initDict(s->getDict());
				
				streamDict.print(outfile); 
				fprintf(outfile,"stream\n");
				while ((c = s->getChar()) != EOF)
					fputc(c, outfile);
				fprintf(outfile,"\nendstream");
				srcStream.free();
			} else {
				srcStream.print(outfile);
			}
			fprintf(outfile,"\nendobj\n");
		}
	}

// output xref and trailer

        int size = xref->getSize();
        int i;
        for (i = 0; i < size; i++) {
            if (xref->getEntry(i)->offset == 0xffffffff)
                break;
        }
        size = i;
        fprintf(outfile, "xref\n");
        fprintf(outfile, "0 %i\n", size);
        for (i = 0; i < size; i++) {
            XRefEntry *e = xref->getEntry(i);
            if (e->type != xrefEntryCompressed)
                fprintf(outfile, "%.10lu %.5i %s\n",
                        (long unsigned) e->offset, e->gen,
                        (e->type == xrefEntryFree ? "f" : "n"));
            else {              // e->offset is the object number of the object stream
                // e->gen is the local index inside that object stream
                //int objStrOffset = xref->getEntry(e->offset)->offset;
                Object tmpObj;

                xref->fetch(i, e->gen, &tmpObj);        // to ensure xref->objStr is set
                ObjectStream *objStr = xref->getObjStr();
                assert(objStr != NULL);
                int *localOffsets = objStr->getOffsets();
                assert(localOffsets != NULL);
//                 fprintf(outfile, "%0.10lu %i n\n",
//                         (long unsigned) (objStrOffset), e->gen);
                fprintf(outfile, "%.10lu 00000 n\n",
                        (long unsigned) (objStr->getFirstOffset() +
                                         localOffsets[e->gen]));
//                         (long unsigned) (objStrOffset + objStr->getStart() + localOffsets[e->gen]));
                tmpObj.free();
            }
        }

// print trailer
	fprintf(outfile,"trailer\n");
	xref->getTrailerDict()->print(outfile);


fprintf (outfile,"\n%%EOF\n");

    catalogDict.free();
    delete doc;
    delete globalParams;
}
