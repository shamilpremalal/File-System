
#include "sfs_api.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include "disk_emu.h"
#include <inttypes.h>

#define PREMALAL_SHAMIL_DISK "sfs_disk.disk"
#define NUM_BLOCKS 1024                  //maximum number of data blocks on the disk.
#define BITMAP_ROW_SIZE (NUM_BLOCKS / 8) /* this essentially mimcs the number of rows we have in the bitmap. \
                                         Will have 128 rows*/
/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)

#define NUM_INODE_BLOCKS (sizeof(inode_t) * NO_OF_INODES / BLOCK_SIZE + 1)       //Blocks needed for inode
#define NUM_DIR_BLOCKS (sizeof(directory_entry) * NO_OF_INODES / BLOCK_SIZE + 1) //number of blocks occuppied by directory

//tables
file_descriptor fd_table[NO_OF_INODES];
directory_entry dir_table[NO_OF_INODES];
inode_t inode_table[NO_OF_INODES]; //Array of inodes

superblock_t sb;

void init_fdt()
{

    for (int i = 0; i < NO_OF_INODES; i++)
    {

        fd_table[i].used = -1;
    }
}
void init_int()
{
    //init inode structure
    for (int i = 0; i < NO_OF_INODES; i++)
    {

        inode_table[i].mode = -1;
        inode_table[i].link_cnt = -1;
        inode_table[i].uid = -1;
        inode_table[i].gid = -1;
        inode_table[i].size = -1;
        inode_table[i].indirectPointer = -1;

        for (int j = 0; j < 12; j++)
        {
            inode_table[i].data_ptrs[j] = -1;
        }
    }
}

void init_super()
{
    sb.magic = 0xACBD0005;
    sb.block_size = BLOCK_SIZE;
    sb.fs_size = NUM_BLOCKS * BLOCK_SIZE;
    sb.inode_table_len = NO_OF_INODES;
    sb.root_dir_inode = 0; // inode that pts to the root dir
}

void init_root_dir_inode()
{
    inode_table[0].size = NUM_DIR_BLOCKS;

    int count = 0;
    for (int i = 0; i < NUM_DIR_BLOCKS; i++)
    {
        inode_table[0].data_ptrs[count] = (int)(NUM_INODE_BLOCKS) + 1 + NUM_DIR_BLOCKS + i;

        count++;
    }

    while (count < 12)
    {
        inode_table[0].data_ptrs[count] = -1;

        count++;
    }
}

void init_inode_table()
{

    // inode table
    for (int i = 0; i <= (int)(NUM_INODE_BLOCKS); i++)
    {
        force_set_index(i);
    }
    int j = 0;
    for (int i = (int)(NUM_INODE_BLOCKS) + 1; i <= (int)(NUM_INODE_BLOCKS) + NUM_DIR_BLOCKS + 1; i++)
    {
        force_set_index(i);
        inode_table[0].data_ptrs[j] = (unsigned)i;
        j++;
    }
    // init the other unassigned data pointer
    for (int i = j; i < 12; i++)
    {
        inode_table[0].data_ptrs[j] = -1;
    }
    inode_table[0].indirectPointer = -1;
}

