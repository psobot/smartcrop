//
//  smartcrop.cpp
//  SmartCrop
//
//  Created by Peter Sobot on 8/26/12.
//  Copyright (c) 2012 Peter Sobot.
//  Licensed under MIT.
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
    cerr << __timer_desc << " took " << __mtime << " ms." << endl; \
    __timer_desc = "";

#define TIMER_END_SHORT \
    timeval __end; \
    gettimeofday(&__end, NULL); \
    __seconds  = __end.tv_sec  - __start.tv_sec; \
    __useconds = __end.tv_usec - __start.tv_usec; \
    __mtime = ((__seconds) * 1000 + __useconds/1000.0) + 0.5; \
    cerr << __mtime << "ms"; \
    __timer_desc = "";


#define MAXPATHLEN 512
#define STRIDE 10
#define DEFAULT_IN_FACTOR 8  // Power of 2, initial scale down factor.
#define DEFAULT_JPEG_QUALITY 75

using namespace std;

//  Credit goes to: http://stackoverflow.com/questions/8401777/simple-glob-in-c-on-unix-system
//  by Piti Ongmongkolkul
inline vector<string> glob(const string& pat){
    glob_t glob_result;
    glob( pat.c_str(),GLOB_TILDE,NULL,&glob_result );
    vector<string> ret;
    for(unsigned int i = 0; i < glob_result.gl_pathc; ++i){
        ret.push_back(string(glob_result.gl_pathv[i]));
    }
    globfree(&glob_result);
    return ret;
}

//  JPEG-decompress FILE into buf.
//  TODO: Take into account rotation bits. (coefficients, really)
long decompress(FILE* file, jpeg_decompress_struct &cinfo, JSAMPLE * &buf, int in_factor) {
    fseek(file, 0, 0);
    
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, true);

    cinfo.scale_num = 1;
    cinfo.scale_denom = in_factor;
    
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
double entropy(JSAMPLE * &in, jpeg_decompress_struct &cinfo, int x, int y, int width, int height, int in_factor) {
#define HISTOGRAM_RESOLUTION 16
    long histogram[HISTOGRAM_RESOLUTION];
    for (int i = 0; i < HISTOGRAM_RESOLUTION; i++) histogram[i] = 0;

    static const float MAX_DATA_VALUE = (float) ( 1 << sizeof( JSAMPLE ) * 8 );

#ifdef DEBUG
    cout << "MAX DATA VALUE IS " << MAX_DATA_VALUE << endl;
    printf("Checking entropy on square from (%4d, %4d) to (%4d, %4d) = ", x, y, x + width, y + height);
#endif

    int incs = 0;
    for (int _y = y; _y < y + height; _y++) {
        for (int _x = x; _x < x + width; _x++) {
            float val = 0;
            if (cinfo.out_color_components == 3) {
                int idx = ((_y * (cinfo.image_width / in_factor)) + _x) * cinfo.out_color_components;
                val = (0.299f * in[idx]) + (0.587f * in[idx+1]) + (0.114 * in[idx+2]);
            } else {
                for (int _c = 0; _c < cinfo.out_color_components; _c++) {
                    int idx = ((_y * (cinfo.image_width / in_factor)) + _x) * cinfo.out_color_components + _c;
                    val += in[idx];
                }
            }
            incs++;
            histogram[(int) (( val / MAX_DATA_VALUE ) * HISTOGRAM_RESOLUTION)]++;
        }
    }
    
    int area = width * height;
    double e = 0.0f;
    double p = 0;
#ifdef DEBUG
        cerr << "Histogram = ";
#endif
    for (int i = 0; i < HISTOGRAM_RESOLUTION; i++) {
#ifdef DEBUG
        cerr << histogram[i];
#endif
        p = (double) histogram[i] / (double) area;
        if (p > 0) {
            e += p * log2(p);
        }
    }

#ifdef DEBUG
    cerr << endl;
    printf("%f\n", -1.0f * e);
#endif

    return -1.0f * e;
}

