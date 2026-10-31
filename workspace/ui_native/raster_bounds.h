#ifndef LUMA_RASTER_BOUNDS_H
#define LUMA_RASTER_BOUNDS_H

#include <limits.h>
#include <stdint.h>

/* Integer scissor conversion shared by retained damage and scene replay.
 * Saturation also avoids undefined float-to-int conversions for invalid or
 * unbounded layout coordinates. No platform math-library ABI is required. */
static int luma_raster_floor(float value) {
    if (value != value) return 0;
    if (value <= (float)INT_MIN) return INT_MIN;
    if (value >= (float)INT_MAX) return INT_MAX;
    int result = (int)value;
    return value < (float)result ? result - 1 : result;
}

static int luma_raster_ceil(float value) {
    if (value != value) return 0;
    if (value <= (float)INT_MIN) return INT_MIN;
    if (value >= (float)INT_MAX) return INT_MAX;
    int result = (int)value;
    return value > (float)result ? result + 1 : result;
}

static int luma_raster_extent(int begin, int end) {
    int64_t extent = (int64_t)end - (int64_t)begin;
    return extent <= 0 ? 0 : extent > INT_MAX ? INT_MAX : (int)extent;
}

#endif
