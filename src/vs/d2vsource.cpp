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

#include <cstdint>
#include <cstdlib>

#include <VapourSynth.h>
#include <VSHelper.h>

#include "d2v.hpp"
#include "d2vsource.hpp"
#include "decode.hpp"
#include "directrender.hpp"

d2vData::~d2vData() {
    if (frame) {
        av_frame_unref(frame);
        av_freep(&frame);
    }
}

static void VS_CC d2vInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    d2vData *d = (d2vData *) *instanceData;
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC d2vGetFrame(int n, int activationReason, void **instanceData, void **frameData,
                                    VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi)
{
    d2vData *d = (d2vData *) *instanceData;
    VSFrameRef *f;
    std::string msg;

    /* Unreference the previously decoded frame. */
    av_frame_unref(d->frame);

    int ret = decodeframe(n, d->d2v.get(), d->dec.get(), d->frame, msg);
    if (ret < 0) {
        vsapi->setFilterError(msg.c_str(), frameCtx);
        return NULL;
    }

    /* Grab our direct-rendered frame. */
    const VSFrameRef *s = (const VSFrameRef *) d->frame->opaque;
    if (!s) {
        vsapi->setFilterError("Seek pattern broke d2vsource! Please send a sample.", frameCtx);
        return NULL;
    }

    /* If our width and height are the same, just return it. */
    if (d->vi.width == d->aligned_width && d->vi.height == d->aligned_height) {
        f = vsapi->copyFrame(s, core);
    } else {
        f = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, NULL, core);

        /* Copy into VS's buffers. */
        for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
            uint8_t *dstp = vsapi->getWritePtr(f, plane);
            const uint8_t *srcp = vsapi->getReadPtr(s, plane);
            int dst_stride = vsapi->getStride(f, plane);
            int src_stride = vsapi->getStride(s, plane);
            int width = vsapi->getFrameWidth(f, plane);
            int height = vsapi->getFrameHeight(f, plane);

            vs_bitblt(dstp, dst_stride, srcp, src_stride, width * d->vi.format->bytesPerSample, height);
        }
    }

    VSMap *props = vsapi->getFramePropsRW(f);

    /*
     * The DGIndex manual simply says:
     *     "The matrix field displays the currently applicable matrix_coefficients value (colorimetry)."
     *
     * I can only assume this lines up with the tables VS uses correctly.
     */
    vsapi->propSetInt(props, "_Matrix", d->d2v->gops[d->d2v->frames[n].gop].matrix, paReplace);
    vsapi->propSetInt(props, "_DurationNum", d->d2v->fps_den, paReplace);
    vsapi->propSetInt(props, "_DurationDen", d->d2v->fps_num, paReplace);
    vsapi->propSetFloat(props, "_AbsoluteTime",
                        (static_cast<double>(d->d2v->fps_den) * n) / static_cast<double>(d->d2v->fps_num), paReplace);

    switch (d->frame->pict_type) {
    case AV_PICTURE_TYPE_I:
        vsapi->propSetData(props, "_PictType", "I", 1, paReplace);
        break;
    case AV_PICTURE_TYPE_P:
        vsapi->propSetData(props, "_PictType", "P", 1, paReplace);
        break;
    case AV_PICTURE_TYPE_B:
        vsapi->propSetData(props, "_PictType", "B", 1, paReplace);
        break;
    default:
        break;
    }

    int fieldbased;
    if (d->d2v->gops[d->d2v->frames[n].gop].flags[d->d2v->frames[n].offset] & FRAME_FLAG_PROGRESSIVE)
        fieldbased = 0;
    else
        fieldbased = 1 + !!(d->d2v->gops[d->d2v->frames[n].gop].flags[d->d2v->frames[n].offset] & FRAME_FLAG_TFF);
    vsapi->propSetInt(props, "_FieldBased", fieldbased, paReplace);

    vsapi->propSetInt(props, "_ChromaLocation", d->d2v->mpeg_type == 1 ? 1 : 0, paReplace);

    return f;
}

static void VS_CC d2vFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    d2vData *d = (d2vData *) instanceData;
    delete d;
}