long smart_crop(JSAMPLE * &in, long in_size, JSAMPLE * &out,
                         jpeg_decompress_struct &cinfo, int _w, int _h, int in_factor) {
    int x = 0;
    int y = 0;
    int width = cinfo.image_width / in_factor;
    int height = cinfo.image_height / in_factor;
    
    int slice_length = 16;
    int c = cinfo.output_components;
    
    int crop_width = _w;
    int crop_height = _h;

    while ((width - x) > crop_width) {
        int slice_width = min(width - x - crop_width, slice_length);
        if (entropy(in, cinfo, x, 0, slice_width, cinfo.image_height / in_factor, in_factor) <
            entropy(in, cinfo, width - slice_width, 0, slice_width, cinfo.image_height / in_factor, in_factor)) {
            x += slice_width;
        } else {
            width -= slice_width;
        }
    }

    while ((height - y) > crop_height) {
        int slice_height = min(height - y - crop_height, slice_length);
        if (entropy(in, cinfo, 0, y, cinfo.image_width / in_factor, slice_height, in_factor) < 
            entropy(in, cinfo, 0, height - slice_height, cinfo.image_width / in_factor, slice_height, in_factor)) {
            y += slice_height;
        } else {
            height -= slice_height;
        }
    }

    out = (JSAMPLE*) malloc(crop_width * crop_height * c); 
    
    for (int _y = 0; _y < crop_height; _y++) {
        for (int _x = 0; _x < crop_width; _x++) {
            for (char _c = 0; _c < c; _c++) {
                int in_idx = (c * (((_y + y) * (cinfo.image_width / in_factor)) + (_x + x))) + _c;
                if (in_idx > in_size) return -1;
                out[c * ((_y * crop_width) + _x) + _c] = in[in_idx];
            }
        }
    }
    
    return crop_width * crop_height * c;
}

int print_usage( const char* my_name ) {
    cerr << "SmartCrop by Peter Sobot" << endl;
    cerr << "------------------------" << endl;
    cerr << "Usage: " << my_name << " {options} input_file input_file ..." << endl;
    cerr << "       -o: output directory. Default output directory is the current directory." << endl;
    cerr << "       -d: date limit.       Default date from is forever ago." << endl;
    cerr << "       -q: jpeg quality.     Default is " << DEFAULT_JPEG_QUALITY << "." << endl;
    cerr << endl;
    cerr << "       -w: width  (in pixels) of crop area. (Default 124px)" << endl;
    cerr << "       -h: height (in pixels) of crop area. (Default 124px)" << endl;
    return EXIT_FAILURE;
}
                