void mksfs(int fresh)
{
    printf("Done Init");

    //Checking if Simple File System Exist
    if (fresh)
    {
        init_fdt();
        init_int();
        init_super();
        init_root_dir_inode();
        printf("All structs initialized\n");
        if (init_fresh_disk(PREMALAL_SHAMIL_DISK, BLOCK_SIZE, NUM_BLOCKS) == -1)
        {
            printf("Could not create new disk file");
            exit(EXIT_FAILURE);
        }
        printf("Disk Initialized\n");

        //Write superblock to 0th block of disk, one block
        write_blocks(0, 1, (void *)&sb);
        printf("Superblock written to disk\n");

        //bitmap
        force_set_index(0);

        //Write inode table to 1st block of disk, write sb.inode_table_len blocks
        write_blocks(1, (int)(NUM_INODE_BLOCKS), (void *)inode_table);
        printf("Inode Table written to disk\n");

        //set inode table in bitmap
        for (int i = 1; i <= (int)(NUM_INODE_BLOCKS); i++)
        {
            force_set_index(i);
        }

        //Write directory table
        write_blocks((int)(NUM_INODE_BLOCKS) + 1, NUM_DIR_BLOCKS, (void *)dir_table);
        printf("Directory Table written to disk\n");

        for (int i = (int)(NUM_INODE_BLOCKS) + 1; i <= (int)(NUM_INODE_BLOCKS) + 1 + NUM_DIR_BLOCKS; i++)
        {
            force_set_index(i);
        }

        //initialize fd_table[0] to inode_table[0]
        fd_table[0].inode = &inode_table[0];
        fd_table[0].inodeIndex = 0;
        fd_table[0].rwptr = 0;

        //occupy bitmap as last block
        force_set_index(NUM_BLOCKS - 1);

        //write bitmap
        uint8_t *free_bit_map = get_bitmap();
        write_blocks(NUM_BLOCKS - 1, 1, (void *)free_bit_map);
        printf("Bitmap written to disk\n");
    }
    else
    {

        init_fdt();

        //File Descriptor Table init
        printf("File Descriptor Table initialized\n");

        init_disk(PREMALAL_SHAMIL_DISK, BLOCK_SIZE, NUM_BLOCKS);
        printf("Disk initialized\n");
        //Read superblock of 0th block of disk, one block
        read_blocks(0, 1, &sb);
        printf("Superblock read from disk\n");

        //Read inode table of 1st block of disk, read sb.inode_table_len blocks
        read_blocks(1, (int)(NUM_INODE_BLOCKS), (void *)inode_table);
        printf("Inode Table read from disk\n");

        //free block
        uint8_t *free_bit_map = get_bitmap();
        read_blocks(NUM_BLOCKS - 1, 1, (void *)free_bit_map);
        printf("Bitmap read from disk\n");
    }
    return;
}
int sfs_getnextfilename(char *fname)
{
    //Must initialize int dir_table_index but why -1 and why is NO_OF_INODE-1

    dir_table_index++;
    int count = 1;

    //If at the end of the table
    if (dir_table_index == sizeof(dir_table) / sizeof(dir_table[0]))
    {
        dir_table_index = -1;
        return 0;
    }
    //Finds next used Directory
    while (dir_table[dir_table_index].used == 0)
    {
        dir_table_index++;
        //If at end of the table
        if (dir_table_index == sizeof(dir_table) / sizeof(dir_table[0]))
        {
            dir_table_index = 0;
            return 0;
        }

        //Checked entire table
        if (count == sizeof(dir_table) / sizeof(dir_table[0]))
        {
            return 0;
        }
        count++;
    }

    for (int i; i < strlen(dir_table[dir_table_index].name); i++)
    {
        fname[i] = (dir_table[dir_table_index].name)[i];
    }
    int i;
    fname[i] = '\0'; //null terminator

    //entries left in the directory
    return sizeof(dir_table) / sizeof(dir_table[0]) - dir_table_index - 1;
}

//Returns Size of a file
int sfs_getfilesize(const char *path)
{
    for (int i; i < sizeof(dir_table) / sizeof(dir_table[0]); i++)
    {
        if (dir_table[i].used == 1)
        {
            if (strcmp(dir_table[i].name, path) == 0)
            {
                return inode_table[dir_table[i].inode].size;
            }
        }
    }

    return 0;
}

int sfs_fopen(char *name)
{
if(strlen(name) > MAX_FILE_NAME || strlen(name) == 0){
    return -1;
}

//Looking for the file
uint64_t inode_table_index = -1;
for(int i =0; i<sizeof(dir_table)/sizeof(dir_table[0]); i++){
    if(dir_table[i].used ==1){
        if(strcmp(dir_table[i].name, name)==0){
            inode_table_index = (int)dir_table[i].inode;
        }
    }
}

//File is not present
if(inode_table_index ==-1){
    //Find 1st available in inode table
    uint64_t new_inode_table_index =0;
    while(inode_table[new_inode_table].used==1 && new_inode_table_index < sizeof(inode_table)/sizeof(inode_table[0])){

    }
}







    return 0;
}
int sfs_fclose(int fileID)
{
    return 0;
}
int sfs_fread(int fileID, char *buf, int length)
{
    return 0;
}
int sfs_fwrite(int fileID, const char *buf, int length)
{
    return 0;
}
int sfs_fseek(int fileID, int loc)
{
    return 0;
}
int sfs_remove(char *file)
{
    return 0;
}
