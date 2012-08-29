//
//  main.cpp
//  SmartCrop
//
//  Created by Peter Sobot on 8/26/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <iostream>
#include <string>
#include <glob.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include "jpeglib.h" // TODO: This is included improperly.

#define OPTIONAL_ARGUMENT(pos, name) (argc > (pos+1) ? argv[pos+1] : DEFAULT_##name)

#define DEFAULT_SRC "/Volumes/Fry HD/Pictures/iPhoto Library/Modified/201*/*/*.jpg"
#define DEFAULT_DEST "/Volumes/why/www.petersobot.com/source/images/thumbs/"

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
#define IN_FACTOR 2  // Power of 2, initial scale down factor.
#define OUT_FACTOR 1

using namespace std;

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
        unsigned char *rowp[1];
        rowp[0] = (JSAMPLE *) buf + row_stride_c * cinfo.output_scanline;
        jpeg_read_scanlines(&cinfo, rowp, 1);
    }
    
    jpeg_finish_decompress(&cinfo);
    
    return cinfo.output_width * cinfo.output_height * cinfo.output_components;
}

//  Determine the entropy of a cropped portion of the buffer.
double entropy(JSAMPLE * &in, jpeg_decompress_struct &cinfo, int x, int y, int width, int height) {
    return 0.0f;
}

long smart_crop(JSAMPLE * &in, long in_size, JSAMPLE * &out, jpeg_decompress_struct &cinfo, int target_size) {
    int x = 0;
    int y = 0;
    int width = cinfo.image_width / IN_FACTOR;
    int height = cinfo.image_height / IN_FACTOR;
    int slice_length = 16;
    int c = cinfo.output_components;
    
    target_size *= OUT_FACTOR;
    int crop_width = target_size;
    int crop_height = target_size;
    /*
    while ((width - x) > target_size) {
        int slice_width = min(width - x - crop_width, slice_length);
        if (entropy(in, cinfo, x, 0, slice_width, cinfo.image_height) <
            entropy(in, cinfo, width - slice_width, 0, slice_width, cinfo.image_height)) {
            x += slice_width;
        } else {
            width -= slice_width;
        }
    }
                
    while ((height - y) > crop_height) {
        int slice_height = min(height - y - crop_height, slice_length);
        if (entropy(in, cinfo, 0, y, cinfo.image_width, slice_height) < 
            entropy(in, cinfo, 0, height - slice_height, cinfo.image_width, slice_height)) {
            y += slice_height;
        } else {
            height -= slice_height;
        }
    }*/
    
    crop_width = crop_height = width/2;
    
    out = (JSAMPLE*) malloc(crop_width * crop_height * c); 
    
    for (int _y = 0; _y < crop_height; _y++) {
        for (int _x = 0; _x < crop_width; _x++) {
            for (char _c = 0; _c < c; _c++) {
                int in_idx = ((_y + y) * cinfo.image_width) + (_x + x) + _c;
                if (in_idx > in_size) {
                    cout << "_y = " << _y << ", _x = " << _x << ", _c = " << (int)_c << endl;
                    cout << "Input buffer size = " << in_size << ", requested index at " << in_idx << ", " << (in_idx - in_size) << " over." << endl;
                    return -1;
                }
                out[(_y * crop_width) + _x + _c] = in[in_idx];   
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
    
    int start_num = glob(output_dir + "*.jpg").size();
    int lim = 50;
    
    jpeg_error_mgr       jerr;
    for (vector<string>::const_iterator it = _inputs.begin(); it != _inputs.end(); it++) {


        FILE * file = fopen((*it).c_str(), "r");
        
        jpeg_decompress_struct colour_cinfo;
        colour_cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&colour_cinfo);
        
        JSAMPLE* colour_buf = NULL;
        long colour_buf_size = decompress(file, colour_cinfo, colour_buf);
        cout << "Colour buffer size: " << colour_buf_size << endl;
        
        
        TIMER_START("Image processing");
        
        JSAMPLE* cropped_buf = NULL;
        long cropped_buf_size = smart_crop(colour_buf, colour_buf_size,
                                           cropped_buf, colour_cinfo, target_size);
        
        if (cropped_buf_size < 0){
            cout << "Cropping failed!" << endl;
            return -1;
        }
        cout << "Cropped buffer size: " << cropped_buf_size << endl;
        
        struct jpeg_compress_struct oinfo;
        oinfo.err = jpeg_std_error(&jerr);
        
        cout << "Writing to " << (*it + ".cropped.jpg") << endl;
        
        FILE * ofile = fopen((*it + ".cropped.jpg").c_str(), "w");
        jpeg_create_compress(&oinfo);
        jpeg_stdio_dest(&oinfo, ofile);

        target_size = colour_cinfo.image_width / (IN_FACTOR * 2);
        oinfo.image_width      = target_size * OUT_FACTOR;
        oinfo.image_height     = target_size * OUT_FACTOR;
        oinfo.scale_denom      = OUT_FACTOR;
        oinfo.input_components = colour_cinfo.output_components;
        oinfo.in_color_space   = colour_cinfo.out_color_space;
        
        jpeg_set_defaults(&oinfo);
        jpeg_set_quality(&oinfo, 95, true);
        jpeg_start_compress(&oinfo, true);
        
        JSAMPROW row_pointer;
        while (oinfo.next_scanline < oinfo.image_height) {
            row_pointer = (JSAMPROW) &colour_buf[oinfo.next_scanline * oinfo.image_width * oinfo.input_components];
            jpeg_write_scanlines(&oinfo, &row_pointer, 1);
        }
        
        jpeg_finish_compress(&oinfo);
        
        TIMER_END;
        free(cropped_buf);            
        free(colour_buf);
        fclose(file);
        fclose(ofile);
        break;
    }

    return 0;
}

