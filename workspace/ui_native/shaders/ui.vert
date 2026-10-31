#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 0) out vec4 color;
layout(location = 1) out vec2 uv;
layout(push_constant) uniform Draw {
    vec4 transform; // NDC scale.xy, logical translation.zw
    vec4 tint;
    vec4 raster; // actual physical/logical scale.xy, or zero for textured geometry
} draw;

void main() {
    vec2 position = in_position + draw.transform.zw;
    uv = in_uv;
    if (draw.raster.x > 0.0) {
        // Snap each stroke's centerline, not each extruded vertex. The AA
        // fringe remains exactly one physical pixel at fractional offsets.
        vec2 anchor = in_uv + draw.transform.zw;
        vec2 delta = (floor(anchor * draw.raster.xy) + 0.5) / draw.raster.xy - anchor;
        if (in_uv.x > -1e20) position.x += delta.x;
        if (in_uv.y > -1e20) position.y += delta.y;
        uv = vec2(0.5);
    }
    gl_Position = vec4(position * draw.transform.xy - 1.0, 0.0, 1.0);
    color = in_color * draw.tint;
}
