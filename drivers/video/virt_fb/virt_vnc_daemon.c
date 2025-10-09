#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define FB_PATH "/dev/fb1"

int main() {
    int fd = open(FB_PATH, O_RDWR);
    if (fd < 0) { perror("open fb1"); return 1; }

    size_t fb_size = 1920*1080*4; // 32bpp
    void *fb = mmap(NULL, fb_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { perror("mmap"); return 1; }

    // simple test: fill red
    memset(fb, 0, fb_size);
    uint32_t *p = fb;
    for (int i=0;i<1920*1080;i++)
        p[i] = 0x00FF0000; // red

    printf("Filled fb1 with red, ready for VNC\n");
    getchar();

    munmap(fb, fb_size);
    close(fd);
    return 0;
}
