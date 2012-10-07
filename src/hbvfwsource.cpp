/*
  HighBit-depth VFW Readerfor AviSynth2.6x

  Copyright (C) 2012 Oka Motofumi

  Authors: Oka Motofumi (chikuzen.mo at gmail dot com)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
*/


#include <windows.h>
#include <vfw.h>
#include "avisynth26.h"

#define HBVFW_VERSION "0.2.0"

struct uv_t {
    WORD u;
    WORD v;
};

struct y16_t {
    BYTE lsb;
    BYTE msb;
};

struct uv16_t {
    BYTE lsb_u;
    BYTE msb_u;
    BYTE lsb_v;
    BYTE msb_v;
};


static void __stdcall
write_interleaved_frame(PVideoFrame& dst, BYTE* buff, VideoInfo& vi,
                        IScriptEnvironment *env)
{
    env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y),
                buff, vi.width, vi.width, vi.height);

    WORD* dstp_u = (WORD*)dst->GetWritePtr(PLANAR_U);
    WORD* dstp_v = (WORD*)dst->GetWritePtr(PLANAR_V);
    int pitch = dst->GetPitch(PLANAR_U) >> 1;
    uv_t* srcp_uv = (uv_t*)(buff + vi.width * vi.height);
    int width = dst->GetRowSize(PLANAR_U) >> 1;
    int height = dst->GetHeight(PLANAR_U);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dstp_u[x] = srcp_uv[x].u;
            dstp_v[x] = srcp_uv[x].v;
        }
        srcp_uv += width;
        dstp_u += pitch;
        dstp_v += pitch;
    }
}


static void __stdcall
write_stacked_frame(PVideoFrame& dst, BYTE* buff, VideoInfo& vi,
                    IScriptEnvironment *env)
{
    int width = dst->GetRowSize(PLANAR_Y);
    int lines = dst->GetHeight(PLANAR_Y) >> 1;
    y16_t* srcp_y = (y16_t*)buff;
    int dst_pitch = dst->GetPitch(PLANAR_Y);
    BYTE* msb_y = dst->GetWritePtr(PLANAR_Y);
    BYTE* lsb_y = msb_y + (dst_pitch * lines);
    for (int y = 0; y < lines; y++) {
        for (int x = 0; x < width; x++) {
            msb_y[x] = srcp_y[x].msb;
            lsb_y[x] = srcp_y[x].lsb;
        }
        srcp_y += width;
        msb_y += dst_pitch;
        lsb_y += dst_pitch;
    }

    width = dst->GetRowSize(PLANAR_U);
    lines = dst->GetHeight(PLANAR_U) >> 1;
    uv16_t* srcp_uv = (uv16_t*)(buff + vi.width * vi.height);
    dst_pitch = dst->GetPitch(PLANAR_U);
    BYTE* msb_u = dst->GetWritePtr(PLANAR_U);
    BYTE* lsb_u = msb_u + dst_pitch * lines;
    BYTE* msb_v = dst->GetWritePtr(PLANAR_V);
    BYTE* lsb_v = msb_v + dst_pitch * lines;
    for (int y = 0; y < lines; y++) {
        for (int x = 0; x < width; x++) {
            msb_u[x] = srcp_uv[x].msb_u;
            lsb_u[x] = srcp_uv[x].lsb_u;
            msb_v[x] = srcp_uv[x].msb_v;
            lsb_v[x] = srcp_uv[x].lsb_v;
        }
        srcp_uv += width;
        msb_u += dst_pitch;
        lsb_u += dst_pitch;
        msb_v += dst_pitch;
        lsb_v += dst_pitch;
    }
}


class HBVFWSource : public IClip {

    VideoInfo vi;
    PAVIFILE file;
    PAVISTREAM stream;
    AVIFILEINFO file_info;
    AVISTREAMINFO stream_info;
    BYTE* buff;
    LONG buff_size;
    void (__stdcall *func_write_frame)(PVideoFrame&, BYTE*, VideoInfo&,
                                       IScriptEnvironment*);

public:
    HBVFWSource(const char* source, bool stacked, IScriptEnvironment* env);
    virtual ~HBVFWSource();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    bool __stdcall GetParity(int n) { return false; }
    void __stdcall GetAudio(void*, __int64, __int64, IScriptEnvironment*) {}
    const VideoInfo& __stdcall GetVideoInfo() { return vi; }
    void __stdcall SetCacheHints(int, int) {}

};


