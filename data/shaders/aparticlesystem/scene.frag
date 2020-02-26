#version 450

//layout (binding = 1) uniform sampler2D shadowMap;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inLightVec;

layout (location = 0) out vec4 outFragColor;

#define ambient 0.1

void main() 
{	
	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 diffuse = max(dot(N, L), ambient) * inColor * 2.0;

	outFragColor = vec4(diffuse, 1.0);
}
