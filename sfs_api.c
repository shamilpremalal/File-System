
#include "sfs_api.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include "disk_emu.h"

#define PREMALAL_SHAMIL_DISK "sfs_disk.disk"
#define NUM_BLOCKS 1024  //maximum number of data blocks on the disk.
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap.  
                                       //Will have 128 rows.
                                       /* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)

#define NUM_INODE_BLOCKS (sizeof(inode_t)*NUM_INODES/BLOCK_SIZE +1) //Blocks needed for inode   
#define NUM_DIR_BLOCKS (sizeof(directory_entry)*NUM_INODES/BLOCK_SIZE + 1)

superblock_t sb;
inode_t inode_table[NUM_INODES]; //Array of inodes
directory_entry dir_table[NUM_INODES-1];

//initialize all bits to high
uint8_t free_bit_map[BITMAP_ROW_SIZE] = { [0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX };

void init_sb(){
    sb.magic = 0xACBD0005;
    sb.block_size = BLOCK_SIZE;
    sb.fs_size = NUM_BLOCKS * BLOCK_SIZE;
    sb.inode_table_len = NUM_INODE_BLOCKS;
    sb.root_dir_inode = 0; // inode that pts to the root dir

}

void init root_dir_inode(){
    inode_t rd;
    rd.mode = 777;
    rd.link_cnt = 1;
    rd.uid = 0;
    rd.gid = 0;
    rd.size = 0;
    inode_table[0] = rd;

}

void initsfs(){
    init_sb();
    init_inode_table();
    init_root_dir_inode();

}

void init_inode_table(){
        
            // inode table
        for(int i = 0; i <= sb.inode_table_len; i++){
            force_set_index(i);
        }
    int j = 0;
        for(i = (int) sb.inode_table_len +1; i <= sb.inode_table_len + NUM_DIR_BLOCKS +1; i++){
            force_set_index(i);
            inode_table[0].data_ptrs[j] = (unsigned) i;
            j++;
        }
        // init the other unassigned data pointer
        for (int i =j; i<12; i++){
            inode_table[0].data_ptrs[j] = -1;
        }
        inode_table[0].indirect_ptrs = -1;
    }
    

void mksfs(int fresh) {
    //Checking if Simple File System Exist
    if(fresh)
    {
    //Creates below
    initsfs();

    // force the bit for the directory table
    // and assign the data pointer in the directory i node
    init_inode_table();

    if (init_fresh_disk(PREMALAL_SHAMIL_DISK,BLOCK_SIZE,NUM_BLOCKS) == -1)
    {
        printf("Could not create new disk file");
        exit(EXIT_FAILURE);
    }

        
        //bitmap
        uint8_t* free_bit_map = get_bitmap();
        force_set_index(NUM_BLOCKS-1);


        //Write superblock to 0th block of disk, one block  
        write_blocks(0, 1, (void*) &sb);

        //Write inode table to 1st block of disk, write sb.inode_table_len blocks
        write_blocks(1, (int) sb.inode_table_len, (void*) inode_table);

        //Write directory table
        write_blocks((int) sb.inode_table_len + 1, NUM_DIR_BLOCKS, (void*) directory_table);

    }
    else {
        
        //Opposite of the above
        //Read superblock of 0th block of disk, one block
        read_blocks(0, 1, &sb);

        //Read inode table of 1st block of disk, read sb.inode_table_len blocks
        read_blocks(1, (int) sb.inode_table_len, (void*) inode_table);

        //Read directory_table
        read_blocks((int) sb.inode_table_len + 1, NUM_DIR_BLOCKS, (void*) directory_table);

        //free block
        uint8_t* free_bit_map = get_bitmap();
        read_blocks(NUM_BLOCKS - 1, 1, (void*) free_bit_map);

    }
}
int sfs_getnextfilename(char *fname){

}
int sfs_getfilesize(const char* path){

}
int sfs_fopen(char *name){

}
int sfs_fclose(int fileID) {

}
int sfs_fread(int fileID, char *buf, int length) {
	
}
int sfs_fwrite(int fileID, const char *buf, int length) {

}
int sfs_fseek(int fileID, int loc) {

}
int sfs_remove(char *file) {


}

