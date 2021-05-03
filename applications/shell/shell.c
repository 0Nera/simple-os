#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syscall.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fs.h>
#include <kernel/keyboard.h>

#define MAX_COMMAND_LEN 255
#define MAX_PATH_LEN 4096

static inline _syscall4(SYS_READDIR, int, sys_readdir, const char *, path, uint, entry_offset, fs_dirent*, buf, uint, buf_size)

int main(int argc, char* argv[]) {
    printf("Welcome to the Shell!\n");
    printf("Shell ARGC(%d)\n", argc);
    for(int i=0; i<argc; i++) {
        printf("  %d: %s\n", i, argv[i]);
    }
    printf("Use 'help' command to show usage\n");
    
    char c;
    char command[MAX_COMMAND_LEN + 1] = {0};
    int n_command_read;
    char prev_command[MAX_COMMAND_LEN + 1] = {0};
    int prev_n_command_read;

    char* cwd = malloc(MAX_PATH_LEN+1);
    char* path_buff = malloc(MAX_PATH_LEN+1);

    while(1) {
        char* r_cwd = getcwd(cwd, MAX_PATH_LEN+1);
        if(r_cwd == NULL) {
            write(STDOUT_FILENO, "...", 3);
        } else {
            write(STDOUT_FILENO, cwd, strlen(cwd));
        }
        write(STDOUT_FILENO, "$ ", 2);

        memset(command, 0, MAX_COMMAND_LEN + 1);
        n_command_read = 0;
        while(1) {
            while(read(STDIN_FILENO, &c, 1) <= 0);
            if(c == '\n') {
                write(STDOUT_FILENO, &c, 1);
                break;
            } else if(c == '\b') {
                if(n_command_read > 0) {
                    write(STDOUT_FILENO, &c, 1);
                    n_command_read--;
                }
            } else if(c == KEY_UP) {
                // clear command entered
                for(int i=0; i<n_command_read;i++) {
                    write(STDOUT_FILENO, &"\b", 1);
                }
                memmove(command, prev_command, MAX_COMMAND_LEN+1);
                n_command_read = prev_n_command_read;
                write(STDOUT_FILENO, command, n_command_read);
            } else if(n_command_read < MAX_COMMAND_LEN) {
                write(STDOUT_FILENO, &c, 1);
                command[n_command_read++] = c;
            }
        }
        memmove(prev_command, command, MAX_COMMAND_LEN + 1);
        prev_n_command_read = n_command_read;

        char* part = strtok(command," ");
        if(part == NULL) {
            continue;
        }
        if(strcmp(part, "help") == 0) {
            printf("Supported commands\n");
            printf("ls: listing dir\n");
        } else if(strcmp(part, "ls") == 0) {
            fs_dirent entry = {0};
            char* ls_path = strtok(NULL," ");
            if(ls_path == NULL) {
                ls_path = "";
            }
            int i = 0;
            while(1) {
                int r = sys_readdir(ls_path, i++, &entry, sizeof(fs_dirent));
                if(r < 0) {
                    printf("ls: error %d\n", r);
                }
                if(r <= 0) {
                    break;
                }
                size_t path_len = strlen(ls_path);
                size_t name_len = strlen(entry.name);
                if(path_len + 1 + name_len > MAX_PATH_LEN) {
                    printf("  File: %s\n", entry.name);
                } else {
                    memmove(path_buff, ls_path, path_len);
                    if(path_len > 0 && ls_path[path_len-1] != '/') {
                        path_buff[path_len++] = '/';
                    }
                    memmove(path_buff + path_len, entry.name, name_len);
                    path_buff[path_len + name_len] = 0;
                    struct stat st = {0};
                    int r_stat = stat(path_buff, &st);
                    if(r_stat < 0) {
                        printf("ls: stat error %d: %s", r_stat, entry.name);
                    } else {
                        char* type;
                        if(S_ISDIR(st.st_mode)) {
                            type = "DIR";
                        } else {
                            type = "FILE";
                        }
                        char* datetime = ctime(&st.st_mtim.tv_sec);
                        printf("  %s: %s %ld %s", entry.name, type, st.st_size, datetime);
                    }
                }
            }
        } else if(strcmp(part, "cd") == 0) {
            char* cd_path = strtok(NULL," ");
            if(cd_path == NULL) {
                continue;
            }
            int r = chdir(cd_path);
            if(r < 0) {
                printf("cd: error %d\n", r);
            }
        } else {
            printf("Unknow command:\n%s\n", command);
        }

    }


    return 0;
}