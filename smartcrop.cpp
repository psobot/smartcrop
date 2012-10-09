//
//  smartcrop.cpp
//  SmartCrop
//
//  Created by Peter Sobot on 8/26/12.
//  Copyright (c) 2012 Peter Sobot. All rights reserved.
//

#include <iostream>
#include <string>
#include <glob.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include <math.h>
#include <assert.h>

extern "C" {
#include <jpeglib.h>
}

#define OPTIONAL_ARGUMENT(pos, name) (argc > (pos+1) ? argv[pos+1] : DEFAULT_##name)

#define DEFAULT_SRC "/Volumes/Fry HD/Pictures/iPhoto Library/Modified/2012/NYC*/*.jpg"//"/Volumes/Fry HD/Pictures/iPhoto Library/Modified/201*/*/*.jpg"
#define DEFAULT_DEST "/Users/psobot/out_imgs/"//"/Volumes/why/www.petersobot.com/source/images/thumbs/"

#define TIMER_START(desc) \
    long __mtime, __seconds, __useconds; \
    const char* __timer_desc = desc; \
    timeval __start; \
    gettimeofday(&__start, NULL);
#define TIMER_END \
    timeval __end; \
    gettimeofday(&__end, NULL); \
    __seconds  = __end.tv_sec  - __start.tv_sec; \
    __useconds = __end.tv_usec - __start.tv_usec; \
    __mtime = ((__seconds) * 1000 + __useconds/1000.0) + 0.5; \
    cout << __timer_desc << " took " << __mtime << " ms." << endl; \
    __timer_desc = "";

#define STRIDE 10
#define IN_FACTOR 8  // Power of 2, initial scale down factor.
#define JPEG_QUALITY 75

using namespace std;

//  Credit goes to: http://stackoverflow.com/questions/8401777/simple-glob-in-c-on-unix-system
//  by Piti Ongmongkolkul
inline vector<string> glob(const string& pat){
    glob_t glob_result;
    glob(pat.c_str(),GLOB_TILDE,NULL,&glob_result);
    vector<string> ret;
    for(unsigned int i = 0; i < glob_result.gl_pathc; ++i){
        ret.push_back(string(glob_result.gl_pathv[i]));
    }
    globfree(&glob_result);
    return ret;
}

//  JPEG-decompress FILE into buf.
long decompress(FILE* file, jpeg_decompress_struct &cinfo, JSAMPLE * &buf) {
    fseek(file, 0, 0);
    
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, true);
    
    //if (cinfo.image_width < target_size * 4) {
        //  Throw error?
    //}
    cinfo.scale_num = 1;
    cinfo.scale_denom = IN_FACTOR;
    
    jpeg_start_decompress(&cinfo);
    
    int row_stride_c = cinfo.output_width * cinfo.output_components;
    buf = (JSAMPLE*) malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);
    
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPLE *rowp[1];
        rowp[0] = (JSAMPLE *) buf + row_stride_c * cinfo.output_scanline;
        jpeg_read_scanlines(&cinfo, rowp, 1);
    }
    
    jpeg_finish_decompress(&cinfo);
    
    return cinfo.output_width * cinfo.output_height * cinfo.output_components;
}

//  Determine the entropy of a cropped portion of the buffer.
double entropy(JSAMPLE * &in, jpeg_decompress_struct &cinfo, int x, int y, int width, int height) {
#define HISTOGRAM_RESOLUTION 16
    long histogram[HISTOGRAM_RESOLUTION];
    for (int i = 0; i < HISTOGRAM_RESOLUTION; i++) histogram[i] = 0;
    
    int incs = 0;
    for (int _y = y; _y < y + height; _y++) {
        for (int _x = x; _x < x + width; _x++) {
            float val = 0;
            if (cinfo.out_color_components == 3) {
                int idx = ((_y * (cinfo.image_width / IN_FACTOR)) + _x) * cinfo.out_color_components;
                val = (0.299f * in[idx]) + (0.587f * in[idx+1]) + (0.114 * in[idx+2]);
            } else {
                for (int _c = 0; _c < cinfo.out_color_components; _c++) {
                    int idx = ((_y * (cinfo.image_width / IN_FACTOR)) + _x) * cinfo.out_color_components + _c;
                    val += in[idx];
                }
            }
            incs++;
            histogram[(JSAMPLE) val / (256/HISTOGRAM_RESOLUTION)]++;
        }
    }
    
    int area = width * height;
    double e = 0.0f;
    double p = 0;
    for (int i = 0; i < HISTOGRAM_RESOLUTION; i++) {
        p = (double) histogram[i] / (double) area;
        if (p > 0) {
            e += p * log2(p);
        }
    }

    return -1.0f * e;
}

