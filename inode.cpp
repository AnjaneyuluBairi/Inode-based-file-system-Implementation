#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <bits/stdc++.h>
using namespace std;

#define DISK_BLOCKS 131072
#define BLOCK_SIZE 4096
#define NO_OF_INODES 78644
#define NO_OF_FILE_DESCRIPTORS 32

struct inode
{
    int filesize;
    char filename[30];
    int pointers[12];
    int first_block;
};

struct file_to_inode
{
    char filename[30];
    int inode_num;
};

struct super_block
{
    int num_blocks_used_by_superblock = ceil((float)sizeof(super_block) / BLOCK_SIZE);
    int num_blocks_used_by_file_to_inode = ceil(((float)sizeof(struct file_to_inode) * NO_OF_INODES) / BLOCK_SIZE);
    int start_index_of_inodes = num_blocks_used_by_superblock + num_blocks_used_by_file_to_inode;
    int num_blocks_used_by_indoes = ceil(((float)(NO_OF_INODES * sizeof(struct inode))) / BLOCK_SIZE);
    int start_index_of_data_blocks = num_blocks_used_by_superblock + num_blocks_used_by_file_to_inode + num_blocks_used_by_indoes;
    int total_available_blocks = DISK_BLOCKS - start_index_of_data_blocks;
    bool free_indoes_list[NO_OF_INODES];
    bool free_data_blocks_list[DISK_BLOCKS];
};

FILE *disk_pointer;
int count_openfiles;
map<string, int> file_to_inode_map;
map<int, string> inode_to_file_map;

vector<int> free_inode_vec;
vector<int> free_data_block_vec;

vector<int> free_fd_vec;
map<int, pair<int, int>> fd_map;
map<int, int> fd_mode_map;

struct inode inode_arr[NO_OF_INODES];
struct file_to_inode file_to_inode_arr[NO_OF_INODES];

char disk_name[100], file_name[100];
static int disk_active = 0;
struct super_block sb;

void store_sb_info_at_the_start()
{
    fseek(disk_pointer, 0, SEEK_SET);
    int length = sizeof(struct super_block);
    char sb_buffer[length];
    bzero(sb_buffer, length);
    memcpy(sb_buffer, &sb, sizeof(sb));
    fwrite(sb_buffer, sizeof(char), sizeof(sb), disk_pointer);
}

void store_file_to_inode_map_info()
{
    fseek(disk_pointer, (sb.num_blocks_used_by_superblock) * BLOCK_SIZE, SEEK_SET);
    int length = sizeof(file_to_inode_arr);
    char dir_buffer[length];
    bzero(dir_buffer, length);
    memcpy(dir_buffer, file_to_inode_arr, length);
    fwrite(dir_buffer, sizeof(char), length, disk_pointer);
}

void store_indoes_info()
{
    fseek(disk_pointer, (sb.start_index_of_inodes) * BLOCK_SIZE, SEEK_SET);
    int length = sizeof(inode_arr);
    char inode_buffer[length];
    bzero(inode_buffer, length);
    memcpy(inode_buffer, inode_arr, length);
    fwrite(inode_buffer, sizeof(char), length, disk_pointer);
}

void clear_all_data_structres()
{
    free_inode_vec.clear();
    free_data_block_vec.clear();
    free_fd_vec.clear();
    fd_mode_map.clear();
    fd_map.clear();
    file_to_inode_map.clear();
    inode_to_file_map.clear();
}

int create_disk(char *disk_name)
{

    char buff[BLOCK_SIZE];

    disk_pointer = fopen(disk_name, "wb");
    memset(buff, 0, BLOCK_SIZE);
    for (int i = 0; i < DISK_BLOCKS; i++)
    {
        fwrite(buff, 1, BLOCK_SIZE, disk_pointer);
    }
    struct super_block sb;

    for (int i = 0; i < sb.start_index_of_data_blocks; i++)
    {
        sb.free_data_blocks_list[i] = true;
    }
    for (int i = sb.start_index_of_data_blocks; i < DISK_BLOCKS; i++)
    {
        sb.free_data_blocks_list[i] = false;
    }

    for (int i = 0; i < NO_OF_INODES; i++)
    {
        sb.free_indoes_list[i] = false;
    }

    for (int i = 0; i < NO_OF_INODES; i++)
    {
        inode_arr[i].first_block = -1;
        for (int j = 0; j < 12; j++)
        {

            inode_arr[i].pointers[j] = -1;
        }
    }

    int length;

    store_sb_info_at_the_start();
    store_file_to_inode_map_info();
    store_indoes_info();

    fclose(disk_pointer);
    cout << "Disk Created" << endl;

    return 1;
}

