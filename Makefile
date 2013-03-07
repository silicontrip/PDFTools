
#./pdftex-1.40.10/src/libs/xpdf/xpdf-3.02/xpdf/
#/Users/d332027/Developer/PDF/poppler-0.20.1

SRCDIR = /Users/d332027/Developer/PDF/poppler-0.20.1
OBJDIR = $(SRCDIR)/poppler
LIBS = $(OBJDIR)/Annot.o $(OBJDIR)/Gfx.o  -L/opt/local/lib -ltiff -L/usr/local/lib -lpoppler
INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/poppler -I$(SRCDIR)/goo -I/opt/local/include 

DEFINES = -DPDF_PARSER_ONLY -DPOPPLER_VERSION="0.20"

all: pdfinfl pdfpage pdffonts pdfimag

pdfpagepop: pdfpagepop.o

pdfinfl: pdfinfl.o

pdfpage: pdfpage.o

pdffonts: pdffonts.o

pdfimag: pdfimag.o

%.o: %.cc
	g++ -g -c $(DEFINES) $(INCLUDES) $<

%: %.o
	g++ $(LIBS) $< -o $@
