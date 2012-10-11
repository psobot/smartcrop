//
//  smartcrop.cpp
//  SmartCrop
//
//  Created by Peter Sobot on 8/26/12.
//  Copyright (c) 2012 Peter Sobot.
//  Licensed under MIT.
//

#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include <math.h>
#include <assert.h>
#include <Magick++.h>
#include <vector>
#include "timeme.h"

#define MAXPATHLEN 512
#define STRIDE 10
#define DEFAULT_IN_FACTOR 8  // Power of 2, initial scale down factor.
#define DEFAULT_JPEG_QUALITY 75
#define IN_FACTOR 8

using namespace std;

//  Determine the entropy of a cropped portion of the buffer.
double entropy(Magick::Image image, int x, int y, int width, int height) {
#define HISTOGRAM_RESOLUTION 16 // TODO: Not a define
    long histogram[HISTOGRAM_RESOLUTION];
    for (int i = 0; i < HISTOGRAM_RESOLUTION; i++) histogram[i] = 0;
    
    // Why not just use Magick's built in histogram and quantizer?
    // Turns out, that's absurdly slow.
    Magick::Pixels _all(image);
    Magick::PixelPacket* pixels = _all.get( x, y, width, height );

    static const float MAX_DATA_VALUE = ( 1 << ( sizeof( Magick::PixelPacket ) * 2 ) );

#ifdef DEBUG
    cout << "MAX DATA VALUE IS " << MAX_DATA_VALUE << endl;
    printf("Checking entropy on square from (%4d, %4d) to (%4d, %4d) = ", x, y, x + width, y + height);
#endif

    for (int _y = 0; _y < height; _y++) {
        for (int _x = 0; _x < width; _x++) {
            Magick::PixelPacket* pixel = (pixels + ((_y * width) + _x));
            float val = (0.299f * (char)pixel->red) + (0.587f * pixel->green) + (0.114 * pixel->blue);
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

Magick::Geometry smart_crop(Magick::Image image, int _w, int _h) {
    int x = 0;
    int y = 0;
    int width = image.columns();
    int height = image.rows();

    int _width = image.columns();
    int _height = image.rows();

    int slice_length = 16;
    
    int crop_width = _w;
    int crop_height = _h;

    while ((width - x) > crop_width) {
        int slice_width = min(width - x - crop_width, slice_length);
        if (entropy(image, x, 0, slice_width, _height) <
            entropy(image, width - slice_width, 0, slice_width, _height)) {
            x += slice_width;
        } else {
            width -= slice_width;
        }
    }
                
    while ((height - y) > crop_height) {
        int slice_height = min(height - y - crop_height, slice_length);
        if (entropy(image, 0, y, _width, slice_height) < 
            entropy(image, 0, height - slice_height, _width, slice_height)) {
            y += slice_height;
        } else {
            height -= slice_height;
        }
    }
    return Magick::Geometry(crop_width, crop_height, x, y);
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
    int done = 0;

    if ( _inputs.size() == 0 ) {
        return print_usage( argv[0] );
    } else {
        cerr << "Thumbnailing " << _inputs.size() << " images..." << endl;
    }

    cerr << "Saving thumbnails to: " << output_dir << endl;
  
    for (vector<string>::const_iterator it = _inputs.begin();
              it != _inputs.end() && done < lim; it++, done++) {
        TimeMe timer("Overall Processing");
        cerr << "Processing " << (*it) << "...";

        Magick::Image input;
        input.read(*it);

        //  Auto-orient the image:
        switch (input.orientation()) {
          case Magick::TopRightOrientation:     input.flip();                       break;
          case Magick::BottomRightOrientation:  input.rotate(180.0);    break;
          case Magick::BottomLeftOrientation:   input.flip();           break;
          case Magick::LeftTopOrientation:      input.flip(); input.rotate(90.0);      break;
          case Magick::RightTopOrientation:     input.rotate(90.0);     break;
          case Magick::RightBottomOrientation:  input.flop(); input.rotate(270.0);     break;
          case Magick::LeftBottomOrientation:   input.rotate(270.0);    break;
          default: break;
        }
        
        {
        //  We want a good-sized search space for our sliding window.
        //  Make this scale to 4x the width and height of our target.

        if ( width  < input.columns() / IN_FACTOR &&
             height < input.rows()    / IN_FACTOR )
             input.scale( Magick::Geometry( input.columns() / IN_FACTOR, input.rows() / IN_FACTOR ) );
        }

        //  Strip metadata and exif tags.
        input.strip();

        //  Crop here!
        input.crop( smart_crop(input, width, height) );

        // Set quality here        
        input.quality( jpeg_quality );

        // Encode here
        stringstream o; string d; o << done; d = o.str();
        input.write(output_dir + "/" + d + ".jpg");

        cerr << " ";
        timer.End( false );
        cerr << "." << endl;
    }

    return 0;
}