void get_sb_from_disk()
{

    char sb_buffer[sizeof(sb)];
    bzero(sb_buffer, sizeof(sb_buffer));
    fread(sb_buffer, sizeof(char), sizeof(sb), disk_pointer);
    memcpy(&sb, sb_buffer, sizeof(sb));
}

void get_file_inode_from_disk()
{
    fseek(disk_pointer, (sb.num_blocks_used_by_superblock) * BLOCK_SIZE, SEEK_SET);
    int length = sizeof(file_to_inode_arr);
    char dir_buffer[length];
    bzero(dir_buffer, length);
    fread(dir_buffer, sizeof(char), length, disk_pointer);
    memcpy(file_to_inode_arr, dir_buffer, length);
}

void get_inodes_block_info_from_disk()
{
    fseek(disk_pointer, (sb.start_index_of_inodes) * BLOCK_SIZE, SEEK_SET);
    int length = sizeof(inode_arr);
    char inode_buffer[length];
    bzero(inode_buffer, length);
    fread(inode_buffer, sizeof(char), length, disk_pointer);
}
int mount_disk(char *name)
{
    disk_pointer = fopen(name, "rb+");
    if (disk_pointer)
    {
        int length;

        get_sb_from_disk();
        get_file_inode_from_disk();
        get_inodes_block_info_from_disk();

        int i = NO_OF_INODES - 1;
        while (i >= 0)
        {
            if (sb.free_indoes_list[i])
            {
                string filename = string(file_to_inode_arr[i].filename);
                file_to_inode_map[filename] = file_to_inode_arr[i].inode_num;
                inode_to_file_map[file_to_inode_arr[i].inode_num] = filename;
            }
            else
            {
                free_inode_vec.push_back(i);
            }
            i--;
        }

        i = DISK_BLOCKS - 1;
        while (i >= sb.start_index_of_data_blocks)
        {
            if (!sb.free_data_blocks_list[i])
            {
                free_data_block_vec.push_back(i);
            }
            i--;
        }

        i = NO_OF_FILE_DESCRIPTORS - 1;
        while (i >= 0)
        {
            free_fd_vec.push_back(i);
            i--;
        }
        cout << "Disk is mounted" << endl;
        disk_active = 1;
        return 1;
    }
    else
    {
        cout << "Disk doesnt exists" << endl;
    }
    return 0;
}

int unmount_disk()
{

    if (disk_active)
    {
        int i = DISK_BLOCKS - 1;
        while (i >= sb.start_index_of_data_blocks)
        {
            sb.free_data_blocks_list[i] = true;
            i--;
        }

        i = 0;
        while (i < free_data_block_vec.size())
        {
            sb.free_data_blocks_list[free_data_block_vec[i]] = false;
            i++;
        }
        free_data_block_vec.clear();

        i = 0;
        while (i < NO_OF_INODES)
        {
            sb.free_indoes_list[i] = true;
            i++;
        }
        i = 0;
        while (i < free_inode_vec.size())
        {
            sb.free_indoes_list[free_inode_vec[i]] = false;
            i++;
        }

        store_sb_info_at_the_start();
        store_file_to_inode_map_info();
        store_indoes_info();

        clear_all_data_structres();

        cout << "Disk Unmounted" << endl;
        fclose(disk_pointer);

        disk_active = 0;
        return 0;
    }
    else
    {
        cout << "error: no open disk" << endl;
        return -1;
    }
}

int single_write(int block, char *buffer, int size, int start_position)
{
    bool check = (block < 0) || (block >= DISK_BLOCKS);
    if (check)
    {
        fprintf(stderr, "block index out of bounds\n");
        return -1;
    }
    int val = fseek(disk_pointer, (block * BLOCK_SIZE) + start_position, SEEK_SET);
    if (val < 0)
    {
        perror("failed to lseek");
        return -1;
    }
    val = fwrite(buffer, sizeof(char), size, disk_pointer);
    if (val < 0)
    {
        perror("failed to write");
        return -1;
    }

    return 0;
}

