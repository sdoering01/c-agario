typedef struct vec2_t {
    float x;
    float y;
} vec2_t;

vec2_t vec2_add(vec2_t v1, vec2_t v2);
vec2_t vec2_sub(vec2_t v1, vec2_t v2);
vec2_t vec2_scale(vec2_t v, float factor);
float vec2_abs(vec2_t v);
vec2_t vec2_norm(vec2_t v);
