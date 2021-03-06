#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/t2fs.h"
#include "../include/bitmap2.h"
#include "../include/apidisk.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// CONFIGURAÇÕES
//////////////////////////////////////////////////////////////////////////////////////////////

typedef struct t2fs_superbloco t_superbloco;
typedef struct t2fs_inode t_inode;
typedef struct t2fs_record t_record;

typedef struct {
	int current;
	t_inode inode;
} open_file;

//////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////
// VAR AUXILIARES
//////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_OPENED_FILES 10
#define maxLenghtRecord 59

int endInodeBitmap, endBlockBitmap, endFirstInode, endFirstBlock;
int tamInodeBitmap, tamBlockBitmap;

int currentInodeIndex = 0;

int first_call = 1;

t_superbloco superbloco;
t_record current_directory;
t_record root;
open_file* open_files[MAX_OPENED_FILES];
int first_inode, first_free_block_bitmap, first_free_inode_bitmap;
DWORD buffer_singleIndPtr[256 / sizeof(DWORD)];

//////////////////////////////////////////////////////////////////////////////////////////////

void printSuperBloco();
void printSuperBloco(){

	printf("id = %s\n", superbloco.id);
	printf("version = %x\n", superbloco.version);
	printf("superblockSize = %d\n", superbloco.superblockSize);

	printf("freeBlocksBitmapSize = %d\n", superbloco.freeBlocksBitmapSize);
	printf("freeInodeBitmapSize = %d\n", superbloco.freeInodeBitmapSize);
	printf("inodeAreaSize = %d\n", superbloco.inodeAreaSize);
	printf("blockSize = %d\n", superbloco.blockSize);
	printf("diskSize = %d\n\n", superbloco.diskSize);
	//printf("Setor size = %d\n", SECTOR_SIZE);
	
	return;

}

void printBuffer(unsigned char * buffer);
void printBuffer(unsigned char * buffer){
	
	printf("*****DEBUGANDO BUFFER*****\n");
	
	int j;	
	for(j = 0; j < SECTOR_SIZE; j++){
			if(buffer[j] != 0){printf("Valor: %d, endereço: buffer[%d]\n", buffer[j], (j) );}
		}
}

void printInode(t_inode iNode);
void printInode(t_inode iNode){
	printf("\n Size in Blocks = %d\n", iNode.blocksFileSize);
	printf("Size in Bytes = %d\n", iNode.bytesFileSize);
	printf("Data ptr 1 %u\n", iNode.dataPtr[0]);
	printf("Data ptr 2 %u\n", iNode.dataPtr[1]);
	printf("Ptr Ind Simples %u\n", iNode.singleIndPtr);
	printf("Ptr Ind Dupla %u\n", iNode.doubleIndPtr);
}

int initSuperBloco();
int initSuperBloco(){

	unsigned char buffer[SECTOR_SIZE];

	if (read_sector(0, buffer) != 0){
		return-1;
	}

	strncpy(superbloco.id, (char*)buffer, 4);
	superbloco.version = *((DWORD*)(buffer + 4));
	superbloco.superblockSize = *((DWORD*)(buffer + 6));
	superbloco.freeBlocksBitmapSize = *((DWORD*)(buffer + 8));
	superbloco.freeInodeBitmapSize = *((DWORD*)(buffer + 10));
	superbloco.inodeAreaSize = *((DWORD*)(buffer + 12));
	superbloco.blockSize = *((DWORD*)(buffer + 14));
	superbloco.diskSize = *((DWORD*)(buffer + 16));

	printSuperBloco();

	return 0;

}

int descobreSetor(int inodeIndex);
int descobreSetor(int inodeIndex){
	
	int partial = superbloco.freeInodeBitmapSize * superbloco.blockSize;
	int i = 0;	
	for(i = 1; i <= partial; i++){
		if(inodeIndex < SECTOR_SIZE * i)
			return i;
	}

	return -1;	
}