void print_list_files()
{
    cout;
    cout << "List of All files";
    cout << endl;
    for (auto i : file_to_inode_map)
    {
        cout << i.first;
        cout << " -> inode : ";
        cout << i.second << endl;
    }
    return;
}

int single_read(int block, char *buffer)
{
    cout;
    if (disk_active == 0)
    {
        fprintf(stderr, "disk not active\n");

        return -1;
    }
    bool check = (block < 0) || (block >= DISK_BLOCKS);
    if (check)
    {
        fprintf(stderr, "block index out of bounds\n");

        return -1;
    }
    int val = fseek(disk_pointer, block * BLOCK_SIZE, SEEK_SET);
    if (val < 0)
    {
        perror("failed to fseek");

        return -1;
    }
    val = fread(buffer, sizeof(char), BLOCK_SIZE, disk_pointer);
    if (val < 0)
    {
        perror("failed to read");

        return -1;
    }

    return 0;
}

void print_list_open_files()
{
    cout << "List of opened files " << endl;
    for (auto i : fd_map)
    {
        cout << "file name: ";
        cout << inode_to_file_map[i.second.first]; // i.second.first gives inode
        cout << " , fd: ";
        cout << i.first << " ,mode:"; // i.first gives fd
        int mode = fd_mode_map[i.first];
        if (mode == 0)
            cout << "READ MODE" << endl;
        if (mode == 1)
            cout << "WRITE MODE" << endl;
        if (mode == 2)
            cout << "APPEND MODE" << endl;
    }
    return;
}

// file opeartions

int create_file(char *name)
{
    string filename = string(name);
    bool check = file_to_inode_map.find(filename) != file_to_inode_map.end();
    if (check)
    {
        cout << "File already present !!!" << endl;
        return -1;
    }
    check = free_inode_vec.size() == 0;
    if (check)
    {
        cout << "No more Inodes available" << endl;
        return -1;
    }
    check = free_data_block_vec.size() == 0;
    if (check)
    {
        cout << " No more DataBlock available" << endl;
        return -1;
    }

    int next_free_inode = free_inode_vec.back();
    int next_free_datablock = free_data_block_vec.back();
    free_inode_vec.pop_back();
    free_data_block_vec.pop_back();

    file_to_inode_map[filename] = next_free_inode;
    inode_to_file_map[next_free_inode] = filename;

    inode_arr[next_free_inode].pointers[0] = next_free_datablock;
    inode_arr[next_free_inode].filesize = 0;

    strcpy(file_to_inode_arr[next_free_inode].filename, name);
    file_to_inode_arr[next_free_inode].inode_num = next_free_inode;

    cout << "File Successfully Created";
    cout << endl;
    return 1;
}

int open_file(char *name)
{
    string filename = string(name);
    bool check = (file_to_inode_map.find(filename) == file_to_inode_map.end());
    if (check)
    {
        cout << " File not found !!!" << endl;
        return -1;
    }
    check = (free_fd_vec.size() == 0);
    if (check)
    {
        cout << "File descriptor not available" << endl;
        return -1;
    }

    int file_mode = -1;
    for (;;)
    {
        cout << "0 : read mode\n1 : write mode\n2 : append mode\n";
        cin >> file_mode;
        bool check = (file_mode < 0 || file_mode > 2);
        if (check)
        {
            cout << "Please enter valid number" << endl;
        }
        else
        {
            break;
        }
    }

    int current_inode = file_to_inode_map[filename];

    int fd = free_fd_vec.back();
    free_fd_vec.pop_back();

    count_openfiles++;
    fd_mode_map[fd] = file_mode;
    fd_map[fd].first = current_inode;
    fd_map[fd].second = 0;

    cout << "File " << filename << " opened with file descriptor  : ";
    cout << fd << endl;
    cout << "fd_mode_map " << fd_mode_map.size() << endl;
    return fd;
}

int close_file(int fd)
{
    bool check = fd_map.find(fd) == fd_map.end();
    if (check)
    {

        cout << "file is not opened yet !!!";
        cout << endl;
        return -1;
    }

    fd_map.erase(fd);
    fd_mode_map.erase(fd);
    free_fd_vec.push_back(fd);
    count_openfiles--;

    cout << "File closed successfully";
    cout << endl;
    return 1;
}

