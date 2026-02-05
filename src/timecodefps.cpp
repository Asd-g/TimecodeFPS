/*
Copyright (c) 2012, Nicholai Main
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


// matroska timecodes to CFR

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static std::wstring utf8_to_utf16(const std::string& str)
{
    if (str.empty())
        return std::wstring();

    const int required_size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (required_size == 0)
        return std::wstring();

    std::wstring wstr;
    wstr.resize(required_size - 1);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], required_size);
    return wstr;
}
#endif // !_WIN32


#include "avs_c_api_loader.hpp"


// begin draw code ********************************************************************************************
// the draw code is pretty bad.  i just slapped it together to get something on the display, don't read too much into it

typedef struct
{
    unsigned numcodes;
    double* codes;
    unsigned* remaps;
} udata_t;

void AVSC_CC tmd_free(AVS_FilterInfo* fi)
{
    udata_t* ud = (udata_t*)fi->user_data;
    free(ud->codes);
    free(ud->remaps);
    delete ud;
    fi->user_data = nullptr;
}

void putpx(int x, int y, unsigned long* dst, int dst_p, unsigned long color)
{
    dst[y * dst_p / 4 + x] = color;
}

void line(int x0, int y0, int x1, int y1, unsigned long* dst, int dst_p, unsigned long color)
{
    int i;
    if (x0 == x1)
    {
        for (i = y0; i <= y1; i++)
            putpx(x0, i, dst, dst_p, color);
    }
    else if (y0 == y1)
    {
        for (i = x0; i <= x1; i++)
            putpx(i, y0, dst, dst_p, color);
    }
}

void box(int x0, int y0, int x1, int y1, unsigned long* dst, int dst_p, unsigned long color)
{
    line(x0, y0, x0, y1, dst, dst_p, color);
    line(x0, y0, x1, y0, dst, dst_p, color);
    line(x0, y1, x1, y1, dst, dst_p, color);
    line(x1, y0, x1, y1, dst, dst_p, color);
}

void fillbox(int x0, int y0, int x1, int y1, unsigned long* dst, int dst_p, unsigned long color, unsigned long fillcolor)
{
    box(x0, y0, x1, y1, dst, dst_p, color);
    int i;
    for (i = y0 + 1; i < y1; i++)
        line(x0 + 1, i, x1 - 1, i, dst, dst_p, fillcolor);
}

AVS_VideoFrame* AVSC_CC tmd_get_frame(AVS_FilterInfo* fi, int n)
{
    udata_t* ud = (udata_t*)fi->user_data;

    avs_helpers::avs_video_frame_ptr ret_ptr{ g_avs_api->avs_new_video_frame_a(fi->env, &fi->vi, FRAME_ALIGN) };
    // 640 by 32
    AVS_VideoFrame* ret = ret_ptr.get();
    unsigned long* dst = (unsigned long*)g_avs_api->avs_get_write_ptr_p(ret, AVS_DEFAULT_PLANE);
    const int dst_p = g_avs_api->avs_get_pitch_p(ret, AVS_DEFAULT_PLANE);

    memset(dst, 0, dst_p * 32);

    // draw CFR frames
    for (int i = 0; i < 10; i++)
    {
        if (n + i - 5 < 0)
            continue;
        if (n + i - 5 >= fi->vi.num_frames)
            continue;
        int y0 = 16;
        int y1 = 31;
        int x0 = i * 64;
        int x1 = (i + 1) * 64 - 1;
        if (i == 5)
            fillbox(x0, y0, x1, y1, dst, dst_p, 0x00ffffff, 0x000000ff);
        else
            box(x0, y0, x1, y1, dst, dst_p, 0x00ffffff);
    }
    // draw VFR frames
    double cfrlen = (fi->vi.fps_denominator * 1000.0 / fi->vi.fps_numerator);

    double leftedge = cfrlen * (n - 5);
    double rightedge = cfrlen * (n + 5);

    if (ud->remaps[n] != (unsigned)-1)
    {
        double thisstart = ud->codes[ud->remaps[n]];
        double thisend = ud->remaps[n] == ud->numcodes - 1 ? rightedge /*ud->codes[n] * 2 - ud->codes[n - 1]*/ : ud->codes[ud->remaps[n] + 1];

        int thisstarti = static_cast<int>((thisstart - leftedge) / (rightedge - leftedge) * 639.0 + 0.5);
        int thisendi = static_cast<int>((thisend - leftedge) / (rightedge - leftedge) * 639.0 + 0.5);
        if (thisstarti < 0) thisstarti = 0;
        if (thisendi > 639) thisendi = 639;

        // debug
        if (thisstarti > 639) thisstarti = 639;
        if (thisendi < 0) thisendi = 0;

        fillbox(thisstarti, 0, thisendi, 15, dst, dst_p, 0x0000ff00, 0x0000ff00);
    }

    int xmin = 639;
    int xmax = 0;

    // scan forwards and backwards to add all frame boxes
    for (int i = static_cast<int>(ud->remaps[n]); i >= 0; i--)
    {
        if (ud->codes[i] < leftedge)
            break;
        int xpos = static_cast<int>((ud->codes[i] - leftedge) / (rightedge - leftedge) * 639.0 + 0.5);
        if (xpos < 0) xpos = 0;
        if (xpos > 639) xpos = 639;
        if (xpos < xmin) xmin = xpos;
        if (xpos > xmax) xmax = xpos;
        line(xpos, 0, xpos, 15, dst, dst_p, 0x00ffffff);

    }

    for (unsigned i = ud->remaps[n] + 1; i < ud->numcodes; i++)
    {
        if (ud->codes[i] > rightedge)
            break;
        int xpos = static_cast<int>((ud->codes[i] - leftedge) / (rightedge - leftedge) * 639.0 + 0.5);
        if (xpos < 0) xpos = 0;
        if (xpos > 639) xpos = 639;
        if (xpos < xmin) xmin = xpos;
        if (xpos > xmax) xmax = xpos;
        line(xpos, 0, xpos, 15, dst, dst_p, 0x00ffffff);
    }
    line(xmin, 0, xmax, 0, dst, dst_p, 0x00ffffff);
    line(xmin, 15, xmax, 15, dst, dst_p, 0x00ffffff);

    return ret;
}