//0 -> bloco
//1 -> inode
//retorna bit ou -1
int readBitmap(unsigned char bitmap, int inodeIndex);
int readBitmap(unsigned char bitmap, int inodeIndex){
		
	int endInicial;

	if(bitmap == 0)
		endInicial = endBlockBitmap;
	else if(bitmap == 1)
		endInicial = endInodeBitmap;
	else
		return -1;

	int setor = descobreSetor(inodeIndex);

	while(!(inodeIndex < SECTOR_SIZE))			
		inodeIndex = inodeIndex - SECTOR_SIZE;

	unsigned char bufferAux[SECTOR_SIZE];
	//printf("setor = %d\n", setor);
	

	if(read_sector(endInicial + ((setor - 1) * SECTOR_SIZE), bufferAux) == 0)
		return bufferAux[inodeIndex];
	else
		return -1;

	
	
	//return 0;
}

int getFirstFreeInode();
int getFirstFreeInode(){
	
	int i,j;
	unsigned char buffer[SECTOR_SIZE];
	
	for(i = 0; i < superbloco.freeInodeBitmapSize * superbloco.blockSize; i++){
		if(read_sector(endInodeBitmap + (i * SECTOR_SIZE), buffer) == 0){
			for(j = 0; j < SECTOR_SIZE; j++){
				printf("Endereço testado = %d, valor lido = %d\n", endInodeBitmap + (i * SECTOR_SIZE) + j, buffer[j]);				
				if(buffer[j] == 0)
					return j;
			}	
		}		
		
	}
	printf("\n\n");

	return -1;
}

int getFirstFreeBlock();
int getFirstFreeBlock(){

	int i,j;
	unsigned char buffer[SECTOR_SIZE];
	
	for(i = 0; i < superbloco.freeBlockBitmapSize * superbloco.blockSize; i++){
		if(read_sector(endBlockBitmap + (i * SECTOR_SIZE), buffer) == 0){
			for(j = 0; j < SECTOR_SIZE; j++){
				printf("Endereço testado = %d, valor lido = %d\n", endBlockBitmap + (i * SECTOR_SIZE) + j, buffer[j]);				
				if(buffer[j] == 0)
					return j;
			}	
		}		
		
	}
	printf("\n\n");

	return -1;
}

int escreveInodeDisco(t_inode inode, int inodeNumber);
int escreveInodeDisco(t_inode inode, int inodeNumber){

	unsigned char bufferWrite[SECTOR_SIZE];
	
	int i;
	for(i=0; i < SECTOR_SIZE; i++)
		bufferWrite[i] = 0;

	int firstFreeInode = inodeNumber;
	//printf("FirstFreeInode = %d\n\n", firstFreeInode);

	if(firstFreeInode % SECTOR_SIZE == 0){
		read_sector(firstFreeInode, bufferWrite);
		memcpy( (void *)&bufferWrite , (void *)&(inode), sizeof(inode));
		write_sector(firstFreeInode, bufferWrite);
		printf("Inode escrito com resto 0\n\n");
	}
	else{
		int firstFreeInodeAux = firstFreeInode;
		
		while(firstFreeInodeAux % SECTOR_SIZE != 0)
			firstFreeInodeAux--;
		
		printf("firstFreeInodeAux = %d\n\n", firstFreeInodeAux);
		
		read_sector(firstFreeInodeAux, bufferWrite);

		//printBuffer(bufferWrite);	

		memcpy( (void *)&bufferWrite + sizeof(inode), (void *)&(inode), sizeof(inode));	

		//printBuffer(bufferWrite);	

		write_sector(firstFreeInodeAux, bufferWrite);
		printf("Inode escrito com resto diferente de 0\n\n");
	}

}

t_inode getInodeFromSector(unsigned char * buffer, int parte);
t_inode getInodeFromSector(unsigned char * buffer, int parte){
	
	t_inode inode = malloc(sizeof(t_inode));

	int i;
	int inicioBuffer = 0 + (32 * parte);
	
	memcpy( (void*)&inode, (void *)&(buffer[inicioBuffer]), sizeof(inode));

	return inode;

}

int checkEnderecoNovoDiretorio(filename);
int checkEnderecoNovoDiretorio(filename){

	unsigned char buffer[SECTOR_SIZE];
	int i, j;
	for(i = 0; i < superbloco.freeInodeBitmapSize * superbloco.blockSize; i++){
		if(read_sector(endInodeBitmap, buffer) == 0){
			for(j = 0; j < SECTOR_SIZE / sizeof(t_inode); j++){
				t_inode inode = getInodeFromSector(buffer, j);

				if(inode.)	
			}
		}
	}	

	return -1;
}

