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
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <cstdio>

#include "compat.hpp"
#include <memory>
#include "d2v.hpp"
#include "decode.hpp"
#include "gop.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

/*
 * AVIO seek function to handle GOP offsets and multi-file support
 * in libavformat without it knowing about it.
 */
static int64_t file_seek(void *opaque, int64_t offset, int whence)
{
    decodecontext *ctx = (decodecontext *) opaque;

    switch(whence) {
    case SEEK_SET: {
        /*
         * This mutli-file seek is likely very broken, but I don't
         * really care much, since it is only used in avformat_find_stream_info,
         * which does its job fine as-is.
         */
        int64_t real_offset = offset + ctx->orig_file_offset;

        for(unsigned int i = ctx->orig_file; i < ctx->cur_file; i++)
            real_offset -= ctx->file_sizes[i];

        while(real_offset > ctx->file_sizes[ctx->cur_file] && ctx->cur_file != ctx->files.size() - 1) {
            real_offset -= ctx->file_sizes[ctx->cur_file];
            ctx->cur_file++;
        }

        while(real_offset < 0 && ctx->cur_file) {
            ctx->cur_file--;
            real_offset += ctx->file_sizes[ctx->cur_file];
        }

        fseeko(ctx->files[ctx->cur_file], real_offset, SEEK_SET);

        return offset;
    }
    case AVSEEK_SIZE: {
        /*
         * Return the total filesize of all files combined,
         * adjusted for GOP offset.
         */
        int64_t size = -((int64_t) ctx->orig_file_offset);
        unsigned int i;

        for(i = ctx->orig_file; i < ctx->file_sizes.size(); i++)
            size += ctx->file_sizes[i];

        return size;
    }
    default:
        /* Shouldn't need to support anything else for our use case. */
        std::cout << "Unsupported seek!" << std::endl;
        return -1;
    }
}

/*
 * AVIO packet reading function to handle GOP offsets and multi-file support
 * in libavformat without it knowing about it.
 */
static int read_packet(void *opaque, uint8_t *buf, int size)
{
    decodecontext *ctx = (decodecontext *) opaque;

    /*
     * If we read in less than we got asked for, and we're
     * not on the last file, then start reading seamlessly
     * on the next file.
     */
    size_t ret = fread(buf, 1, size, ctx->files[ctx->cur_file]);
    if (ret < (size_t) size && ctx->cur_file != ctx->files.size() - 1) {
        ctx->cur_file++;
        fseeko(ctx->files[ctx->cur_file], 0, SEEK_SET);
        ret += fread(buf + ret, 1, size - ret, ctx->files[ctx->cur_file]);
    }

    return ret == 0 ? AVERROR_EOF : static_cast<int>(ret);
}

/* Conditionally free all memebers of decodecontext. */
decodecontext::~decodecontext()
{
    av_freep(&in);
    av_packet_free(&inpkt);

    if (fctx) {
        if (fctx->pb)
            av_freep(&fctx->pb);

        avformat_close_input(&fctx);
    }

    for (size_t i = 0; i < files.size(); i++)
        fclose(files[i]);

    if (avctx) {
        avcodec_close(avctx);
        av_freep(&avctx);
    }
}

