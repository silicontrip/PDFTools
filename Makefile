
#./pdftex-1.40.10/src/libs/xpdf/xpdf-3.02/xpdf/
#/Users/d332027/Developer/PDF/poppler-0.20.1

#SRCDIR = /Users/d332027/Developer/PDF/poppler-0.20.1
SRCDIR = 
OBJDIR = $(SRCDIR)/poppler
OBJLIBS = $(OBJDIR)/Annot.o $(OBJDIR)/Gfx.o  
LIBS = -ltiff -L/usr/local/lib -lpoppler
INCLUDES = -I/usr/local/include/poppler/  

DEFINES = -DPDF_PARSER_ONLY -DPOPPLER_VERSION="0.20"

all:  pdfpage pdfimag pdffonts

pdfpagepop: pdfpagepop.o

pdfinfl: pdfinfl.o

pdfpage: pdfpage.o

pdffonts: pdffonts.o

pdfimag: pdfimag.o

%.o: %.cc
	g++ -g -c $(DEFINES) $(INCLUDES) $<

%: %.o
	g++ $(LIBS) $< -o $@


clean:
	rm -f pdfinfl pdfpage pdffonts pdfimag
