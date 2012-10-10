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
    
    long __seconds, __useconds;
    double __mtime;

    __seconds  = __end.tv_sec  - __start.tv_sec;
    __useconds = __end.tv_usec - __start.tv_usec;
    __mtime = ((__seconds) * 1000 + __useconds/1000.0) + 0.5;
    
    if ( _long_output )
        std::cerr << description << " took " << __mtime << " ms." << std::endl;
    else
        std::cerr << __mtime << "ms";
    ended = true;
}

void TimeMe::End() {
    End( long_output );
}

TimeMe::~TimeMe() {
    End();
}
