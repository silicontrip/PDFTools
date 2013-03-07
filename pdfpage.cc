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
static  int *objects;
static int pdfsize;

#define OBLISTSIZE 65536

static int oblistpointer = 0;
static int oblist[OBLISTSIZE];

void dump_obj (int objnum, int objgen, FILE *outfile) {
	
	Object srcStream,streamDict,streamLength,filterType;
	Stream *s,*t;
	int length=0;
	int c;
	char key[32];
	
	srcStream.initNull();
	filterType.initNull();
	
	xref->fetch(objnum,objgen,&srcStream);
	if (!srcStream.isNull()) {
		//	fprintf (stderr,"obj %d @ %d\n",objnum,ftell(outfile));
		objects[objnum] = ftell(outfile);
		fprintf (outfile,"%d %d obj\n",objnum,objgen);
		
		if (srcStream.isStream()) {
			
			// I wonder if there is an easier way.
			//	while ((c = s->getChar()) != EOF)
			//		length++;
			
			//		streamLength.initInt(length);
			// get Base Stream
			s = srcStream.getStream();
			s = s->getUndecodedStream();
			
			streamDict.initDict(s->getDict());
			streamDict.print(outfile); 
			
			s->reset();
			fprintf(outfile,"\nstream\n");
			
			int count =0;
			
			while ((c = s->getChar()) != EOF) {
				fputc(c, outfile); count ++;
			}
			fprintf(outfile,"\nendstream");
			//	fprintf (stderr,"obj %d length %d\n",objnum,count);
			srcStream.free();
		} else {
			srcStream.print(outfile);
		}
		fprintf(outfile,"\nendobj\n");
	}
	
}

void recurse_obj_ref (int objnum, int objgen, FILE *outfile);


void recurse_obj(Object *srcStream,FILE *outfile) {
	
	int n;
	char *key;
	Object obj;
	Stream *s;
	
	
	if (srcStream->isDict()) {
		for (n=0; n<srcStream->dictGetLength(); n++) {
			srcStream->dictGetValNF(n, &obj);
			key = srcStream->dictGetKey(n);
			
			//fprintf(stderr,"type: %s\n",obj.getTypeName());
			
			// we don't want to recurse back up the tree
			if (strcmp(key,"Parent")) {
				if (strcmp(key,"P")) { // appears to be the parent key for annots.
					
					//	fprintf(stderr,"key: %s\n",key);
					
					// need to scan refs, arrays and dicts
					if (obj.isRef()) {
						recurse_obj_ref(obj.getRefNum(),obj.getRefGen(),outfile);
					}
					
					if (obj.isDict() || obj.isArray()) { recurse_obj(&obj,outfile); }
				}
			}/* else {
			 srcStream->dictGetVal(n,&ref);
			 }*/
		}
	} else if (srcStream->isArray()) {
		for (n=0; n<srcStream->arrayGetLength(); n++) {
			srcStream->arrayGetNF(n, &obj);
			if (obj.isRef()) {
				recurse_obj_ref(obj.getRefNum(),obj.getRefGen(),outfile);
			}
			if (obj.isDict() || obj.isArray()) { recurse_obj(&obj,outfile); }
		}
	} else if (srcStream->isStream()) {
		// streams contain Dicts, that can contain refs
		s = srcStream->getStream();
		obj.initDict(s->getDict());
		recurse_obj(&obj,outfile);
		
	}
	
}

void recurse_obj_ref (int objnum, int objgen, FILE *outfile) {
	
	Object obj;
	
	
	//  stop infinite recursion.
	
	if (!objects[objnum]) {
		objects[objnum]=1;
		
		obj.initNull();
		xref->fetch(objnum,objgen,&obj);
		
		
		// fprintf (stderr,"object: %d\n",objnum);
		
		
		dump_obj(objnum,objgen,outfile);
		
		recurse_obj(&obj,outfile);
	} else {
		;
		//	fprintf (stderr,"Warning: reference to already seen object %d\n",objnum);
	}
	
}

