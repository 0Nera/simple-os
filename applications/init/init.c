#include <stddef.h>
#include <syscall.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <common.h>
#include <fcntl.h>
#include <unistd.h>
#include <fs.h>
#include <sys/wait.h>

static inline _syscall0(SYS_YIELD, int, sys_yield)
static inline _syscall1(SYS_DUP, int, sys_dup, int, fd)
static inline _syscall2(SYS_TRUNCATE_FD, int, sys_truncate_fd, int, fd, uint, size)
static inline _syscall2(SYS_TRUNCATE_PATH, int, sys_truncate_path, const char*, path, uint, size)

static void test_multi_process()
{
    printf("Test yielding\r\n");
    sys_yield();
    printf("Welcome Back User World!\r\n");

    int fork_ret = fork();
    int child_exit_status;

    if(fork_ret) {
        // parent
        printf("This is parent, child PID: %u\r\n", fork_ret);
        // sys_yield();
        int wait_ret = wait(&child_exit_status);
        if(wait_ret < 0) {
            printf("No child exited\r\n");
        } else {
            printf("Child %u exited, exit code = %d\r\n", wait_ret, WEXITSTATUS(child_exit_status));
        }
    } else {
        // child
        printf("This is child, exiting with code 123\r\n");
        exit(123);
    }
}

static void test_libc() {
    printf("Welcome to %s!\r\n", "Newlib");
    printf("Current Epoch: %lld\r\n", time(NULL));

    const char* str2 = "Test malloc/free!\r\n";
    char* buf = malloc(100);
    memmove(buf, str2, strlen(str2)+1);
    printf("%s", buf);
    free(buf);
}

static void test_file_system() {
    struct stat st = {0};
    char buf1[100] = {0};
    int fd;
    
    fd = open("/home/RAND.OM", O_RDWR);
    if(fd >= 0) {
        int read_in = read(fd, buf1, 10);
        fstat(fd, &st);
        int closed = close(fd);
        printf("FD(%d), READ(%d), CLOSE(%d), MODTIME(%lld)\r\n", fd, read_in, closed, st.st_mtim.tv_sec);
        printf("READ content: \r\n %s \r\n", buf1);
    } else {
        printf("OPEN error\r\n");
    }

    const char* to_write = "Hello User I/O World!";
    fd = open("/home/RAND.OM", O_RDWR);
    if(fd >= 0) {
        int written = write(fd, to_write, strlen(to_write) + 1);
        int lseek_res = lseek(fd, -(strlen(to_write) + 1), SEEK_CUR);
        memset(buf1, 0, 100);
        int read_in = read(fd, buf1, strlen(to_write) + 1);
        fstat(fd, &st);
        int closed = close(fd);
        printf("FD(%d), WRITE(%d), SEEK(%d), READ(%d), CLOSE(%d), MODTIME(%lld)\r\n", fd, written, lseek_res, read_in, closed, st.st_mtim.tv_sec);
        printf("READ content: \r\n %s \r\n", buf1);
    } else {
        printf("OPEN error\r\n");
    }

    // Test FILE struct based file I/O
    FILE *fp = fopen("/home/RAND.OM", "r");
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    linelen = __getline(&line, &linecap, fp);
    printf("fopen+getline content(%ld/%ld): \r\n %s \r\n", linelen, linecap, line);
    free(line);

    // Test file creation, truncating and deletion
    const char* to_write1 = "New content!";
    fd = open("/home/newfile", O_CREAT|O_RDWR);
    // sys_test(1);
    if(fd>=0) {
        int written = write(fd, to_write1, strlen(to_write1) + 1);
        printf("newfile: WRITTEN(%d)\r\n", written);
        fstat(fd, &st);
        printf("newfile: SIZE(%ld)\r\n", st.st_size);
        close(fd);
        truncate("/home/newfile", 3);
    }
    fd = open("/home/newfile", O_RDWR);
    if(fd>=0) {
        fstat(fd, &st);
        printf("newfile: SIZE(%ld)\r\n", st.st_size);
        ftruncate(fd, 5);
        fstat(fd, &st);
        printf("newfile: SIZE(%ld)\r\n", st.st_size);
        memset(buf1, 0, 100);
        int read_in = read(fd, buf1, strlen(to_write1) + 1);
        printf("newfile: READ(%d), CONTENT(%s)\r\n", read_in, buf1);
        close(fd);
    }
    int res_link = link("/home/newfil", "/home/newfil.1");
    printf("Link(%d)\r\n", res_link);
    int res_rename = rename("/home/newfile", "/home/newfile.2");
    printf("Rename(%d)\r\n", res_rename);
    int res_unlink = unlink("/home/newfile.2");
    printf("Unlink(%d)\r\n", res_unlink);
    res_unlink = unlink("/home/newfile");
    printf("Unlink(%d)\r\n", res_unlink);
}


int main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    // Matching STDIN_FILENO/STDOUT_FILENO/STDERR_FILENO in Newlib unistd.h
    // file descriptor 0: stdin, console, STDIN_FILENO
    open("/console", O_RDWR);
    // file descriptor 1: stdout, console, STDOUT_FILENO
    sys_dup(0);
    // file descriptor 2: stderr, console, STDERR_FILENO
    sys_dup(0);

    printf("Hello User World!\r\n");

    // Perform tests of user space features
    test_multi_process();
    test_libc();
    test_file_system();

    // Execute the shell
    char* shell_argv[] = {"/usr/bin/shell.elf", NULL};
    printf("EXEC Shell\r\n");
    execve("/usr/bin/shell.elf", shell_argv, NULL);
}