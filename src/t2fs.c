#include <t2fs.h>
#include <bitmap2.h>
#include <apidisk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_OPENED_FILES 10


typedef struct t2fs_superbloco t_superbloco;
typedef struct t2fs_inode t_inode;
typedef struct t2fs_record t_record;

typedef struct {
    int current;
    t_inode inode;
} open_file;

int first_call = 1;
t_superbloco superbloco;
t_record current_directory;
open_file* open_files[MAX_OPENED_FILES];
int first_inode, first_free_block_bitmap, first_free_inode_bitmap;
DWORD buffer_singleIndPtr[256/sizeof(DWORD)];

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
}

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

    /*******************************************************************************************************/

    if(directory->dataPtr[1] == INVALID_PTR){
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
    }

    /*******************************************************************************************************/
    if(directory->singleIndPtr == INVALID_PTR){
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
    }
    /***********************************************************************************************************/
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
}

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
}