int main(int argc, const char * argv[]) {
    char _tmp[MAXPATHLEN];
    string output_dir = ( getcwd( _tmp, MAXPATHLEN ) ? string( _tmp ) : string("") );
    string date_from;
    int jpeg_quality = DEFAULT_JPEG_QUALITY;
    int height = 124;
    int width = 124;

    char c;
    while ((c = getopt(argc, (char **) argv, "o:d:q:w:h:")) != -1) {
        switch (c) {
            case 'o':
                output_dir = string( optarg );
                break;
            case 'd':
                date_from = string( optarg );
                break;
            case 'q':
                jpeg_quality = atoi( optarg );
                if ( jpeg_quality < 1 || jpeg_quality > 100 ) {
                    cerr << "JPEG quality level (" << jpeg_quality << ") out of range." << endl;
                    jpeg_quality = jpeg_quality > 100 ? 100 : 1;
                    cerr << "Snapping quality to " << jpeg_quality << "." << endl;
                }
                break;
            case 'w':
                width = atoi( optarg );
                break;
            case 'h':
                height = atoi( optarg );
                break;
            default:
                return print_usage( argv[0] );
        }
    }

    vector<string> inputs;
    vector<string> _inputs;

    for ( int i = optind; i < argc; i++ )
        _inputs.push_back( argv[i] );

    if ( !date_from.empty() ) {
        cerr << "Limiting input images since date " << date_from << endl;

        tm _since;
        strptime( date_from.c_str(), "%D", &_since );
        time_t since = mktime( &_since );
        for ( vector<string>::const_iterator it = _inputs.begin();
              it != _inputs.end(); it++ ) {
            struct stat st = {0};
            int ret = lstat( (*it).c_str(), &st );
            if ( ret == -1 ) {
                perror( "lstat" );
                return EXIT_FAILURE;
            }
            if ( st.st_mtimespec.tv_sec >= since ) {
                inputs.push_back(*it);
            }
        }
    } else {
        inputs = _inputs;
    }
    
    int lim = 5000;
    
    jpeg_error_mgr jerr;
    int done = 0;

    if ( _inputs.size() == 0 ) {
        return print_usage( argv[0] );
    } else {
        cerr << "Thumbnailing " << _inputs.size() << " images..." << endl;
    }

    cerr << "Saving thumbnails to: " << output_dir << endl;
  
    for (vector<string>::const_iterator it = _inputs.begin();
              it != _inputs.end() && done < lim; it++, done++) {
        TIMER_START("Image processing");
        FILE * file = fopen((*it).c_str(), "r");
        cerr << "Processing " << (*it) << "...";
        
        jpeg_decompress_struct colour_cinfo;
        colour_cinfo.err = jpeg_std_error(&jerr);

        JSAMPLE* colour_buf = NULL;
        JSAMPLE* cropped_buf = NULL;
        long cropped_buf_size = -1;
        long colour_buf_size  = -1;
        
        for ( int in_factor = DEFAULT_IN_FACTOR;
              cropped_buf_size < 0 && in_factor > 0;
              in_factor /= 2 ) {
            jpeg_create_decompress(&colour_cinfo);
            
            colour_buf_size = decompress(file, colour_cinfo, colour_buf, in_factor);
            jpeg_destroy_decompress(&colour_cinfo);
            
            cropped_buf_size = smart_crop(colour_buf, colour_buf_size,
                                          cropped_buf, colour_cinfo, width, height, in_factor);
            free(colour_buf);
        }

        if ( cropped_buf_size < 0 ) {
            cerr << "Cropping failed!" << endl;
            free(cropped_buf);            
            fclose(file);
            continue;
        }
        
        struct jpeg_compress_struct oinfo;
        oinfo.err = jpeg_std_error(&jerr);
        
        char str[12];
        sprintf(str, "%d", done);
        FILE * ofile = fopen((output_dir + "/" + str + ".jpg").c_str(), "w");
        
        if ( !ofile ) {
            //  Early out of the entire program - we can't write to the output dir!
            free( cropped_buf );
            fclose( file );

            cerr << "Could not open output file for writing!" << endl;
            cerr << "  (file path was: " << (output_dir + "/" + str + ".jpg") << ")" << endl;
            return EXIT_FAILURE;
        }
        
        jpeg_create_compress(&oinfo);
        jpeg_stdio_dest(&oinfo, ofile);

        oinfo.image_width      = width;
        oinfo.image_height     = height;
        oinfo.input_components = colour_cinfo.output_components;
        oinfo.in_color_space   = colour_cinfo.out_color_space;
        
        jpeg_set_defaults(&oinfo);
        jpeg_set_quality(&oinfo, jpeg_quality, true);
        jpeg_start_compress(&oinfo, true);
        
        JSAMPROW row_pointer;
        while (oinfo.next_scanline < oinfo.image_height) {
            row_pointer = (JSAMPROW) &cropped_buf[oinfo.next_scanline *
                                                  oinfo.image_width   *
                                                  oinfo.input_components];
            jpeg_write_scanlines(&oinfo, &row_pointer, 1);
        }
        
        jpeg_finish_compress(&oinfo);
        jpeg_destroy_compress(&oinfo);
        
        cerr << " ";
        TIMER_END_SHORT;
        cerr << "." << endl;

        free(cropped_buf);            
        fclose(file);
        fclose(ofile);
    }

    return 0;
}
