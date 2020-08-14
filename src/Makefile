# Makefile for fsdriver3 program.
# Shortcut to building/running files. To be updated as we add files.
#
# 'make' or 'make fsdriver3' or 'make all' will create executable file
#	called fsdriver3.
# 'make clean' removes everything created by this makefile;
#	calls 'rm -f fsdriver3'
#
# See below links for more:
# http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/
# https://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html
# https://makefiletutorial.com


# This is created using link #1

IDIR =../include
CC=gcc
CFLAGS=-I$(IDIR)

ODIR=obj
LDIR =../lib

LIBS=-lm

_DEPS = FileSystem.h fsLow.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = fsdriver3.o FileSystem.o fsLow.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fsdriver3: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
