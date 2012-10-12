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

#define HBVFW_VERSION "0.2.2"


static inline DWORD to_dword(BYTE b0, BYTE b1, BYTE b2, BYTE b3)
{
    return ((DWORD)b0 << 24) | ((DWORD)b1 << 16) | ((DWORD)b2 << 8) | (DWORD)b3;
}


static void __stdcall
write_interleaved_frame(PVideoFrame& dst, BYTE* buff, VideoInfo& vi,
                        IScriptEnvironment *env)
{
    struct uv_t {
        WORD u; WORD v;
    };

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

    struct y16_t {
        BYTE lsb0; BYTE msb0; BYTE lsb1; BYTE msb1;
        BYTE lsb2; BYTE msb2; BYTE lsb3; BYTE msb3;
    };

    struct uv16_t {
        BYTE lsb_u0; BYTE msb_u0; BYTE lsb_v0; BYTE msb_v0;
        BYTE lsb_u1; BYTE msb_u1; BYTE lsb_v1; BYTE msb_v1;
        BYTE lsb_u2; BYTE msb_u2; BYTE lsb_v2; BYTE msb_v2;
        BYTE lsb_u3; BYTE msb_u3; BYTE lsb_v3; BYTE msb_v3;
    };

    BYTE* srcp = buff;
    int src_pitch = vi.width * 2;

    int width = (dst->GetRowSize(PLANAR_Y) + 3) >> 2;
    int lines = dst->GetHeight(PLANAR_Y) >> 1;
    int dst_pitch = dst->GetPitch(PLANAR_Y) >> 2;
    DWORD* msb_y = (DWORD*)dst->GetWritePtr(PLANAR_Y);
    DWORD* lsb_y = msb_y + (dst_pitch * lines);
    for (int y = 0; y < lines; y++) {
        y16_t* srcp_y = (y16_t*)(srcp + y * src_pitch);
        for (int x = 0; x < width; x++) {
            msb_y[x] = to_dword(srcp_y[x].msb3, srcp_y[x].msb2,
                                srcp_y[x].msb1, srcp_y[x].msb0);
            lsb_y[x] = to_dword(srcp_y[x].lsb3, srcp_y[x].lsb2,
                                srcp_y[x].lsb1, srcp_y[x].lsb0);
        }
        msb_y += dst_pitch;
        lsb_y += dst_pitch;
    }

    srcp += vi.width * vi.height;

    width = (dst->GetRowSize(PLANAR_U) + 3) >> 2;
    lines = dst->GetHeight(PLANAR_U) >> 1;
    dst_pitch = dst->GetPitch(PLANAR_U);
    BYTE* msb_u_orig = dst->GetWritePtr(PLANAR_U);
    BYTE* lsb_u_orig = msb_u_orig + dst_pitch * lines;
    BYTE* msb_v_orig = dst->GetWritePtr(PLANAR_V);
    BYTE* lsb_v_orig = msb_v_orig + dst_pitch * lines;
    for (int y = 0; y < lines; y++) {
        uv16_t* srcp_uv = (uv16_t*)(srcp + y * src_pitch);
        DWORD* msb_u = (DWORD*)(msb_u_orig + y * dst_pitch);
        DWORD* lsb_u = (DWORD*)(lsb_u_orig + y * dst_pitch);
        DWORD* msb_v = (DWORD*)(msb_v_orig + y * dst_pitch);
        DWORD* lsb_v = (DWORD*)(lsb_v_orig + y * dst_pitch);
        for (int x = 0; x < width; x++) {
            msb_u[x] = to_dword(srcp_uv[x].msb_u3, srcp_uv[x].msb_u2,
                                srcp_uv[x].msb_u1, srcp_uv[x].msb_u0);
            lsb_u[x] = to_dword(srcp_uv[x].lsb_u3, srcp_uv[x].lsb_u2,
                                srcp_uv[x].lsb_u1, srcp_uv[x].lsb_u0);
            msb_v[x] = to_dword(srcp_uv[x].msb_v3, srcp_uv[x].msb_v2,
                                srcp_uv[x].msb_v1, srcp_uv[x].msb_v0);
            lsb_v[x] = to_dword(srcp_uv[x].lsb_v3, srcp_uv[x].lsb_v2,
                                srcp_uv[x].lsb_v1, srcp_uv[x].lsb_v0);
        }
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
    buff = (BYTE*)malloc(buff_size + 32);
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
