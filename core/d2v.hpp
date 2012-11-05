#ifndef D2V_H
#define D2V_H

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "gop.hpp"

#define D2V_VERSION "16"

#define GOP_FLAG_CLOSED 0x400

using namespace std;

enum streamtype {
    ELEMENTARY = 0,
    PROGRAM    = 1,
    TRANSPORT  = 2,
    PVA        = 3
};

enum scaletype {
    TV = 0,
    PC = 1
};

static const enum scaletype scaletype_conv[2] = {
    TV,
    PC
};

static const enum streamtype streamtype_conv[4] = {
    ELEMENTARY,
    PROGRAM,
    TRANSPORT,
    PVA
};

static const int idct_algo_conv[8] = {
    FF_IDCT_AUTO,
    FF_IDCT_LIBMPEG2MMX,
    FF_IDCT_LIBMPEG2MMX,
    FF_IDCT_LIBMPEG2MMX,
    FF_IDCT_AUTO,
    FF_IDCT_AUTO,
    FF_IDCT_XVIDMMX,
    FF_IDCT_SIMPLEMMX
};

typedef struct location {
    int startfile;
    int startoffset;
    int endfile;
    int endoffset;
} location;

typedef struct d2vcontext {
    int num_files;
    string *files;

    enum streamtype stream_type;
    int ts_pid;
    int mpeg_type;
    int idct_algo;
    enum scaletype yuvrgb_scale;
    int width;
    int height;
    int fps_num;
    int fps_den;
    location loc;

    vector<frame> frames;
    vector<gop> gops;
} d2vcontext;

void d2vfreep(d2vcontext **ctx);
d2vcontext *d2vparse(char *filename);
string d2vgetpath(char *d2v_path, string file);

#endif
