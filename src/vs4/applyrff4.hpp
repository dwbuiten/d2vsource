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

#ifndef APPLYRFF_H
#define APPLYRFF_H

#include <VapourSynth4.h>
#include <VSHelper4.h>
#include <memory>

#include "d2v.hpp"

namespace vs4 {

typedef enum rffFieldType {
    Top,
    Bottom,
    Progressive
} rffFieldType;

typedef struct rffField {
    int frame; // Source frame for this field.
    rffFieldType type;
} rffField;

typedef struct rffData {
    std::unique_ptr<d2vcontext> d2v;
    std::vector<rffField> fields; // Output fields, in the order they are to be displayed.

    VSVideoInfo vi;
    VSNode *node;
} rffData;

void VS_CC rffCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);

}

#endif