// end draw code ********************************************************************************************



AVS_VideoFrame* AVSC_CC tmm_get_frame(AVS_FilterInfo* fi, int n)
{
    unsigned* framemappings = (unsigned*)fi->user_data;

    if (framemappings[n] != (unsigned)(-1))
        return g_avs_api->avs_get_frame(fi->child, framemappings[n]);

    // return a new blank frame
    avs_helpers::avs_video_frame_ptr ret_ptr{ g_avs_api->avs_new_video_frame_a(fi->env, &fi->vi, FRAME_ALIGN) };
    AVS_VideoFrame* ret = ret_ptr.get();

    if (!avs_is_yuv(&fi->vi))
    {
        // RGB: set all to 0
        memset(g_avs_api->avs_get_write_ptr_p(ret, AVS_DEFAULT_PLANE), 0, g_avs_api->avs_get_pitch_p(ret, AVS_DEFAULT_PLANE)
            * g_avs_api->avs_get_height_p(ret, AVS_DEFAULT_PLANE));
    }
    else
    { // yuv: y plane to 0, u+v planes to 128
      // don't care about limited range for Y plane, since it's still black
        memset(g_avs_api->avs_get_write_ptr_p(ret, AVS_PLANAR_Y), 0, g_avs_api->avs_get_pitch_p(ret, AVS_PLANAR_Y) * g_avs_api->avs_get_height_p(ret, AVS_PLANAR_Y));
        memset(g_avs_api->avs_get_write_ptr_p(ret, AVS_PLANAR_U), 128, g_avs_api->avs_get_pitch_p(ret, AVS_PLANAR_U) * g_avs_api->avs_get_height_p(ret, AVS_PLANAR_U));
        memset(g_avs_api->avs_get_write_ptr_p(ret, AVS_PLANAR_V), 128, g_avs_api->avs_get_pitch_p(ret, AVS_PLANAR_V) * g_avs_api->avs_get_height_p(ret, AVS_PLANAR_V));
    }
    return ret;
}

