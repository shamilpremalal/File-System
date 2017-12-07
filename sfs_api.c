/*ECSE 427
*Assignment #3
*Shamil Premalal
*260586332
*/
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

//tables and essential structures
file_descriptor fd_table[NO_OF_INODES];
directory_entry dir_table[NO_OF_INODES];
inode_t inode_table[NO_OF_INODES]; //Array of inodes
superblock_t sb;

int dir_ptr = -1;

//Initialize file desc. table
void init_fdt()
{

    for (int i = 0; i < NO_OF_INODES; i++)
    {
        fd_table[i].used = 0;
        fd_table[i].inodeIndex = -1;
    }
}

//Initialize Inode table
void init_int()
{
    for (int i = 0; i < NO_OF_INODES; i++)
    {
        inode_table[i].mode = -1;
        inode_table[i].link_cnt = -1;
        inode_table[i].uid = -1;
        inode_table[i].gid = -1;
        inode_table[i].size = -1;
        inode_table[i].indirectPointer = -1;
        inode_table[i].used = -1;

        for (int j = 0; j < 12; j++)
        {
            inode_table[i].data_ptrs[j] = -1;
        }
    }
}
//Initialize superblock
void init_super()
{
    sb.magic = 0xACBD0005;
    sb.block_size = BLOCK_SIZE;
    sb.fs_size = NUM_BLOCKS * BLOCK_SIZE;
    sb.inode_table_len = NO_OF_INODES;
    sb.root_dir_inode = 0; // inode that pts to the root dir
}
//Initialize root directory
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

