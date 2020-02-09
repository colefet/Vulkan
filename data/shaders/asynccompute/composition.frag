#version 450

layout (binding = 0) uniform sampler2D samplerposition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerSSAO;
layout (binding = 4) uniform sampler2D samplerSSAOBlur;
layout (binding = 5) uniform sampler2D shadowMap;
layout (binding = 6) uniform sampler2D shadowCoord;
layout (binding = 7) uniform UBO 
{
	mat4 _dummy;
	int ssao;
	int ssaoOnly;
	int ssaoBlur;
} uboParams;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

#define ambient 0.1
float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = ambient;
		}
	}
	return shadow;
}
float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}

void main() 
{
	vec3 fragPos = texture(samplerposition, inUV).rgb;
	vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
	vec4 albedo = texture(samplerAlbedo, inUV);
	 
	float ssao = (uboParams.ssaoBlur == 1) ? texture(samplerSSAOBlur, inUV).r : texture(samplerSSAO, inUV).r;
	
	vec4 shadow_coord = texture(shadowCoord, inUV);
	float shadow = filterPCF(shadow_coord / shadow_coord.w);

	vec3 lightPos = vec3(0.0);
	vec3 L = normalize(lightPos - fragPos);
	float NdotL = max(0.5, dot(normal, L));

	if (uboParams.ssaoOnly == 1)
	{
		outFragColor.rgb = ssao.rrr;
	}
	else
	{
		vec3 baseColor = albedo.rgb * NdotL;
		if (uboParams.ssao == 1)
		{
			outFragColor.rgb = baseColor*ssao.rrr;
		}
		else
		{
			outFragColor.rgb = baseColor;
		}
		outFragColor.rgb*=shadow;
	}
}