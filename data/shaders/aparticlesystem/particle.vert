#version 450

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec4 vParam0;//x:gradient U y:scale

layout (binding = 2) uniform UBO
{
	mat4 model;
	mat4 view;
  mat4 projection;
} SceneMatrices;

layout (location = 0) out vec4 fColor;
layout (location = 1) out float fGradientUV;
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};

void main () 
{
  gl_PointSize = 8.0*vParam0.y;
  gl_Position = SceneMatrices.projection * SceneMatrices.view * SceneMatrices.model * vec4(vPos.xyz, 1.0);

  fColor = vec4(0.035);
  fGradientUV = vParam0.x;
}