int escreveDiretorioPaiAvo(char *filename, int blockNumber);
int escreveDiretorioPaiAvo(char *filename, int blockNumber){

}


int criarDiretorio(char *filename){

	//if(checkEnderecoNovoDiretorio(filename) != 0)
	//	return -1;

	t_record novoDiretorio = malloc(sizeof(t_record));

	novoDiretorio->TypeVal = TYPEVAL_DIRETORIO;
	strcpy(novoDiretorio->name, filename);
	novoDiretorio->inodeNumber = getFirstFreeInode();

	if(writeBitmap(1, 1, getFirstFreeInode()) != 0)
		return -1;

	t_inode inode = malloc(sizeof(t_inode));;

	inode.blocksFileSize = sizeof(novoDiretorio) / superbloco.blockSize;//conferir
	inode.bytesFileSize = sizeof(novoDiretorio);
	inode.dataPtr[0] = getFirstFreeBlock();
	inode.dataPtr[1] = INVALID_PTR;
	inode.singleIndPtr = INVALID_PTR;
	inode.doubleIndPtr = INVALID_PTR;

	if(escreveInodeDisco(inode, inodeNumber) != 0)
		return -1;

	if(writeBitmap(0, 1, getFirstFreeBlock()) != 0)
		return -1;

	//escreveDiretorioPaiAvo(filename, inode.dataPtr[0]);

}

/* Poderia fazer com ponteiro ou variáveis declaradas, não sei qual a melhor, copiei do Bernardo e fiz com as variaveis.
Outra coisa que não saquei é que a area de iNodes daria que poderiamos colocar o dobro do numero de inode, ou inodes com
duas vezes mais memória (64 bytes dai). (?) | Off-topic -> size da struct do inode: 32. Size da struct de record: 64.*/
int atualizarBlocos(unsigned char bitmap, unsigned char buffer, int inodeIndex, int setor);
int atualizarBlocos(unsigned char bitmap, unsigned char buffer, int inodeIndex, int setor){

	int endInicial;

	if(bitmap == 0)
		endInicial = endFirstBlock;
	else if(bitmap == 1)
		endInicial = endFirstInode;
	else
		return -1;

	/* Solução com ponteiros. 
	memcpy( (void*)&blocoAtual[ (posicao%InodePorBlocos)*sizeof(Inode) ], (void *)&(Inode), sizeof(Inode));
	t_inode * inode = malloc(sizeof(t_inode));
	*/
	
	t_inode inode;	
	t_record * root = malloc(sizeof(t_record));

	/*printf("tamanho do inode é %d\n", sizeof(inode));printf("tamanho da struct é %d\n\n", sizeof(t_inode));
	printf("tamanho do record é %d\n", sizeof(root));printf("tamanho da struct é %d\n", sizeof(t_record));*/

	root->TypeVal = TYPEVAL_DIRETORIO;
	strcpy(root->name, "./root");
	root->inodeNumber = 0;

	inode.blocksFileSize = sizeof(root) / superbloco.blockSize;//conferir
	inode.bytesFileSize = sizeof(root);
	inode.dataPtr[0] = endFirstBlock;
	inode.dataPtr[1] = INVALID_PTR;
	inode.singleIndPtr = inode.doubleIndPtr = INVALID_PTR;

	/*
		Como só escreve num setor inteiro, tem que fazer essa mao ai pra dar certo
	*/

	

	//printBuffer(bufferWrite);
	
	return 0;
}

