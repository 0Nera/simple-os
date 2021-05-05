#include <stddef.h>
#include <string.h>
#include <kernel/errno.h>
#include <kernel/console.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/stat.h>

static int console_read(struct fs_mount_point* mount_point, const char * path, char *buf, uint size, uint offset, struct fs_file_info *fi)
{
    UNUSED_ARG(mount_point);
    UNUSED_ARG(path);
    UNUSED_ARG(offset);
    UNUSED_ARG(fi);

    uint char_read = 0;
    while(char_read < size) {
        char c = read_key_buffer();
        if(c == 0) {
            return char_read;
        }
        *buf = c;
        buf++;
        char_read++; 
    }
    return char_read;
}

static int str2int(char* arg, int default_val)
{
    if(arg == NULL || arg[0] == 0) {
        return default_val;
    }
    size_t len = strlen(arg);
    int r = 0;
    for(size_t i=0; i<len; i++) {
        if(arg[i] == '-') {
            r  = -r;
        } else {
            r = r*10 + arg[i] - '0';
        }
    }
    return r;
}

// Process a (VT-100) terminal escaped control sequence
// return how many characters had been consumed after '\x1b'
static int process_escaped_sequence(const char* buf, size_t size)
{
    if(size < 3) {
        return 0;
    }
    if(buf[0] != '\x1b' || buf[1] != '[') {
        // we only support escaped sequence starts with "\x1b["
        return 0;
    }
    char arg1[16] = {0};
    char arg2[16] = {0};
    char* args[2] = {arg1, arg2};
    char command = 0;
    uint start = 2;
    uint argc = 0;
    for(uint i=start; i<size; i++) {
        if(buf[i] == ';' || (buf[i] >= 'a' && buf[i] <= 'z') || (buf[i] >= 'A' && buf[i] <= 'Z')) {
            assert(i - start < 16);
            if(start < i && argc < 2) {
                memmove(args[argc++], &buf[start], i - start);
                start = i+1;
            }
            if(buf[i] == ';') {
                continue;
            }
            command = buf[i];
            if(command == 'J') {
                // clear screen
                int pos = str2int(arg1, 2);
                if(pos == 2) {
                    terminal_clear_screen();
                }
            } else if(command == 'H') {
                // set cursor
                int row = str2int(arg1, 1);
                int col = str2int(arg2, 1);
                set_cursor(row - 1, col - 1);
            } else if(command == 'C') {
                int col_delta = str2int(arg1, 1);
                if(col_delta < 1) {
                    col_delta = 1;
                }
                move_cursor(0, col_delta);
            } else if(command == 'B') {
                int row_delta = str2int(arg1, 1);
                if(row_delta < 1) {
                    row_delta = 1;
                }
                move_cursor(row_delta, 0);
            } else {
                // unsupported command
            }
            return i;
        }
    }
    return 0;
}

static int console_write(struct fs_mount_point* mount_point, const char * path, const char *buf, uint size, uint offset, struct fs_file_info *fi)
{
    UNUSED_ARG(mount_point);
    UNUSED_ARG(path);
    UNUSED_ARG(offset);
    UNUSED_ARG(fi);

    for(uint i=0; i<size; i++) {
        char c = buf[i];
        if(c == '\x1b') {
            int skip = process_escaped_sequence(&buf[i], size - i);
            i += skip;
        } else {
            terminal_putchar(c);
        }
    }
    return size;
}

static int console_getattr(struct fs_mount_point* mount_point, const char * path, struct fs_stat * st, struct fs_file_info *fi)
{
    UNUSED_ARG(mount_point);
    UNUSED_ARG(path);
    UNUSED_ARG(fi);
    
    st->mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFCHR;
    return 0;
}

static int console_mount(struct fs_mount_point* mount_point, void* option)
{
    UNUSED_ARG(option);
    mount_point->operations = (struct file_system_operations) {
        .read = console_read,
        .write = console_write,
        .getattr = console_getattr
    };

    return 0;
}

static int console_unmount(struct fs_mount_point* mount_point)
{
    UNUSED_ARG(mount_point);
    return 0;
}


int console_init(struct file_system* fs)
{
    fs->mount = console_mount;
    fs->unmount = console_unmount;
    fs->fs_global_meta = NULL;
    fs->status = FS_STATUS_READY;
    return 0;
}