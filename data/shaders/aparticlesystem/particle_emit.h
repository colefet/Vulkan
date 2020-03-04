#include "particle_common.h"

layout (binding = 0) uniform UBOEmitter
{
	uint emitCount;
	uint particleLifeMax;
	float randomSeed;
	float padding2;
} uboEmitter;


float rand(vec2 co){return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);}

vec3 GetEmitPosition()
{
    //point
    return vec3(0,0,0);
    //ring
	//float2 circle_point = random_point_on_unit_circle();
	//return float3(0, circle_point * particle_globals._emission_shape_radius); 
}

//Velocity Type:Cone 
//theta in shperical coordinate system
float GetCircleRadian(float particle_random)
{
    float value =2;
    return rand(vec2(uboEmitter.randomSeed, particle_random ))*PI*value;
}
//phi in shperical coordinate system
float GetConeRadian(float particle_random)
{
    float value = 1.0/6.0;
    return rand(vec2(uboEmitter.randomSeed+0.123251,particle_random+0.054385))*PI*value;
}
vec3 GetEmitVelocity(float particle_random)
{
    //cone
    float radian = GetCircleRadian(particle_random);
    float cone_radian = GetConeRadian(particle_random);

    vec3 vel = vec3(
        sin(cone_radian)*sin(radian),
        cos(cone_radian),
        sin(cone_radian)*cos(radian));
    return vel;
}
