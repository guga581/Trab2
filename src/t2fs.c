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
t_inode current_directory;
open_file* open_files[MAX_OPENED_FILES];
int first_inode, first_free_block_bitmap, first_free_inode_bitmap;

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
    read_sector(first_inode*superbloco.blockSize, buffer);
    strncpy((char*)&current_directory, buffer, sizeof(t_inode));

    printf("blocksFileSize = %d\n", current_directory.blocksFileSize);
    printf("bytesFileSize = %d\n", current_directory.bytesFileSize);
    printf("dataPtr[0] = %x\n", current_directory.dataPtr[0]);
    printf("dataPtr[1] = %x\n", current_directory.dataPtr[1]);
    printf("singleIndPtr = %x\n", current_directory.singleIndPtr);
    printf("doubleIndPtr = %x\n\n", current_directory.doubleIndPtr);

    for(i = 0; i < MAX_OPENED_FILES; i++){
        open_files[i] = NULL;
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
    sector = (first_inode + first_free_inode / 8)* superbloco.blockSize; // 8 is the number of inodes per sector
    read_sector(sector, buffer);
    printf("%d\n", (first_free_inode));
    strncpy(buffer+(first_free_inode % 8)*32, (char*) &new_inode, sizeof(t_inode));
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

// Only creates in the root directory
FILE2 create2 (char *filename)
{
    open_file* new_file;
    t_record new_record;
    t_inode new_inode;
    int first_free_block, first_inode, handle;
    if(first_call)
        initialize();

    new_record.TypeVal = TYPEVAL_REGULAR;
    strncpy(new_record.name, filename, 58);

    new_inode.blocksFileSize = 0;
    new_inode.bytesFileSize = 0;
    new_inode.dataPtr[0] = INVALID_PTR;
    new_inode.dataPtr[1] = INVALID_PTR;
    new_inode.singleIndPtr = INVALID_PTR;
    new_inode.doubleIndPtr = INVALID_PTR;

    first_inode = write_inode(new_inode);

    if(first_inode==-1){
        printf("Ops\n");
        return first_inode;
    }

    setBitmap2 (0, first_inode, 1);
    printf("%d\n", getBitmap2(0,first_inode));

    new_file = malloc(sizeof(open_file));

    handle = next_free_handle();

    new_file->current = 0;
    new_file->inode = new_inode;

    open_files[handle] = new_file;

    return handle;

}

int delete2 (char *filename)
{
    if(first_call)
        initialize();
}

FILE2 open2 (char *filename)
{
    if(first_call)
        initialize();
}

int close2 (FILE2 handle)
{
    if(first_call)
        initialize();
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

