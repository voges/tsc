#ifndef TSC_VERSION_H
#define TSC_VERSION_H

#define TSC_VERSION_MAJOR 2
#define TSC_VERSION_MINOR 0
#define TSC_VERSION_PATCH 0
#define TSC_VERSION "2.0.0"

#include "timestamp.h"
#include "gitrevision.h"

#define TSC_UTCTIMESTAMP _CMAKE_UTCTIME_
#define TSC_GITREVISION_LONG _CMAKE_GITREVISION_LONG_
#define TSC_GITREVISION_SHORT _CMAKE_GITREVISION_SHORT_

#define TSC_BUILD_YEAR \
    ( \
        (TSC_UTCTIMESTAMP[ 0] - '0') * 1000 +\
        (TSC_UTCTIMESTAMP[ 1] - '0') *  100 +\
        (TSC_UTCTIMESTAMP[ 2] - '0') *   10 +\
        (TSC_UTCTIMESTAMP[ 3] - '0')\
    )

#endif // TSC_VERSION_H

