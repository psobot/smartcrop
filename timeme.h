#ifndef TIMEME_H
#define TIMEME_H

#include <sys/time.h>

class TimeMe {
  public:
    TimeMe(const char* description, bool long_output = true);
    void End( bool long_output );
    void End( );
    ~TimeMe();

  private:
    const char* description;
    bool long_output;
    bool ended;
    timeval __start;
};

#endif
