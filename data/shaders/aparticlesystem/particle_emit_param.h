layout (binding = 0) uniform UBOEmitter
{
	uint emitCount;
	uint particleLifeMax;
	float randomSeed;
	float padding2;
} uboEmitter;

//Hot Update Rigion
float g_circle_radian =2.0;
float g_cone_radian = 1.0/6.0;