int remove_content_inode(int current_inode)
{
    bool deleted = false;
    int i = 0;

    while (i < 12)
    {
        bool c1 = (inode_arr[current_inode].pointers[i] != -1);
        if (c1)
        {
            free_data_block_vec.push_back(inode_arr[current_inode].pointers[i]);
            inode_arr[current_inode].pointers[i] = -1;
        }
        else
        {
            deleted = true;
            break;
        }
        i++;
    }
    inode_arr[current_inode].filesize = 0;
    i = 0;
    while (i < 12)
    {
        inode_arr[current_inode].pointers[i] = -1;
        i++;
    }
    return 0;
}

int delete_file(char *name)
{
    string filename = string(name);

    bool check = file_to_inode_map.find(filename) == file_to_inode_map.end();
    if (check)
    {
        cout << " File doesn't exist";
        cout << endl;
        return -1;
    }

    int current_inode = file_to_inode_map[filename];
    int i = 0;
    while (i < NO_OF_FILE_DESCRIPTORS)
    {
        bool c1 = fd_map.find(i) != fd_map.end();
        bool c2 = fd_map[i].first == current_inode;
        if (c1 && c2)
        {
            cout << "error: Can not delete an opened file";
            cout << endl;
            return -1;
        }
        i++;
    }

    remove_content_inode(current_inode);

    free_inode_vec.push_back(current_inode);

    file_to_inode_arr[current_inode].inode_num = -1;
    char emptyname[30] = "";
    strcpy(file_to_inode_arr[current_inode].filename, emptyname);
    inode_to_file_map.erase(file_to_inode_map[filename]);
    file_to_inode_map.erase(filename);
    cout << "File Deleted successfully ";
    cout << endl;
    return 0;
}

int inner_write(int fd, char *buff, int len, int *bytes_written)
{
    bool cond1, cond2, cond3;
    int cur_position = fd_map[fd].second;
    int file_inode = fd_map[fd].first;
    int full_data_block = cur_position / BLOCK_SIZE;

    bool check = (cur_position % BLOCK_SIZE != 0);
    if (check)
    {
        bool c1 = (full_data_block < 12);
        if (c1)
        {
            int db_w = inode_arr[file_inode].pointers[full_data_block];
            single_write(db_w, buff, len, cur_position % BLOCK_SIZE);
            int temp4 = fd_map[fd].second;
            fd_map[fd].second = fd_map[fd].second + len;
            *bytes_written = *bytes_written + len;
            inode_arr[file_inode].filesize = inode_arr[file_inode].filesize + len;
        }
    }
    else
    {
        bool c1 = (inode_arr[fd_map[fd].first].filesize == 0);
        if (c1)
        {
            single_write(inode_arr[file_inode].pointers[0], buff, len, 0);
            fd_map[fd].second = fd_map[fd].second + len;
            *bytes_written = *bytes_written + len;
            inode_arr[file_inode].filesize = inode_arr[file_inode].filesize + len;
        }
        else
        {
            bool c1 = (cur_position == 0);
            if (!c1)
            {
                if (full_data_block < 12) // 12 10
                {
                    bool check = (free_data_block_vec.size() == 0);
                    if (check)
                    {
                        cout << " no data blocks are free";
                        cout << endl;
                        return -1;
                    }
                    inode_arr[file_inode].pointers[full_data_block] = free_data_block_vec.back();
                    int db_to_write = free_data_block_vec.back();
                    free_data_block_vec.pop_back();

                    single_write(db_to_write, buff, len, cur_position % BLOCK_SIZE);
                    *bytes_written += len;
                    fd_map[fd].second += len;
                    inode_arr[file_inode].filesize += len;
                }
            }
            else
            {
                bool c2 = (fd_map[fd].second == 0);
                if (c2)
                {
                    remove_content_inode(file_inode);
                    bool c3 = (free_data_block_vec.size() == 0);
                    if (c3)
                    {
                        cout << " no data blocks are free";
                        cout << endl;
                        return -1;
                    }

                    inode_arr[file_inode].pointers[0] = free_data_block_vec.back();
                    free_data_block_vec.pop_back();
                }
                single_write(inode_arr[file_inode].pointers[0], buff, len, 0);
                *bytes_written = *bytes_written + len;
                int temp = inode_arr[file_inode].filesize;
                inode_arr[file_inode].filesize = inode_arr[file_inode].filesize + len;
                fd_map[fd].second = fd_map[fd].second + len;
            }
        }
    }
    return 0;
}

