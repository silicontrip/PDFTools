//========================================================================
//
// pdffonts.cc
//
// Copyright 2001-2007 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <tiffio.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "parseargs.h"
#include "GString.h"
#include "gmem.h"
#include "GlobalParams.h"
#include "Error.h"
#include "Object.h"
#include "Dict.h"
#include "GfxFont.h"
#include "Annot.h"
#include "PDFDoc.h"

// NB: this must match the definition of GfxFontType in GfxFont.h.



static int firstPage = 1;
static int lastPage = 0;
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static char cfgFileName[256] = "";
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;
static int extract = 0;

static ArgDesc argDesc[] = {
{"-f",      argInt,      &firstPage,     0,
"first page to examine"},
{"-l",      argInt,      &lastPage,      0,
"last page to examine"},
{"-opw",    argString,   ownerPassword,  sizeof(ownerPassword),
"owner password (for encrypted files)"},
{"-upw",    argString,   userPassword,   sizeof(userPassword),
"user password (for encrypted files)"},
{"-cfg",        argString,      cfgFileName,    sizeof(cfgFileName),
"configuration file to use in place of .xpdfrc"},
{"-e", argInt, &extract, 0, "Extract Image to file"},
{"-v",      argFlag,     &printVersion,  0,
"print copyright and version info"},
{"-h",      argFlag,     &printHelp,     0,
"print usage information"},
{"-help",   argFlag,     &printHelp,     0,
"print usage information"},
{"--help",  argFlag,     &printHelp,     0,
"print usage information"},
{"-?",      argFlag,     &printHelp,     0,
"print usage information"},
{NULL}
};

static Ref *fonts;
static int fontsLen;
static int fontsSize;

static void scanImage(Dict *resDict) {
	Object obj1, obj2, xObjDict, xObj, resObj;
	Ref r;
	int i;
	char *name;
	Dict *sDict;
	
	// recursively scan any resource dictionaries in objects in this
	// resource dictionary
	resDict->lookup("XObject", &xObjDict);
	if (xObjDict.isDict()) {
		for (i = 0; i < xObjDict.dictGetLength(); ++i) {
			name=xObjDict.dictGetKey(i);
			xObjDict.dictGetVal(i, &xObj);
			xObjDict.dictGetValNF(i, &obj1);
			if (xObj.isStream()) {
				sDict = xObj.streamGetDict();
				sDict->lookup("Subtype", &resObj);
				// TODO: scan Fm Images
				if (resObj.isName()) {
					if(!strncmp("Image",resObj.getName(),5)) {
						
						printf("%d %d R ",obj1.getRefNum(),obj1.getRefGen());
						
						if (name != NULL) {
							printf ("%s",name);
						} else {
							printf ("(null)");
						}
						sDict->lookup("Width", &resObj); printf (" %d",resObj.getInt());
						sDict->lookup("Height", &resObj); printf (" %d",resObj.getInt());
						sDict->lookup("ColorSpace", &resObj); 
						if (resObj.isName()) {
							printf (" %s",resObj.getName());
						} else if (resObj.isArray()) {
							resObj.arrayGet(0, &obj1);
							if (obj1.isName()) 
								printf (" %s",obj1.getName());
							
							resObj.arrayGet(1, &obj1);
							if (obj1.isName()) 
								printf (" (%s)",obj1.getName());
							
						}
						printf("\n");

					} else if(!strncmp("Form",resObj.getName(),4)) {
						Object yObjDict;
					//	printf("form name: %s ",resObj.getName());
						sDict->lookup("Resources", &yObjDict);
					//	yObjDict.print(); printf("\n");
						scanImage(yObjDict.getDict());
					} else {
						printf (" %s\n",resObj.getTypeName());
					}
				}
				
				// TODO: scan Fm Images
				
				resObj.free();
			}
			xObj.free();
		}
	}
	//else { printf("Not Dict %s\n",xObjDict.getTypeName()); }
	xObjDict.free();
}