void AVSC_CC tmm_free(AVS_FilterInfo* fi)
{
    free(fi->user_data);
}


unsigned* frameremap(unsigned* numout, const double* in, unsigned ncodes, int fpsnum, int fpsden, int* dup, int* drop, double th1, double th2, double* avgerr, bool lastfr)
{
    unsigned* remaps = NULL;
    unsigned numremaps = 0;
    unsigned numremapspace = 0;

    int ndup = 0;
    int ndrop = 0;

    // calculate error mean
    double errsum = 0.0;
    unsigned errn = 0;

    // length of one frame in MS;
    double cfrlen = (fpsden * 1000.0 / fpsnum);

    // scale threshholds to ms
    th1 *= cfrlen;
    th2 *= cfrlen;

    numremapspace = 1024;
    remaps = (unsigned int*)malloc(1024 * sizeof(unsigned));
    if (!remaps)
        return NULL;

    unsigned i;

    // at each step, process input frame i - 1
    // (when i = 0, blank starting frame
    for (i = 0; i < ncodes; i++)
    {
        // start time of this frame (CFR)
        double cfrstart = numremaps * cfrlen;
        // end time of this frame (VFR)
        double vfrend = in[i];
        // amount of space we "want" to cover with this frmae
        double diff = vfrend - cfrstart;

        // attempt to insert a first copy of frame at low threshold
        // special: don't bother with this when i == 0 (since it's not a real frame and we don't care if we lose it)
        if (i && diff > th1)
        {
            // add frame
            if (numremaps == numremapspace)
            {
                numremapspace *= 2;
                remaps = (unsigned int*)realloc(remaps, numremapspace * sizeof(unsigned));
                if (!remaps)
                    return NULL;
            }
            remaps[numremaps++] = i - 1;

            diff -= cfrlen;
        }
        else if (i)
        {
            ndrop++;
        }
        // attempt to insert additional copies of frame at low threshold
        while (diff > th2)
        {
            // add frame
            if (numremaps == numremapspace)
            {
                numremapspace *= 2;
                remaps = (unsigned int*)realloc(remaps, numremapspace * sizeof(unsigned));
                if (!remaps)
                    return NULL;
            }
            remaps[numremaps++] = i - 1;

            diff -= cfrlen;
            if (i)
                ndup++;
        }

        // record error: start time of frame i (not i - 1) in cfr vs vfr
        errn++;
        errsum += std::abs(vfrend - (numremaps * cfrlen));

    }

    if (lastfr)
    {
        // last frame: guestimate duration based on the previous frame
        int lflen = static_cast<int>((in[i - 1] - in[i - 2]) / cfrlen + 0.5);
        // but insert it at least once, might as well
        if (lflen < 1) lflen = 1;
        ndup--;
        while (lflen--)
        {
            // add frame
            if (numremaps == numremapspace)
            {
                numremapspace *= 2;
                remaps = (unsigned int*)realloc(remaps, numremapspace * sizeof(unsigned));
                if (!remaps)
                    return NULL;
            }
            remaps[numremaps++] = i - 1;

            ndup++;
        }
    }

    *numout = numremaps;
    if (dup)
        *dup = ndup;
    if (drop)
        *drop = ndrop;
    if (avgerr)
        *avgerr = errsum / errn;
    return remaps;

}

