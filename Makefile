CXXFLAGS := --std=c++14 -Wall -Wno-sign-compare -O2 -g -DNDEBUG
CPPFLAGS := -I libs -I libs/emilib
LDLIBS   := -lstdc++ -lpthread -ldl

BIN = wfc
DISTDIR = build
OUTPUTDIR = output
SRCDIR  = src
SRCFILES = $(wildcard $(SRCDIR)/*.cpp)
OBJFILES = $(patsubst $(SRCDIR)/%.cpp, $(DISTDIR)/%.o, $(SRCFILES))
RM = rm -rf
MKDIR = mkdir -p


all: setup $(BIN)

$(BIN): $(OBJFILES)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $^ $(LDLIBS) -o $(BIN)

$(DISTDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: run clean distclean

setup: $(DISTDIR) $(OUTPUTDIR)
	git submodule update --init --recursive

$(DISTDIR):
	$(MKDIR) $(DISTDIR)

$(OUTPUTDIR):
	$(MKDIR) $(OUTPUTDIR)

run: all
	./$(BIN)

clean:
	$(RM) $(OBJFILES) $(DISTDIR)

distclean: clean
	$(RM) $(BIN) $(OUTPUTDIR)
