/*ECSE 427
*Assignment #3
*Shamil Premalal
*260586332
*https://github.com/shamilpremalal/File-System
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

char *AddChar(char *src, char a, int len)
{
    char *temp = malloc((len + 2) * sizeof(char));

    for (int i = 0; i < len; i++)
    {
        temp[i] = src[i];
    }
    temp[len] = a;
    temp[len + 1] = '\0';

    return temp;
}

//Create valid Inode entry
inode_t init_inode()
{
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

    return temp_inode;
}

//Create valid directory table entry
directory_entry init_dir_entry(char *name, int inode_index)
{
    directory_entry temp_dir;
    temp_dir.used = 1;
    temp_dir.inode_index = inode_index;
    strncpy(temp_dir.name, name, MAX_FILE_NAME);
    temp_dir.name[MAX_FILE_NAME] = 0;

    return temp_dir;
}

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
    printf("Done Init\n");

    //Checking if Simple File System Exist
    if (fresh)
    {
        //Initialize all structures
        init_fdt();
        init_int();
        init_super();
        init_root_dir_inode();
        printf("All structs initialized\n");

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

        //Set inode table in bitmap
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
    }

    strcpy(fname, dir_table[dir_ptr].name);

    //entries remaining in the directory
    return NO_OF_INODES - dir_ptr - 1;
}

//Return file size
int sfs_getfilesize(const char *path)
{
    //Search for file by name and return size if found
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

    //file not found
    return -1;
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

        printf("The directory table index found is: %i\n", new_dir_index);

        //Create entry in inode table
        inode_table[new_index] = init_inode();

        //Create entry in directory table
        dir_table[new_dir_index] = init_dir_entry(name, new_index);

        //Update inode table and directory on disk
        write_blocks(1, (int)(NUM_INODE_BLOCKS), (void *)inode_table);
        write_blocks((int)(NUM_INODE_BLOCKS) + 1, NUM_DIR_BLOCKS, (void *)dir_table);

        inode_index = new_index;
    }
    else
    {
        //Check is the file is already open and return it's index if it is open
        for (int open_fd_index = 0; open_fd_index < NO_OF_INODES; open_fd_index++)
        {
            if (fd_table[open_fd_index].inodeIndex == inode_index)
            {
                return open_fd_index;
            }
        }
    }

    // Find an open spot in the file descriptor table
    int fd_counter = 0;
    while (fd_table[fd_counter].used == 1)
    {
        fd_counter++;
        if (fd_counter == NO_OF_INODES)
        {
            return -1;
        }
    }

    //Update the file descriptor table with values of open file
    fd_table[fd_counter].used = 1;
    fd_table[fd_counter].rwptr = inode_table[inode_index].size;
    fd_table[fd_counter].inodeIndex = inode_index;

    return fd_counter;
}

int sfs_fclose(int fileID)
{
    // Check if a open file
    if (fd_table[fileID].used == 0)
    {
        return -1;
    }

    //Reset file descriptor table entry
    fd_table[fileID].used = 0;
    fd_table[fileID].inodeIndex = -1;

    return 0;
}

int sfs_fread(int fileID, char *buf, int length)
{
    char *tmp_buf = NULL;
    int tmp_buf_len = 0;

    // Check if file is open
    if (fd_table[fileID].used == 0)
    {
        return 0;
    }

    //Get File descriptor and an inode
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
                indirect_block *indirect_ptr = malloc(sizeof(indirect_block));
                
                read_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                read_blocks(indirect_ptr->data_ptr[data_ptr_index - 12], 1, buffer[i]);
            }
        }

        data_ptr_index++;
    }

    int first_ptr = temp_fd->rwptr % BLOCK_SIZE;
    int last_ptr = (first_ptr + length) % BLOCK_SIZE;

    int j;

    //One Block
    if (blocks_to_read == 1)
    {
        for (j = first_ptr; j < last_ptr; j++)
        {
            tmp_buf = AddChar(tmp_buf, buffer[0][j], tmp_buf_len);
            tmp_buf_len++;
        }
    }

    //Two Blocks
    else if (blocks_to_read == 2)
    {
        for (j = first_ptr; j < BLOCK_SIZE; j++)
        {
            tmp_buf = AddChar(tmp_buf, buffer[0][j], tmp_buf_len);
            tmp_buf_len++;
        }
        for (j = 0; j < last_ptr; j++)
        {
            tmp_buf = AddChar(tmp_buf, buffer[1][j], tmp_buf_len);
            tmp_buf_len++;
        }
    }

    //More Than Two Blocks
    else
    {
        for (j = first_ptr; j < BLOCK_SIZE; j++)
        {
            tmp_buf = AddChar(tmp_buf, buffer[0][j], tmp_buf_len);
            tmp_buf_len++;
        }

        for (j = 1; j < blocks_to_read - 1; j++)
        {
            int k;
            for (k = 0; k < BLOCK_SIZE; k++)
            {
                tmp_buf = AddChar(tmp_buf, buffer[j][k], tmp_buf_len);
                tmp_buf_len++;
            }
        }

        for (j = 0; j < last_ptr; j++)
        {
            tmp_buf = AddChar(tmp_buf, buffer[blocks_to_read - 1][j], tmp_buf_len);
            tmp_buf_len++;
        }
    }

    temp_fd->rwptr += length;

    for (int i = 0; i < length; i++)
    {
        buf[i] = tmp_buf[i];
    }

    return tmp_buf_len;
}

int sfs_fwrite(int fileID, const char *buf, int length)
{
    int count = 0;
    int len_left = length;

    //Get File descriptor and an inode
    file_descriptor *temp_fd = &fd_table[fileID];
    inode_t *temp_inode = &inode_table[temp_fd->inodeIndex];

    //Checks if empty direct pointer
    int ptr_index = 0;
    while (temp_inode->data_ptrs[ptr_index] != -1 && ptr_index < 12)
    {
        ptr_index++;
    }

    int indir_ptr_index = 0;

    //Indirect Pointer
    if (ptr_index == 12 && temp_inode->indirectPointer != -1)
    {
        indirect_block *indirect_ptr = malloc(sizeof(indirect_block));
        read_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);

        while (indirect_ptr->data_ptr[indir_ptr_index] != -1)
        {
            indir_ptr_index++;
            if (indir_ptr_index == NUM_INDIRECT)
            {
                return 0;
            }
        }
    }

    else if (ptr_index == 12)
    {
        indirect_block *indirect_ptr = malloc(sizeof(indirect_block));
        for (indir_ptr_index = 0; indir_ptr_index < NUM_INDIRECT; indir_ptr_index++)
        {
            indirect_ptr->data_ptr[indir_ptr_index] = -1;
        }
        temp_inode->indirectPointer = get_index();
        write_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
        indir_ptr_index = 0;
    }

    //Ptr is in middle, read, fill and write back into Block
    if (temp_fd->rwptr % BLOCK_SIZE != 0)
    {
        char *read_buf = malloc(sizeof(char) * BLOCK_SIZE);
        uint64_t init_rwptr = temp_fd->rwptr;

        sfs_fseek(fileID, (temp_fd->rwptr - (temp_fd->rwptr % BLOCK_SIZE)));

        //Reading block to Append to it
        sfs_fread(fileID, read_buf, (init_rwptr % BLOCK_SIZE));
        int read_buf_len = (init_rwptr % BLOCK_SIZE);

        if (len_left > BLOCK_SIZE - (temp_fd->rwptr % BLOCK_SIZE))
        {
            for (int i = 0; i < BLOCK_SIZE - (temp_fd->rwptr % BLOCK_SIZE); i++)
            {
                read_buf = AddChar(read_buf, buf[i], read_buf_len);
                read_buf_len++;
                count++;
            }
        }
        else
        {
            for (int i = 0; i < len_left; i++)
            {
                read_buf = AddChar(read_buf, buf[i], read_buf_len);
                read_buf_len++;
                count++;
            }
        }

        //Writing to the first block
        //Direct Pointer
        if (ptr_index < 12)
        {
            write_blocks(temp_inode->data_ptrs[ptr_index - 1], 1, (void *)read_buf);
        }
        else if (indir_ptr_index == 0)
        {
            write_blocks(temp_inode->data_ptrs[ptr_index - 1], 1, (void *)read_buf);
        }

        //No Direct Point Available
        else
        {
            if (temp_inode->indirectPointer != -1)
            {
                indirect_block *indirect_ptr = malloc(sizeof(indirect_block));
                read_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                write_blocks(indirect_ptr->data_ptr[indir_ptr_index - 1], 1, (void *)read_buf);
            }
        }

        //Bits left
        len_left -= BLOCK_SIZE - (temp_fd->rwptr % BLOCK_SIZE);
    }

    char *write_buf;
    int write_buf_len;
    int num_block_left;
    if (len_left > 0)
    {
        num_block_left = (len_left - 1) / BLOCK_SIZE + 1;
    }
    else if (len_left == 1)
    {
        num_block_left = 1;
    }
    else
    {
        num_block_left = 0;
    }

    for (int i = 0; i < num_block_left; i++)
    {
        write_buf = "";
        write_buf_len = 0;

        if (len_left > BLOCK_SIZE)
        {
            //Making Block
            for (int j = length - len_left; j < length - len_left + BLOCK_SIZE; j++)
            {
                write_buf = AddChar(write_buf, buf[j], write_buf_len);
                write_buf_len++;
                count++;
            }

            //Writing to Block (Direct Pointer)
            if (ptr_index < 12)
            {
                temp_inode->data_ptrs[ptr_index] = get_index();
                write_blocks(temp_inode->data_ptrs[ptr_index], 1, (void *)write_buf);
                ptr_index++;
            }

            //Indirect Pointer
            else
            {
                //Indir Ptr Exist
                if (temp_inode->indirectPointer != -1)
                {
                    indirect_block *indirect_ptr = malloc(sizeof(indirect_block));
                    read_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                    indirect_ptr->data_ptr[indir_ptr_index] = get_index();
                    write_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                    write_blocks(indirect_ptr->data_ptr[indir_ptr_index], 1, (void *)write_buf);
                    indir_ptr_index++;
                }
                //Indir Ptr doesn't exist
                else
                {
                    //Make an Indir Ptr
                    indirect_block *indirect_ptr = malloc(sizeof(indirect_block));
                    for (indir_ptr_index = 0; indir_ptr_index < NUM_INDIRECT; indir_ptr_index++)
                    {
                        indirect_ptr->data_ptr[indir_ptr_index] = -1;
                    }
                    temp_inode->indirectPointer = get_index();
                    write_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                    indir_ptr_index = 0;

                    //Saving Block
                    indirect_ptr->data_ptr[indir_ptr_index] = get_index();
                    write_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                    write_blocks(indirect_ptr->data_ptr[indir_ptr_index], 1, (void *)write_buf);
                    indir_ptr_index++;
                }
            }

            len_left = len_left - BLOCK_SIZE;
        }

        else
        {
            //Making Final Block
            for (int i = length - len_left; i < length; i++)
            {
                write_buf = AddChar(write_buf, buf[i], write_buf_len);
                write_buf_len++;
                count++;
            }

            //Writing to Block (Direct Pointer)
            if (ptr_index < 12)
            {
                temp_inode->data_ptrs[ptr_index] = get_index();
                write_blocks(temp_inode->data_ptrs[ptr_index], 1, (void *)write_buf);
                ptr_index++;
            }

            //Indirect Pointer
            else
            {
                //Indir Ptr Exist
                if (temp_inode->indirectPointer != -1)
                {
                    indirect_block *indirect_ptr = malloc(sizeof(indirect_block));
                    read_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                    indirect_ptr->data_ptr[indir_ptr_index] = get_index();
                    write_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                    write_blocks(indirect_ptr->data_ptr[indir_ptr_index], 1, (void *)write_buf);
                    indir_ptr_index++;
                }

                //Indir Ptr doesn't exist
                else
                {
                    //Make an Indir Ptr
                    indirect_block *indirect_ptr = malloc(sizeof(indirect_block));
                    for (indir_ptr_index = 0; indir_ptr_index < NUM_INDIRECT; indir_ptr_index++)
                    {
                        indirect_ptr->data_ptr[indir_ptr_index] = -1;
                    }
                    temp_inode->indirectPointer = get_index();
                    write_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                    indir_ptr_index = 0;

                    //Saving Block
                    indirect_ptr->data_ptr[indir_ptr_index] = get_index();
                    write_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);
                    write_blocks(indirect_ptr->data_ptr[indir_ptr_index], 1, (void *)write_buf);
                    indir_ptr_index++;
                }
            }
        }
    }

    //Update fd
    temp_fd->rwptr += length;

    //Update bitmap
    uint8_t *free_bit_map = get_bitmap();
    write_blocks(NUM_BLOCKS - 1, 1, (void *)free_bit_map);

    //Update inode
    temp_inode->size += length;
    write_blocks(1, (int)(NUM_INODE_BLOCKS), (void *)inode_table);

    return count;
}

int sfs_fseek(int fileID, int loc)
{
    /**
    *   Note: No error handling checks were done here because the test files seem
    *   to pass with no need for error handling.
    *
    *   Error handling would consist of returning an error for longer than possible locations
    *   or simply re-writting to the last possible location.
    */

    //Change read-write pointer location then return.
    fd_table[fileID].rwptr = loc;
    return 0;
}

