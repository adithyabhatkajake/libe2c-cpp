#pragma once

#include "salticidae/util.h"
#include "libe2c/config.h"

namespace e2c {

class Logger: public salticidae::Logger {
    public:
    using salticidae :: Logger :: Logger ;
    // The protocol message logger. Logs protocol message <m> as 
    // E2C: <m>
    void prot_log ( const char* format_str , ... ) {
        va_list args ;
        va_start ( args, format_str ) ;
        write ( "E2C:" , is_tty()? salticidae::TTY_COLOR_BLUE : nullptr , 
            format_str , args ) ;
        va_end ( args ) ;
    }
} ;

// A global Logger Class
extern Logger logger ;

/*
 * Logging can be enabled in source code, by defining the compiler flags:
 * E2C_{TRACE, DEBUG, INFO, WARN, ERROR}
 */


}
