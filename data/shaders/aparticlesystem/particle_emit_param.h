//Hot Update Rigion
vec3 GetEmitPosition(float particle_random)
{
	return vec3(0, 0, 0);
}
float g_circle_radian =2.0;
float GetCircleRadian(float particle_random)
{
	return rand(vec2(uboEmitter.randomSeed, particle_random)) * PI * g_circle_radian;
}
float g_cone_radian =1.0/6.0;
float GetConeRadian(float particle_random)
{
	return rand(vec2(uboEmitter.randomSeed+0.123251,particle_random+0.054385))*PI*g_cone_radian;
}
vec3 GetEmitVelocity(float particle_random)
{
	float radian = GetCircleRadian(particle_random);
	float cone_radian = GetConeRadian(particle_random);
	vec3 vel = vec3(
		sin(cone_radian) * sin(radian),
		cos(cone_radian),
		sin(cone_radian) * cos(radian));
	return vel;
}