void print_object_tree(XRef *xref, Object *obj, int level) {
	Object ref,type,font;
	char *key;
	Stream *s;
	int count = 0;
	
	// test we haven't recursed into this object already;
	
	for (count=0; count < oblistpointer; count++) 
		if (obj->getRefNum() == oblist[count]) {
			// seen before
			return;
		}
	
	oblist[oblistpointer++] = obj->getRefNum();
	
	if (oblistpointer == OBLISTSIZE) {
		fprintf(stderr,"Object array full.\n");
		exit(-1);
	}

	
	if (obj->isRef()) {
		for (count=0; count < level;count++) { printf ("  ");}
		printf ("%d %d",obj->getRefNum(), obj->getRefGen());
		xref->fetch(obj->getRefNum(),obj->getRefGen(),&ref);
		printf(" %s",ref.getTypeName());
		
		if (ref.isStream()){
			//fprintf (stderr,"is stream getstream\n");
			s = ref.getStream();			
			//fprintf (stderr,"initDict\n");
			type.initDict(s->getDict());
			printf(" ");
			type.print();
			/*//fprintf (stderr,"dictLookup\n");
			type.dictLookup("Length",&font);
			//fprintf (stderr,"isInt()\n");
			if (font.isInt()) {
				//fprintf (stderr,"getInt()\n");
				printf ( " %d",font.getInt());
			}
			//fprintf (stderr,"ref.isStream() exit\n");
*/
		} else if (ref.isDict()) {
			ref.dictLookup("Type",&type);
			if (type.isName()) {
				printf(" %s",type.getName());
				
				if (type.isName("Font")) {
					ref.dictLookup("BaseFont",&font);
					if (font.isName()) {
						printf(" %s",font.getName());
					}
				}
				 
			}
		}
	
		printf("\n");
		print_object_tree(xref,&ref,level);
	} else {
			if (obj->isDict()) {
		
		for (int n=0; n<obj->dictGetLength(); n++) {
			obj->dictGetValNF(n, &ref);
			key = obj->dictGetKey(n);
			
			//fprintf(stderr,"type: %s\n",obj.getTypeName());
			
			// we don't want to recurse back up the tree
			if (strcmp(key,"Parent")) {
				if (strcmp(key,"P")) { // appears to be the parent key for annots.
					print_object_tree(xref,&ref,level+1);
				}
			}
			
		}
	} else if (obj->isArray()) {
		for (int n=0; n<obj->arrayGetLength(); n++) {
			obj->arrayGetNF(n, &ref);
			 print_object_tree(xref,&ref,level+1); 
		}
	} else if (obj->isStream()) {
		// streams contain Dicts, that can contain refs
		s = obj->getStream();
		ref.initDict(s->getDict());
		print_object_tree(xref,&ref,level+1); 
	}
	}
	
}	
	
int countPages(Object *obj) {
	
	Object ref;
	char *key;
	Stream *s;
	int count = 0;
	// if page return 1
	
	if(obj->isDict()) {
		if (obj->dictIs("Page")) {
			return 1;
		}
	}
	
	if (obj->isDict()) {
		
		for (int n=0; n<obj->dictGetLength(); n++) {
			obj->dictGetVal(n, &ref);
			key = obj->dictGetKey(n);
			
			//fprintf(stderr,"type: %s\n",obj.getTypeName());
			
			// we don't want to recurse back up the tree
			if (strcmp(key,"Parent")) {
				if (strcmp(key,"P")) { // appears to be the parent key for annots.
					count += countPages(&ref);
				}
			}
			
		}
	} else if (obj->isArray()) {
		for (int n=0; n<obj->arrayGetLength(); n++) {
			obj->arrayGet(n, &ref);
			count += countPages(&ref);
		}
	} else if (obj->isStream()) {
		// streams contain Dicts, that can contain refs
		s = obj->getStream();
		ref.initDict(s->getDict());
		count += countPages(&ref);
		
	}
	
	return count;
	
}

void getParent (int objnum, int objgen, Object *ref) {
	
	Object obj;
	char *key;
	
	
	ref->initNull();
	obj.initNull();
	xref->fetch(objnum,objgen,&obj);
	
	if (obj.isDict()) {
		for (int n=0; n<obj.dictGetLength(); n++) {
			key = obj.dictGetKey(n);
			//fprintf (stderr, "key: %s\n",key); 
			if (!strcmp(key,"Parent")) {
				obj.dictGetValNF(n,ref);
				//	ref->print(stderr);
				break;
			}
		}
	}
}	


