/*
* Vulkan Example - Particle system
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <random>
#include <memory>
#include <fstream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"
#include "VulkanModel.hpp"

#define ENABLE_VALIDATION true

#define VERTEX_BUFFER_BIND_ID 0
#if defined(__ANDROID__)
// Lower particle count on Android for performance reasons
#define MAX_PARTICLE_COUNT 128 * 1024
#else
#define MAX_PARTICLE_COUNT 256 * 1024
#endif
#define PI 3.1415926535898f

enum EmitShape {
	ES_POINT,
	ES_RING,
};
class CEmitShape
{
public:
	void SetEmitShape(EmitShape shape) { m_emit_shape = shape; };
	EmitShape GetEmitShape() { return m_emit_shape; };
	virtual std::string GetPositionCode() = 0;
private:
	EmitShape m_emit_shape = ES_POINT;
};

class CEmitShapePoint: public CEmitShape
{
public:
	CEmitShapePoint() { SetEmitShape(ES_POINT); };
	~CEmitShapePoint() {};
	std::string GetPositionCode()
	{
		std::string str;
		str += "vec3 GetEmitPosition(float particle_random)\n\
{\n\
	return vec3(0, 0, 0);\n\
}\n";
		return str;
	}
private:
};
class CEmitShapeRing : public CEmitShape
{
public:
	CEmitShapeRing() { SetEmitShape(ES_RING); };
	~CEmitShapeRing() {};
	std::string GetPositionCode() { return ""; };
private:
	float m_radius=0;
};

enum VelocityType {
	VT_CONE,
	VT_RADIAL,
	VT_SURFACE_NORMAL,
};
class CVelocityType
{
public:
	void SetVelocityType(VelocityType velocity) { m_emit_velocity = velocity; };
	VelocityType GetVelocityType() { return m_emit_velocity; };
	virtual std::string GetVelocityCode() = 0;
private:
	VelocityType m_emit_velocity = VT_CONE;
};

class CVelocityTypeCone : public CVelocityType
{
public:
	CVelocityTypeCone() { SetVelocityType(VT_CONE); };
	~CVelocityTypeCone() {};

	std::string GetRadianCode()
	{
		std::string str;
		str += "float g_circle_radian ="+ m_radian_str +";\n";
		str += "float GetCircleRadian(float particle_random)\n\
{\n\
	return rand(vec2(uboEmitter.randomSeed, particle_random)) * PI * g_circle_radian;\n\
}\n";
		return str;
	}
	std::string GetConeRadianCode()
	{
		std::string str;
		str += "float g_cone_radian =" + m_cone_radian_str + ";\n";
		str += "float GetConeRadian(float particle_random)\n\
{\n\
	return rand(vec2(uboEmitter.randomSeed+0.123251,particle_random+0.054385))*PI*g_cone_radian;\n\
}\n";
		return str;
	}
	std::string GetVelocityCode()
	{
		//todo:transfer to bytecode and interprete
		//std::string str="vec3(sin("+m_cone_radian_code +") * sin(" + m_radian_code + "), cos(" + m_cone_radian_code + "), sin(" + m_cone_radian_code + ")* cos(" + m_radian_code + "))";

		std::string str;
		str += GetRadianCode();
		str += GetConeRadianCode();
		str+= "vec3 GetEmitVelocity(float particle_random)\n\
{\n\
	float radian = GetCircleRadian(particle_random);\n\
	float cone_radian = GetConeRadian(particle_random);\n\
	vec3 vel = vec3(\n\
		sin(cone_radian) * sin(radian),\n\
		cos(cone_radian),\n\
		sin(cone_radian) * cos(radian));\n\
	return vel;\n\
}\n";
		return str;
	}
	void SetRadian(const std::string& str) { m_radian_str = str; }
	const std::string& GetRadian() { return m_radian_str; }
	void SetConeRadian(const std::string& str) { m_cone_radian_str = str; }
	const std::string& GetConeRadian() { return m_cone_radian_str; }
private:
	//result from bytecode interpreter
	float m_radian = 0;
	float m_cone_radian = 0;
	//shader code for baked HLSL
	std::string m_radian_str = "2.0";
	std::string m_cone_radian_str = "1.0/6.0";
};
class CVelocityTypeRadial : public CVelocityType
{
public:
	CVelocityTypeRadial() { SetVelocityType(VT_RADIAL); };
	~CVelocityTypeRadial() {};
	std::string GetVelocityCode() { return ""; };
};

class CParticleScale
{
public:
	void SetParticleScale(const std::string& str) { m_scale_str = str; };
	const std::string& GetParticleScale() { return m_scale_str; };
	std::string GetParticleScaleCode()
	{
		std::string str;
		str += "float g_particle_scale =" + m_scale_str + ";\n";
		str += "float GetParicleScale()\n\
{\n\
	return  g_particle_scale;\n\
}\n";
		return str;
	};
private:
	std::string m_scale_str = "8.0";
};
class CParticleSystem
{
public:
	CParticleSystem()
	{
		InitEmitShape(ES_POINT);
		InitEmitVelocity(VT_CONE);
		m_particle_scale = std::make_unique<CParticleScale>();
	};
	~CParticleSystem(){};
	void Initial(float timer)
	{
		m_age = 0;
	}
	void Destroy(VkDevice device)
	{
	}
	bool InitEmitShape(EmitShape shape)
	{
		switch (shape)
		{
		case ES_POINT: {
			m_emit_shape = std::make_unique<CEmitShapePoint>();
		}break;
		case ES_RING: {
			m_emit_shape = std::make_unique<CEmitShapeRing>();
		}break;
		default:
			return false;
		}
		return true;
	}
	
	bool InitEmitVelocity(VelocityType velocity)
	{
		switch (velocity)
		{
		case VT_CONE: {
			m_velocity_type = std::make_unique<CVelocityTypeCone>();
		}break;
		case VT_RADIAL: {
			m_velocity_type = std::make_unique<CVelocityTypeRadial>();

		}break;
		case VT_SURFACE_NORMAL: {

		}break;
		default:
			return false;
		}
		return true;
	}
	void Reset()
	{
		m_age = 0;
		m_delta_time = 0;
		m_emitted_count = 0;
		m_next_emit_count = 0;
	}
	void Update(float timer)
	{
		m_delta_time += (timer * 1000 );
		m_age += floorf(m_delta_time);
		m_delta_time -= floorf(m_delta_time);

		m_next_emit_count = (m_age / m_emit_interval + 1)*m_emit_count-m_emitted_count;
		m_emitted_count += m_next_emit_count;
	};
	
	const std::string& GetTex() { return m_particle_tex; }
	const std::string& GetGradientTex() { return m_gradient_tex; }

	void SetPos(const glm::vec3& pos) { m_pos = pos; };
	const glm::vec3& GetPos() { return m_pos; };

	void SetRot(const glm::vec3& rot) { m_rot = rot; };
	const glm::vec3& GetRot() { return m_rot; };

	void SetMaxParticleCount(uint32_t num) { m_max_particle_count = ceil(num/4.0f)*4; };
	uint32_t GetMaxParticleCount() { return m_max_particle_count; };
	//uint32_t GetParticleCount() { return m_particle_count; };

	//emission
	CEmitShape* GetEmitShapeObject() { return m_emit_shape.get(); };
	void SetEmitShape(EmitShape shape) { InitEmitShape(shape); };
	EmitShape GetEmitShape() {if (!m_emit_shape) {return ES_POINT;}   return m_emit_shape->GetEmitShape(); };

	CVelocityType* GetVelocityTypeObject() { return m_velocity_type.get(); }
	void SetVelocityType(VelocityType velocity) { InitEmitVelocity(velocity); };
	VelocityType GetVelocityType() { if (!m_velocity_type) { return VT_CONE; }   return m_velocity_type->GetVelocityType(); };
	
	uint32_t GetEmitParticleCount() { return m_next_emit_count; };
	uint32_t GetEmitParticleLifeMax() { return m_particle_life_max; };

	void SetStr(const std::string& str){m_str = str;};
	const std::string& GetStr() { return m_str; };
	//Appearance
	CParticleScale* GetParticleScaleObject() { return m_particle_scale.get(); };

private:
	uint32_t m_age = 0;//(ms)Particle system age
	float m_delta_time = 0;//(ms)delta teme for update
	uint32_t m_emitted_count = 0;//emitted particle count
	uint32_t m_next_emit_count = 0;//emit particle count next time
	
	//General
	glm::vec3 m_pos = { 0.0f, 0.0f, 0.0f };
	glm::vec3 m_rot = { 0.0f, 0.0f, 0.0f };
	uint32_t m_max_particle_count = 1024*2;//MAX_PARTICLE_COUNT;//max particle count for this particle system
	uint32_t m_emit_count = 1;//every time emit count
	uint32_t m_emit_interval = 1;//emit interval (ms)

	std::string m_str = "Hello World!";

	//
	//EmitShape m_emit_shape = ES_POINT;
	std::unique_ptr<CEmitShape> m_emit_shape;
	std::unique_ptr<CVelocityType> m_velocity_type;
	uint32_t m_particle_life_max = 2000;
	
	//Appearance
	std::string m_particle_tex = "textures/particle01_rgba.ktx";
	std::string m_gradient_tex = "textures/particle_gradient_rgba.ktx";
	std::unique_ptr<CParticleScale> m_particle_scale;
};

class VulkanExample : public VulkanExampleBase
{
public:
	vks::Model m_scene;
	CParticleSystem m_particles;
	float timer = 0.0f;
	float animStart = 20.0f;
	
	bool m_animate = true;

	//-------------------ParticleSystemRenderer-----------------------
	struct {
		vks::Texture2D particle;
		vks::Texture2D gradient;
		void Destroy()
		{
			particle.destroy();
			gradient.destroy();
		}
	} m_textures;

	struct UBOs
	{
		struct SEmitterParam {					// Compute shader uniform block object
			uint32_t emitCount;					//		Frame emit particle count
			uint32_t particleLifeMax;
			float randomSeed;
			int32_t padding2;
		} emitterParams;
		vks::Buffer uboEmitter;				// Uniform buffer object containing particle system parameters
		void Destroy()
		{
			uboEmitter.destroy();
		}
		
	} m_UBOs;
	struct SSBOs{
		struct SParticle {							// SSBO particle declaration
			glm::vec3 pos;							// Particle position
			float lifeMax;
			glm::vec3 vel;							// Particle velocity
			float life;
			glm::vec4 gradientPos;					// Texture coordiantes for the gradient ramp map
		};
		vks::Buffer ssboParticles;
		vks::Buffer ssboDeadList;
		vks::Buffer ssboAliveList;
		vks::Buffer ssboAliveListAfterSimulate;
		vks::Buffer ssboStagingBuffer; //used for swapping alive list
		vks::Buffer ssboCounter;

		uint32_t EMIT_OFFSET = 0;
		uint32_t SIMULATE_OFFSET = EMIT_OFFSET+ 4 * 4;
		uint32_t DRAW_OFFSET = SIMULATE_OFFSET + 4 * 4;
		
		vks::Buffer ssboIndirect;
		void Destroy()
		{
			ssboIndirect.destroy();
			ssboCounter.destroy();
			ssboStagingBuffer.destroy();
			ssboAliveListAfterSimulate.destroy();
			ssboAliveList.destroy();
			ssboDeadList.destroy();
			ssboParticles.destroy();
		}
	} m_SSBOs;
	
	uint32_t m_discriptor_count = 6;
	// Resources for the compute particle simulation
	struct SComputeUpdateCounterBegin {
		VkDescriptorSetLayout descriptorSetLayout;	// shader binding layout
		VkDescriptorSet descriptorSet;				// shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of pipeline
		VkPipeline pipeline;						// Compute pipeline
		void Destroy(VkDevice device)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		}
	} m_updater_begin_binding;
	// Resources for the compute particle simulation
	struct SComputeEmission {
		struct SEmitterParam {					// Compute shader uniform block object
			int32_t emitCount;						//		Frame emit particle count
			float padding0;							//		x position of the attractor
			float padding1;							//		y position of the attractor
			int32_t padding2;
		} emitterParams;
		vks::Buffer uboEmitter;				// Uniform buffer object containing particle system parameters

		VkDescriptorSetLayout descriptorSetLayout;	// shader binding layout
		VkDescriptorSet descriptorSet;				// shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of pipeline
		VkPipeline pipeline;						// Compute pipeline
		void Destroy(VkDevice device)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			uboEmitter.destroy();
		}
	} m_emission_binding;
	// Resources for the compute particle simulation
	struct SComputeSimulation {
		struct SAttractorParam {					// Compute shader uniform block object
			float deltaT;							//		Frame delta time
			float destX;							//		x position of the attractor
			float destY;							//		y position of the attractor
			int32_t particleCount;
		} attractorParams;
		vks::Buffer uboAttractor;				// Uniform buffer object containing particle system parameters
		
	
		VkDescriptorSetLayout descriptorSetLayout;	// shader binding layout
		VkDescriptorSet descriptorSet;				// shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of pipeline
		VkPipeline pipeline;						// Compute pipeline
		void Destroy(VkDevice device)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			uboAttractor.destroy();
		}
	} m_simulation_binding;
	struct SComputeUpdateCounterEnd {
		VkDescriptorSetLayout descriptorSetLayout;	// shader binding layout
		VkDescriptorSet descriptorSet;				// shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of pipeline
		VkPipeline pipeline;						// Compute pipeline
		void Destroy(VkDevice device)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		}
	} m_updater_end_binding;

	// Resources for the graphics part of the example
	struct SGraphicEnvironment {
		struct SceneVertex {
			glm::vec3 pos;
			glm::vec2 uv;
			glm::vec3 color;
			glm::vec3 normal;
		};
		struct SSceneMatrices {
			glm::mat4 projection;
			glm::mat4 view;
			glm::mat4 model;
			glm::vec4 lightPos = glm::vec4(0.0f, -25.0f, 0.0f, 0.0f);
		} sceneMatrices;
		vks::Buffer uboSceneMatrices;
		VkDescriptorSetLayout descriptorSetLayout;	// shader binding layout
		VkDescriptorSet descriptorSet;				// shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of  pipeline
		VkPipeline pipeline;						// rendering pipeline
		void Destroy(VkDevice device)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			uboSceneMatrices.destroy();
		}
	} m_environment_binding;
	
	// Resources for the graphics part of the example
	struct SGraphicComposition {
		struct SSceneMatrices {
			glm::mat4 model;
			glm::mat4 view;
			glm::mat4 projection;
		} sceneMatrices;
		vks::Buffer uboSceneMatrices;
		VkDescriptorSetLayout descriptorSetLayout;	// shader binding layout
		VkDescriptorSet descriptorSet;				// shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of  pipeline
		VkPipeline pipeline;						// rendering pipeline
		void Destroy(VkDevice device)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			uboSceneMatrices.destroy();
		}
	} m_composition_binding;

	// Resources for the compute part of the example
	struct {
		VkQueue queue;								// Separate queue for compute commands (queue family may differ from the one used for graphics)
		VkCommandPool commandPool;					// Use a separate command pool (queue family may differ from the one used for graphics)
		VkCommandBuffer commandBuffer;				// Command buffer storing the dispatch commands and barriers
		VkFence fence;								// Synchronization fence to avoid rewriting compute CB if still in use
		
	} compute;
	//-------------------~ParticleSystemRenderer----------------------

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Particle system";
		settings.overlay = true;

		//setting camera
		/*camera.type = Camera::CameraType::firstperson;
		camera.movementSpeed = 5.0f;
		camera.position = {0.0f, 5.0f, -5.0f };
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 64.0f);*/
		float zNear = 0.1f;
		float zFar = 1024.0f;
		camera.type = Camera::CameraType::lookat;
		camera.setPerspective(45.0f, (float)width / (float)height, zNear, zFar);
		//camera.setRotation(glm::vec3(-20.5f, -673.0f, 0.0f));
		camera.setRotation(glm::vec3(150.0f, -10.0f, 0.0f));
		camera.setPosition(glm::vec3(0.0f, 0.0f, -80.0f));
		zoomSpeed = 10.0f;

		//particle system start play
		m_particles.Initial(frameTimer);
		m_particles.SetPos(glm::vec3 (0.0f, 7.0f, 0.0f));
		m_particles.SetRot(glm::vec3(0.0f, 0.0f, 0.0f));
	}

	~VulkanExample()
	{
		m_updater_begin_binding.Destroy(device);
		m_emission_binding.Destroy(device);
		m_simulation_binding.Destroy(device);
		m_updater_end_binding.Destroy(device);
		m_environment_binding.Destroy(device);
		m_composition_binding.Destroy(device);

		// Compute
		vkDestroyFence(device, compute.fence, nullptr);
		vkDestroyCommandPool(device, compute.commandPool, nullptr);

		m_SSBOs.Destroy();
		m_UBOs.Destroy();
		m_textures.Destroy();
		m_scene.destroy();
	}
	//generate emit shader code
	std::string GetEmitCode()
	{
		std::string str;
		str += m_particles.GetEmitShapeObject()->GetPositionCode();
		str += m_particles.GetVelocityTypeObject()->GetVelocityCode();
		return str;
	}
	
	int FillEmitShader() {
		std::string path = getAssetPath() + "shaders/aparticlesystem/particle_emit_param.h";

		std::ifstream in;
		in.open(path);

		std::string file_str;
		std::string line_str;
		if (!in.is_open()) {
			std::cout << "打开文件失败" << std::endl;
		}
		while (getline(in, line_str)) {
			file_str += line_str + "\n";
			std::string replace_start = "//Hot Update Rigion";
			if(line_str.find(replace_start)!=std::string::npos){
				break;
			}
			
		}
		in.close();

		//file_str += "float g_circle_radian =2.0;\n";
		//file_str += "float g_cone_radian = 1.0/6.0;\n";
		file_str += GetEmitCode();

		//write back
		std::ofstream  out;
		out.open(path);
		out.flush();
		out << file_str;
		out.close();

		return 0;
	}
	void UpdateEmitShader()
	{
		FillEmitShader();
		std::string path = getAssetPath() + "shaders/aparticlesystem/generate_emit.bat";
		system(path.c_str());
		Reset();
	}

	//generate simulate shader code
	std::string GetSimulateCode()
	{
		std::string str;
		str += m_particles.GetParticleScaleObject()->GetParticleScaleCode();
		return str;
	}

	int FillSimulateShader() {
		std::string path = getAssetPath() + "shaders/aparticlesystem/particle_simulate_param.h";

		std::ifstream in;
		in.open(path);

		std::string file_str;
		std::string line_str;
		if (!in.is_open()) {
			std::cout << "打开文件失败" << std::endl;
		}
		while (getline(in, line_str)) {
			file_str += line_str + "\n";
			std::string replace_start = "//Hot Update Rigion";
			if (line_str.find(replace_start) != std::string::npos) {
				break;
			}

		}
		in.close();

		//file_str += "float g_circle_radian =2.0;\n";
		//file_str += "float g_cone_radian = 1.0/6.0;\n";
		file_str += GetSimulateCode();

		//write back
		std::ofstream  out;
		out.open(path);
		out.flush();
		out << file_str;
		out.close();

		return 0;
	}
	void UpdateSimulateShader()
	{
		FillSimulateShader();
		std::string path = getAssetPath() + "shaders/aparticlesystem/generate_simulate.bat";
		system(path.c_str());
		Reset();
	}
	
	void LoadAssets()
	{
		vks::VertexLayout vertexLayout = vks::VertexLayout({
			vks::VERTEX_COMPONENT_POSITION,
			vks::VERTEX_COMPONENT_UV,
			vks::VERTEX_COMPONENT_COLOR,
			vks::VERTEX_COMPONENT_NORMAL,
		});
		m_scene.loadFromFile(getAssetPath() + "models/shadowscene_fire.dae", vertexLayout, 1.0f, vulkanDevice, queue);
		
		m_textures.particle.loadFromFile(getAssetPath() + m_particles.GetTex(), VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		m_textures.gradient.loadFromFile(getAssetPath() + m_particles.GetGradientTex(), VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	}

	void Reset()
	{
		m_particles.Reset();

		m_updater_begin_binding.Destroy(device);
		m_emission_binding.Destroy(device);
		m_simulation_binding.Destroy(device);
		m_updater_end_binding.Destroy(device);
		m_environment_binding.Destroy(device);
		m_composition_binding.Destroy(device);

		m_SSBOs.Destroy();
		
		//BuildSSBOsWithParticle();
		BuildSSBOs();

		SetupDescriptorPool();
		BuildComputeUpdateCounterBegin();
		BuildComputeEmission();
		BuildComputeSimulation();
		BuildComputeUpdateCounterEnd();
		BuildGraphicEnvironment();
		BuildGraphicComposition();

		buildComputeCommandBuffer();
		buildCommandBuffers();
	}

	void SetupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1+2+1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1+4),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), m_discriptor_count);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void BuildComputeUpdateCounterBegin()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Emitter Uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			// counter shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			// indirect buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_updater_begin_binding.descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_updater_begin_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_updater_begin_binding.pipelineLayout));

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &m_updater_begin_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &m_updater_begin_binding.descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// same as setLayoutBindings
			vks::initializers::writeDescriptorSet(m_updater_begin_binding.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_UBOs.uboEmitter.descriptor),
			vks::initializers::writeDescriptorSet(m_updater_begin_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &m_SSBOs.ssboCounter.descriptor),
			vks::initializers::writeDescriptorSet(m_updater_begin_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &m_SSBOs.ssboIndirect.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Create pipeline
		VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(m_updater_begin_binding.pipelineLayout, 0);
		pipelineCreateInfo.stage = loadShader(getAssetPath() + "shaders/aparticlesystem/update_counter_begin.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_updater_begin_binding.pipeline));
	}
	
	void BuildComputeEmission()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Emitter Uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			// Particle shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			// dead list shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			// alive list shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			// alive list after simulate shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			// counter shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_emission_binding.descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_emission_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_emission_binding.pipelineLayout));

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &m_emission_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &m_emission_binding.descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// same as setLayoutBindings
			vks::initializers::writeDescriptorSet(m_emission_binding.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_UBOs.uboEmitter.descriptor),
			vks::initializers::writeDescriptorSet(m_emission_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &m_SSBOs.ssboParticles.descriptor),
			vks::initializers::writeDescriptorSet(m_emission_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &m_SSBOs.ssboDeadList.descriptor),
			vks::initializers::writeDescriptorSet(m_emission_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &m_SSBOs.ssboAliveList.descriptor),
			vks::initializers::writeDescriptorSet(m_emission_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &m_SSBOs.ssboAliveListAfterSimulate.descriptor),
			vks::initializers::writeDescriptorSet(m_emission_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &m_SSBOs.ssboCounter.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Create pipeline
		VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(m_emission_binding.pipelineLayout, 0);
		pipelineCreateInfo.stage = loadShader(getAssetPath() + "shaders/aparticlesystem/particle_emit.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_emission_binding.pipeline));
	}
	
	void BuildComputeSimulation()
	{
		// Attactor ubo
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&m_simulation_binding.uboAttractor,
			sizeof(m_simulation_binding.attractorParams));
		
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			//Uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			// Particle shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			// dead list shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			// alive list shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			// alive list after simulate shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			// counter shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_simulation_binding.descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_simulation_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_simulation_binding.pipelineLayout));

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &m_simulation_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &m_simulation_binding.descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// same as setLayoutBindings
			vks::initializers::writeDescriptorSet(m_simulation_binding.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_simulation_binding.uboAttractor.descriptor),
			// Particle storage buffer
			vks::initializers::writeDescriptorSet(m_simulation_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &m_SSBOs.ssboParticles.descriptor),
			vks::initializers::writeDescriptorSet(m_simulation_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &m_SSBOs.ssboDeadList.descriptor),
			vks::initializers::writeDescriptorSet(m_simulation_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &m_SSBOs.ssboAliveList.descriptor),
			vks::initializers::writeDescriptorSet(m_simulation_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &m_SSBOs.ssboAliveListAfterSimulate.descriptor),
			vks::initializers::writeDescriptorSet(m_simulation_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &m_SSBOs.ssboCounter.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Create pipeline
		VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(m_simulation_binding.pipelineLayout, 0);
		pipelineCreateInfo.stage = loadShader(getAssetPath() + "shaders/aparticlesystem/particle_simulate.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_simulation_binding.pipeline));
	}

	void BuildComputeUpdateCounterEnd()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Emitter Uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			// counter shader storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			// indirect buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_updater_end_binding.descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_updater_end_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_updater_end_binding.pipelineLayout));

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &m_updater_end_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &m_updater_end_binding.descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// same as setLayoutBindings
			vks::initializers::writeDescriptorSet(m_updater_end_binding.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_UBOs.uboEmitter.descriptor),
			vks::initializers::writeDescriptorSet(m_updater_end_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &m_SSBOs.ssboCounter.descriptor),
			vks::initializers::writeDescriptorSet(m_updater_end_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &m_SSBOs.ssboIndirect.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Create pipeline
		VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(m_updater_end_binding.pipelineLayout, 0);
		pipelineCreateInfo.stage = loadShader(getAssetPath() + "shaders/aparticlesystem/update_counter_end.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_updater_end_binding.pipeline));
	}

	void BuildGraphicEnvironment()
	{
		// Scene matrices ubo
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&m_environment_binding.uboSceneMatrices,
			sizeof(m_environment_binding.sceneMatrices));

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			// Binding 1 : Fragment shader image sampler
			//vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_environment_binding.descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_environment_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_environment_binding.pipelineLayout));

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &m_environment_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &m_environment_binding.descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(m_environment_binding.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_environment_binding.uboSceneMatrices.descriptor),
			// Binding 1 : Fragment shader shadow sampler
			//vks::initializers::writeDescriptorSet(m_environment_binding.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptor)
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Create Rendering pipeline
		{
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
			VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
			VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
			VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
			VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
			VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
			VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
			std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

			// Load shaders
			std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
			shaderStages[0] = loadShader(getAssetPath() + "shaders/aparticlesystem/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = loadShader(getAssetPath() + "shaders/aparticlesystem/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

			// Additive blending
			/*blendAttachmentState.colorWriteMask = 0xF;
			blendAttachmentState.blendEnable = VK_TRUE;
			blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
			blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;*/
			
			// Vertex input state for scene rendering
			const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
				vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(SGraphicEnvironment::SceneVertex), VK_VERTEX_INPUT_RATE_VERTEX)
			};
			// Describes memory layout and shader positions
			const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
				// Location 0 : Position
				vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SGraphicEnvironment::SceneVertex, pos)),
				// Location 1 : Normal
				vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SGraphicEnvironment::SceneVertex, normal)),
				// Location 2 : Texture coordinates
				vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(SGraphicEnvironment::SceneVertex, uv)),
				// Location 3 : Color
				vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SGraphicEnvironment::SceneVertex, color)),
			};
			VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
			vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
			vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
			vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
			vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

			VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(m_environment_binding.pipelineLayout, renderPass, 0);
			pipelineCreateInfo.pVertexInputState = &vertexInputState;
			pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
			pipelineCreateInfo.pRasterizationState = &rasterizationState;
			pipelineCreateInfo.pColorBlendState = &colorBlendState;
			pipelineCreateInfo.pMultisampleState = &multisampleState;
			pipelineCreateInfo.pViewportState = &viewportState;
			pipelineCreateInfo.pDepthStencilState = &depthStencilState;
			pipelineCreateInfo.pDynamicState = &dynamicState;
			pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			pipelineCreateInfo.pStages = shaderStages.data();
			pipelineCreateInfo.renderPass = renderPass;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_environment_binding.pipeline));
		}
	}
	
	void BuildGraphicComposition()
	{
		// Scene matrices ubo
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&m_composition_binding.uboSceneMatrices,
			sizeof(m_composition_binding.sceneMatrices));
		
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			//Particle color map
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			//Particle gradient ramp
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			//VS UBO
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 2),								
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_composition_binding.descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =vks::initializers::pipelineLayoutCreateInfo(&m_composition_binding.descriptorSetLayout,1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_composition_binding.pipelineLayout));

		VkDescriptorSetAllocateInfo allocInfo =vks::initializers::descriptorSetAllocateInfo(descriptorPool,&m_composition_binding.descriptorSetLayout,1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &m_composition_binding.descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0 : Particle color map
			vks::initializers::writeDescriptorSet(m_composition_binding.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &m_textures.particle.descriptor),
			// Binding 1 : Particle gradient ramp
			vks::initializers::writeDescriptorSet(m_composition_binding.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &m_textures.gradient.descriptor),
			//VS UBO
			vks::initializers::writeDescriptorSet(m_composition_binding.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &m_composition_binding.uboSceneMatrices.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Create Rendering pipeline
		{
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0, VK_FALSE);
			VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
			VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
			VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
			VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
			VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
			VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
			std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

			// Load shaders
			std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
			shaderStages[0] = loadShader(getAssetPath() + "shaders/aparticlesystem/particle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = loadShader(getAssetPath() + "shaders/aparticlesystem/particle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

			// Additive blending
			blendAttachmentState.colorWriteMask = 0xF;
			blendAttachmentState.blendEnable = VK_TRUE;
			blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
			blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

			// Vertex input state for scene rendering
			const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
				vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(SSBOs::SParticle), VK_VERTEX_INPUT_RATE_VERTEX)
			};
			// Describes memory layout and shader positions
			const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
				// Location 0 : Position
				vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SSBOs::SParticle, pos)),
				// Location 1 : Gradient position
				vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SSBOs::SParticle, gradientPos))
			};
			VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
			vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
			vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
			vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
			vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

			VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(m_composition_binding.pipelineLayout, renderPass, 0);
			pipelineCreateInfo.pVertexInputState = &vertexInputState;
			pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
			pipelineCreateInfo.pRasterizationState = &rasterizationState;
			pipelineCreateInfo.pColorBlendState = &colorBlendState;
			pipelineCreateInfo.pMultisampleState = &multisampleState;
			pipelineCreateInfo.pViewportState = &viewportState;
			pipelineCreateInfo.pDepthStencilState = &depthStencilState;
			pipelineCreateInfo.pDynamicState = &dynamicState;
			pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			pipelineCreateInfo.pStages = shaderStages.data();
			pipelineCreateInfo.renderPass = renderPass;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_composition_binding.pipeline));
		}
	}

	void prepareCompute()
	{
		// Create a compute capable device queue
		// The VulkanDevice::createLogicalDevice functions finds a compute capable queue and prefers queue families that only support compute
		// Depending on the implementation this may result in different queue family indices for graphics and computes,
		// requiring proper synchronization (see the memory barriers in buildComputeCommandBuffer)
		vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.compute, 0, &compute.queue);


		// Separate command pool as queue family for compute may be different than graphics
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &compute.commandPool));

		// Create a command buffer for compute operations
		VkCommandBufferAllocateInfo cmdBufAllocateInfo =vks::initializers::commandBufferAllocateInfo(compute.commandPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY,1);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &compute.commandBuffer));

		// Fence for compute CB sync
		VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
		VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &compute.fence));

		// Build a single command buffer containing the compute dispatch commands
		buildComputeCommandBuffer();
	}
	void buildCommandBuffers()
	{
		// Destroy command buffers if already present
		if (!checkCommandBuffers())
		{
			destroyCommandBuffers();
			createCommandBuffers();
		}

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			// Draw the particle system using the update vertex buffer

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
			VkDeviceSize offsets[1] = { 0 };

			// 3D scene
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_environment_binding.pipeline);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_environment_binding.pipelineLayout, 0, 1, &m_environment_binding.descriptorSet, 0, NULL);
			vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &m_scene.vertices.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], m_scene.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(drawCmdBuffers[i], m_scene.indexCount, 1, 0, 0, 0);

			//particle system
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_composition_binding.pipeline);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_composition_binding.pipelineLayout, 0, 1, &m_composition_binding.descriptorSet, 0, NULL);

			vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &m_SSBOs.ssboParticles.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], m_SSBOs.ssboAliveListAfterSimulate.buffer, 0, VK_INDEX_TYPE_UINT32);
			// If the multi draw feature is supported:
			// One draw call for an arbitrary number of ojects
			// Index offsets and instance count are taken from the indirect buffer
			vkCmdDrawIndexedIndirect(drawCmdBuffers[i], m_SSBOs.ssboIndirect.buffer, m_SSBOs.DRAW_OFFSET, 1, 4*4);
			//vkCmdDraw(drawCmdBuffers[i], m_particles.GetParticleCount(), 1, 0, 0);
			
			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void buildComputeCommandBuffer()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VK_CHECK_RESULT(vkBeginCommandBuffer(compute.commandBuffer, &cmdBufInfo));

		{// Add memory barrier to ensure that the (graphics) indirect commad has fetched attributes before compute starts to write to the buffer
			VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
			bufferBarrier.buffer = m_SSBOs.ssboIndirect.buffer;
			bufferBarrier.size = m_SSBOs.ssboIndirect.descriptor.range;
			bufferBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;						// Vertex shader invocations have finished reading from the buffer
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;								// Compute shader wants to write to the buffer
			// Compute and graphics queue may have different queue families (see VulkanDevice::createLogicalDevice)
			// For the barrier to work across different queues, we need to set their family indices
			bufferBarrier.srcQueueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;			// Required as compute and graphics queue may have different families
			bufferBarrier.dstQueueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;			// Required as compute and graphics queue may have different families
			vkCmdPipelineBarrier(
				compute.commandBuffer,
				VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				1, &bufferBarrier,
				0, nullptr);
		}
		//update counter
		{
			vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_updater_begin_binding.pipeline);
			vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_updater_begin_binding.pipelineLayout, 0, 1, &m_updater_begin_binding.descriptorSet, 0, 0);
			vkCmdDispatch(compute.commandBuffer,1, 1, 1);
		}
		{// Add memory barrier to ensure that compute shader has finished writing to the buffer
			VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
			bufferBarrier.buffer = m_SSBOs.ssboIndirect.buffer;
			bufferBarrier.size = m_SSBOs.ssboIndirect.descriptor.range;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;						// Vertex shader invocations have finished reading from the buffer
			bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;								// Compute shader wants to write to the buffer
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			vkCmdPipelineBarrier(
				compute.commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				1, &bufferBarrier,
				0, nullptr);
		}
		//emit
		{
			vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_emission_binding.pipeline);
			vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_emission_binding.pipelineLayout, 0, 1, &m_emission_binding.descriptorSet, 0, 0);
			vkCmdDispatchIndirect(compute.commandBuffer, m_SSBOs.ssboIndirect.buffer , m_SSBOs.EMIT_OFFSET);
		}
		{// Add memory barrier to ensure that compute shader has finished initialize to particle buffer
			VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
			bufferBarrier.buffer = m_SSBOs.ssboAliveList.buffer;
			bufferBarrier.size = m_SSBOs.ssboAliveList.descriptor.range;
			//VkMemoryBarrier bufferBarrier = vks::initializers::memoryBarrier();
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;						// Vertex shader invocations have finished reading from the buffer
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;								// Compute shader wants to write to the buffer
			//bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			vkCmdPipelineBarrier(
				compute.commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				1, & bufferBarrier,
				0, nullptr);
		}
		
		//{// Add memory barrier to ensure that the (graphics) vertex shader has fetched attributes before compute starts to write to the buffer
		//	VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
		//	bufferBarrier.buffer = m_SSBOs.ssboParticles.buffer;
		//	bufferBarrier.size = m_SSBOs.ssboParticles.descriptor.range;
		//	bufferBarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;						// Vertex shader invocations have finished reading from the buffer
		//	bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;								// Compute shader wants to write to the buffer
		//	// Compute and graphics queue may have different queue families (see VulkanDevice::createLogicalDevice)
		//	// For the barrier to work across different queues, we need to set their family indices
		//	bufferBarrier.srcQueueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;			// Required as compute and graphics queue may have different families
		//	bufferBarrier.dstQueueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;			// Required as compute and graphics queue may have different families
		//	vkCmdPipelineBarrier(
		//		compute.commandBuffer,
		//		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		//		VK_FLAGS_NONE,
		//		0, nullptr,
		//		1, &bufferBarrier,
		//		0, nullptr);
		//	
		//}
		// Compute particle movement
		{
			vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_simulation_binding.pipeline);
			vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_simulation_binding.pipelineLayout, 0, 1, &m_simulation_binding.descriptorSet, 0, 0);
			//vkCmdDispatch(compute.commandBuffer, (m_particles.GetMaxParticleCount() + 255) / 256, 1, 1);
			vkCmdDispatchIndirect(compute.commandBuffer, m_SSBOs.ssboIndirect.buffer, m_SSBOs.SIMULATE_OFFSET);
		}
		{// Add memory barrier to ensure that compute shader has finished initialize to particle buffer
			VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
			bufferBarrier.buffer = m_SSBOs.ssboAliveListAfterSimulate.buffer;
			bufferBarrier.size = m_SSBOs.ssboAliveListAfterSimulate.descriptor.range;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			vkCmdPipelineBarrier(
				compute.commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				1, &bufferBarrier,
				0, nullptr);
		}
		//update counter
		{
			vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_updater_end_binding.pipeline);
			vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_updater_end_binding.pipelineLayout, 0, 1, &m_updater_end_binding.descriptorSet, 0, 0);
			vkCmdDispatch(compute.commandBuffer, 1, 1, 1);
		}
		{// Add memory barrier to ensure that compute shader has finished writing to the buffer
		// Without this the (rendering) vertex shader may display incomplete results (partial data from last frame)
			VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
			bufferBarrier.buffer = m_SSBOs.ssboIndirect.buffer;
			bufferBarrier.size = m_SSBOs.ssboIndirect.descriptor.range;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;								// Compute shader has finished writes to the buffer
			bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;						// Vertex shader invocations want to read from the buffer
			// Compute and graphics queue may have different queue families (see VulkanDevice::createLogicalDevice)
			// For the barrier to work across different queues, we need to set their family indices
			bufferBarrier.srcQueueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;			// Required as compute and graphics queue may have different families
			bufferBarrier.dstQueueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;			// Required as compute and graphics queue may have different families
			vkCmdPipelineBarrier(
				compute.commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				1, &bufferBarrier,
				0, nullptr);
			
		}
		
		vkEndCommandBuffer(compute.commandBuffer);
	}

	// Setup and fill the compute shader storage buffers containing the particles
	void BuildUBOs()
	{
		// Emitter ubo
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&m_UBOs.uboEmitter,
			sizeof(m_UBOs.emitterParams));
	}
	// Setup and fill the compute shader storage buffers containing the particles
	void BuildSSBOsWithParticle()
	{
		//Particle buffer
		{
			//std::default_random_engine rndEngine(benchmark.active ? 0 : (unsigned)time(nullptr));
			//std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);

			//// Initial particle positions
			//std::vector<SSBOs::SParticle> particleBuffer(m_particles.GetMaxParticleCount());
			//for (auto& particle : particleBuffer) {
			//	particle.pos = glm::vec3(rndDist(rndEngine), rndDist(rndEngine),0.0f);
			//	particle.vel = glm::vec3(0.0f);
			//	particle.gradientPos.x = particle.pos.x / 2.0f;
			//}

			std::vector<SSBOs::SParticle> particleBuffer(m_particles.GetMaxParticleCount());

			// Staging
			// SSBO won't be changed on the host after upload so copy to device local memory
			vks::Buffer stagingBuffer;
			VkDeviceSize storageBufferSize = particleBuffer.size() * sizeof(SSBOs::SParticle);
			vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&stagingBuffer,
				storageBufferSize,
				particleBuffer.data());

			vulkanDevice->createBuffer(
				// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&m_SSBOs.ssboParticles,
				storageBufferSize);

			// Copy to staging buffer
			VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBufferCopy copyRegion = {};
			copyRegion.size = storageBufferSize;
			vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_SSBOs.ssboParticles.buffer, 1, &copyRegion);
			VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);
			stagingBuffer.destroy();
		}
		//dead list
		{
			std::vector<uint32_t> dead_list(m_particles.GetMaxParticleCount());
			for (uint32_t i = 0; i < dead_list.size(); ++i)
			{
				dead_list[i] = i;
			}

			// Staging
			// SSBO won't be changed on the host after upload so copy to device local memory
			vks::Buffer stagingBuffer;
			VkDeviceSize storageBufferSize = dead_list.size() * sizeof(uint32_t);
			vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&stagingBuffer,
				storageBufferSize,
				dead_list.data());

			vulkanDevice->createBuffer(
				// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&m_SSBOs.ssboDeadList,
				storageBufferSize);

			// Copy to staging buffer
			VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBufferCopy copyRegion = {};
			copyRegion.size = storageBufferSize;
			vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_SSBOs.ssboDeadList.buffer, 1, &copyRegion);
			VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);
			stagingBuffer.destroy();
		}
		// alive list
		{
			std::vector<uint32_t> alive_list(m_particles.GetMaxParticleCount());

			// SSBO won't be changed on the host after upload so copy to device local memory
			VkDeviceSize storageBufferSize = alive_list.size() * sizeof(uint32_t);
			vulkanDevice->createBuffer(
				// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&m_SSBOs.ssboAliveList,
				storageBufferSize);
		}
		// alive list after simulate
		{
			std::vector<uint32_t> alive_list_after_simulate(m_particles.GetMaxParticleCount());

			// SSBO won't be changed on the host after upload so copy to device local memory
			VkDeviceSize storageBufferSize = alive_list_after_simulate.size() * sizeof(uint32_t);
			vulkanDevice->createBuffer(
				// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&m_SSBOs.ssboAliveListAfterSimulate,
				storageBufferSize);
		}
		//staging buffer
		{
			std::vector<uint32_t> list(m_particles.GetMaxParticleCount());

			// SSBO won't be changed on the host after upload so copy to device local memory
			VkDeviceSize storageBufferSize = list.size() * sizeof(uint32_t);
			vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&m_SSBOs.ssboStagingBuffer,
				storageBufferSize);
		}
		// counter
		{
			uint32_t emit_count = 0;
			uint32_t dead_count = m_particles.GetMaxParticleCount();
			uint32_t alive_count = 0;
			uint32_t alive_count_after_simulate = 0;
			std::vector<uint32_t> counter = { emit_count, dead_count, alive_count, alive_count_after_simulate };

			// Staging
			// SSBO won't be changed on the host after upload so copy to device local memory
			vks::Buffer stagingBuffer;
			VkDeviceSize storageBufferSize = counter.size() * sizeof(uint32_t);
			vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&stagingBuffer,
				storageBufferSize,
				counter.data());

			vulkanDevice->createBuffer(
				// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&m_SSBOs.ssboCounter,
				storageBufferSize);

			// Copy to staging buffer
			VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBufferCopy copyRegion = {};
			copyRegion.size = storageBufferSize;
			vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_SSBOs.ssboCounter.buffer, 1, &copyRegion);
			VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);
			stagingBuffer.destroy();
		}
	}
	void BuildSSBOs()
	{
		BuildSSBOsWithParticle();
		
		// indirect buffer
		{
			struct SIndirectParam
			{
				glm::u32vec3 dispatchEmit;
				float padding0;
				glm::u32vec3 dispatchSimulate;
				float padding1;
				glm::u32vec4 dispatchDraw;
			};

			VkDeviceSize storageBufferSize = sizeof(SIndirectParam);
			vulkanDevice->createBuffer(
				// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&m_SSBOs.ssboIndirect,
				storageBufferSize);
		}
	}

	void UpdateUBOEmitter()
	{
		m_UBOs.emitterParams.emitCount = m_particles.GetEmitParticleCount();
		m_UBOs.emitterParams.particleLifeMax = m_particles.GetEmitParticleLifeMax();
		m_UBOs.emitterParams.randomSeed = frameTimer * 1000;//:ms
		//std::cout << m_UBOs.emitterParams.emitCount << std::endl;

		// Map for host access
		VK_CHECK_RESULT(m_UBOs.uboEmitter.map());
		memcpy(m_UBOs.uboEmitter.mapped, &m_UBOs.emitterParams, sizeof(m_UBOs.emitterParams));
		m_UBOs.uboEmitter.unmap();
	}
	
	void UpdateUBOAttractor()
	{
		m_simulation_binding.attractorParams.deltaT = frameTimer*1000;//:ms
		if (m_animate)
		{
			m_simulation_binding.attractorParams.destX = sin(glm::radians(timer * 360.0f)) * 0.75f;
			m_simulation_binding.attractorParams.destY = 0.0f;
		}
		else
		{
			float normalizedMx = (mousePos.x - static_cast<float>(width / 2)) / static_cast<float>(width / 2);
			float normalizedMy = (mousePos.y - static_cast<float>(height / 2)) / static_cast<float>(height / 2);
			m_simulation_binding.attractorParams.destX = normalizedMx;
			m_simulation_binding.attractorParams.destY = normalizedMy;
		}
		//m_simulation_binding.attractorParams.particleCount = m_particles.GetParticleCount();

		// Map for host access
		VK_CHECK_RESULT(m_simulation_binding.uboAttractor.map());
		memcpy(m_simulation_binding.uboAttractor.mapped, &m_simulation_binding.attractorParams, sizeof(m_simulation_binding.attractorParams));
		m_simulation_binding.uboAttractor.unmap();
	}
	void UpdateUBOMatrices()
	{
		//scene matrix
		{
			glm::mat4 local = glm::mat4(1.0f);
			local = glm::rotate(local,  PI, glm::vec3(1, 0, 0));
			
			m_environment_binding.sceneMatrices.projection = camera.matrices.perspective;
			m_environment_binding.sceneMatrices.view = camera.matrices.view;
			m_environment_binding.sceneMatrices.model = local;

			// Map for host access
			VK_CHECK_RESULT(m_environment_binding.uboSceneMatrices.map());
			memcpy(m_environment_binding.uboSceneMatrices.mapped, &m_environment_binding.sceneMatrices, sizeof(m_environment_binding.sceneMatrices));
			m_environment_binding.uboSceneMatrices.unmap();
		}
		
		//particle matrix
		glm::mat4 local = glm::mat4(1.0f);
		//local = glm::scale(local, glm::vec3(0.5f));
		local = glm::rotate(local, m_particles.GetRot()[0]*PI, glm::vec3(1, 0, 0));
		local = glm::translate(local, m_particles.GetPos());

		m_composition_binding.sceneMatrices.projection = camera.matrices.perspective;
		m_composition_binding.sceneMatrices.view = camera.matrices.view;
		m_composition_binding.sceneMatrices.model = local;

		// Map for host access
		VK_CHECK_RESULT(m_composition_binding.uboSceneMatrices.map());
		memcpy(m_composition_binding.uboSceneMatrices.mapped, &m_composition_binding.sceneMatrices, sizeof(m_composition_binding.sceneMatrices));
		m_composition_binding.uboSceneMatrices.unmap();
	}
	void UpdateUBO()
	{
		UpdateUBOEmitter();
		UpdateUBOAttractor();
		UpdateUBOMatrices();
	}

	void Update()
	{
		m_particles.Update(frameTimer);
		UpdateUBO();

		// Swap CURRENT alivelist with NEW alivelist
		// Copy to staging buffer
		VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy copyRegion = {};
		copyRegion.size = m_SSBOs.ssboAliveListAfterSimulate.size;
		//vkCmdCopyBuffer(copyCmd, m_SSBOs.ssboAliveList.buffer, m_SSBOs.ssboStagingBuffer.buffer, 1, &copyRegion);
		vkCmdCopyBuffer(copyCmd, m_SSBOs.ssboAliveListAfterSimulate.buffer, m_SSBOs.ssboAliveList.buffer, 1, &copyRegion);
		//vkCmdCopyBuffer(copyCmd, m_SSBOs.ssboStagingBuffer.buffer, m_SSBOs.ssboAliveListAfterSimulate.buffer, 1, &copyRegion);
		VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);
	}
	
	void draw()
	{
		VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &compute.commandBuffer;

		VK_CHECK_RESULT(vkQueueSubmit(compute.queue, 1, &computeSubmitInfo, compute.fence));

		// Submit graphics commands
		VulkanExampleBase::prepareFrame();

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();

		// Submit compute commands
		vkWaitForFences(device, 1, &compute.fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &compute.fence);
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		LoadAssets();
		
		BuildUBOs();
		BuildSSBOs();
		
		SetupDescriptorPool();
		BuildComputeUpdateCounterBegin();
		BuildComputeEmission();
		BuildComputeSimulation();
		BuildComputeUpdateCounterEnd();
		BuildGraphicEnvironment();
		BuildGraphicComposition();

		UpdateUBO();

		prepareCompute();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();

		if (m_animate)
		{
			if (animStart > 0.0f)
			{
				animStart -= frameTimer * 5.0f;
			}
			else if (animStart <= 0.0f)
			{
				timer += frameTimer * 0.04f;
				if (timer > 1.f)
					timer = 0.f;
			}
		}
		Update();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay)
	{
		if (overlay->header("Particle System")) {
			if (overlay->treeNodeBegin("General")) {
				glm::vec3 pos = m_particles.GetPos();
				if (overlay->inputFloat3("Position", &pos[0], 2)) {
					//m_environment_binding.sceneMatrices.lightPos = glm::vec4(pos.x * 1.0f, pos.y * -10.0f, pos.z * -1.0f, 1.0);
					m_particles.SetPos(pos);
					UpdateUBOMatrices();
				}
				
				glm::vec3 rot = m_particles.GetRot();
				if (overlay->sliderFloat3("Rotate", &rot[0], 0.0f, 2.0f)) {
					m_particles.SetRot(rot);
					UpdateUBOMatrices();
				}

				int32_t num = m_particles.GetMaxParticleCount();
				if (overlay->sliderInt("Max Particle", &num, 4, 4 * 1024)) {
					m_particles.SetMaxParticleCount(num);
					Reset();
				}
				overlay->treeNodeEnd();
			}
			
			if (overlay->treeNodeBegin("Emission")) {
				if (overlay->treeNodeBegin("Rate")) {
					overlay->treeNodeEnd();
				}
				if (overlay->treeNodeBegin("Batch")) {
					overlay->treeNodeEnd();
				}
				if (overlay->treeNodeBegin("Shape")) {
					//overlay->text("Shape");
					int32_t shape_index = m_particles.GetEmitShape();
					if (overlay->comboBox("##Shape", &shape_index, { "point", "ring" })) {
						m_particles.SetEmitShape((EmitShape)shape_index);
						//updateUniformBuffers();
					}
					switch (shape_index) {
					case EmitShape::ES_POINT:
					{
						CEmitShapePoint* shape_obj = (CEmitShapePoint*)m_particles.GetEmitShape();
						if (!shape_obj) { break; }
					}break;
					case EmitShape::ES_RING:
					{
						CEmitShapeRing* shape_obj = (CEmitShapeRing*)m_particles.GetEmitShape();
						if (!shape_obj) { break; }
							
						//overlay->text("Radius");
						glm::vec3 pos = glm::vec3(0, 0, 0);
						if (overlay->sliderFloat("Radius", &pos[0], 0.0f, 2.0f)) {
							//updateUniformBuffers();
						}

						//overlay->text("Velocity");
						//for resize
						auto func = [](ImGuiInputTextCallbackData* data)->int {
							delete data->Buf;
							data->Buf = new char(data->BufSize);
							return 0;
						};
						std::string str = m_particles.GetStr();
						char* buffer = new char[1024];
						str.copy(buffer, str.size());
						buffer[str.size()] = '\0';
						if (overlay->inputEditor("Velocity", buffer, 1024, func)) {
							m_particles.SetStr(buffer);
						}
						delete buffer;

						//glm::vec3 vel = glm::vec3(0, 0, 0);
						//if (overlay->sliderFloat("Velocity", &vel[0], 0.0f, 2.0f)) {
						//	//updateUniformBuffers();
						//}
					}break;
					}
					overlay->treeNodeEnd();
				}
				
				if (overlay->treeNodeBegin("Velocity")) {
					//overlay->text("Velocity");
					int32_t velocity_type = m_particles.GetVelocityType();
					if (overlay->comboBox("____________", &velocity_type, { "cone", "radial" })) {
						m_particles.SetVelocityType((VelocityType)velocity_type);
						//updateUniformBuffers();
					}
					switch (velocity_type) {
					case VelocityType::VT_CONE: {
						CVelocityTypeCone* velocity_type_obj = (CVelocityTypeCone*)m_particles.GetVelocityTypeObject();
						if (!velocity_type_obj) { break; }
						char* buffer = new char[1024];
						{
							std::string str = velocity_type_obj->GetRadian();
							str.copy(buffer, str.size());
							buffer[str.size()] = '\0';
							if (overlay->inputEditor("Radian", buffer, 1024)) {
								velocity_type_obj->SetRadian(buffer);
							}
						}
						{
							std::string str = velocity_type_obj->GetConeRadian();
							str.copy(buffer, str.size());
							buffer[str.size()] = '\0';
							if (overlay->inputEditor("ConeRadian", buffer, 1024)) {
								velocity_type_obj->SetConeRadian(buffer);
							}
						}
						delete buffer;

						//glm::vec3 vel = glm::vec3(0, 0, 0);
						//if (overlay->sliderFloat("##Velocity", &vel[0], 0.0f, 2.0f)) {
						//	//updateUniformBuffers();
						//}
					}break;
					case VelocityType::VT_RADIAL: {}break;
					default:break;
					}
					if (overlay->button("Done")) {
						UpdateEmitShader();
					}
					overlay->treeNodeEnd();
				}
				overlay->treeNodeEnd();
			}
			if (overlay->treeNodeBegin("Simulation")) {
				if (overlay->button("TestCase")) {
					//UpdateEmitShader();
				}
				overlay->treeNodeEnd();
			}
			if (overlay->treeNodeBegin("Appearance")) {
				if (overlay->treeNodeBegin("Color")) {
					overlay->treeNodeEnd();
				}
				if (overlay->treeNodeBegin("Size")) {
					CParticleScale* particle_scale_obj = (CParticleScale*)m_particles.GetParticleScaleObject();
					if (particle_scale_obj)
					{
						char* buffer = new char[1024];
						std::string str = particle_scale_obj->GetParticleScale();
						str.copy(buffer, str.size());
						buffer[str.size()] = '\0';
						if (overlay->inputEditor("Scale", buffer, 1024)) {
							particle_scale_obj->SetParticleScale(buffer);
						}
						delete buffer;
					}
					overlay->treeNodeEnd();
				}
				overlay->treeNodeEnd();
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()