int write_file(int fd, int mode)
{

    bool check = (fd_map.find(fd) == fd_map.end());
    if (check)
    {

        cout << "File descriptor " << fd << " doesn't exist";
        cout << endl;
        return -1;
    }

    int current_inode = fd_map[fd].first;
    if (mode == 1) // writing
    {
        bool c1 = (fd_mode_map[fd] != 1);
        if (c1)
        {

            cout << "File descriptor " << fd << " is not opened in write mode";
            cout << endl;
            return -1;
        }

        for (int i = 0; i < NO_OF_FILE_DESCRIPTORS; i++)
        {
            bool c1 = (fd_map.find(i) != fd_map.end());
            if (c1)
            {
                bool c2 = (fd_map[i].first == current_inode);
                if (c2)
                {
                    bool c3 = (fd_mode_map[i] == 0);
                    if (c3)
                    {
                        fd_map[i].second = 0;
                    }
                }
            }
        }
    }

    unsigned int rem_bytes_last_block = BLOCK_SIZE - ((fd_map[fd].second) % BLOCK_SIZE);
    int len = -1;
    cout << "Enter content of file here: " << endl;
    string str;
    cout.flush();
    while (1)
    {
        string temp;
        getline(cin, temp);
        if (temp == "eof")
        {
            str.pop_back();
            str += '\0';
            break;
        }
        str += temp;
        str += '\n';
    }
    str.pop_back();
    int bytes_written = 0;
    bool check2 = (rem_bytes_last_block >= str.size());
    if (check2 == false)
    {

        len = rem_bytes_last_block;
        str = str.substr(len);
        char buff[len + 1];
        bzero(buff, len);
        memcpy(buff, str.c_str(), len);
        buff[len] = '\0';
        inner_write(fd, buff, len, &bytes_written);

        int rem_blocks = str.size() / BLOCK_SIZE;
        while (rem_blocks--)
        {
            len = BLOCK_SIZE;
            char buff[len + 1];
            bzero(buff, len);
            memcpy(buff, str.c_str(), len);
            buff[len] = '\0';
            str = str.substr(len);
            bool c1 = (inner_write(fd, buff, len, &bytes_written) == -1);
            if (c1)
            {
                cout << "no Enough space so able to write only ";
                cout << bytes_written << " bytes written.";
                return -1;
            }
        }
        int remaining_size = str.size() % BLOCK_SIZE;
        len = remaining_size;
        char buff1[len + 1];
        bzero(buff, len);
        memcpy(buff1, str.c_str(), len);
        buff1[len] = '\0';
        bool c1 = (inner_write(fd, buff1, len, &bytes_written) == -1);
        if (c1)
        {
            cout << "No Enough space so able to write only ";
            cout << bytes_written << " bytes written.";
            cout << endl;
            return -1;
        }
    }
    else
    {
        len = str.size();
        char buff[len + 1];
        bzero(buff, len);
        memcpy(buff, str.c_str(), len);
        buff[len] = '\0';
        inner_write(fd, buff, len, &bytes_written);
    }
    cout << bytes_written << "bytes written into the File  Successfully.";
    cout << endl;
    return 0;
}