int main(int argc, char *argv[])
{
    char *p, buf[1024];
    PDFDoc *doc;
    GString *fileName;
    Stream *s;
    Object srcStream, srcName, catalogDict, Parent, *trailer;
    FILE *outfile = stdout;
    char *outname;
    int objnum = 0, objgen = 0, parnum=0, pargen=0,freeobj=0,pages=1;
    bool extract_xref_table = false;
    int c,xrefref,catalogref;
    fprintf(stderr, "xpdf library version %s\n", xpdfVersion);
    if (argc != 4 && argc != 2) {
        fprintf(stderr,
                "Usage: pdfpage <PDF-file> [<object number> <outfilename>]\n");
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
            outname=argv[3];
    }
    xref = doc->getXRef();
    catalogDict.initNull();
    xref->getCatalog(&catalogDict);
    if (!catalogDict.isDict("Catalog")) {
        fprintf(stderr, "No Catalog found\n");
        exit(1);
    }
    srcStream.initNull();
	if (objnum > 0) {
		
		outfile = fopen(outname,"w");
		
		getParent(objnum,objgen,&Parent);
		xref->fetch(objnum,objgen,&srcStream);
		pages=countPages(&srcStream);
		
		fprintf(stderr,"Extracting %d page%c\n",pages,pages>1?'s':' ');
		
		if (Parent.isRef() ) {
			parnum = Parent.getRefNum();
			pargen = Parent.getRefGen();
		} else {  // we'll assume that the specified object is the parent if there is no parent
			parnum= objnum;
			pargen= objgen;
		}
		
		pdfsize = xref->getNumObjects();
		objects = ( int *) malloc(pdfsize * sizeof( int)+1);
		for (int n=1;n<=pdfsize; n++) 
			objects[n]=0;
		
		fprintf(outfile,"%%PDF-1.3\n%%Äåòå\n");
		recurse_obj_ref(objnum,objgen, outfile);
		
		// Allocate parent if not already
		// check to see if a top level parent pages already exists.
		if (!objects[parnum]) {
			objects[parnum] = ftell(outfile);
			fprintf(outfile , "%d %d obj\n<<  /Type /Pages /Kids [ %d %d R ] /Count %d >>\n",parnum,pargen,objnum,objgen,pages);
		}		
		
		for (int n=1;n<=pdfsize; n++) 
			if (objects[n]==0) { catalogref = n; break;}
		
		objects[catalogref] = ftell(outfile);
		
		for (int n=1;n<=pdfsize; n++) 
			if (objects[n]==0) { freeobj = n; break; }
		
		//this needs to point to the top level pages entry if no parnum
		fprintf(outfile,"%d 0 obj\n<< /Type /Catalog /Pages %d %d R\n/Outlines %d 0 R>>\nendobj\n",catalogref,parnum,pargen,freeobj);
		
		
		objects[freeobj] = ftell(outfile);
		fprintf(outfile,"%d 0 obj\n<< /Type Outlines /Count 0 >>\nendobj\n",freeobj);
		
		
		xrefref = ftell(outfile);
		fprintf(outfile,"xref\n");
		fprintf(outfile,"0 %d\n",pdfsize);
		fprintf(outfile,"%010d %05d n\n",0,65535);
		
		for (int n=1;n<=pdfsize; n++) 
			fprintf (outfile,"%010d %05d n\n",objects[n],0);
		
		fprintf(outfile,"trailer\n");
		fprintf(outfile,"<< /Size %d /Root %d 0 R >>\n",pdfsize,catalogref);
		fprintf(outfile,"startxref\n%d\n%%%%EOF\n",xrefref);
		free (objects);
		
		fclose(outfile);
	} else {
	
		// print object tree
		// or page tree
		
		printf ("PDF tree\n");
		trailer = xref->getTrailerDict();
		print_object_tree(xref,trailer,0);
		
		
	}
    catalogDict.free();
    delete doc;
    delete globalParams;
}
