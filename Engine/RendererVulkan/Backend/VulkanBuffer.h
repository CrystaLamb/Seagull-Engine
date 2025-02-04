#pragma once

#include "Defs/Defs.h"
#include "Render/Buffer.h"

#include "VulkanConfig.h"
#include "VulkanAllocator.h"

#include "volk.h"

namespace SG
{

	class VulkanContext;

	class VulkanBuffer
	{
	public:
		VulkanBuffer(VulkanContext& c, const BufferCreateDesc& CI);
		~VulkanBuffer();
		SG_CLASS_NO_COPY_ASSIGNABLE(VulkanBuffer);

		bool   UploadData(const void* pData);
		bool   UploadData(const void* pData, UInt32 size, UInt32 offset);
		UInt32 SizeCPU() const { return size; }
		UInt32 SizeGPU() const;
		static VulkanBuffer* Create(VulkanContext& c, const BufferCreateDesc& CI);

		template <typename DataType>
		DataType* MapMemory();
		void      UnmapMemory();
	private:
		friend class VulkanCommandBuffer;
		friend class VulkanDescriptorDataBinder;
		VulkanContext& context;

#if SG_USE_VULKAN_MEMORY_ALLOCATOR
		VmaAllocation  vmaAllocation;
#else
		VkDeviceMemory memory;
#endif
		VkBuffer        buffer;
		UInt32          size;
		EBufferType     type;
		EGPUMemoryUsage memoryUsage;

		EGPUMemoryFlag  memoryFlag;
		void*           pMappedMemory = nullptr; //! Only the memoryFlag has EGPUMemoryFlag::efPersistent_Map bit, it is valid,
	};

	template <typename DataType>
	DataType* VulkanBuffer::MapMemory()
	{
		if (!IsHostVisible(memoryUsage)) // device local in GPU
		{
			SG_LOG_WARN("Try to upload data to device local memory! Please use sub-buffer to upload data.");
			return false;
		}

		if (SG_HAS_ENUM_FLAG(memoryFlag, EGPUMemoryFlag::efPersistent_Map))
			return reinterpret_cast<DataType*>(pMappedMemory);

#if SG_USE_VULKAN_MEMORY_ALLOCATOR
		DataType* pMappedMemory = nullptr;
		VK_CHECK(vmaMapMemory(context.vmaAllocator, vmaAllocation, (void**)&pMappedMemory),
			SG_LOG_ERROR("Failed to map vulkan buffer!"); return false;);
		return pMappedMemory;
#else
		DataType* pMappedMemory = nullptr;
		VK_CHECK(vkMapMemory(context.device.logicalDevice, memory, 0, size, 0, (void**)&pMappedMemory),
			SG_LOG_ERROR("Failed to map vulkan buffer!"); return false;);
		return pMappedMemory;
#endif
	}

}