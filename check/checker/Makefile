#-----------------------------------------------------------------------------
# GMP Library
#-----------------------------------------------------------------------------
GMP_LDFLAGS 	=  -lgmp

#-----------------------------------------------------------------------------
# ZLIB Library
#-----------------------------------------------------------------------------
ZLIB_LDFLAGS  	=  -lz

#-----------------------------------------------------------------------------
# Main Program
#-----------------------------------------------------------------------------
MAINOBJ       	=  gmputils.o \
						main.o \
						model.o \
						mpsinput.o

BIN           	=  bin
OBJ            =  obj
SRC            =  src
MAINFILE       =  bin/solchecker

MAINOBJFILES   =  $(addprefix $(OBJ)/,$(MAINOBJ))

#-----------------------------------------------------------------------------
# Rules
#-----------------------------------------------------------------------------
.SILENT: $(MAINFILE) $(MAINOBJFILES)

.PHONY: all
all: $(MAINFILE) $(MAINOBJFILES)

$(OBJ):
	@-mkdir -p $(OBJ)

$(BIN):
	@-mkdir -p $(BIN)

.PHONY: clean
clean: $(BIN) $(OBJ)
	@echo "-> remove objective files"
	@-rm -f $(OBJ)/*.o
	@-rmdir $(OBJ)
	@echo "-> remove binary"
	@-rm -f $(MAINFILE)
	@-rmdir $(BIN)

$(MAINFILE): $(BIN) $(OBJ) $(MAINOBJFILES)
	@echo "-> linking $@"
	g++ $(MAINOBJFILES) $(ZLIB_LDFLAGS) $(GMP_LDFLAGS) -o $@

$(OBJ)/%.o: $(SRC)/%.cpp
	@echo "-> compiling $@"
	g++ -c $< -o $@
