#version 450

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec4 vGradientUV;

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
  gl_PointSize = 8.0;
  gl_Position = vec4(vPos.xy, 1.0, 1.0);
  //gl_Position = SceneMatrices.projection * SceneMatrices.view * SceneMatrices.model * vPos;

  fColor = vec4(0.035);
  fGradientUV = vGradientUV.x;
}