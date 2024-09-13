#include <math.h>

#include "geometry.h"

vec2_t vec2_add(vec2_t v1, vec2_t v2) {
    return (vec2_t){ v1.x + v2.x, v1.y + v2.y };
}

vec2_t vec2_sub(vec2_t v1, vec2_t v2) {
    return (vec2_t){ v1.x - v2.x, v1.y - v2.y };
}

vec2_t vec2_scale(vec2_t v, float factor) {
    return (vec2_t){ factor * v.x, factor * v.y };
}

float vec2_abs(vec2_t v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

vec2_t vec2_norm(vec2_t v) {
    float length = vec2_abs(v);
    if (length != 0) {
        return vec2_scale(v, 1 / length);
    } else {
        // We cannot normalize the vector, so just return it as is.
        return v;
    }
}
