#version 450

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;
layout (location = 3) in vec3 vColor;


layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	mat4 model;
	vec4 lightPos;
} ubo;

layout (location = 0) out vec3 fNormal;
layout (location = 1) out vec3 fColor;
layout (location = 2) out vec3 fLightVec;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
	gl_Position = ubo.projection * ubo.view * ubo.model * vec4(vPos.xyz, 1.0);

	fColor = vColor;
	//not necessary for model translate.
    //outNormal = mat3(ubo.model) * vNormal;
	//outLightVec = mat3(ubo.model) * normalize(ubo.lightPos.xyz - vPos);
	fNormal = vNormal;
	fLightVec = normalize(ubo.lightPos.xyz - vPos);
}