/* Initialize everything we can with regards to decoding */
decodecontext *decodeinit(d2vcontext *dctx, int threads, std::string& err)
{
    std::unique_ptr<decodecontext> ret(new decodecontext());

    /* Set our stream index to -1 (uninitialized). */
    ret->stream_index = -1;

    /* Open each file and stash its size. */
    for(int i = 0; i < dctx->num_files; i++) {
        FILE *in;
        int64_t size;

#ifdef _WIN32
        wchar_t filename[_MAX_PATH];

        size = MultiByteToWideChar(CP_UTF8, 0, dctx->files[i].c_str(), -1, filename, ARRAYSIZE(filename));
        if (!size) {
            err  = "Cannot parse file name: ";
            err += dctx->files[i];
            return NULL;
        }

        in = _wfopen(filename, L"rb");
#else
        in = fopen(dctx->files[i].c_str(), "rb");
#endif

        if (!in) {
            err  = "Cannot open file: ";
            err += dctx->files[i];
            return NULL;
        }

        fseeko(in, 0, SEEK_END);
        size = ftello(in);
        fseeko(in, 0, SEEK_SET);

        ret->file_sizes.push_back(size);
        ret->files.push_back(in);
    }

    /*
     * Register all of our demuxers, parsers, and decoders.
     * Ideally, to create a smaller binary, we only enable the
     * following:
     *
     * Demuxers: mpegvideo, mpegps, mpegts, h264.
     * Parsers: mpegvideo, mpegaudio, h264.
     * Decoders: mpeg1video, mpeg2video, h264.
     */
    /* API calls no longer needed, but comment left for info purposes. */

    /* Set the correct decoder. */
    if (dctx->mpeg_type == 1) {
        ret->incodec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    } else if (dctx->mpeg_type == 2) {
        ret->incodec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
    } else if (dctx->mpeg_type == 264) {
        ret->incodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    } else {
        err = "Invalid MPEG Type.";
        return NULL;
    }

    /* Allocate the codec's context. */
    ret->avctx = avcodec_alloc_context3(ret->incodec);
    if (!ret->avctx) {
        err = "Cannot allocate AVCodecContext.";
        return NULL;
    }

    /* Set the IDCT algorithm. */
    ret->avctx->idct_algo = dctx->idct_algo;

    /* Set the thread count. */
    ret->avctx->thread_count = threads;

    /* Open it. */
    int av_ret = avcodec_open2(ret->avctx, ret->incodec, NULL);
    if (av_ret < 0) {
        err = "Cannot open decoder.";
        return NULL;
    }

    /* Allocate the scratch buffer for our custom AVIO context. */
    ret->in = (uint8_t *) av_malloc(32 * 1024);
    if (!ret->in) {
        err = "Cannot alloc inbuf.";
        return NULL;
    }

    ret->inpkt = av_packet_alloc();
    if (!ret->inpkt) {
        err = "Cannot alloc packet.";
        return NULL;
    }

    /* We don't want to hear all the info it has. */
    av_log_set_level(AV_LOG_PANIC);

    return ret.release();
}

