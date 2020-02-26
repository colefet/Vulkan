#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;


layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	mat4 model;
	vec4 lightPos;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outLightVec;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
	gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPos.xyz, 1.0);

	outColor = inColor;
	outNormal = inNormal;
    outNormal = mat3(ubo.model) * inNormal;
	outLightVec = normalize(ubo.lightPos.xyz - inPos);
}