//0 -> bloco
//1 -> inode
int writeBitmap(unsigned char bitmap, unsigned char buffer, int inodeIndex);
int writeBitmap(unsigned char bitmap, unsigned char buffer, int inodeIndex){
		
	int endInicial;

	if(bitmap == 0)
		endInicial = endBlockBitmap;
	else if(bitmap == 1)
		endInicial = endInodeBitmap;
	else
		return -1;

	int setor = descobreSetor(inodeIndex);

	unsigned char bufferAux[SECTOR_SIZE];
	//printf("setor = %d\n", setor);
	

	read_sector(endInicial + ((setor - 1) * SECTOR_SIZE), bufferAux);

	
	while(!(inodeIndex < SECTOR_SIZE))			
		inodeIndex = inodeIndex - SECTOR_SIZE;	
	
	bufferAux[inodeIndex] = buffer;

	//printf("Debug writebitmap: escrever no setor = %d", write_sector(endInicial + ((setor - 1) * SECTOR_SIZE));	
	

	if (write_sector(endInicial + ((setor - 1) * SECTOR_SIZE), bufferAux) != 0){
		return -1;
	}
		
	if(atualizarBlocos(bitmap, buffer, inodeIndex, setor) == -1)
		printf("Erro ao atualizar os blocos!\n");

	return 0;
}

void printInodeBitmap();
void printInodeBitmap(){
	
	int i, j;
	unsigned char buffer[SECTOR_SIZE];
	
	for(i = 1; i < 8192; i++){
		read_sector(0 + ((i - 1) * SECTOR_SIZE), buffer);
		for(j = 0; j < SECTOR_SIZE; j++){
			if(buffer[j] != 0){printf("Valor: %d, endereço: %d\n", buffer[j], (0 + ((i - 1) * SECTOR_SIZE) + j) );}
		}
	}

	//printf();
}


int initFilesAndDirectories();
int initFilesAndDirectories(){
	
	//nao consegui fazer a funcao do sor funcionar...
	//setBitmap2 (int handle, int bitNumber, int bitValue)
	//if(setBitmap2 (0, 4, 1) == -1)
	//	printf("dd\n");
	
	//bitmap, valor, endereco
	if (writeBitmap(1, 1, 0) != 0){
		printf("Erro ao criar root!\n");
	}

	if(readBitmap(1, 1) != -1)
		printf("endereço lido = %d, valor = %d\n", 1, readBitmap(1, 1)); 
	else
		printf("Erro ronaldo!\n");
	//debug
	printInodeBitmap();
	
	return 0;

}

void debugandoBitmaps();
void debugandoBitmaps(){
	unsigned char buffer[SECTOR_SIZE];

	int i = 0;

	for (i = 0; i < tamBlockBitmap / 256; i++){
		// * 1 -> bitmapBlock
		// * 2 ->inodeBitmap
		if (read_sector((endInodeBitmap)+(SECTOR_SIZE * i), buffer) != 0){
			printf("Erro ao debugar!\n");
			return;
		}
		int j;

		for (j = 0; j < SECTOR_SIZE; j++){
			printf("freeBlocksBitmapSize = %d , indice %d\n", buffer[j], i*SECTOR_SIZE + j);
		}
	}

	

}

int initVarAuxiliares();
int initVarAuxiliares(){

	endBlockBitmap = SECTOR_SIZE * superbloco.blockSize * superbloco.superblockSize;
	endInodeBitmap = endBlockBitmap + (SECTOR_SIZE * superbloco.blockSize * superbloco.freeBlocksBitmapSize);
	endFirstInode = endInodeBitmap + (SECTOR_SIZE * superbloco.freeInodeBitmapSize * superbloco.blockSize);
	endFirstBlock = endFirstInode + (SECTOR_SIZE * superbloco.inodeAreaSize * superbloco.blockSize);
	tamInodeBitmap = (SECTOR_SIZE * superbloco.freeInodeBitmapSize * superbloco.blockSize);
	tamBlockBitmap = (SECTOR_SIZE * superbloco.blockSize * superbloco.freeBlocksBitmapSize);

	printf("\nendBlockBitmap = %d\n", endBlockBitmap);
	printf("endInodeBitmap = %d\n", endInodeBitmap);
	printf("endFirstInode = %d\n", endFirstInode);
	printf("endFirstBlock = %d\n\n", endFirstBlock);

	return 0;

}

void initialize();
void initialize(){

	if (initSuperBloco() != 0)
		printf("Erro ao criar superbloco!\n");

	if (initVarAuxiliares() != 0){
		printf("Erro ao inicializar var auxiliares!\n");
	}

	//debugandoBitmaps();

	if (initFilesAndDirectories() != 0)
		printf("Erro ao criar arquivos e diretórios!\n");


}