long smart_crop(JSAMPLE * &in, long in_size, JSAMPLE * &out, jpeg_decompress_struct &cinfo, int target_size) {
    int x = 0;
    int y = 0;
    int width = cinfo.image_width / IN_FACTOR;
    int height = cinfo.image_height / IN_FACTOR;
    int slice_length = 16;
    int c = cinfo.output_components;
    
    int crop_width = target_size;
    int crop_height = target_size;
    
    while ((width - x) > target_size) {
        int slice_width = min(width - x - crop_width, slice_length);
        if (entropy(in, cinfo, x, 0, slice_width, cinfo.image_height / IN_FACTOR) <
            entropy(in, cinfo, width - slice_width, 0, slice_width, cinfo.image_height / IN_FACTOR)) {
            x += slice_width;
        } else {
            width -= slice_width;
        }
    }
                
    while ((height - y) > crop_height) {
        int slice_height = min(height - y - crop_height, slice_length);
        if (entropy(in, cinfo, 0, y, cinfo.image_width / IN_FACTOR, slice_height) < 
            entropy(in, cinfo, 0, height - slice_height, cinfo.image_width / IN_FACTOR, slice_height)) {
            y += slice_height;
        } else {
            height -= slice_height;
        }
    }
    
    out = (JSAMPLE*) malloc(crop_width * crop_height * c); 
    
    for (int _y = 0; _y < crop_height; _y++) {
        for (int _x = 0; _x < crop_width; _x++) {
            for (char _c = 0; _c < c; _c++) {
                int in_idx = (c * (((_y + y) * (cinfo.image_width / IN_FACTOR)) + (_x + x))) + _c;
                if (in_idx > in_size) {
                    cout << "_y = " << _y << ", _x = " << _x << ", _c = " << (int)_c << endl;
                    cout << "Input buffer size = " << in_size << ", requested index at " << in_idx << ", " << (in_idx - in_size) << " over." << endl;
                    return -1;
                }
                out[c * ((_y * crop_width) + _x) + _c] = in[in_idx];
            }
        }
    }
    
    return crop_width * crop_height * c;
}
                
int main(int argc, const char * argv[]) {
    vector<string> inputs;
    vector<string> _inputs = glob(OPTIONAL_ARGUMENT(0, SRC));
    string output_dir = OPTIONAL_ARGUMENT(1, DEST);
    
    if (argc > 3) {
        tm _since;
        strptime(argv[3], "%D", &_since);
        time_t since = mktime(&_since);
        for (vector<string>::const_iterator it = _inputs.begin(); it != _inputs.end(); it++) {
            struct stat st = {0};
            int ret = lstat((*it).c_str(), &st);
            if (ret == -1) {
                perror("lstat");
                return EXIT_FAILURE;
            }
            if (st.st_mtimespec.tv_sec >= since) {
                inputs.push_back(*it);
            }
        }
    } else {
        inputs = _inputs;
    }
    
    int target_size = 124;
    int lim = 5000;
    
    jpeg_error_mgr       jerr;
    int done = 0;
    if (_inputs.size() == 0) {
        cout << "No files!" << endl;
    }
    for (vector<string>::const_iterator it = _inputs.begin(); it != _inputs.end() && done < lim; it++, done++) {
        TIMER_START("Image processing");
        FILE * file = fopen((*it).c_str(), "r");
        cout << "Opened " << (*it) << endl;
        
        jpeg_decompress_struct colour_cinfo;
        colour_cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&colour_cinfo);
        
        JSAMPLE* colour_buf = NULL;
        long colour_buf_size = decompress(file, colour_cinfo, colour_buf);
        jpeg_destroy_decompress(&colour_cinfo);
        
        
        JSAMPLE* cropped_buf = NULL;
        long cropped_buf_size = smart_crop(colour_buf, colour_buf_size,
                                           cropped_buf, colour_cinfo, target_size);
        
        free(colour_buf);
        
        if (cropped_buf_size < 0){
            cout << "Cropping failed!" << endl;
            free(cropped_buf);            
            fclose(file);
            continue;
        }
        
        struct jpeg_compress_struct oinfo;
        oinfo.err = jpeg_std_error(&jerr);
        
        //cout << "Writing to " << (*it + ".cropped.jpg") << endl;
        
        char str[12];
        sprintf(str, "%d", done);
        FILE * ofile = fopen((output_dir + "/" + str + ".jpg").c_str(), "w");
        jpeg_create_compress(&oinfo);
        jpeg_stdio_dest(&oinfo, ofile);

        //target_size = colour_cinfo.image_width / (IN_FACTOR * 2);
        oinfo.image_width      = target_size;
        oinfo.image_height     = target_size;
        oinfo.input_components = colour_cinfo.output_components;
        oinfo.in_color_space   = colour_cinfo.out_color_space;
        
        jpeg_set_defaults(&oinfo);
        jpeg_set_quality(&oinfo, JPEG_QUALITY, true);
        jpeg_start_compress(&oinfo, true);
        
        JSAMPROW row_pointer;
        while (oinfo.next_scanline < oinfo.image_height) {
            row_pointer = (JSAMPROW) &cropped_buf[oinfo.next_scanline * oinfo.image_width * oinfo.input_components];
            jpeg_write_scanlines(&oinfo, &row_pointer, 1);
        }
        
        //  TODO: If this next line fails, our outpath may not exist. Verify.
        jpeg_finish_compress(&oinfo);
        jpeg_destroy_compress(&oinfo);
        
        TIMER_END;
        free(cropped_buf);            
        fclose(file);
        fclose(ofile);
    }

    return 0;
}
