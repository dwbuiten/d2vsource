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

#include <VapourSynth4.h>
#include <VSHelper4.h>

#include "applyrff4.hpp"
#include "d2vsource4.hpp"

using namespace vs4;

VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin *plugin, const VSPLUGINAPI *vspapi) {
    vspapi->configPlugin("com.sources.d2vsource", "d2v", "D2V Source", VS_MAKE_VERSION(1, 1), VAPOURSYNTH_API_VERSION, 0, plugin);

    vspapi->registerFunction("Source", "input:data;threads:int:opt;nocrop:int:opt;rff:int:opt;", "clip:vnode;", d2vCreate, 0, plugin);
    vspapi->registerFunction("ApplyRFF", "clip:clip;d2v:data;", "clip:vnode;", rffCreate, 0, plugin);
}
