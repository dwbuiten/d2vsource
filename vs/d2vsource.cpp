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

void VS_CC d2vInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    d2vData *d = (d2vData *) *instanceData;
    vsapi->setVideoInfo(&d->vi, 1, node);
}

const VSFrameRef *VS_CC d2vGetFrame(int n, int activationReason, void **instanceData, void **frameData,
                                    VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi)
{
    d2vData *d = (d2vData *) *instanceData;
    VSFrameRef *s, *f;
    string msg;
    int ret;

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
    vs_bitblt(vsapi->getWritePtr(f, 0), vsapi->getStride(f, 0), vsapi->getWritePtr(s, 0), vsapi->getStride(s, 0),
              d->vi.width, d->vi.height);
    vs_bitblt(vsapi->getWritePtr(f, 1), vsapi->getStride(f, 1), vsapi->getWritePtr(s, 1), vsapi->getStride(s, 1),
              d->vi.width >> d->vi.format->subSamplingW, d->vi.height >> d->vi.format->subSamplingH);
    vs_bitblt(vsapi->getWritePtr(f, 2), vsapi->getStride(f, 2), vsapi->getWritePtr(s, 2), vsapi->getStride(s, 2),
              d->vi.width >> d->vi.format->subSamplingW, d->vi.height >> d->vi.format->subSamplingH);

    return f;
}

void VS_CC d2vFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    d2vData *d = (d2vData *) instanceData;
    d2vfreep(&d->d2v);
    decodefreep(&d->dec);
    av_freep(&d->frame);
    free(d);
}

void VS_CC d2vCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
    d2vData *data;
    string msg;
    bool no_crop;
    int err;

    /* Allocate our private data. */
    data = (d2vData *) malloc(sizeof(*data));
    if (!data) {
        vsapi->setError(out, "Cannot allocate private data.");
        return;
    }

    data->d2v = d2vparse((char *) vsapi->propGetData(in, "input", 0, 0), msg);
    if (!data->d2v) {
        vsapi->setError(out, msg.c_str());
        return;
    }

    data->dec = decodeinit(data->d2v, msg);
    if (!data->dec) {
        vsapi->setError(out, msg.c_str());
        return;
    }

    /*
     * Make our private data available to libavcodec, and
     * set our custom get/release_buffer funcs.
     */
    data->dec->avctx->opaque         = (void *) data;
    data->dec->avctx->get_buffer     = VSGetBuffer;
    data->dec->avctx->release_buffer = VSReleaseBuffer;

    /* Last frame is crashy right now. */
    data->vi.numFrames = data->d2v->frames.size();
    data->vi.width     = data->d2v->width;
    data->vi.height    = data->d2v->height;
    data->vi.fpsNum    = data->d2v->fps_num;
    data->vi.fpsDen    = data->d2v->fps_den;

    /* Stash the pointer to our core. */
    data->core = core;
    data->api  = (VSAPI *) vsapi;

    /*
     * Stash our aligned width and height for use with our
     * custom get_buffer, since it could require this.
     */
    data->aligned_width  = FFALIGN(data->vi.width, 16);
    data->aligned_height = FFALIGN(data->vi.height, 32);

    data->frame = avcodec_alloc_frame();
    if (!data->frame) {
        vsapi->setError(out, "Cannot allocate AVFrame.");
        return;
    }

    /*
     * Decode 1 frame to find out how the chroma is subampled.
     * The first time our custom get_buffer is called, it will
     * fill in data->vi.format.
     */
    data->format_set = false;
    err              = decodeframe(0, data->d2v, data->dec, data->frame, msg);
    if (err < 0) {
        msg.insert(0, "Failed to decode test frame: ");
        vsapi->setError(out, msg.c_str());
        return;
    }

    /* See if nocrop is enabled, and set the width/height accordingly. */
    no_crop = !!vsapi->propGetInt(in, "nocrop", 0, &err);
    if (err)
        no_crop = false;

    if (no_crop) {
        data->vi.width  = data->aligned_width;
        data->vi.height = data->aligned_height;
    }

    vsapi->createFilter(in, out, "d2vsource", d2vInit, d2vGetFrame, d2vFree, fmSerial, 0, data, core);
}