int decodeframe(int frame_num, d2vcontext *ctx, decodecontext *dctx, AVFrame *out, std::string& err)
{
    bool next = true;

    /* Get our frame and the GOP its in. */
    frame f = ctx->frames[frame_num];
    gop g = ctx->gops[f.gop];

    /*
     * The offset is how many frames we have to decode from our
     * current position in order to get to the frame we want.
     * The initial offset is obtaiend during the parsing of the
     * D2V file, but it may be more in an open GOP situation,
     * which we handle below.
     */
    int offset = f.offset;

    /*
     * If we're in a open GOP situation, then start decoding
     * from the previous GOP (one at most is needed), and adjust
     * out offset accordingly.
     */
    if (!(g.info & GOP_FLAG_CLOSED)) {
        if (f.gop == 0) {
            int n = 0;

            /*
             * Adjust the offset by the number of frames
             * that require of the previous GOP when the
             * first GOP is open.
             */
            while(!(g.flags[n] & FRAME_FLAG_DECODABLE_WITHOUT_PREVIOUS_GOP))
                n++;

            /*
             * Only adjust the offset if it's feasible;
             * that is, if it produces a positive offset.
             */
            offset = n > f.offset ? 0 : f.offset - n;

            /*
             * If the offset is 0, force decoding.
             *
             * FIXME: This method increases the number
             * of frames to be decoded.
             */
            next = offset != 0;
        } else {
            int n = frame_num;
            frame t = ctx->frames[n];

            g = ctx->gops[f.gop - 1];

            /*
             * Find the offset of the last frame in the
             * previous GOP and add it to our offset.
             */
            while(t.offset)
                t = ctx->frames[--n];

            t = ctx->frames[--n];

            /*
             * Subtract number of frames that require the
             * previous GOP.
             */
            n = 0;
            if (!(g.info & GOP_FLAG_CLOSED))
                while(!(g.flags[n] & FRAME_FLAG_DECODABLE_WITHOUT_PREVIOUS_GOP))
                    n++;

            offset += t.offset + 1 - n;
        }
    }

    /*
     * Check if we're decoding linearly, and if the GOP
     * of the current frame and previous frame are either
     * the same, or also linear. If so, we can decode
     * linearly.
     */
    next = next && (dctx->last_gop == f.gop || dctx->last_gop == f.gop - 1) && dctx->last_frame == frame_num - 1;

    /* Skip GOP initialization if we're decoding linearly. */
    if (!next) {
        /* Free out format and AVIO contexts from the previous seek. */
        if (dctx->fctx) {
            if (dctx->fctx->pb)
                av_freep(&dctx->fctx->pb);

            avformat_close_input(&dctx->fctx);
        }

        /* Seek to our GOP offset and stash the info. */
        fseeko(dctx->files[g.file], g.pos, SEEK_SET);
        dctx->orig_file_offset = g.pos;
        dctx->orig_file        = g.file;
        dctx->cur_file         = g.file;

        /* Allocate format context. */
        dctx->fctx = avformat_alloc_context();
        if (!dctx->fctx) {
            err = "Cannot allocate AVFormatContext.";
            return -1;
        }

        /*
         * Find the demuxer for our input type, and also set
         * the "filename" that we pass to libavformat when
         * we open the demuxer with our custom AVIO context.
         */
        if (ctx->stream_type == ELEMENTARY) {
            if (ctx->mpeg_type == 264) {
                dctx->fctx->iformat = av_find_input_format("h264");
                dctx->fakename  = "fakevideo.h264";
            } else {
                dctx->fctx->iformat = av_find_input_format("mpegvideo");
                dctx->fakename  = "fakevideo.m2v";
            }
        } else if (ctx->stream_type == PROGRAM) {
            dctx->fctx->iformat = av_find_input_format("mpeg");
            dctx->fakename      = "fakevideo.vob";
        } else if (ctx->stream_type == TRANSPORT) {
            dctx->fctx->iformat = av_find_input_format("mpegts");
            dctx->fakename      = "fakevideo.ts";
        } else {
            err = "Unsupported format.";
            avformat_close_input(&dctx->fctx);
            return -1;
        }

        /*
         * Initialize out custom AVIO context that libavformat
         * will use instead of a file. It uses our custom packet
         * reading and seeking functions that transparently work
         * with our indexed GOP offsets and multiple files.
         */
        dctx->fctx->pb = avio_alloc_context(dctx->in, 32 * 1024, 0, dctx, read_packet, NULL, file_seek);

        /* Open the demuxer. */
        int av_ret = avformat_open_input(&dctx->fctx, dctx->fakename, NULL, NULL);
        if (av_ret < 0) {
            err = "Cannot open buffer in libavformat.";
            avformat_close_input(&dctx->fctx);
            return -1;
        }

        /*
         * Flush the buffers of our codec's context so we
         * don't need to re-initialize it.
         */
        avcodec_flush_buffers(dctx->avctx);

        /*
         * Call the abomination function to find out
         * how many streams we have.
         */
        avformat_find_stream_info(dctx->fctx, NULL);

        /* Free and re-initialize any existing packet. */
        av_packet_unref(dctx->inpkt);
    }

    /*
     * Set our stream index if we need to.
     * Set it to the stream that matches our MPEG-TS PID if applicable.
     */
    if (dctx->stream_index == -1) {
        unsigned int i;

        if (ctx->ts_pid > 0) {
            for(i = 0; i < dctx->fctx->nb_streams; i++)
                if (dctx->fctx->streams[i]->id == ctx->ts_pid)
                    break;
        } else {
            for(i = 0; i < dctx->fctx->nb_streams; i++)
                if (dctx->fctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                    break;
        }

        if (i >= dctx->fctx->nb_streams) {
            if (ctx->ts_pid > 0)
                err = "PID does not exist in source file.";
            else
                err = "No video stream found.";

            avformat_close_input(&dctx->fctx);
            return -1;
        }

        dctx->stream_index = (int) i;
    }

    /*
     * We don't need to read a new packet in if we are decoding
     * linearly, since it's still there from the previous iteration.
     */
    if (!next)
        av_read_frame(dctx->fctx, dctx->inpkt);

    /* If we're decoding linearly, there is obviously no offset. */
    int o = next ? 0 : offset;
    for(int j = 0; j <= o; j++) {
        while(dctx->inpkt->stream_index != dctx->stream_index) {
            av_packet_unref(dctx->inpkt);
            av_read_frame(dctx->fctx, dctx->inpkt);
        }

        while (avcodec_receive_frame(dctx->avctx, out) == AVERROR(EAGAIN)) {
            avcodec_send_packet(dctx->avctx, dctx->inpkt);

            do {
                av_packet_unref(dctx->inpkt);
                av_read_frame(dctx->fctx, dctx->inpkt);
            } while(dctx->inpkt->stream_index != dctx->stream_index);
        }

        /* Unreference all but the last frame. */
        if (j != o)
            av_frame_unref(out);
    }

    /*
     * Stash the frame number we just decoded, and the GOP it
     * is a part of so we can check if we're decoding linearly
     * later on.
     */
    dctx->last_gop   = f.gop;
    dctx->last_frame = frame_num;

    return 0;
}
