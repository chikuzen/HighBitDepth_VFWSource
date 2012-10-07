HBVFWSource.dll
	High-bit-depth VFW source reader for AviSynth2.6x

Author Oka Motofumi (chikuzen.mo at gmail dot com)



usage:

HBVFWSource(string source, bool "stacked")
source: source file path
stacked: if this is set to true, MSB/LSB will be separated and be stacked vertially(default false).

#sample.avs
LoadPlugin("HBVFWSource.dll")
LoadPlugin("flash3kyuu_deband.dll")
HBVFWSource("YUV42XP16.vpy")
f3kdb_dither(stacked=false, input_depth=16)


note:
supported formats(FourCC) are P216, P210, P016 and P010.
audio is unsupported.



requirements:
AviSynth2.6alpha3 or later
WindowsXP/Vista/7
Microsoft Visual C++ 2010 Redistributable Package
SSE2 capable CPU



sourcecode:
https://github.com/chikuzen/HighBitDepth_VFWSource/