int main(int argc, char *argv[]){

	initialize();

	/*unsigned char buffer[SECTOR_SIZE];
	char *p1 = strtok(NULL, " ");
	if (p1==NULL) {
	printf ("Erro no comando.\n");
	//continue;
	}
	int sector = atoi(p1);
	int error = read_sector (sector, buffer);
	if (error) {
	printf ("read_sector (%d) error = %d\n", sector, error);
	//continue;
	}

	if(!read_sector(0, buffer[256]))
	printf("OK");
	else
	printf("erro");

	if(!write_sector(0, (unsigned char*)superbloco.version))
	printf("OK");
	else
	printf("erro");
	return 0;*/
}






/*
void initialize()
{
int i;
BYTE buffer[256];
if(read_sector (0, (unsigned char*) &superbloco))
printf("Erro ao ler o superbloco");
//strncpy((char*)&superbloco, buffer, sizeof(t_superbloco)*2);
char idchar[5];
idchar[0] = superbloco.id[0];
idchar[1] = superbloco.id[1];
idchar[2] = superbloco.id[2];
idchar[3] = superbloco.id[3];
idchar[4] = '\0';
printf("id = %s\n", idchar);
printf("version = %x\n", superbloco.version);
printf("superblockSize = %d\n", superbloco.superblockSize);
printf("freeBlocksBitmapSize = %d\n", superbloco.freeBlocksBitmapSize);
printf("freeInodeBitmapSize = %d\n", superbloco.freeInodeBitmapSize);
printf("inodeAreaSize = %d\n", superbloco.inodeAreaSize);
printf("blockSize = %d\n", superbloco.blockSize);
printf("diskSize = %d\n\n", superbloco.diskSize);

first_free_block_bitmap = superbloco.superblockSize;
first_free_inode_bitmap = first_free_block_bitmap + superbloco.freeBlocksBitmapSize;
first_inode = first_free_inode_bitmap + superbloco.freeInodeBitmapSize;

current_directory.TypeVal = TYPEVAL_DIRETORIO;
strcpy(current_directory.name, ".");
current_directory.inodeNumber = 0;

for(i = 0; i < MAX_OPENED_FILES; i++){
open_files[i] = NULL;
}

for(i = 0; i < 256 / sizeof(DWORD); i++){
buffer_singleIndPtr[i] = INVALID_PTR;
}

first_call = 0;
}*/


/*
char* to_absolute_path(char* filename){
return filename;
}

int write_inode(t_inode new_inode){
BYTE buffer[256];
unsigned int sector;
int first_free_inode = searchBitmap2 (0,0);
if(first_inode == -1)
return -1;
sector = (first_inode + first_free_inode ) * superbloco.blockSize; // 8 is the number of inodes per sector
read_sector(sector, buffer);
memcpy(buffer+(first_free_inode % 8)*32, (char*) &new_inode, sizeof(t_inode));
write_sector(sector, buffer);

return first_free_inode;
}

int getcwd2 (char *pathname, int size)
{
if(first_call)
initialize();
}

int identify2 (char *name, int size)
{
char names[] = "Henrique Goetz - 00274719\nMatheusAlanBergmann - 00274704";
if(first_call)
initialize();

strncpy(name, names, size);

if(size < strlen(names))
return -1;
return 0;
}

int next_free_handle(){
int i;
for(i=0; i<MAX_OPENED_FILES; i++){
if(!open_files[i])
return i;
}
return -1;
}

int allocate_new_block(){
int first_free_block = searchBitmap2 (1,0);
printf("Block %d allocated\n", first_free_block);
return first_free_block;
}

int find_first_record_slot(t_record* sector_of_records){
int i;
for(i = 0; i<256/sizeof(t_record); i++){
if(sector_of_records[i].TypeVal == TYPEVAL_INVALIDO){
return i;
}
printf("%s\n", sector_of_records[i].name);

}
return -1;
}

// TODO: Test if it works with more than 72 files in the same directory

int write_record(t_inode* directory, t_record new_record){
t_record sector_of_records[256/sizeof(t_record)];
DWORD single_indirection_pointers[256/sizeof(DWORD)], double_indirection_pointers[256/sizeof(DWORD)];
int i, j, next_free_record = -1;


if(directory->dataPtr[0] == INVALID_PTR){
directory->dataPtr[0] = allocate_new_block();
setBitmap2(1,directory->dataPtr[0], 1);
//update_inode(directory)
}
read_sector(directory->dataPtr[0]*superbloco.blockSize, (char*) &sector_of_records);

next_free_record = find_first_record_slot(sector_of_records);

if(next_free_record != -1){
printf("Saved in dataPtr[0]\n");
sector_of_records[next_free_record] = new_record;
write_sector(directory->dataPtr[0]*superbloco.blockSize, (char*)&sector_of_records);
setBitmap2(1,directory->dataPtr[0], 1);
return 1;
}
*/
/*******************************************************************************************************/