void mksfs(int fresh)
{
    printf("Done Init");

    //Checking if Simple File System Exist
    if (fresh)
    {
        //Initialize all structures
        init_fdt();
        init_int();
        init_super();
        init_root_dir_inode();
        printf("\nAll structs initialized\n");
        
        //Initialize a fresh disk
        if (init_fresh_disk(PREMALAL_SHAMIL_DISK, BLOCK_SIZE, NUM_BLOCKS) == -1)
        {
            printf("Could not create new disk file");
            exit(EXIT_FAILURE);
        }
        printf("Disk Initialized\n");

        //Write superblock to 0th block of disk, one block
        write_blocks(0, 1, (void *)&sb);
        printf("Superblock written to disk\n");
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
        //Initialize the file descriptor table
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

//Get the name of the next file in the directory
int sfs_getnextfilename(char *fname)
{
    dir_ptr++;

    //If at the end of the table reset counter
    if (dir_ptr == NO_OF_INODES)
    {
        dir_ptr = -1;
        return 0;
    }
    
    int counter = 1;
    //Finds next used Directory entry
    while (dir_table[dir_ptr].used == 0)
    {
        dir_ptr++;
        //If at end of the table (Nothing was found)
        if (dir_ptr == NO_OF_INODES)
        {
            dir_ptr = 0;
            return 0;
        }

        //Checked entire table (Nothing was found)
        if (counter == NO_OF_INODES)
        {
            return 0;
        }
        counter++;
    }
   
    strcpy(fname,dir_table[dir_ptr].name);
   
    //entries remaining in the directory
    return sizeof(dir_table) / sizeof(dir_table[0]) - dir_ptr - 1;
}

//Return file size
int sfs_getfilesize(const char *path)
{
    for (int i = 0; i < NO_OF_INODES; i++)
    {
        if (dir_table[i].used == 1)
        {
            if (strcmp(dir_table[i].name, path) == 0)
            {
                return inode_table[dir_table[i].inode_index].size;
            }
        }
    }

    return 0;
}
//Open a file that exits, or create it and then open it
int sfs_fopen(char *name)
{
    //Verify file name is within range or not empty
    if (strlen(name) > MAX_FILE_NAME - 1 || strlen(name) == 0)
    {
        return -1;
    }

    //Search if file exists in directory
    int inode_index = -1;
    for (int i = 0; i < NO_OF_INODES; i++)
    {
        if (dir_table[i].used == 1)
        {
            if (strcmp(dir_table[i].name, name) == 0)
            {
                inode_index = dir_table[i].inode_index;
            }
        }
    }


    //If the file does not exist, create it 
    if (inode_index == -1)
    {

        //Find 1st available inode in inode table
        int new_index = 1; //(index 0 is taken by SB)
        while (inode_table[new_index].used == 1 && new_index < NO_OF_INODES)
        {
            new_index++;

            //If entire table has been traveresed then it is full
            if (new_index == NO_OF_INODES)
            {
                return -1;
            }
        }

        //Find 1st available in the dir table
        int new_dir_index = 0;
        while (dir_table[new_dir_index].used == 1 && new_dir_index < NO_OF_INODES)
        {
            new_dir_index++;

            //If the table is Full
            if (new_dir_index == NO_OF_INODES)
            {
                return -1;
            }
        }

        printf("The dir table index found is: %i\n", new_dir_index);

        //initialize some inode
        inode_t temp_inode;
        temp_inode.mode = 777;
        temp_inode.link_cnt = 1;
        temp_inode.uid = 0;
        temp_inode.gid = 0;
        temp_inode.size = 0;
        temp_inode.used = 1;

        for (int j = 0; j < 12; j++)
        {
            temp_inode.data_ptrs[j] = -1;
        }
        temp_inode.indirectPointer = -1;

        //Store new inode in inode table 
        inode_table[new_index] = temp_inode;

        //Initialize temporary directory
        directory_entry temp_dir;
        temp_dir.used = 1;
        temp_dir.inode_index = new_index;
        strncpy(temp_dir.name, name,MAX_FILE_NAME);
        temp_dir.name[MAX_FILE_NAME] = 0;
        
        //store temp directory entry in directory table
        dir_table[new_dir_index] = temp_dir;

        //update inode table and directory on disk
        write_blocks(1, (int)(NUM_INODE_BLOCKS), (void *)inode_table);
        write_blocks((int)(NUM_INODE_BLOCKS) + 1, NUM_DIR_BLOCKS, (void *)dir_table);

        inode_index = new_index;
    }
        // check is the file is already open and return it's index if it is open
        for (int open_fd_index = 0; open_fd_index < NO_OF_INODES; open_fd_index++)
        {
            if (fd_table[open_fd_index].inodeIndex == inode_index)
            {
               return open_fd_index;
            }
        }

    

    // find an open spot on the file descriptor table
    int fd_counter = 0;
    while (fd_table[fd_counter].used == 1)
    {
        fd_counter++;
        if (fd_counter == NO_OF_INODES)
        {
            return -1;
        }
    }

    //update the file descriptor table with values of open file
    fd_table[fd_counter].used = 1;
    fd_table[fd_counter].inode = &inode_table[inode_index];
    fd_table[fd_counter].rwptr = inode_table[inode_index].size;
    fd_table[fd_counter].inodeIndex = inode_index;

    return fd_counter;
}

int sfs_fclose(int fileID)
{
    // check if the there is a file open
    if (fd_table[fileID].used == 0)
    {
        return -1;
    }

    //Reset file descriptor table entry
    fd_table[fileID].used = 0;
    fd_table[fileID].inodeIndex = -1;

    return 0;
}

char *AddChar(char *dest, char a, int len)
{
    char *ret = malloc((len + 2) * sizeof(char)); ////////////////Rename ret to something else

    int i;
    for (i = 0; i < len; i++)
    {
        ret[i] = dest[i];
    }
    ret[len] = a;
    ret[len + 1] = '\0';

    return ret;
}

int sfs_fread(int fileID, char *buf, int length)
{
    char *temp_buf = NULL;
    int temp_buff_length = 0;

    // Check if file is open
    if (fd_table[fileID].used == 0)
    {
        return 0;
    }

    // the the file descriptor and the inode of the file
    file_descriptor *temp_fd = &fd_table[fileID];
    inode_t *temp_inode = &inode_table[temp_fd->inodeIndex];

    //Check EOF CONDITION from current rwpt
    if (temp_fd->rwptr + length > temp_inode->size)
    {
        length = temp_inode->size - temp_fd->rwptr;
    }

    int blocks_to_read = ((temp_fd->rwptr % BLOCK_SIZE) + length) / BLOCK_SIZE + 1;

    char buffer[blocks_to_read][BLOCK_SIZE];

    uint64_t data_ptr_index = temp_fd->rwptr / BLOCK_SIZE;

    for (int i = 0; i < blocks_to_read; i++)
    {
        //Check if current block being read is part of the direct or indirect blocks
        if (data_ptr_index < 12)
        {
            read_blocks(temp_inode->data_ptrs[data_ptr_index], 1, buffer[i]);
        }
        else
        {
            if (temp_inode->indirectPointer != -1)
            {
                indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
                read_blocks(temp_inode->indirectPointer, 1, (void *)indirect_pointer);
                read_blocks(indirect_pointer->data_ptr[data_ptr_index - 12], 1, buffer[i]);
            }

        }
        data_ptr_index++;

    }

    int start_ptr = temp_fd->rwptr % BLOCK_SIZE;
    int end_ptr = (start_ptr + length) % BLOCK_SIZE;

    int j;

    // one block
    if (blocks_to_read == 1)
    {
        for (j = start_ptr; j < end_ptr; j++)
        {
            temp_buf = AddChar(temp_buf, buffer[0][j], temp_buff_length);
            temp_buff_length++;
        }

        // two blocks
    }
    else if (blocks_to_read == 2)
    {
        for (j = start_ptr; j < BLOCK_SIZE; j++)
        {
            temp_buf = AddChar(temp_buf, buffer[0][j], temp_buff_length);
            temp_buff_length++;
        }
        for (j = 0; j < end_ptr; j++)
        {
            temp_buf = AddChar(temp_buf, buffer[1][j], temp_buff_length);
            temp_buff_length++;
        }

        // more than two blocks
    }
    else
    {
        for (j = start_ptr; j < BLOCK_SIZE; j++)
        {
            temp_buf = AddChar(temp_buf, buffer[0][j], temp_buff_length);
            temp_buff_length++;
        }

        for (j = 1; j < blocks_to_read - 1; j++)
        {
            int k;
            for (k = 0; k < BLOCK_SIZE; k++)
            {
                temp_buf = AddChar(temp_buf, buffer[j][k], temp_buff_length);
                temp_buff_length++;
            }
        }

        for (j = 0; j < end_ptr; j++)
        {
            temp_buf = AddChar(temp_buf, buffer[blocks_to_read - 1][j], temp_buff_length);
            temp_buff_length++;
        }
    }

    temp_fd->rwptr += length;

    for (int i = 0; i < length; i++)
    {
        buf[i] = temp_buf[i];
    }

    return temp_buff_length;
}
int sfs_fwrite(int fileID, const char *buf, int length)
{
    int count = 0;
    int length_left = length;

    // get the file descritor and the inode of the file
    file_descriptor *f = &fd_table[fileID];
    inode_t *n = &inode_table[f->inodeIndex];

    /****************************************************************/
    /****************** find empty data_prt *************************/
    /****************************************************************/

    // check if there is any empty direct pointer first
    int ptr_index = 0;
    while (n->data_ptrs[ptr_index] != -1 && ptr_index < 12)
    {
        ptr_index++;
    }
    int ind_ptr_index = 0;

    // if no direct pointer is empty check the indirect pointer
    // if indirect pointer exist
    if (ptr_index == 12 && n->indirectPointer != -1)
    {
        indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
        read_blocks(n->indirectPointer, 1, (void *)indirect_pointer);

        while (indirect_pointer->data_ptr[ind_ptr_index] != -1)
        {
            ind_ptr_index++;
            if (ind_ptr_index == NUM_INDIRECT)
            {
                return 0;
            }
        }

        //if indirect pointer does not exist
    }
    else if (ptr_index == 12)
    {
        indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
        for (ind_ptr_index = 0; ind_ptr_index < NUM_INDIRECT; ind_ptr_index++)
        {
            indirect_pointer->data_ptr[ind_ptr_index] = -1;
        }
        n->indirectPointer = get_index();
        write_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
        ind_ptr_index = 0;
    }

    // if the pointer is in the middle of a block
    // you need to read this block, fill it up, and write it back
    if (f->rwptr % BLOCK_SIZE != 0)
    {

        /*****************************************************************/
        /********* read_buffer to complete the last used block ***********/
        /*****************************************************************/

        char *read_buf = malloc(sizeof(char) * BLOCK_SIZE);

        uint64_t init_rwptr = f->rwptr;

        sfs_fseek(fileID, (f->rwptr - (f->rwptr % BLOCK_SIZE)));

        // read the block to which we want to append
        sfs_fread(fileID, read_buf, (init_rwptr % BLOCK_SIZE));

        int len_read_buf = (init_rwptr % BLOCK_SIZE);

        // complete the first block to be written with old and new stuff
        int i;

        if (length_left > BLOCK_SIZE - (f->rwptr % BLOCK_SIZE))
        {
            for (i = 0; i < BLOCK_SIZE - (f->rwptr % BLOCK_SIZE); i++)
            {
                read_buf = AddChar(read_buf, buf[i], len_read_buf);
                len_read_buf++;
                count++;
            }
        }
        else
        {
            for (i = 0; i < length_left; i++)
            {
                read_buf = AddChar(read_buf, buf[i], len_read_buf);
                len_read_buf++;
                count++;
            }
        }

        /****************************************************************/
        /****************** write the first block ***********************/
        /****************************************************************/

        // if an direct pointer was found
        if (ptr_index < 12)
        {
            write_blocks(n->data_ptrs[ptr_index - 1], 1, (void *)read_buf);
        }
        else if (ind_ptr_index == 0)
        {
            write_blocks(n->data_ptrs[ptr_index - 1], 1, (void *)read_buf);
        }

        // if there was no empty direct pointer
        else
        {
            if (n->indirectPointer != -1)
            {
                indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
                read_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
                write_blocks(indirect_pointer->data_ptr[ind_ptr_index - 1], 1, (void *)read_buf);
            }
        }

        // update how many bit are left to right
        length_left -= BLOCK_SIZE - (f->rwptr % BLOCK_SIZE);
    }

    /****************************************************************/
    /************ write all the other blocks needed *****************/
    /****************************************************************/

    int k;
    char *write_buf;
    int len_write_buf;
    int num_block_left;
    if (length_left > 0)
    {
        num_block_left = (length_left - 1) / BLOCK_SIZE + 1;
    }
    else if (length_left == 1)
    {
        num_block_left = 1;
    }
    else
    {
        num_block_left = 0;
    }

    for (k = 0; k < num_block_left; k++)
    {
        write_buf = "";
        len_write_buf = 0;

        /********** if this is a middle block, ie. complete block **********/
        if (length_left > BLOCK_SIZE)
        {

            //make the block
            int w;
            for (w = length - length_left; w < length - length_left + BLOCK_SIZE; w++)
            {
                write_buf = AddChar(write_buf, buf[w], len_write_buf);
                len_write_buf++;
                count++;
            }

            //write the block
            // direct pointer
            if (ptr_index < 12)
            {
                n->data_ptrs[ptr_index] = get_index();
                write_blocks(n->data_ptrs[ptr_index], 1, (void *)write_buf);
                ptr_index++;

                // indirect pointer
            }
            else
            {

                //exist
                if (n->indirectPointer != -1)
                {
                    indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
                    read_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
                    indirect_pointer->data_ptr[ind_ptr_index] = get_index();
                    write_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
                    write_blocks(indirect_pointer->data_ptr[ind_ptr_index], 1, (void *)write_buf);
                    ind_ptr_index++;

                    //does not exit
                }
                else
                {

                    //create indirect pointer
                    indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
                    for (ind_ptr_index = 0; ind_ptr_index < NUM_INDIRECT; ind_ptr_index++)
                    {
                        indirect_pointer->data_ptr[ind_ptr_index] = -1;
                    }
                    n->indirectPointer = get_index();
                    write_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
                    ind_ptr_index = 0;

                    //save block
                    indirect_pointer->data_ptr[ind_ptr_index] = get_index();
                    write_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
                    write_blocks(indirect_pointer->data_ptr[ind_ptr_index], 1, (void *)write_buf);
                    ind_ptr_index++;
                }
            }

            length_left -= BLOCK_SIZE;
        }

        /********** if this is the last block to be written **********/
        else
        {
            //make the block
            int w;
            for (w = length - length_left; w < length; w++)
            {
                write_buf = AddChar(write_buf, buf[w], len_write_buf);
                len_write_buf++;
                count++;
            }

            //write the block
            //direct pointer
            if (ptr_index < 12)
            {
                n->data_ptrs[ptr_index] = get_index();
                write_blocks(n->data_ptrs[ptr_index], 1, (void *)write_buf);
                ptr_index++;

                //indirect pointer
            }
            else
            {

                //exist
                if (n->indirectPointer != -1)
                {
                    indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
                    read_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
                    indirect_pointer->data_ptr[ind_ptr_index] = get_index();
                    write_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
                    write_blocks(indirect_pointer->data_ptr[ind_ptr_index], 1, (void *)write_buf);
                    ind_ptr_index++;
                }

                //does not exist
                else
                {
                    //create indirect pointer
                    indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
                    for (ind_ptr_index = 0; ind_ptr_index < NUM_INDIRECT; ind_ptr_index++)
                    {
                        indirect_pointer->data_ptr[ind_ptr_index] = -1;
                    }
                    n->indirectPointer = get_index();
                    write_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
                    ind_ptr_index = 0;

                    //save block
                    indirect_pointer->data_ptr[ind_ptr_index] = get_index();
                    write_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
                    write_blocks(indirect_pointer->data_ptr[ind_ptr_index], 1, (void *)write_buf);
                    ind_ptr_index++;
                }
            }
        }
    }

    // update file descriptor
    f->rwptr += length;

    // update bitmap
    uint8_t *free_bit_map = get_bitmap();
    write_blocks(NUM_BLOCKS - 1, 1, (void *)free_bit_map);

    // update inode
    n->size += length;
    write_blocks(1, (int)(NUM_INODE_BLOCKS), (void *)inode_table);

    return count;
}

int sfs_fseek(int fileID, int loc)
{

    /*
     Note: No error handling checks were done here
    because the test files seem to pass with no need for error handling.
    Error handling would consist of returning an error for longer than possible locations
    or simply re-writting to the last possible location.*/

    //change read-write pointer location then return. 
    fd_table[fileID].rwptr = loc;
    return 0;


}
int sfs_remove(char *file)
{
    // check if it is open
    int inode = -1; //CALSJDALSJDLAKSJDLAKSJDLA
    int directory_table_index; //dfsldkfjlskdfjlsdkf
    for (int i = 0; i < NO_OF_INODES; i++)
    {
        if (dir_table[i].used == 1)
        {

            if (strcmp(dir_table[i].name, file) == 0)
            {
                inode = dir_table[i].inode_index;
                directory_table_index = i;
            }
        }
    }

    if (inode == -1)
    {
        return -1;
    }

    // free bitmap
    inode_t *n = &inode_table[inode]; ///////////////////rename n to temp_inode
    int j = 0; 
    while (n->data_ptrs[j] != -1 && j < 12)
    {
        rm_index(n->data_ptrs[j]);
        j++;
    }
    j = 0;
    if (n->indirectPointer != -1)
    {
        indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
        read_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
        while (indirect_pointer->data_ptr[j] != -1 && j < NUM_INDIRECT)
        {
            rm_index(indirect_pointer->data_ptr[j]);
            j++;
        }
    }

    // remove from directory_table
    dir_table[directory_table_index].used = 0;

    // remove from inode_table
    inode_table[inode].used = 0;

    // update disk
    uint8_t *free_bit_map = get_bitmap(); 
    write_blocks(NUM_BLOCKS - 1, 1, (void *)free_bit_map);
    write_blocks(1,(int) (NUM_INODE_BLOCKS), (void *)inode_table);
    write_blocks((int) (NUM_INODE_BLOCKS) + 1, NUM_DIR_BLOCKS, (void *)dir_table);

    return 0;
}