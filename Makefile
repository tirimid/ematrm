.PHONY: all clean install uninstall

CPP := g++
CPPFLAGS := -std=c++20 -pedantic
INSTBIN := /usr/bin/ematrm

all: ematrm

clean:
	rm -f ematrm

install: ematrm
	cp $< $(INSTBIN)

uninstall:
	rm -f $(INSTBIN)

ematrm: ematrm.cc
	$(CPP) $(CPPFLAGS) -o $@ $<