/*if(directory->dataPtr[1] == INVALID_PTR){
directory->dataPtr[1] = allocate_new_block();
setBitmap2(1,directory->dataPtr[1], 1);
//update_inode(directory)
}
read_sector(directory->dataPtr[1]*superbloco.blockSize, (char*) &sector_of_records);

next_free_record = find_first_record_slot(sector_of_records);

if(next_free_record != -1){
printf("Saved in dataPtr[1]\n");
sector_of_records[next_free_record] = new_record;
write_sector(directory->dataPtr[1]*superbloco.blockSize, (char*)&sector_of_records);
printf("%d\n", setBitmap2(1,directory->dataPtr[1], 1));
return 1;
}*/

/*******************************************************************************************************/
/*if(directory->singleIndPtr == INVALID_PTR){
printf("directory->singleIndPtr == INVALID_PTR\n");
directory->singleIndPtr = allocate_new_block();
write_sector(directory->singleIndPtr*superbloco.blockSize, (char*) &buffer_singleIndPtr);
setBitmap2(1,directory->singleIndPtr, 1);
//update_inode(directory)
}
read_sector(directory->singleIndPtr*superbloco.blockSize, (char*) &single_indirection_pointers);
for(i = 0; i < 256/sizeof(DWORD); i++){
if(single_indirection_pointers[i] == INVALID_PTR){
printf("single_indirection_pointers[%d] == INVALID_PTR", i);
single_indirection_pointers[i] = allocate_new_block();
write_sector(directory->singleIndPtr*superbloco.blockSize, (char*)&single_indirection_pointers);
setBitmap2(1,single_indirection_pointers[i], 1);
}
read_sector(single_indirection_pointers[i]*superbloco.blockSize, (char*) &sector_of_records);
next_free_record = find_first_record_slot(sector_of_records);
if(next_free_record != -1){
printf("Saved in single_indirection_pointer[%d]\n",i);
sector_of_records[next_free_record] = new_record;
write_sector(single_indirection_pointers[i]*superbloco.blockSize, (char*)&sector_of_records);
setBitmap2(1,next_free_record, 1);
return 1;
}
}*/
/***********************************************************************************************************/
/*
if(directory->doubleIndPtr == INVALID_PTR){
printf("directory->doubleIndPtr == INVALID_PTR\n");
directory->doubleIndPtr = allocate_new_block();
write_sector(directory->doubleIndPtr*superbloco.blockSize, (char*) &buffer_singleIndPtr);
setBitmap2(1,directory->doubleIndPtr, 1);
//update_inode(directory)
}
read_sector(directory->doubleIndPtr*superbloco.blockSize, (char*) &double_indirection_pointers);
for(i = 0; i < 256/sizeof(DWORD); i++){
if(double_indirection_pointers[i] == INVALID_PTR){
printf("double_indirection_pointers[%d] == INVALID_PTR", i);
double_indirection_pointers[i] = allocate_new_block();
write_sector(directory->doubleIndPtr*superbloco.blockSize, (char*)&double_indirection_pointers);
setBitmap2(1,double_indirection_pointers[i], 1);
}
if(double_indirection_pointers[i] == INVALID_PTR){
printf("double_indirection_pointers[i] == INVALID_PTR\n");
double_indirection_pointers[i] = allocate_new_block();
write_sector(double_indirection_pointers[i]*superbloco.blockSize, (char*) &buffer_singleIndPtr);
setBitmap2(1,double_indirection_pointers[i], 1);
//update_inode(directory)
}
read_sector(double_indirection_pointers[i]*superbloco.blockSize, (char*) &single_indirection_pointers);
for(j = 0; j < 256/sizeof(DWORD); j++){
if(single_indirection_pointers[j] == INVALID_PTR){
printf("single_indirection_pointers[%d] == INVALID_PTR", j);
single_indirection_pointers[j] = allocate_new_block();
write_sector(double_indirection_pointers[j]*superbloco.blockSize, (char*)&single_indirection_pointers);
setBitmap2(1,single_indirection_pointers[j], 1);
}
read_sector(single_indirection_pointers[j]*superbloco.blockSize, (char*) &sector_of_records);
next_free_record = find_first_record_slot(sector_of_records);
if(next_free_record != -1){
printf("Saved in single_indirection_pointer[%d]\n",j);
sector_of_records[next_free_record] = new_record;
write_sector(single_indirection_pointers[j]*superbloco.blockSize, (char*)&sector_of_records);
setBitmap2(1,next_free_record, 1);
return 1;
}
}
}
}

void updateInode(int inode, t_inode new_inode){
int sector, inodesPerSector;
BYTE buffer[256];
sector = (first_inode + (inode/8)) * superbloco.blockSize; // 8 is the number of inodes per sector
read_sector(sector, buffer);
memcpy(buffer+(inode % 8)*sizeof(t_inode), (char*) &new_inode, sizeof(t_inode));
write_sector(sector, buffer);
}*/
/*
// Only creates in the root directory
FILE2 create2 (char *filename)
{
BYTE buffer[256];
open_file* new_file;
t_record new_record;
t_inode new_inode, directory_inode;
int first_free_block, first_free_inode;
if(first_call)
initialize();

new_inode.blocksFileSize = 0;
new_inode.bytesFileSize = 0;
new_inode.dataPtr[0] = INVALID_PTR;
new_inode.dataPtr[1] = INVALID_PTR;
new_inode.singleIndPtr = INVALID_PTR;
new_inode.doubleIndPtr = INVALID_PTR;

first_free_inode = write_inode(new_inode);

if(first_free_inode==-1){
return first_free_inode;
}

new_record.TypeVal = TYPEVAL_REGULAR;
strncpy(new_record.name, filename, 58);

read_sector(first_inode*superbloco.blockSize, (char*) &buffer);
memcpy((char*) &directory_inode, buffer, sizeof(t_inode));

printf("directory_inode = %x", directory_inode.dataPtr[0]);

write_record(&directory_inode, new_record);

updateInode(current_directory.inodeNumber, directory_inode);

return 0;

}

int delete2 (char *filename)
{
if(first_call)
initialize();
}

FILE2 open2 (char *filename)
{
BYTE bufer[256];
if(first_call)
initialize();

}

int close2 (FILE2 handle)
{
int lidos;
if(first_call)
initialize();

if(!open_files[handle])
return -1;

lidos = open_files[handle]->current;

free(open_files[handle]);

open_files[handle] = NULL;

return lidos;
}

int read2 (FILE2 handle, char *buffer, int size)
{
if(first_call)
initialize();
}

int write2 (FILE2 handle, char *buffer, int size)
{
if(first_call)
initialize();
}

int truncate2 (FILE2 handle)
{
if(first_call)
initialize();
}

int seek2 (FILE2 handle, DWORD offset)
{
if(first_call)
initialize();
}

int mkdir2 (char *pathname)
{
if(first_call)
initialize();
}

int rmdir2 (char *pathname)
{
if(first_call)
initialize();
}

int chdir2 (char *pathname)
{
if(first_call)
initialize();
}

DIR2 opendir2 (char *pathname)
{
if(first_call)
initialize();
}

int readdir2 (DIR2 handle, DIRENT2 *dentry)
{
if(first_call)
initialize();
}

int closedir2 (DIR2 handle)
{
if(first_call)
initialize();
}*/

