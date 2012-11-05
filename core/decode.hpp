#ifndef DECODE_H
#define DECODE_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include <stdint.h>
}

typedef struct decodecontext {
    vector<FILE *> files;
    vector<int64_t> file_sizes;

    AVCodecContext *avctx;
    AVFormatContext *fctx;
    AVCodec *incodec;
    string *fakename;

    AVPacket inpkt;

    int last_frame;
    int last_gop;

    uint8_t *in;

    int orig_file;
    int cur_file;
    int orig_file_offset;
} decodecontext;

decodecontext *decodeinit(d2vcontext *dctx);
void decodefreep(decodecontext **ctx);
int decodeframe(int frame, d2vcontext *ctx, decodecontext *dctx, AVFrame *out);

#endif
