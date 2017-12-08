/*ECSE 427
*Assignment #3
*Shamil Premalal
*260586332
*https://github.com/shamilpremalal/File-System
*/

#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>

#define PREMALAL_SHAMIL_DISK "sfs_disk.disk"

#define MAX_FILE_NAME 21 // Last bit for null termination!
#define MAXFILENAME 21 // Last bit for null termination! (FOR FUSE TESTING)
#define MAX_EXTENSION_NAME 3
#define NUM_BLOCKS 1024                  //maximum number of data blocks on the disk.
#define BITMAP_ROW_SIZE (NUM_BLOCKS / 8) /* this essentially mimcs the number of rows we have in the bitmap.                                         Will have 128 rows*/

#define BLOCK_SIZE 1024
#define NO_OF_BLOCKS 1024
#define NO_OF_INODES 100
#define NUM_INDIRECT NO_OF_BLOCKS/sizeof(unsigned int)


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
 * inode pointer towards the inode in the inode table
 * rwptr where in the file to start   
 * used when used or not
 */
typedef struct file_descriptor
{
    int inodeIndex;
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

typedef struct indirect_block{
unsigned int data_ptr[NUM_INDIRECT];
}indirect_block;

//helper methods
char *AddChar(char *dest, char a, int len);
inode_t init_inode();
directory_entry init_dir_entry(char *name, int inode_index);
void init_fdt();
void init_int();
void init_super();
void init_root_dir_inode();
void init_sfs();

//Class API 
void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char *path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char *file);


#endif //_INCLUDE_SFS_API_H_
