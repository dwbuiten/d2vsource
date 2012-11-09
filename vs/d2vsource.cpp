/*
 * VapourSynth D2V Plugin
 *
 * Copyright (c) 2012 Derek Buitenhuis
 *
 * This file is part of d2vsource.
 *
 * d2vsource is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * d2vsource is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with d2vsource; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

extern "C" {
#include <stdint.h>
#include <stdlib.h>
}

#include "VapourSynth.h"
#include "VSHelper.h"

#include "d2v.hpp"
#include "decode.hpp"

typedef struct {
    d2vcontext *d2v;
    decodecontext *dec;
    AVFrame *frame;
    VSVideoInfo vi;
} d2vData;

static void VS_CC d2vInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    d2vData *d = (d2vData *) * instanceData;
    vsapi->setVideoInfo(&d->vi, node);
}

static const VSFrameRef *VS_CC d2vGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    d2vData *d = (d2vData *) * instanceData;
    VSFrameRef *f;
    string msg;
    int stride;
    int p, i, ret, w, h;
    uint8_t *sptr, *dptr;

    f = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, NULL, core);

    ret = decodeframe(n, d->d2v, d->dec, d->frame, msg);
    if (ret < 0) {
        vsapi->setFilterError(msg.c_str(), frameCtx);
        return NULL;
    }

    /*
     * Copy into VS's buffers.
     * Should be switched to direct rendering at some point via
     * a custom get_buffer().
     */
    for(p = 0; p < d->vi.format->numPlanes; p++){
        stride = vsapi->getStride(f, p);
        dptr   = vsapi->getWritePtr(f, p);
        sptr   = d->frame->data[p];
        w      = p ? d->vi.width / 2 : d->vi.width;
        h      = p ? d->vi.height / 2 : d->vi.height;
        for(i = 0; i < h; i++) {
            memcpy(dptr, sptr, w);
            dptr += stride;
            sptr += d->frame->linesize[p];
        }
    }

    return f;
}

static void VS_CC d2vFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    d2vData *d = (d2vData *) instanceData;
    d2vfreep(&d->d2v);
    decodefreep(&d->dec);
    av_freep(&d->frame);
    free(d);
}

static void VS_CC d2vCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    d2vData d;
    d2vData *data;
    VSNodeRef *cref;
    string msg;
    int err;

    d.d2v = d2vparse((char *)vsapi->propGetData(in, "input", 0, 0), msg);
    if (!d.d2v) {
        vsapi->setError(out, msg.c_str());
        return;
    }

    /* Last frame is crashy right now */
    d.vi.numFrames = d.d2v->frames.size() - 1;
    d.vi.width     = d.d2v->width;
    d.vi.height    = d.d2v->height;
    d.vi.format    = vsapi->getFormatPreset(pfYUV420P8, core);
    d.vi.fpsNum    = d.d2v->fps_num;
    d.vi.fpsDen    = d.d2v->fps_den;

    d.dec = decodeinit(d.d2v, msg);
    if (!d.dec) {
        vsapi->setError(out, msg.c_str());
        return;
    }

    d.frame = avcodec_alloc_frame();
    if (!d.frame) {
        vsapi->setError(out, "Cannot alloc frame.");
        return;
    }

    data = (d2vData *) malloc(sizeof(d));
    *data = d;

    cref = vsapi->createFilter(in, out, "d2vsource", d2vInit, d2vGetFrame, d2vFree, fmSerial, 0, data, core);
    vsapi->propSetNode(out, "clip", cref, 0);
    vsapi->freeNode(cref);
    return;
}

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.sources.d2vsource", "d2v", "D2V Source", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Source", "input:data;", d2vCreate, 0, plugin);
}