int read_file(int fd)
{
    bool check = (fd_map.find(fd) == fd_map.end());
    if (check)
    {
        cout << " file is not opened";
        cout << endl;
        return -1;
    }
    check = fd_mode_map[fd] != 0;
    if (check)
    {

        cout << " file with descriptor " << fd << " is not opened in read mode";
        cout << endl;
        return -1;
    }

    int current_inode = fd_map[fd].first;
    struct inode in = inode_arr[current_inode];
    int filesize = in.filesize;

    char *buffer = new char[filesize];
    char *print_buffer = buffer;

    int bytes_read = 0;
    bool reading_partial = false;
    int fs = fd_map[fd].second;

    int number_of_blocks = ceil(((float)inode_arr[current_inode].filesize) / BLOCK_SIZE);
    int total_blocks = number_of_blocks;
    char read_buffer[BLOCK_SIZE];
    int i = 0;
    while (i < 12)
    {
        bool c1 = (number_of_blocks == 0);
        if (c1)
        {
            break;
        }
        int block_number = in.pointers[i];

        single_read(block_number, read_buffer);
        bool c2 = (total_blocks - number_of_blocks >= (fs / BLOCK_SIZE));
        bool c3 = (number_of_blocks > 1);
        if (c2 && c3)
        {
            if (reading_partial)
            {
                memcpy(buffer, read_buffer, BLOCK_SIZE);
                buffer = buffer + BLOCK_SIZE;
                bytes_read += BLOCK_SIZE;
            }
            else
            {
                reading_partial = true;
                memcpy(buffer, read_buffer + (fs % BLOCK_SIZE), (BLOCK_SIZE - fs % BLOCK_SIZE));
                buffer = buffer + (BLOCK_SIZE - fs % BLOCK_SIZE);
                bytes_read += BLOCK_SIZE - fs % BLOCK_SIZE;
            }
        }
        number_of_blocks--;
        i++;
    }
    bool c3 = ((total_blocks - fs / BLOCK_SIZE) > 1);
    bool c4 = ((total_blocks - fs / BLOCK_SIZE) == 1);
    if (c3)
    {
        memcpy(buffer, read_buffer, (inode_arr[current_inode].filesize) % BLOCK_SIZE);
        bytes_read = bytes_read + (inode_arr[current_inode].filesize) % BLOCK_SIZE;
    }
    else if (c4)
    {
        memcpy(buffer, read_buffer + (fs % BLOCK_SIZE), (inode_arr[current_inode].filesize) % BLOCK_SIZE - fs % BLOCK_SIZE);
        bytes_read = bytes_read + (inode_arr[current_inode].filesize) % BLOCK_SIZE - fs % BLOCK_SIZE;
    }
    cout << endl;
    print_buffer[bytes_read] = '\0';
    cout.flush();
    cout << "Content of file is:" << endl;
    cout << print_buffer << endl;
    cout.flush();

    cout << "File read successfully";
    cout << endl;
    return 1;
}

int user_menu()
{
    int choice;
    int fd = -1;
    for (;;)
    {
        cout << endl;
        cout << "1 : create file" << endl;
        cout << "2 : open file" << endl;
        cout << "3 : read file" << endl;
        cout << "4 : write file" << endl;
        cout << "5 : append file" << endl;
        cout << "6 : close file" << endl;
        cout << "7 : delete file" << endl;
        cout << "8 : list of files" << endl;
        cout << "9 : list of opened files" << endl;
        cout << "10: unmount" << endl;
        cout << endl;
        cin.clear();
        cin >> choice;

        if (choice == 1)
        {
            cout << "Enter filename to create : ";
            cin >> file_name;
            create_file(file_name);
        }
        else if (choice == 2)
        {
            cout << "Enter filename to open : ";
            cin >> file_name;
            open_file(file_name);
        }
        else if (choice == 3)
        {
            cout << "Enter fd to read : ";
            cin >> fd;
            read_file(fd);
            cin.clear();
            cout.flush();
        }
        else if (choice == 4)
        {
            cout << "Enter fd to write : ";
            cin >> fd;
            write_file(fd, 1);
            cin.clear();
            cout.flush();
        }
        else if (choice == 5)
        {
            cout << "Enter fd to append : ";
            cin >> fd;
            // write_file(fd, 2);
            cin.clear();
            cout.flush();
        }
        else if (choice == 6)
        {
            cout << "Enter fd to close : ";
            cin >> fd;
            close_file(fd);
        }
        else if (choice == 7)
        {
            cout << "Enter filename to delete : ";
            cin >> file_name;
            delete_file(file_name);
        }
        else if (choice == 8)
        {
            print_list_files();
        }
        else if (choice == 9)
        {
            print_list_open_files();
        }
        else if (choice == 10)
        {
            unmount_disk();
            return 0;
        }
        else
        {
            cout << "Please enter valid choice." << endl;
            cin.clear();
        }
    }
}
int main()
{
    int ch;
    for (;;)
    {
        cout << "1:Create disk" << endl;
        cout << "2:Mount disk" << endl;
        cout << "0:Exit" << endl;
        cin >> ch;
        if (ch == 0)
        {
            cout << "BYE";
            break;
        }
        else if (ch == 1)
        {
            cout << "Enter disk name to create : " << endl;
            cin >> disk_name;
            if (FILE *file = fopen(disk_name, "r"))
            {
                fclose(file);
                cout<<"Disk already exists"<<endl;
            }
            else
            {
                create_disk(disk_name);
            }
        }
        else if (ch == 2)
        {
            cout << "Enter disk name to mount : " << endl;
            cin >> disk_name;
            if (mount_disk(disk_name))
            {
                user_menu();
            }
        }
        else
        {
            cout << "enter valid number" << endl;
        }
    }
    return 0;
}
