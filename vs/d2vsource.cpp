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

#include <VapourSynth.h>
#include <VSHelper.h>

#include "d2v.hpp"
#include "d2vsource.hpp"
#include "decode.hpp"
#include "directrender.hpp"

static void VS_CC d2vInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    d2vData *d = (d2vData *) * instanceData;
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC d2vGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    d2vData *d = (d2vData *) * instanceData;
    VSFrameRef *s, *f;
    string msg;
    int sstride, dstride;
    int p, i, ret, w, h;
    uint8_t *sptr, *dptr;

    ret = decodeframe(n, d->d2v, d->dec, d->frame, msg);
    if (ret < 0) {
        vsapi->setFilterError(msg.c_str(), frameCtx);
        return NULL;
    }

    /* Grab our direct-rendered frame. */
    s = (VSFrameRef *) d->frame->opaque;

    /* If our width and height are the same, just return it. */
    if (d->vi.width == d->aligned_width && d->vi.height == d->aligned_height) {
        f = (VSFrameRef *) vsapi->cloneFrameRef(s);
        return f;
    }

    f = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, NULL, core);

    /* Copy into VS's buffers. */
    for(p = 0; p < d->vi.format->numPlanes; p++) {
        sstride = vsapi->getStride(f, p);
        dstride = vsapi->getStride(s, p);
        dptr    = vsapi->getWritePtr(f, p);
        sptr    = vsapi->getWritePtr(s, p);
        w       = p ? d->vi.width >> d->vi.format->subSamplingW : d->vi.width;
        h       = p ? d->vi.height >> d->vi.format->subSamplingH : d->vi.height;
        for(i = 0; i < h; i++) {
            memcpy(dptr, sptr, w);
            dptr += sstride;
            sptr += dstride;
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
    string msg;
    bool no_crop;
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

    /* Stash the pointer to our core */
    d.core = core;
    d.api  = (VSAPI *) vsapi;

    data = (d2vData *) malloc(sizeof(d));
    *data = d;

    data->dec = decodeinit(data->d2v, msg);
    if (!data->dec) {
        vsapi->setError(out, msg.c_str());
        return;
    }

    /*
     * Stash our aligned width and height for use with our
     * custom get_buffer, since it could require this.
     */
    data->aligned_width  = FFALIGN(data->vi.width, 16);
    data->aligned_height = FFALIGN(data->vi.height, 32);

    /* See if nocrop is enabled, and set the width/height accordingly. */
    no_crop = !!vsapi->propGetInt(in, "nocrop", 0, &err);
    if (err)
        no_crop = false;

    if (no_crop) {
        data->vi.width  = data->aligned_width;
        data->vi.height = data->aligned_height;
    }

    /*
     * Make our private data available to libavcodec, and
     * set our custom get/release_buffer funcs.
     */
    data->dec->avctx->opaque         = (void *) data;
    data->dec->avctx->get_buffer     = VSGetBuffer;
    data->dec->avctx->release_buffer = VSReleaseBuffer;

    data->frame = avcodec_alloc_frame();
    if (!data->frame) {
        vsapi->setError(out, "Cannot alloc frame.");
        return;
    }

    vsapi->createFilter(in, out, "d2vsource", d2vInit, d2vGetFrame, d2vFree, fmSerial, 0, data, core);
    return;
}

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.sources.d2vsource", "d2v", "D2V Source", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Source", "input:data;nocrop:int:opt;", d2vCreate, 0, plugin);
}