HBVFWSource::HBVFWSource(const char* source, bool stacked,
                         IScriptEnvironment* env)
{
    AVIFileInit();

    ZeroMemory(&vi, sizeof(VideoInfo));

    if (AVIFileOpen(&file, source, OF_READ, NULL) != 0) {
        env->ThrowError("HBVFWSource: couldn't open file %s", source);
    }

    if (AVIFileInfo(file, &file_info, sizeof(AVIFILEINFO)) != 0) {
        env->ThrowError("HBVFWSource: coudn't get file info");
    }

    if (AVIFileGetStream(file, &stream, streamtypeVIDEO, 0) != 0 ) {
        env->ThrowError("HBVFWSource: couldn't get video stream");
    }

    AVIStreamInfo(stream, &stream_info, sizeof(AVISTREAMINFO));

    const struct {
        DWORD fourcc;
        int avs_pix_type;
    } table[] = {
        { MAKEFOURCC('P', '2', '1', '6'), VideoInfo::CS_YV16    },
        { MAKEFOURCC('P', '2', '1', '0'), VideoInfo::CS_YV16    },
        { MAKEFOURCC('P', '0', '1', '6'), VideoInfo::CS_I420    },
        { MAKEFOURCC('P', '0', '1', '0'), VideoInfo::CS_I420    },
        { stream_info.fccHandler,         VideoInfo::CS_UNKNOWN }
    };

    int i = 0;
    while (table[i].fourcc != stream_info.fccHandler) i++;
    if (table[i].avs_pix_type == VideoInfo::CS_UNKNOWN) {
        union {
            DWORD fcc;
            char c[4];
        }; 
        fcc = stream_info.fccHandler;
        env->ThrowError("HBVFWSource: unsupported format '%c%c%c%c'",
                        c[3], c[2], c[1], c[0]);
    }

    AVIStreamRead(stream, 0, 1, NULL, 0, &buff_size, NULL);
    buff = (BYTE*)malloc(buff_size);
    if (!buff) {
        env->ThrowError("HBVFWSource: out of memory");
    }

    vi.pixel_type = table[i].avs_pix_type;
    vi.width = file_info.dwWidth << (stacked ? 0 : 1);
    vi.height = file_info.dwHeight << (stacked ? 1 : 0);
    vi.fps_numerator = stream_info.dwRate;
    vi.fps_denominator = stream_info.dwScale;
    vi.num_frames = stream_info.dwLength;
    vi.SetFieldBased(false);
    func_write_frame = stacked ? write_stacked_frame : write_interleaved_frame;
}


HBVFWSource::~HBVFWSource() {
    if (buff) {
        free(buff);
    }
    if (stream) {
        AVIStreamRelease(stream);
    }
    if (file) {
        AVIFileRelease(file);
    }
    AVIFileExit();
}


PVideoFrame __stdcall HBVFWSource::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame dst = env->NewVideoFrame(vi);
    LONG size;
    if (AVIStreamRead(stream, n, 1, buff, buff_size, &size, NULL)) {
        return dst;
    }

    func_write_frame(dst, buff, vi, env);

    return dst;
}


AVSValue __cdecl
create_vfw_source(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    if (!args[0].Defined()) {
        env->ThrowError("HBVFWSource: No source specified");
    }
    return new HBVFWSource(args[0].AsString(), args[1].AsBool(false), env);
}


extern "C" __declspec(dllexport) const char* __stdcall
AvisynthPluginInit2(IScriptEnvironment* env)
{
    env->AddFunction("HBVFWSource","[source]s[stacked]b", create_vfw_source, 0);
    return "HBVFWSource for AviSynth2.6x version" HBVFW_VERSION;
}
