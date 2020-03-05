layout (binding = 0) uniform UBOSimulate
{
	float deltaT;
	float destX;
	float destY;
	int padding0;
} uboSimulate;

//Particle storage buffer
layout(std140, binding = 1) buffer SSBOParticle 
{
    Particle particles[ ];
}ssboParticle;
layout(std140, binding = 2) buffer SSBODead
{
    uvec4 list[ ];
}ssboDead;
layout(std140, binding = 3) buffer SSBOAlive
{
    uvec4 list[ ];
}ssboAlive;
layout(std140, binding = 4) buffer SSBOAliveAfterSimulate
{
    uvec4 list[ ];
}ssboAliveAfterSimulate;
layout(std140, binding = 5) buffer SSBOCounter
{
    uint emitCount;
    uint deadCount;
    uint aliveCount;
    uint aliveCountAfterSimulate;
}ssboCounter;

//Normalized particle lifetime
float GetParticleAge(uint index)
{
	return 1.0-float(ssboParticle.particles[index].life)/float(ssboParticle.particles[index].lifeMax);
}