AVS_Value AVSC_CC tmm_create(AVS_ScriptEnvironment* env, AVS_Value args, void* unused)
{
    const char* filename = avs_helpers::get_opt_arg<const char*>(env, args, 1).value_or("timecodes.txt");
    int fpsnum = avs_helpers::get_opt_arg<int>(env, args, 2).value_or(25);
    int fpsden = avs_helpers::get_opt_arg<int>(env, args, 3).value_or(1);
    int reporting = avs_helpers::get_opt_arg<int>(env, args, 4).value_or(0);
    double threshone = avs_helpers::get_opt_arg<double>(env, args, 5).value_or(0.4);
    double threshmore = avs_helpers::get_opt_arg<double>(env, args, 6).value_or(0.9);
    int starting = avs_helpers::get_opt_arg<int>(env, args, 7).value_or(0);

    // Split filename if it's a semicolon separated ist
    std::stringstream ss;
    ss << filename;
    std::vector<std::string> filenames;

    while (ss.good())
    {
        std::string substr;
        std::getline(ss, substr, ';');
        filenames.push_back(substr);
    }


    if (fpsnum < 1) fpsnum = 1;
    if (fpsden < 1) fpsden = 1;
    /*
    if (threshone < 0.01) threshone = 0.01;
    if (threshmore < 0.02) threshmore = 0.02;
    */

    char buff[1024];
    int ncodes = 0;
    int ncodespace = 0;
    double* codes = NULL;

    for (size_t i = 0; i < filenames.size(); i++) {

        FILE* fil = nullptr;
#ifdef _WIN32
        std::wstring w_filename = utf8_to_utf16(filenames[i]);
        fil = _wfopen(w_filename.c_str(), L"r");
#else
        fil = fopen(filenames[i].c_str(), "r");
#endif // _WIN32

        if (!fil)
            return avs_new_value_error("couldn't open file for reading!");


        if (!fgets(buff, 1024, fil) || (strcmp(buff, "# timecode format v2\n") && strcmp(buff, "# timestamp format v2\n")))
        {
            fclose(fil);
            return avs_new_value_error("file doesn't appear to be mkvtoolnix timecodes v2");
        }

        // read in file
        while (fgets(buff, 1024, fil))
        {
            if (ncodes == ncodespace)
            {
                ncodespace = ncodespace ? ncodespace * 2 : 1024;
                codes = (double*)realloc(codes, ncodespace * sizeof(double));
                if (!codes)
                {
                    fclose(fil);
                    return avs_new_value_error("out of memory!");
                }
            }
            if (sscanf(buff, "%lf", codes + ncodes) != 1)
            {
                free(codes);
                fclose(fil);
                return avs_new_value_error("parse error on timecode file");
            }
            ncodes++;
        }
        fclose(fil);

    }

    if (ncodes == 0)
    {
        return avs_new_value_error("no timecodes in file?");
    }


    avs_helpers::avs_clip_ptr clip_ptr{ g_avs_api->avs_take_clip(avs_array_elt(args, 0), env) };
    AVS_Clip* clip = clip_ptr.get();
    const AVS_VideoInfo* vi = g_avs_api->avs_get_video_info(clip);

    if (ncodes != vi->num_frames && ncodes - 1 != vi->num_frames)
    {
        free(codes);
        return avs_new_value_error("number of timecodes neither doesn't match number of input frames nor does have one more value than number of input frames");
    }

    if (starting)
    {
        for (int i = ncodes; i >= 0; i--)
            codes[i] -= codes[0];
    }

    unsigned numremaps;
    unsigned* remaps;

    int ndup;
    int ndrop;

    double avgerr;

    remaps = frameremap(&numremaps, codes, ncodes, fpsnum, fpsden, &ndup, &ndrop, threshone, threshmore, &avgerr, ncodes == vi->num_frames);


    if (!remaps)
    {
        return avs_new_value_error("some sort of processing error? (are timecodes in order?)");
    }

    AVS_FilterInfo* fi;
    avs_helpers::avs_clip_ptr clipout_ptr{ g_avs_api->avs_new_c_filter(env, &fi, avs_array_elt(args, 0), 1) };
    AVS_Clip* clipout = clipout_ptr.get();
    fi->vi.num_frames = numremaps;

    fi->vi.fps_numerator = fpsnum;
    fi->vi.fps_denominator = fpsden;

    if (reporting)
    {
        std::unique_ptr<udata_t> ud = std::make_unique<udata_t>();

        std::stringstream ss;
        ss << "FPSNUM: " << fpsnum
            << " FPSDEN: " << fpsden
            << " DUPS: " << ndup
            << " DROPS: " << ndrop
            << " AVGERR: " << avgerr
            << " TH1 " << threshone
            << "  TH2+ " << threshmore;

        std::string msg_utf8 = ss.str();
        std::string title_utf8 = "TimeCodeFPS information";

#ifdef _WIN32
        auto utf8_to_wstring = [](const std::string& utf8_str) -> std::wstring 
            {
            if (utf8_str.empty())
                return L"";
            const int len = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), (int)utf8_str.length(), NULL, 0);
            std::wstring wstr(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), (int)utf8_str.length(), &wstr[0], len);
            return wstr;
            };
        
        std::wstring wmsg = utf8_to_utf16(msg_utf8);
        std::wstring wtitle = utf8_to_utf16(title_utf8);

        MessageBox(NULL, wmsg.c_str(), wtitle.c_str(), 0);
