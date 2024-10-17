# --------------------------------------------------
# default settings
# --------------------------------------------------
OPT 								=  Release
CC  								=  g++
LIB 								= 	-L../../../ThirdParty/cplex/lib
INC 								=  -I../../../ThirdParty/cplex/include
LDFLAGS 							=  -lm -lcplex -lpthread -ldl
BINDIR 							=  ../../../check/bin
EXE 								=  cplex_callback_ndp
CFILE 							=  $(wildcard src/*.cpp)

ifeq ($(OPT),Debug)
	CFLAGS = -O0 -Wall -g -DDEBUG -std=c++11
else
	OPT = Release
	CFLAGS = -O3 -Wall -DNDEBUG -std=c++11
endif

# --------------------------------------------------
# Rulers
# --------------------------------------------------
.PHONY: all
all: $(BINDIR) $(EXE)

$(BINDIR):
	@-mkdir $(BINDIR)

$(EXE): $(CFILE)
	$(CC) $(CFLAGS) $(INC) $^ $(LIB) $(LDFLAGS) -o $(BINDIR)/$@

.PHONY: clean
clean:
	@-rm -rf $(BINDIR)/cplex_callback_ndp
