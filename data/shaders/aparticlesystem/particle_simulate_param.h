#include "particle_simulate_declare.h"
//Hot Update Rigion
float GetParicleScale(uint index)
{
	return  mix(3.0,1.0,GetParticleAge(index));
}