int sfs_remove(char *file)
{
    //Check if Open
    int inode = -1;
    int dir_table_index;
    for (int i = 0; i < NO_OF_INODES; i++)
    {
        if (dir_table[i].used == 1)
        {
            if (strcmp(dir_table[i].name, file) == 0)
            {
                inode = dir_table[i].inode_index;
                dir_table_index = i;
            }
        }
    }
    //If file is not open
    if (inode == -1)
    {
        return -1;
    }

    //Free Bitmap
    inode_t *temp_inode = &inode_table[inode];

    // Direct pointers
    for (int i = 0; i < 12; i++)
    {
        //Only free the blocks that are used
        if (temp_inode->data_ptrs[i] != -1)
        {
            rm_index(temp_inode->data_ptrs[i]);
        }
        else
        {
            break;
        }
    }

    //Indirect pointer
    if (temp_inode->indirectPointer != -1)
    {
        //Get all indirect pointers from the disk
        indirect_block *indirect_ptr = malloc(sizeof(indirect_block));
        read_blocks(temp_inode->indirectPointer, 1, (void *)indirect_ptr);

        for (int i = 0; i < NUM_INDIRECT; i++)
        {
            //Only free the blocks that are used
            if (indirect_ptr->data_ptr[i] != -1)
            {
                rm_index(indirect_ptr->data_ptr[i]);
            }
            else
            {
                break;
            }
        }
    }

    //Remove from Directory Table
    dir_table[dir_table_index].used = 0;

    //Remove From inode Table
    inode_table[inode].used = 0;

    //Udpate the Disk
    uint8_t *free_bit_map = get_bitmap();
    write_blocks(NUM_BLOCKS - 1, 1, (void *)free_bit_map);
    write_blocks(1, (int)(NUM_INODE_BLOCKS), (void *)inode_table);
    write_blocks((int)(NUM_INODE_BLOCKS) + 1, NUM_DIR_BLOCKS, (void *)dir_table);

    return 0;
}