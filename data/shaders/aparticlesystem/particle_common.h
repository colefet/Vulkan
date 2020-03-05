#define PI 3.1415926535898

struct Particle
{
	vec3 pos;
    float lifeMax;
	vec3 vel;
    float life;
	vec4 gradientUV;
};
void SetVec4Element(inout uvec4 vec, uint mod_value, uint value) {
    if(mod_value==0){
        vec.x = value;
    }else if(mod_value==1){
        vec.y = value;
    }else if(mod_value==2){
        vec.z = value;
    }else if(mod_value==3){
        vec.w = value;
    }
}

uint GetVec4Element(uvec4 vec, uint mod_value) {
    if(mod_value==0){
        return vec.x;
    }else if(mod_value==1){
        return vec.y;
    }else if(mod_value==2){
        return vec.z;
    }else if(mod_value==3){
        return vec.w;
    }
}

float rand(vec2 co){return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);}

