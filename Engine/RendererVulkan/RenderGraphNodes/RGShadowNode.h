#pragma once

#include "RendererVulkan/RenderGraph/RenderGraphNode.h"

#include "Stl/SmartPtr.h"
#include "Math/MathBasic.h"

namespace SG
{

	class VulkanContext;
	class VulkanPipelineSignature;
	class VulkanPipeline;
	class VulkanShader;
	class VulkanRenderTarget;

	class DirectionalLight;

	class RGShadowNode final : public RenderGraphNode
	{
	public:
		RGShadowNode(VulkanContext& context, RenderGraph* pRenderGraph);
		~RGShadowNode();
	private:
		virtual void Reset() override;
		virtual void Prepare(VulkanRenderPass* pRenderpass) override;
		virtual void Draw(DrawInfo& context) override;
	private:
		VulkanContext& mContext;
		LoadStoreClearOp  mDepthRtLoadStoreOp;

		RefPtr<VulkanShader> mpShadowShader;
		RefPtr<VulkanShader> mpShadowInstanceShader;
		RefPtr<VulkanPipelineSignature> mpShadowPipelineSignature;
		VulkanPipeline* mpShadowPipeline;
		VulkanPipeline* mpShadowInstancePipeline;

		bool mbDrawShadow = true;
	};

}