#ifndef _UGEAR_UTIL_H
#define _UGEAR_UTIL_H


#include <math.h>

// simple wrap around for -180 and + 180 
inline double wraparound( double dta )
{
    if ( dta > M_PI ) dta -= M_2PI;
    if ( dta < -M_PI ) dta += M_2PI;
   
    return dta;
}	


#endif // _UGEAR_UTIL_H
