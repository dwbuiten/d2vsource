#ifndef GOP_H
#define GOP_H

#include <vector>

extern "C" {
#include <stdint.h>
}

using namespace std;

typedef struct frame {
    int gop;
    int offset;
} frame;

typedef struct gop {
    uint16_t info;
    int matrix;
    int file;
    uint64_t pos;
    int skip;
    int vob;
    int cell;
    vector<uint8_t> flags;
} gop;

#endif
