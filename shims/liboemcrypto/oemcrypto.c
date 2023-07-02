#include <stdio.h>
#include <string.h>

FILE* kopen(char* filename, char* modes) {
    char* kfilename = filename;

    if (!strcmp(filename, "/efs/wv.keys") || !strcmp(filename, "/efs/cpk/wv.keys")) {
        kfilename = "/mnt/vendor/efs/wv.keys";
    }

    return fopen(kfilename, modes);
}
