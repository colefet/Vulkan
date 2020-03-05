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
//~Hot Update Rigion
vec3 GetEmitPosition(float particle_random)
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
    return rand(vec2(uboEmitter.randomSeed, particle_random ))*PI*g_circle_radian;
}
//phi in shperical coordinate system
float GetConeRadian(float particle_random)
{
    float value = 1.0/6.0;
    return rand(vec2(uboEmitter.randomSeed+0.123251,particle_random+0.054385))*PI*g_cone_radian;
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
