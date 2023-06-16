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

#ifndef D2VSOURCE_H
#define D2VSOURCE_H

#include <VapourSynth4.h>
#include <VSHelper4.h>
#include <memory>

#include "d2v.hpp"
#include "decode.hpp"

namespace vs4 {

typedef struct d2vData {
    std::unique_ptr<d2vcontext> d2v;
    std::unique_ptr<decodecontext> dec;
    AVFrame *frame;
    VSVideoInfo vi;
    VSCore *core;
    const VSAPI *api;

    int aligned_height;
    int aligned_width;

    int last_decoded;
    int linear_threshold;

    bool format_set;

    ~d2vData();
} d2vData;

void VS_CC d2vCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);

}

#endif
