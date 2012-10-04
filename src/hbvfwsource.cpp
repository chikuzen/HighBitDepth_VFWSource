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

#define HBVFW_VERSION "0.1.1"

typedef union {
    DWORD fcc;
    struct {
        char c0;
        char c1;
        char c2;
        char c3;
    } c;
} fcc_t;


typedef struct {
    WORD u;
    WORD v;
} uv_t;


class HBVFWSource : public IClip {

    VideoInfo vi;
    PAVIFILE file;
    PAVISTREAM stream;
    AVIFILEINFO file_info;
    AVISTREAMINFO stream_info;
    BYTE *buff;
    LONG buff_size;

public:
    HBVFWSource(const char *source, IScriptEnvironment *env);
    virtual ~HBVFWSource();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env);
    bool __stdcall GetParity(int n) { return false; }
    void __stdcall GetAudio(void *, __int64, __int64, IScriptEnvironment*) {}
    const VideoInfo& __stdcall GetVideoInfo() { return vi; }
    void __stdcall SetCacheHints(int cachehints,int frame_range) {}

};


HBVFWSource::HBVFWSource(const char *source, IScriptEnvironment *env)
{
    AVIFileInit();

    ZeroMemory(&vi, sizeof(VideoInfo));

    if (AVIFileOpen(&file, source, OF_READ, NULL) != 0) {
        env->ThrowError("HBVFWSource: couldn't open file %s", source);
    }

    if (AVIFileInfo(file, &file_info, sizeof(AVIFILEINFO)) != 0) {
        env->ThrowError("HBVFWSource: coudn't get file info");
    }

    if (AVIFileGetStream(file, &stream, 0, 0) != 0 ) {
        env->ThrowError("HBVFWSource: couldn't get stream");
    }

    AVIStreamInfo(stream, &stream_info, sizeof(AVISTREAMINFO));

     const struct {
        DWORD fourcc;
        int avs_pix_type;
        int shift;
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
        fcc_t fcc;
        fcc.fcc = stream_info.fccHandler;
        env->ThrowError("HBVFWSource: unsupported format '%c%c%c%c'",
                        fcc.c.c0, fcc.c.c1, fcc.c.c2, fcc.c.c3);
    }

    vi.pixel_type = table[i].avs_pix_type;
    vi.width = file_info.dwWidth << 1;
    vi.height = file_info.dwHeight;
    vi.fps_numerator = stream_info.dwRate;
    vi.fps_denominator = stream_info.dwScale;
    vi.num_frames = stream_info.dwLength;
    vi.SetFieldBased(false);

    AVIStreamRead(stream, 0, 1, NULL, 0, &buff_size, NULL);
    buff = (BYTE *)malloc(buff_size);
    if (!buff) {
        env->ThrowError("HBVFWSource: out of memory");
    }
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

    env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y),
                buff, vi.width, vi.width, vi.height);

    WORD* dstp_u = (WORD*)dst->GetWritePtr(PLANAR_U);
    WORD* dstp_v = (WORD*)dst->GetWritePtr(PLANAR_V);
    int pitch = dst->GetPitch(PLANAR_U) >> 1;
    uv_t* srcp_uv = (uv_t*)(buff + vi.width * vi.height);
    int width = vi.width >> 1;
    int height = dst->GetHeight(PLANAR_U);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dstp_u[x] = srcp_uv[x].u;
            dstp_v[x] = srcp_uv[x].v;
        }
        srcp_uv += width >> 1;
        dstp_u += pitch;
        dstp_v += pitch;
    }

    return dst;
}


AVSValue __cdecl create_vfw_source(AVSValue args, void* user_data,
                                   IScriptEnvironment* env)
{
    if (!args[0].Defined()) {
        env->ThrowError("HBVFWSource: No source specified");
    }
    return new HBVFWSource(args[0].AsString(), env);
}


extern "C" __declspec(dllexport) const char* __stdcall
AvisynthPluginInit2(IScriptEnvironment* env)
{
  env->AddFunction("HBVFWSource","[source]s", create_vfw_source, 0);
  return "HBVFWSource for AviSynth2.6x version" HBVFW_VERSION;
}
