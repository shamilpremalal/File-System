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
int dir_table_index = -1;

void init_fdt()
{

    for (int i = 0; i < NO_OF_INODES; i++)
    {

        fd_table[i].used = 0;
        fd_table[i].inodeIndex = -1;
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
        inode_table[i].used = -1;

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
        printf("\nAll structs initialized\n");
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
    if (dir_table_index == NO_OF_INODES)
    {
        dir_table_index = -1;
        return 0;
    }
    //Finds next used Directory
    while (dir_table[dir_table_index].used == 0)
    {
        dir_table_index++;
        //If at end of the table
        if (dir_table_index == NO_OF_INODES)
        {
            dir_table_index = 0;
            return 0;
        }

        //Checked entire table
        if (count == NO_OF_INODES)
        {
            return 0;
        }
        count++;
    }
    int i;
    for (i = 0; i < strlen(dir_table[dir_table_index].name); i++)
    {
        fname[i] = (dir_table[dir_table_index+1].name)[i];
    }
    fname[i] = '\0'; //null terminator

    strcpy(fname,dir_table[dir_table_index].name);
    //entries left in the directory
    return sizeof(dir_table) / sizeof(dir_table[0]) - dir_table_index - 1;
}

//Returns Size of a file
int sfs_getfilesize(const char *path)
{
    for (int i = 0; i < NO_OF_INODES; i++)
    {
        if (dir_table[i].used == 1)
        {
            printf("dir table name is: %s \n",dir_table[i].name);
            if (strcmp(dir_table[i].name, path) == 0)
            {
                return inode_table[dir_table[i].inode_index].size;
            }
        }
    }

    return 0;
}

int sfs_fopen(char *name)
{
    if (strlen(name) > MAX_FILE_NAME - 1 || strlen(name) == 0)
    {
        return -1;
    }

    //Look for the file
    int inode_index = -1;
    for (int i = 0; i < NO_OF_INODES; i++)
    {
        if (dir_table[i].used == 1)
        {
            if (strcmp(dir_table[i].name, name) == 0)
            {
                inode_index = dir_table[i].inode_index;
                printf("File Found in dir table at index: %d\n",inode_index);

            }
        }
    }


    //File does not exist
    if (inode_index == -1)
    {
        printf("File Not Found\n");

        //Find 1st available in inode table
        int new_index = 1;
        while (inode_table[new_index].used == 1 && new_index < NO_OF_INODES)
        {
            new_index++;

            //If the table is Full
            if (new_index == NO_OF_INODES)
            {
                return -1;
            }
        }

        printf("The inode table index found is: %i\n", new_index);


        //Find 1st available in the dir table
        int new_dir_table_index = 0;
        while (dir_table[new_dir_table_index].used == 1 && new_dir_table_index < NO_OF_INODES)
        {
            new_dir_table_index++;

            //If the table is Full
            if (new_dir_table_index == NO_OF_INODES)
            {
                return -1;
            }
        }

        printf("The dir table index found is: %i\n", new_dir_table_index);


        //A free index in the Inode table and the discriptor table has been found

        //initialize a temporary I node
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

        //new inode table entry
        inode_table[new_index] = temp_inode;

        //Initialize temporary directory
        directory_entry temp_dir;
        temp_dir.used = 1;
        temp_dir.inode_index = new_index;

        strncpy(temp_dir.name, name,MAX_FILE_NAME);
        temp_dir.name[MAX_FILE_NAME] = 0;
        
        //store temp
        dir_table[new_dir_table_index] = temp_dir;

        //updating inode table
        write_blocks(1, (int)(NUM_INODE_BLOCKS), (void *)inode_table);

        //update dir_table
        write_blocks((int)(NUM_INODE_BLOCKS) + 1, NUM_DIR_BLOCKS, (void *)dir_table);

        inode_index = new_index;
    }
        // check is the file is already open
        int open_file_index;
        printf("Value of I_node index to find is: %i\n", inode_index);
        for (open_file_index = 0; open_file_index < NO_OF_INODES; open_file_index++)
        {
            if (fd_table[open_file_index].inodeIndex == inode_index)
            {
                 printf("The inode index stored in fd_table at %i is %i\n", open_file_index,fd_table[open_file_index].inodeIndex);
                printf("The file des index returned is: %i\n", open_file_index);
                return open_file_index;
            }
        }

    

    // find a spot on the file descriptor table
    int new_fdt_index = 0;
    while (fd_table[new_fdt_index].used == 1)
    {
        new_fdt_index++;
        if (new_fdt_index == NO_OF_INODES)
        {
            printf("SFS > There is no more space in the file descriptor table!\n");
            return -1;
        }
    }

    fd_table[new_fdt_index].used = 1;
    fd_table[new_fdt_index].inode = &inode_table[inode_index];
    fd_table[new_fdt_index].rwptr = inode_table[inode_index].size;
    fd_table[new_fdt_index].inodeIndex = inode_index;

    return new_fdt_index;
}

int sfs_fclose(int fileID)
{
    // check if the there is a file open
    if (fd_table[fileID].used == 0)
    {
        printf("SFS > The file %i was not used! \n", fileID);
        return -1;
    }

    fd_table[fileID].used = 0;
    fd_table[fileID].inodeIndex = -1;

    return 0;
}

char *appendCharToCharArray(char *array, char a, int len)
{
    char *ret = malloc((len + 2) * sizeof(char));

    int i;
    for (i = 0; i < len; i++)
    {
        ret[i] = array[i];
    }
    ret[len] = a;
    ret[len + 1] = '\0';

    return ret;
}

int sfs_fread(int fileID, char *buf, int length)
{
    char *temp_buf = NULL;
    int len_temp_buf = 0;

    // make sure this is an open file
    if (fd_table[fileID].used == 0)
    {
        return 0;
    }

    // the the file descriptor and the inode of the file
    file_descriptor *f = &fd_table[fileID];
    inode_t *n = &inode_table[f->inodeIndex];

    //make sure you dont read pass the edn of file
    if (f->rwptr + length > n->size)
    {
        length = n->size - f->rwptr;
    }

    // how many blocks to read
    int num_block_read = ((f->rwptr % BLOCK_SIZE) + length) / BLOCK_SIZE + 1;

    char buffer[num_block_read][BLOCK_SIZE];

    // find in which block is the RWPTR
    uint64_t data_ptr_index = f->rwptr / BLOCK_SIZE;
   // printf("\n\n\n\n INITIAL VALUE OF DATA_PTR_INDEX %lu , NUMBER OF BLOCKS TO READ IS: %i\n\n\n", data_ptr_index,num_block_read);

    /****************************************************************/
    /********************** read the blocks *************************/
    /****************************************************************/
    int i;
    for (i = 0; i < num_block_read; i++)
    {

        // if the block to read is in the direct pointer
        if (data_ptr_index < 12)
        {
            read_blocks(n->data_ptrs[data_ptr_index], 1, buffer[i]);
        }

        // if the block to read is in the indirect pointer
        else
        {
            //if indirect pointer exist
            if (n->indirectPointer != -1)
            {
                indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
                read_blocks(n->indirectPointer, 1, (void *)indirect_pointer);
             //   printf("\n\n\n\n i is %i DATA_PTR_INDEX VALUE %lu NUMBER OF BLOCKS TO READ IS: %i\n\n\n",i, data_ptr_index,num_block_read);
                read_blocks(indirect_pointer->data_ptr[data_ptr_index - 12], 1, buffer[i]);
            }
            else
            {
                printf("SFS > ERROR While reading the file");
            }
        }
        data_ptr_index++;

    }

    /***************************************************************/
    /******* put all the blocks in the output buffer ***************/
    /***************************************************************/

    int start_ptr = f->rwptr % BLOCK_SIZE;
    int end_ptr = (start_ptr + length) % BLOCK_SIZE;

    int j;

    // one block
    if (num_block_read == 1)
    {
        for (j = start_ptr; j < end_ptr; j++)
        {
            temp_buf = appendCharToCharArray(temp_buf, buffer[0][j], len_temp_buf);
            len_temp_buf++;
        }

        // two blocks
    }
    else if (num_block_read == 2)
    {
        for (j = start_ptr; j < BLOCK_SIZE; j++)
        {
            temp_buf = appendCharToCharArray(temp_buf, buffer[0][j], len_temp_buf);
            len_temp_buf++;
        }
        for (j = 0; j < end_ptr; j++)
        {
            temp_buf = appendCharToCharArray(temp_buf, buffer[1][j], len_temp_buf);
            len_temp_buf++;
        }

        // more than two blocks
    }
    else
    {
        for (j = start_ptr; j < BLOCK_SIZE; j++)
        {
            temp_buf = appendCharToCharArray(temp_buf, buffer[0][j], len_temp_buf);
            len_temp_buf++;
        }

        for (j = 1; j < num_block_read - 1; j++)
        {
            int k;
            for (k = 0; k < BLOCK_SIZE; k++)
            {
                temp_buf = appendCharToCharArray(temp_buf, buffer[j][k], len_temp_buf);
                len_temp_buf++;
            }
        }

        for (j = 0; j < end_ptr; j++)
        {
            temp_buf = appendCharToCharArray(temp_buf, buffer[num_block_read - 1][j], len_temp_buf);
            len_temp_buf++;
        }
    }

    f->rwptr += length;

    for (i = 0; i < length; i++)
    {
        buf[i] = temp_buf[i];
    }

    return len_temp_buf;
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
                read_buf = appendCharToCharArray(read_buf, buf[i], len_read_buf);
                len_read_buf++;
                count++;
            }
        }
        else
        {
            for (i = 0; i < length_left; i++)
            {
                read_buf = appendCharToCharArray(read_buf, buf[i], len_read_buf);
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
                write_buf = appendCharToCharArray(write_buf, buf[w], len_write_buf);
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
                write_buf = appendCharToCharArray(write_buf, buf[w], len_write_buf);
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
    // error checking
    if (loc < 0 || loc > MAX_RWPTR)
    {
        printf("SFS > Wrong RW location!\n");
    }

    fd_table[fileID].rwptr = loc;
    return 0;
}
int sfs_remove(char *file)
{
    // check if it is open
    int inode = -1;
    int i;
    int directory_table_index;
    for (i = 0; i < NO_OF_INODES; i++)
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
        printf("SFS > File not found!\n");
        return -1;
    }

    // free bitmap
    inode_t *n = &inode_table[inode];
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