static void saveImage(int objnum, int objgen, PDFDoc *doc)
{
	
	XRef *xref;
	Object srcStream;
	Dict *details;
	Object val,val2,val3;
	int w,h,count=0;
	char *buffer;
	uint16 *palette[4];
	TIFF *image;
	Stream *s;
	int c,bpp=1,bufsize,photo=-1;
	int bpc=0,pallen=0;
	
	xref = doc->getXRef();
	xref->fetch(objnum,objgen,&srcStream);
	
	if (srcStream.isStream()) {
		
		details = srcStream.streamGetDict();
		
		details->lookup("ColorSpace",&val);
		if(val.isArray()) {
			// most likely an indexed or ICC color space.
			details->lookup("ColorSpace",&val2);
			val2.arrayGet(0, &val);
		}
		photo=PHOTOMETRIC_MINISBLACK;
		if(val.isName()) {
			if(!strncmp("DeviceCMYK",val.getName(),10)) { bpp=4; photo=PHOTOMETRIC_SEPARATED;}
			if(!strncmp("DeviceGray",val.getName(),10)) { bpp=1; photo=PHOTOMETRIC_MINISBLACK;}
			if(!strncmp("DeviceRGB",val.getName(),9)) { bpp=3; photo=PHOTOMETRIC_RGB;}
			// Probably should include the ICC or calibration information into the TIFF
			if(!strncmp("ICCBased",val.getName(),8)) { bpp=3; photo=PHOTOMETRIC_RGB;}
			if(!strncmp("CalRGB",val.getName(),6)) { bpp=3; photo=PHOTOMETRIC_RGB;}
			if(!strncmp("Indexed",val.getName(),7)) { 
				photo=PHOTOMETRIC_PALETTE;
				
				val2.arrayGet(2, &val3);
				if (val3.isInt()) { pallen = val3.getInt();}
				val2.arrayGet(1, &val3);
				// not sure if bpp needs to be 1 for indexed
				//printf("name: %s\n",
				if (val3.isName()) {
					if(!strncmp("DeviceRGB",val3.getName(),9)) { bpp=3; }
					if(!strncmp("DeviceCMYK",val3.getName(),10)) { bpp=4; }
					if(!strncmp("ICCBased",val3.getName(),8)) { bpp=3; }
				//	if(!strncmp("DeviceGray",val3.getName(),10)) { bpp=1; photo=PHOTOMETRIC_MINISBLACK;}
				}
				
				bufsize =  (pallen+1) * bpp;
				for(int i=0;i < 4; i++) 
					palette[i] = (uint16*)	_TIFFmalloc(bufsize * sizeof(uint16));
				
				// read pallette.
				val2.arrayGet(3, &val3);
				if (val3.isStream()) {
					s = val3.getStream();
					s->reset();
					count =0;
					while (( c = s->getChar()) != EOF) { 
						palette[count%bpp][count/bpp]=c<<8;
						//	printf ("count %d [%d][%d] = %d\n",count,count%bpp,count/bpp,c);
						
						count++;
					}
				} else if (val3.isString()) {
					buffer = val3.getString()->getCString();
					count =0;
					while (count<bufsize) {						
						palette[count%bpp][count/bpp]=buffer[count]<<8;
						count++;
					}
					
				} else {
					fprintf(stderr,"cannot find palette stream (%s)\n",val3.getTypeName());
				}
				bpp=1;
				
			}
		}
		details->lookup("Height",&val); if (val.isInt()) { h = val.getInt(); }
		details->lookup("Width",&val); if (val.isInt()) { w = val.getInt(); }
		details->lookup("BitsPerComponent",&val); if (val.isInt()) { bpc = val.getInt(); }
		
		if (bpc>0) {
			s = srcStream.getStream();
			if (s==NULL) {
				fprintf (stderr,"source stream is null. WTF?\n");
			}
			bufsize = w * h * bpp * bpc / 8;
			buffer = (char *) malloc(bufsize);
			
			sprintf(buffer,"object%d_%d.tif",objnum,objgen);
			if((image = TIFFOpen(buffer, "w")) == NULL){
				printf("Could not open %s for writing\n",buffer);
				exit(-1);
			}
			
			s->reset();
			count =0;
			while (( c = s->getChar()) != EOF) { *(buffer+count++)=c; }
			
			if (count !=bufsize) {
				fprintf (stderr,"count (%d) does not match image size (%d). corrupted pdf?\n",count,bufsize);
			}
			TIFFSetField(image, TIFFTAG_IMAGEWIDTH, w);
			TIFFSetField(image, TIFFTAG_IMAGELENGTH, h);
			TIFFSetField(image, TIFFTAG_BITSPERSAMPLE, bpc);
			TIFFSetField(image, TIFFTAG_SAMPLESPERPIXEL, bpp);
			TIFFSetField(image, TIFFTAG_ROWSPERSTRIP, h);
			
			TIFFSetField(image, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			TIFFSetField(image, TIFFTAG_PHOTOMETRIC, photo);
			TIFFSetField(image, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
			TIFFSetField(image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			
			TIFFSetField(image, TIFFTAG_XRESOLUTION, 100.0);
			TIFFSetField(image, TIFFTAG_YRESOLUTION, 100.0);
			TIFFSetField(image, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
			
			if (pallen>0) {
				// set tiff pallete
			//	TIFFSetField(image, TIFFTAG_INDEXED, 1);
				// not sure how to tell the tiff it's a DeviceCMYK or DeviceGray...
				if (photo != PHOTOMETRIC_SEPARATED) 
					TIFFSetField(image, TIFFTAG_COLORMAP, palette[0], palette[1], palette[2]);
				else 
					TIFFSetField(image, TIFFTAG_COLORMAP, palette[0], palette[1], palette[2],palette[3]);

				//	TIFFSetField(image, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
			}
			
			
			TIFFWriteEncodedStrip(image, 0, buffer, count);
			TIFFClose(image);
			
			if (photo==PHOTOMETRIC_PALETTE)
				for(int i=0;i < 4; i++) 
					_TIFFfree(palette[i]);
			
			
			printf("Saved.\n");
			
			free(buffer);
		} else {
			fprintf (stderr,"Unhandled color space %s.\n",val.getName());
		}
		
	}
	srcStream.free();
	
}

int main(int argc, char *argv[]) {
	PDFDoc *doc;
	GString *fileName;
	GString *ownerPW, *userPW;
	GBool ok;
	Page *page;
	Dict *resDict;
	Annots *annots;
	Object obj1, obj2;
	int pg, i;
	int exitCode;
	
	exitCode = 99;
	
	// parse args
	ok = parseArgs(argDesc, &argc, argv);
	if (!ok || argc != 2 || printVersion || printHelp) {
		fprintf(stderr, "pdffonts version %s\n", xpdfVersion);
		fprintf(stderr, "%s\n", xpdfCopyright);
		if (!printVersion) {
			printUsage("pdffonts", "<PDF-file>", argDesc);
		}
		goto err0;
	}
	fileName = new GString(argv[1]);
	
	// read config file
	//globalParams = new GlobalParams(cfgFileName);
	// globalParams = new GlobalParams();
	
	
	// open PDF file
	if (ownerPassword[0] != '\001') {
		ownerPW = new GString(ownerPassword);
	} else {
		ownerPW = NULL;
	}
	if (userPassword[0] != '\001') {
		userPW = new GString(userPassword);
	} else {
		userPW = NULL;
	}
	doc = new PDFDoc(fileName, ownerPW, userPW);
	if (userPW) {
		delete userPW;
	}
	if (ownerPW) {
		delete ownerPW;
	}
	if (!doc->isOk()) {
		exitCode = 1;
		goto err1;
	}
	
	if (extract>0) {
		saveImage(extract,0,doc);
	} else {
		
		// get page range
		if (firstPage < 1) {
			firstPage = 1;
		}
		if (lastPage < 1 || lastPage > doc->getNumPages()) {
			lastPage = doc->getNumPages();
		}
		
		// scan the fonts
		//  printf("name                                 type              emb sub uni object ID\n");
		//  printf("------------------------------------ ----------------- --- --- --- ---------\n");
		fonts = NULL;
		fontsLen = fontsSize = 0;
		for (pg = firstPage; pg <= lastPage; ++pg) {
			page = doc->getCatalog()->getPage(pg);
			if ((resDict = page->getResourceDict())) {
				scanImage(resDict);
			}
			annots = new Annots(doc->getXRef(), doc->getCatalog(),
								page->getAnnots(&obj1));
			obj1.free();
			for (i = 0; i < annots->getNumAnnots(); ++i) {
				if (annots->getAnnot(i)->getAppearance(&obj1)->isStream()) {
					obj1.streamGetDict()->lookup("Resources", &obj2);
					if (obj2.isDict()) {
						scanImage(obj2.getDict());
					}
					obj2.free();
				}
				obj1.free();
			}
			delete annots;
		}
	}
	exitCode = 0;
	
	// clean up
	gfree(fonts);
err1:
	delete doc;
	delete globalParams;
err0:
	
	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	
	return exitCode;
}


