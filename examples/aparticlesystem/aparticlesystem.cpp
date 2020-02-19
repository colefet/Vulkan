/*
* Vulkan Example - Particle system
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <random>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"

#define ENABLE_VALIDATION true

#define VERTEX_BUFFER_BIND_ID 0
#if defined(__ANDROID__)
// Lower particle count on Android for performance reasons
#define MAX_PARTICLE_COUNT 128 * 1024
#else
#define MAX_PARTICLE_COUNT 256 * 1024
#endif

#if !(defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
// iOS & macOS: VulkanExampleBase::getAssetPath() implemented externally to allow access to Objective-C components
const std::string getAssetPath()
{
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	return "";
#elif defined(VK_EXAMPLE_DATA_DIR)
	return VK_EXAMPLE_DATA_DIR;
#else
	return "./../data/";
#endif
}
#endif

class CParticleSystem
{
public:
	CParticleSystem() {};
	~CParticleSystem(){};
	void Destroy(VkDevice device)
	{
		
	}

	uint32_t GetParticleCount() { return m_particle_count; };
	const std::string& GetTex() { return m_particle_tex; }
	const std::string& GetGradientTex() { return m_gradient_tex; }

private:
	uint32_t m_particle_count = 100;
	uint32_t m_interval = 0;
	
	std::string m_particle_tex = "textures/particle01_rgba.ktx";
	std::string m_gradient_tex = "textures/particle_gradient_rgba.ktx";

	glm::vec3 m_pos = { 0.0f, 0.0f, 0.0f };
	
};

class VulkanExample : public VulkanExampleBase
{
public:
	CParticleSystem m_particles;
	float timer = 0.0f;
	float animStart = 20.0f;
	
	bool m_animate = true;

	//-------------------ParticleSystemRenderer-----------------------
	struct {
		vks::Texture2D particle;
		vks::Texture2D gradient;
	} textures;

	uint32_t m_discriptor_count = 2;
	// Resources for the compute particle updating
	struct SComputeUpdate {
		struct SAttractorParam {					// Compute shader uniform block object
			float deltaT;							//		Frame delta time
			float destX;							//		x position of the attractor
			float destY;							//		y position of the attractor
			int32_t particleCount;
		} attractorParams;
		vks::Buffer uboAttractor;				// Uniform buffer object containing particle system parameters
		
		struct SParticle {							// SSBO particle declaration
			glm::vec2 pos;							// Particle position
			glm::vec2 vel;							// Particle velocity
			glm::vec4 gradientPos;					// Texture coordiantes for the gradient ramp map
		};
		vks::Buffer ssboParticles;					// (Shader) storage buffer object containing the particles
		VkDescriptorSetLayout descriptorSetLayout;	// shader binding layout
		VkDescriptorSet descriptorSet;				// shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of pipeline
		VkPipeline pipeline;						// Compute pipeline
		void Destroy(VkDevice device)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			ssboParticles.destroy();
			uboAttractor.destroy();
		}
	} m_update_binding;
	
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
		camera.type = Camera::CameraType::firstperson;
		camera.movementSpeed = 5.0f;
		camera.position = { 7.5f, -6.75f, 0.0f };
		camera.setRotation(glm::vec3(5.0f, 90.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 64.0f);
	}

	~VulkanExample()
	{
		m_update_binding.Destroy(device);
		m_composition_binding.Destroy(device);

		// Compute
		vkDestroyFence(device, compute.fence, nullptr);
		vkDestroyCommandPool(device, compute.commandPool, nullptr);

		textures.particle.destroy();
		textures.gradient.destroy();
	}

	void LoadAssets()
	{
		textures.particle.loadFromFile(getAssetPath() + m_particles.GetTex(), VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.gradient.loadFromFile(getAssetPath() + m_particles.GetGradientTex(), VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	}

	void SetupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), m_discriptor_count);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}
	
	void BuildComputeUpdate()
	{
		// Particles ssbo
		BuildParticleSSBO();
		// Attactor ubo
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&m_update_binding.uboAttractor,
			sizeof(m_update_binding.attractorParams));
		
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			//Binding 0 :  Particle position storage buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			//Binding 1 : Uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_update_binding.descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_update_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_update_binding.pipelineLayout));

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &m_update_binding.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &m_update_binding.descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0 : Particle position storage buffer
			vks::initializers::writeDescriptorSet(m_update_binding.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &m_update_binding.ssboParticles.descriptor),
			// Binding 1 : Uniform buffer
			vks::initializers::writeDescriptorSet(m_update_binding.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &m_update_binding.uboAttractor.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Create pipeline
		VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(m_update_binding.pipelineLayout, 0);
		pipelineCreateInfo.stage = loadShader(getAssetPath() + "shaders/aparticlesystem/particle.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_update_binding.pipeline));
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
			vks::initializers::writeDescriptorSet(m_composition_binding.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.particle.descriptor),
			// Binding 1 : Particle gradient ramp
			vks::initializers::writeDescriptorSet(m_composition_binding.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.gradient.descriptor),
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
			VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
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
				vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(SComputeUpdate::SParticle), VK_VERTEX_INPUT_RATE_VERTEX)
			};
			// Describes memory layout and shader positions
			const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
				// Location 0 : Position
				vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SComputeUpdate::SParticle, pos)),
				// Location 1 : Gradient position
				vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SComputeUpdate::SParticle, gradientPos))
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

	// Setup and fill the compute shader storage buffers containing the particles
	void BuildParticleSSBO()
	{
		std::default_random_engine rndEngine(benchmark.active ? 0 : (unsigned)time(nullptr));
		std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);

		// Initial particle positions
		std::vector<SComputeUpdate::SParticle> particleBuffer(MAX_PARTICLE_COUNT);
		for (auto& particle : particleBuffer) {
			particle.pos = glm::vec2(rndDist(rndEngine), rndDist(rndEngine));
			particle.vel = glm::vec2(0.0f);
			particle.gradientPos.x = particle.pos.x / 2.0f;
		}

		VkDeviceSize storageBufferSize = particleBuffer.size() * sizeof(SComputeUpdate::SParticle);

		// Staging
		// SSBO won't be changed on the host after upload so copy to device local memory 

		vks::Buffer stagingBuffer;

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
			&m_update_binding.ssboParticles,
			storageBufferSize);

		// Copy to staging buffer
		VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy copyRegion = {};
		copyRegion.size = storageBufferSize;
		vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_update_binding.ssboParticles.buffer, 1, &copyRegion);
		VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);

		stagingBuffer.destroy();
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

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_composition_binding.pipeline);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_composition_binding.pipelineLayout, 0, 1, &m_composition_binding.descriptorSet, 0, NULL);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &m_update_binding.ssboParticles.buffer, offsets);
			vkCmdDraw(drawCmdBuffers[i], m_particles.GetParticleCount(), 1, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void buildComputeCommandBuffer()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VK_CHECK_RESULT(vkBeginCommandBuffer(compute.commandBuffer, &cmdBufInfo));

		// Compute particle movement

		// Add memory barrier to ensure that the (graphics) vertex shader has fetched attributes before compute starts to write to the buffer
		VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
		bufferBarrier.buffer = m_update_binding.ssboParticles.buffer;
		bufferBarrier.size = m_update_binding.ssboParticles.descriptor.range;
		bufferBarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;						// Vertex shader invocations have finished reading from the buffer
		bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;								// Compute shader wants to write to the buffer
		// Compute and graphics queue may have different queue families (see VulkanDevice::createLogicalDevice)
		// For the barrier to work across different queues, we need to set their family indices
		bufferBarrier.srcQueueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;			// Required as compute and graphics queue may have different families
		bufferBarrier.dstQueueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;			// Required as compute and graphics queue may have different families
		vkCmdPipelineBarrier(
			compute.commandBuffer,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);

		vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_update_binding.pipeline);
		vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_update_binding.pipelineLayout, 0, 1, &m_update_binding.descriptorSet, 0, 0);

		// Dispatch the compute job
		vkCmdDispatch(compute.commandBuffer, (m_particles.GetParticleCount()+255) / 256, 1, 1);

		// Add memory barrier to ensure that compute shader has finished writing to the buffer
		// Without this the (rendering) vertex shader may display incomplete results (partial data from last frame) 
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;								// Compute shader has finished writes to the buffer
		bufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;						// Vertex shader invocations want to read from the buffer
		bufferBarrier.buffer = m_update_binding.ssboParticles.buffer;
		bufferBarrier.size = m_update_binding.ssboParticles.descriptor.range;
		// Compute and graphics queue may have different queue families (see VulkanDevice::createLogicalDevice)
		// For the barrier to work across different queues, we need to set their family indices
		bufferBarrier.srcQueueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;			// Required as compute and graphics queue may have different families
		bufferBarrier.dstQueueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;			// Required as compute and graphics queue may have different families
		vkCmdPipelineBarrier(
			compute.commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);

		vkEndCommandBuffer(compute.commandBuffer);
	}

	void UpdateUniformBufferAttractor()
	{
		m_update_binding.attractorParams.deltaT = frameTimer * 2.5f;
		if (m_animate)
		{
			m_update_binding.attractorParams.destX = sin(glm::radians(timer * 360.0f)) * 0.75f;
			m_update_binding.attractorParams.destY = 0.0f;
		}
		else
		{
			float normalizedMx = (mousePos.x - static_cast<float>(width / 2)) / static_cast<float>(width / 2);
			float normalizedMy = (mousePos.y - static_cast<float>(height / 2)) / static_cast<float>(height / 2);
			m_update_binding.attractorParams.destX = normalizedMx;
			m_update_binding.attractorParams.destY = normalizedMy;
		}
		m_update_binding.attractorParams.particleCount = m_particles.GetParticleCount();

		// Map for host access
		VK_CHECK_RESULT(m_update_binding.uboAttractor.map());
		memcpy(m_update_binding.uboAttractor.mapped, &m_update_binding.attractorParams, sizeof(m_update_binding.attractorParams));
		m_update_binding.uboAttractor.unmap();
	}
	void UpdateUniformBufferMarices()
	{
		m_composition_binding.sceneMatrices.projection = camera.matrices.perspective;
		m_composition_binding.sceneMatrices.view = camera.matrices.view;
		m_composition_binding.sceneMatrices.model = glm::mat4(1.0f);

		// Map for host access
		VK_CHECK_RESULT(m_composition_binding.uboSceneMatrices.map());
		memcpy(m_composition_binding.uboSceneMatrices.mapped, &m_composition_binding.sceneMatrices, sizeof(m_composition_binding.sceneMatrices));
		m_composition_binding.uboSceneMatrices.unmap();
	}
	void UpdateUBO()
	{
		UpdateUniformBufferAttractor();
		UpdateUniformBufferMarices();
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
		
		SetupDescriptorPool();
		BuildComputeUpdate();
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
		UpdateUBO();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay)
	{
		if (overlay->header("Settings")) {
			overlay->checkBox("Moving attractor", &m_animate);
			//overlay->checkBox("Moving attractor", &animate);
		}
	}
};

VULKAN_EXAMPLE_MAIN()