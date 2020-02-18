#version 450

layout (binding = 0) uniform sampler2D samplerColorMap;
layout (binding = 1) uniform sampler2D samplerGradientRamp;

layout (location = 0) in vec4 fColor;
layout (location = 1) in float fGradientPos;

layout (location = 0) out vec4 FRAG_COLOR;

void main () 
{
	vec3 color = texture(samplerGradientRamp, vec2(fGradientPos, 0.0)).rgb;
	FRAG_COLOR.rgb = texture(samplerColorMap, gl_PointCoord).rgb * color;
}
