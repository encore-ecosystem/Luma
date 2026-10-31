#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "sdl_api.h"

/* IEC 61966-2-1 transfer functions for UI color authoring. */
float luma_color_decode(float value) {
    return value <= 0.04045f ? value / 12.92f : powf((value + 0.055f) / 1.055f, 2.4f);
}
float luma_color_encode(float value) {
    return value <= 0.0031308f ? value * 12.92f : 1.055f * powf(value, 1.0f / 2.4f) - 0.055f;
}
float luma_color_number(encore_str value) {
    size_t length = value.object ? value.object->len : 0;
    if (!length) return NAN;
    char *text = malloc(length + 1);
    if (text) { memcpy(text, value.object->data, length); text[length] = '\0'; }
    if (!text) return NAN;
    char *end = NULL;
    float parsed = strtof(text, &end);
    bool valid = end != text && end == text + length && isfinite(parsed);
    free(text);
    return valid ? parsed : NAN;
}
