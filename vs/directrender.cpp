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

#include <libavcodec/avcodec.h>
}

#include "d2vsource.hpp"

#include <VapourSynth.h>
#include <VSHelper.h>

int VSGetBuffer(AVCodecContext *avctx, AVFrame *pic)
{
    VSFrameRef *vs_frame;
    d2vData *data = (d2vData *) avctx->opaque;
    int i;

    vs_frame = data->api->newVideoFrame(data->vi.format, data->aligned_width, data->aligned_height, NULL, data->core);

    pic->opaque              = (void *) vs_frame;
    pic->type                = FF_BUFFER_TYPE_USER;
    pic->extended_data       = pic->data;
    pic->pkt_pts             = avctx->pkt ? avctx->pkt->pts : AV_NOPTS_VALUE;
    pic->width               = data->aligned_width;
    pic->height              = data->aligned_height;
    pic->format              = PIX_FMT_YUV420P;
    pic->sample_aspect_ratio = avctx->sample_aspect_ratio;

    for(i = 0; i < data->vi.format->numPlanes; i++) {
        pic->base[i]     = data->api->getWritePtr(vs_frame, i);
        pic->data[i]     = pic->base[i];
        pic->linesize[i] = data->api->getStride(vs_frame, i);
    }

    return 0;
}

void VSReleaseBuffer(AVCodecContext *avctx, AVFrame *pic)
{
    VSFrameRef *vs_frame = (VSFrameRef *) pic->opaque;
    d2vData *data = (d2vData *) avctx->opaque;
    int i;

    for(i = 0; i < data->vi.format->numPlanes; i++) {
        pic->base[i]     = NULL;
        pic->data[i]     = NULL;
        pic->linesize[i] = 0;
    }

    data->api->freeFrame(vs_frame);
}
