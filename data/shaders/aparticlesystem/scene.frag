#version 450

//layout (binding = 1) uniform sampler2D shadowMap;

layout (location = 0) in vec3 fNormal;
layout (location = 1) in vec3 fColor;
layout (location = 2) in vec3 fLightVec;

layout (location = 0) out vec4 FRAG_COLOR;

#define ambient 0.2

void main() 
{	
	vec3 N = normalize(fNormal);
	vec3 L = normalize(fLightVec);
	vec3 diffuse = max(dot(N, L), ambient) * fColor;

	FRAG_COLOR = vec4(diffuse, 1.0);
}
