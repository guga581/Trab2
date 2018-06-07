#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
#

CC=gcc
LIB_DIR=./lib/
INC_DIR=./include/
BIN_DIR=./bin/
SRC_DIR=./src/
CFLAGSObj=-m32 -c
CFLAGSExe=-m32 -o

all: directory lib 

directory:
	mkdir lib -p -v

lib: t2fs.o
	ar crs $(LIB_DIR)libt2fs.a $(LIB_DIR)apidisk.o $(BIN_DIR)t2fs.o

t2fs.o: $(SRC_DIR)t2fs.c $(INC_DIR)apidisk.h $(INC_DIR)t2fs.h
	$(CC) $(CFLAGSObj) $(SRC_DIR)t2fs.c -Wall -o $(BIN_DIR)t2fs.o  
 
clean:
	find $(BIN_DIR) $(LIB_DIR) $(SRC_DIR) -type f ! -name 't2fs_disk.dat' ! -name 'apidisk.o' ! -name 't2fs.c' -delete
