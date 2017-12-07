#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>

#define MAX_FILE_NAME 21 // Last bit for null termination!
#define MAXFILENAME 21 // Last bit for null termination! (FOR FUSE)

#define MAX_EXTENSION_NAME 3

#define BLOCK_SIZE 1024
#define NO_OF_BLOCKS 1024
#define NO_OF_INODES 100
#define NUM_INDIRECT NO_OF_BLOCKS/sizeof(unsigned int)

typedef struct indirect_t{
unsigned int data_ptr[NUM_INDIRECT];
}indirect_t;

typedef struct superblock_t
{
    uint64_t magic;
    uint64_t block_size;
    uint64_t fs_size;
    uint64_t inode_table_len;
    uint64_t root_dir_inode;
} superblock_t;

typedef struct inode_t
{
    int  used;
    unsigned int mode;
    unsigned int link_cnt;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
    unsigned int data_ptrs[12];
    unsigned int indirectPointer; // points to a data block that points to other data blocks (Single indirect)
} inode_t;

/*
 * inodeIndex    which inode this entry describes
 * inode  pointer towards the inode in the inode table
 * rwptr    where in the file to start   
 */
typedef struct file_descriptor
{
    int inodeIndex;
    inode_t *inode; 
    uint64_t rwptr;
    int used;
} file_descriptor;

typedef struct directory_entry
{
    int inode_index;
    int used;
    int num;                  // represents the inode number of the entry.
    char name[MAX_FILE_NAME]; // represents the name of the entry.
} directory_entry;

//helper methods

void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char *path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char *file);

void init_fdt();
void init_int();
void init_super();
void init_root_dir_inode();
void init_sfs();

#endif //_INCLUDE_SFS_API_H_
