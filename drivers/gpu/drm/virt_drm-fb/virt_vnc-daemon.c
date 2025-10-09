/*
 * Simple daemon to read virt DRM framebuffer and expose via raw VNC
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define WIDTH 1920
#define HEIGHT 1080
#define BPP 4 // XRGB8888
#define FB_DEV "/dev/fb1"

int main() {
    int fd = open(FB_DEV, O_RDONLY);
    if (fd < 0) {
        perror("open fb");
        return 1;
    }

    size_t fb_size = WIDTH * HEIGHT * BPP;
    void *fb = mmap(NULL, fb_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        perror("mmap fb");
        return 1;
    }

    // Simple raw test: dump first line pixel values
    uint32_t *p = fb;
    for (int i = 0; i < WIDTH; i++) {
        printf("%08x ", p[i]);
        if (i % 16 == 15) printf("\n");
    }

    printf("Mapped virt DRM FB at %p\n", fb);

    // TODO: Connect to x11vnc or implement RFB directly
    pause();

    munmap(fb, fb_size);
    close(fd);
    return 0;
}
