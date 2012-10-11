#include "timeme.h"
#include <iostream>

TimeMe::TimeMe( const char* description, bool long_output )
    : description(description)
    , long_output(long_output)
    , ended(false)
    {
    gettimeofday(&__start, NULL);
}

void TimeMe::End( bool _long_output ) {
    if ( ended ) return;
    timeval __end;
    gettimeofday(&__end, NULL);
    
    double mtime = (( (__end.tv_sec * 1000000) + __end.tv_usec ) -
                    ( __start.tv_usec +  __start.tv_sec * 1000000 )) / 1000.0f;
    
    if ( _long_output )
        std::cerr << description << " took " << mtime << " ms." << std::endl;
    else
        std::cerr << mtime << "ms";
    ended = true;
}

void TimeMe::End() {
    End( long_output );
}

TimeMe::~TimeMe() {
    End();
}