void VS_CC d2vCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
    std::string msg;
    int err;

    /* Need to get thread info before anything to pass to decodeinit(). */
    int threads = int64ToIntS(vsapi->propGetInt(in, "threads", 0, &err));
    if (err)
        threads = 0;

    if (threads < 0) {
        vsapi->setError(out, "Invalid number of threads.");
        return;
    }

    /* Allocate our private data. */
    std::unique_ptr<d2vData> data(new d2vData());

    data->d2v.reset(d2vparse(vsapi->propGetData(in, "input", 0, 0), msg));
    if (!data->d2v) {
        vsapi->setError(out, msg.c_str());
        return;
    }

    data->dec.reset(decodeinit(data->d2v.get(), threads, msg));
    if (!data->dec) {
        vsapi->setError(out, msg.c_str());
        return;
    }

    /*
     * Make our private data available to libavcodec, and
     * set our custom get/release_buffer funcs.
     */
    data->dec->avctx->opaque         = (void *) data.get();
    data->dec->avctx->get_buffer2    = VSGetBuffer;

    data->vi.numFrames = (int) data->d2v->frames.size();
    data->vi.width     = data->d2v->width;
    data->vi.height    = data->d2v->height;
    data->vi.fpsNum    = data->d2v->fps_num;
    data->vi.fpsDen    = data->d2v->fps_den;

    /* Stash the pointer to our core. */
    data->core = core;
    data->api  = vsapi;

    /*
     * Stash our aligned width and height for use with our
     * custom get_buffer, since it could require this.
     */
    data->aligned_width  = FFALIGN(data->vi.width, 16);
    data->aligned_height = FFALIGN(data->vi.height, 32);

    data->frame = av_frame_alloc();
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
    err              = decodeframe(0, data->d2v.get(), data->dec.get(), data->frame, msg);
    if (err < 0) {
        msg.insert(0, "Failed to decode test frame: ");
        vsapi->setError(out, msg.c_str());
        return;
    }

    if (!data->format_set) {
        vsapi->setError(out, "Source: video has unsupported pixel format.");
        return;
    }

    /* See if nocrop is enabled, and set the width/height accordingly. */
    bool no_crop = !!vsapi->propGetInt(in, "nocrop", 0, &err);

    if (no_crop) {
        data->vi.width  = data->aligned_width;
        data->vi.height = data->aligned_height;
    }

    vsapi->createFilter(in, out, "d2vsource", d2vInit, d2vGetFrame, d2vFree, fmUnordered, nfMakeLinear, data.release(), core);

    bool rff = !!vsapi->propGetInt(in, "rff", 0, &err);
    if (err)
        rff = true;

    if (rff) {
        VSPlugin *d2vPlugin = vsapi->getPluginById("com.sources.d2vsource", core);
        VSPlugin *corePlugin = vsapi->getPluginById("com.vapoursynth.std", core);
        VSNodeRef *before = vsapi->propGetNode(out, "clip", 0, NULL);
        VSMap *args = vsapi->createMap();

        vsapi->propSetNode(args, "clip", before, paReplace);
        vsapi->freeNode(before);

        VSMap *ret    = vsapi->invoke(corePlugin, "Cache", args);
        VSNodeRef *middle = vsapi->propGetNode(ret, "clip", 0, NULL);
        vsapi->freeMap(ret);

        vsapi->propSetNode(args, "clip", middle, paReplace);
        vsapi->propSetData(args, "d2v", vsapi->propGetData(in, "input", 0, NULL),
                           vsapi->propGetDataSize(in, "input", 0, NULL), paReplace);
        vsapi->freeNode(middle);

        ret = vsapi->invoke(d2vPlugin, "ApplyRFF", args);
        vsapi->freeMap(args);

        const char *error = vsapi->getError(ret);
        if (error) {
            vsapi->setError(out, error);
            vsapi->freeMap(ret);
            return;
        }

        VSNodeRef *after = vsapi->propGetNode(ret, "clip", 0, NULL);

        vsapi->propSetNode(out, "clip", after, paReplace);
        vsapi->freeNode(after);
        vsapi->freeMap(ret);
    }
}