#else
#ifdef __APPLE__
        std::string script = "display dialog \"" + msg_utf8 + "\" with title \"" + title_utf8 + "\"";
        std::string command = "osascript -e '" + script + "'";
        system(command.c_str());
#else
        std::string command = "zenity --info --title=\"" + title_utf8 + "\" --text=\"" + msg_utf8 + "\"";
        system(command.c_str());
#endif // __APPLE__
#endif // _WIN32        

        // for reporting, we make a different clip
        fi->vi.width = 640;
        fi->vi.height = 32;
        fi->vi.pixel_type = AVS_CS_BGR32;

        ud->numcodes = ncodes;
        ud->codes = codes;
        ud->remaps = remaps;

        fi->get_frame = tmd_get_frame;
        fi->user_data = ud.release();
        fi->free_filter = tmd_free;
    }
    else
    {
        free(codes);
        fi->get_frame = tmm_get_frame;
        fi->user_data = remaps;
        fi->free_filter = tmm_free;
    }


    AVS_Value ret;
    g_avs_api->avs_set_to_clip(&ret, clipout);

    return ret;
}

const char* AVSC_CC avisynth_c_plugin_init(AVS_ScriptEnvironment* __restrict env)
{
    static constexpr int REQUIRED_INTERFACE_VERSION{ 9 };
    static constexpr int REQUIRED_BUGFIX_VERSION{ 2 };
    static constexpr std::string_view required_functions_storage[]{
        "avs_pool_free",           // avs loader helper functions
        "avs_release_clip",        // avs loader helper functions
        "avs_release_value",       // avs loader helper functions
        "avs_release_video_frame", // avs loader helper functions
        "avs_take_clip",           // avs loader helper functions
        "avs_add_function",
        "avs_new_c_filter",
        "avs_new_video_frame_a",
        "avs_set_to_clip",
        "avs_get_frame",
        "avs_get_height_p",
        "avs_get_pitch_p",
        "avs_get_video_info",
        "avs_get_write_ptr_p"
    };
    static constexpr std::span<const std::string_view> required_functions{ required_functions_storage };

    if (!avisynth_c_api_loader::get_api(env, REQUIRED_INTERFACE_VERSION, REQUIRED_BUGFIX_VERSION, required_functions)) {
        std::cerr << avisynth_c_api_loader::get_last_error() << std::endl;
        return avisynth_c_api_loader::get_last_error();
    }

    g_avs_api->avs_add_function(env,
        "timecodefps", "c[timecodes]s[fpsnum]i[fpsden]i[report]b[threshone]f[threshmore]f[start]b",
        tmm_create, 0);
    return "Matroska v2 timecodes -> CFR";
}
