VARS_OLD := $(.VARIABLES)

DEPSDIR = .deps
OBJDIR = .obj
SRCDIR = src

DEPFLAGS=-MT $@ -MMD -MP -MF $(DEPSDIR)/$*.d
FLAGS=-g -std=c++17 -Wall -pedantic
LIBS=-lncurses

SRC = main reactor
OBJPATH = $(patsubst %, $(OBJDIR)/%.o, $(SRC))

MAIN = main

all: $(MAIN)

clean:
	rm -f $(MAIN)
	rm -rf $(OBJDIR)
	rm -rf $(DEPSDIR)

.PHONY: clean

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(DEPSDIR)/%.d $(PARSERH) | $(DEPSDIR) $(OBJDIR)
	g++ $(DEPFLAGS) -c -o $@ $< $(FLAGS)

$(DEPSDIR): ; mkdir -p $@
$(OBJDIR): ; mkdir -p $@

DEPS := $(SRC)
DEPSFILES := $(patsubst %,$(DEPSDIR)/%.d, $(DEPS))
$(DEPSFILES):
include $(wildcard $(DEPSFILES))

$(MAIN): $(OBJPATH)
	g++ -o $@ $^ $(FLAGS) $(LIBS)

vars:; $(foreach v, $(filter-out $(VARS_OLD) VARS_OLD,$(.VARIABLES)), $(info $(v) = $($(v)))) @#noop
