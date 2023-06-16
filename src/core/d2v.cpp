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

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "compat.hpp"
#include "d2v.hpp"
#include <memory>
#include "gop.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

static std::string d2vgetpath(const char *d2v_path, const std::string& file)
{
    std::string path;
    std::string d2v       = d2v_path;
    size_t delim_pos = d2v.rfind(PATH_DELIM) + 1;

    if ((file.substr(0, 1) == "/" || file.substr(1, 1) == ":") || (d2v.substr(0, 1) != "/" && d2v.substr(1, 1) != ":")) {
        path = file;
    } else {
        path  = d2v.substr(0, delim_pos);
        path += file;
    }

    return path;
}

/* Parse the entire D2V index and build the GOP and frame lists. */
d2vcontext *d2vparse(const char *filename, std::string& err)
{
    std::string line;

    std::unique_ptr<d2vcontext> ret(new d2vcontext());

    ret->stream_type   = UNSET;
    ret->ts_pid        = -1;
    ret->loc.startfile = -1;

#ifdef _WIN32
    wchar_t wide_filename[_MAX_PATH];

    int fnlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, wide_filename, ARRAYSIZE(wide_filename));
    if (!fnlen) {
        err  = "D2V filename is invalid: ";
        err += filename;
        return NULL;
    }

    std::unique_ptr<FILE, decltype(&fclose)> input(_wfopen(wide_filename, L"rb"), &fclose);
#else
    std::unique_ptr<FILE, decltype(&fclose)> input(fopen(filename, "rb"), &fclose);
#endif

    if (!input) {
        err = "D2V cannot be opened.";
        return NULL;
    }

    /* Check the DGIndexProjectFile version. */
    d2vgetline(input.get(), line);
    if (line.substr(18, 2) != D2V_VERSION && line.substr(18, 2) != "42") {
        err = "D2V Version is unsupported!";
        return NULL;
    }

    /* Get the number of files. */
    d2vgetline(input.get(), line);
    ret->num_files = atoi(line.c_str());
    if (!ret->num_files) {
        err = "Invalid D2V File.";
        return NULL;
    }

    /* Allocate files array. */
    ret->files.resize(ret->num_files);

    /* Read them all in. */
    for(int i = 0; i < ret->num_files; i++) {
        d2vgetline(input.get(), line);
        if (line.length()) {
            ret->files[i] = d2vgetpath(filename, line);
        } else {
            err = "Invalid file set in D2V.";
            return NULL;
        }
    }

    /* Next line should be empty. */
    d2vgetline(input.get(), line);
    if (line.length()) {
        err = "Invalid D2V structure.";
        return NULL;
    }

    /* Iterate over the D2V header and fill the context members. */
    d2vgetline(input.get(), line);
    while(line.length()) {
        size_t mid = line.find("=");
        std::string l   = line.substr(0, mid);
        std::string r   = line.substr(mid + 1, line.length() - 1);

        if (l == "Stream_Type") {
            int type = atoi(r.c_str());

            ret->stream_type = streamtype_conv[type];
        } else if (l == "MPEG2_Transport_PID") {
            size_t pos = r.find(",");

            ret->ts_pid = strtoul(r.substr(0, pos).c_str(), NULL, 16);
        } else if (l == "MPEG_Type") {
            ret->mpeg_type = atoi(r.c_str());
        } else if (l == "iDCT_Algorithm") {
            int algo = atoi(r.c_str());

            ret->idct_algo = idct_algo_conv[algo];
        } else if (l == "YUVRGB_Scale") {
            int type = atoi(r.c_str());

            ret->yuvrgb_scale = scaletype_conv[type];
        } else if (l == "Picture_Size") {
            size_t pos = r.find("x");

            ret->width  = atoi(r.substr(      0, pos           ).c_str());
            ret->height = atoi(r.substr(pos + 1, r.length() - 1).c_str());
        } else if (l == "Frame_Rate") {
            size_t start = r.find("(") + 1;
            size_t sep   = r.find("/");
            size_t end   = r.find(")");

            ret->fps_num = atoi(r.substr(  start, sep).c_str());
            ret->fps_den = atoi(r.substr(sep + 1, end).c_str());
        } else if (l == "Location") {
            size_t pos1 = r.find(",");
            size_t pos2 = r.find(",", pos1 + 1);
            size_t pos3 = r.find(",", pos2 + 1);

            ret->loc.startfile   = strtoul(r.substr(       0, pos1          ).c_str(), NULL, 10);
            ret->loc.startoffset = strtoul(r.substr(pos1 + 1, pos2          ).c_str(), NULL, 16);
            ret->loc.endfile     = strtoul(r.substr(pos2 + 1, pos3          ).c_str(), NULL, 10);
            ret->loc.endoffset   = strtoul(r.substr(pos3 + 1, r.length() - 1).c_str(), NULL, 16);
        }

        d2vgetline(input.get(), line);
    }

    /* Some basic validation of input headers. */
    if (ret->fps_num <= 0 || ret->fps_den <= 0) {
        err = "Invalid framerate in D2V header.";
        return NULL;
    } else if (ret->mpeg_type != 1 && ret->mpeg_type != 2 && ret->mpeg_type != 264) {
        err = "Invalid MPEG type in D2V header.";
        return NULL;
    } else if (ret->width <= 0 || ret->height <= 0) {
        err = "Invalid dimensions in D2V header.";
        return NULL;
    } else if (ret->stream_type == TRANSPORT && ret->ts_pid < 0) {
        err = "Invalid PID in D2V header.";
        return NULL;
    } else if (ret->stream_type == UNSET) {
        err = "Invalid stream type in D2V header.";
        return NULL;
    } else if (ret->loc.startfile < 0 || ret->loc.startoffset < 0 ||
               ret->loc.endfile < ret->loc.startfile ||
               (ret->loc.endfile == ret->loc.startfile && ret->loc.endoffset < ret->loc.startoffset)) {
        err = "Invalid location in D2V header.";
        return NULL;
    }

    /* Read in all GOPs. */
    int i = 0;
    d2vgetline(input.get(), line);
    while(line.length()) {
        std::istringstream ss(line);
        gop cur_gop = {};

        ss >> std::hex >> cur_gop.info;
        ss >> std::dec >> cur_gop.matrix;
        ss >> std::dec >> cur_gop.file;
        ss >> std::dec >> cur_gop.pos;
        ss >> std::dec >> cur_gop.skip;
        ss >> std::dec >> cur_gop.vob;
        ss >> std::dec >> cur_gop.cell;

        int offset = 0;
        while(!ss.eof()) {
            uint16_t flags;
            frame f;

            ss >> std::hex >> flags;

            /*
             * We have to use a 16-bit int to force the stringstream to
             * read more than one character, so double check its size.
             */
            assert(flags <= 0xFF);

            cur_gop.flags.push_back((uint8_t) flags);

            f.gop    = i;
            f.offset = offset;
            ret->frames.push_back(f);

            offset++;
        }

        /* The last flag is always 'ff' to signify the end of the stream. */
        if (cur_gop.flags.back() == 0xFF) {
            cur_gop.flags.pop_back();
            ret->frames.pop_back();
        }

        ret->gops.push_back(cur_gop);
        i++;

        d2vgetline(input.get(), line);
    }

    if (!ret->frames.size() || !ret->gops.size()) {
        err = "No frames in D2V file!";
        return NULL;
    }

    return ret.release();
}
