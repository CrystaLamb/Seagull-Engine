#include "StdAfx.h"
#include "RendererVulkan/RenderGraphNodes/RGDebugNode.h"

#include "System/System.h"
#include "Profile/Profile.h"
#include "Render/Shader/ShaderComiler.h"

#include "RendererVulkan/Backend/VulkanContext.h"
#include "RendererVulkan/Backend/VulkanShader.h"
#include "RendererVulkan/Backend/VulkanPipeline.h"
#include "RendererVulkan/Backend/VulkanPipelineSignature.h"

#include "RendererVulkan/Resource/RenderResourceRegistry.h"

namespace SG
{

	RGDebugNode::RGDebugNode(VulkanContext& context, RenderGraph* pRenderGraph)
		:RenderGraphNode(pRenderGraph), mContext(context),
		// Set to default clear ops
		mColorRtLoadStoreOp({ ELoadOp::eLoad, EStoreOp::eStore, ELoadOp::eDont_Care, EStoreOp::eDont_Care }),
		mDepthRtLoadStoreOp({ ELoadOp::eLoad, EStoreOp::eStore, ELoadOp::eLoad, EStoreOp::eDont_Care })
	{
		SG_PROFILE_FUNCTION();

		ShaderCompiler compiler;
		mpDebugLineShader = VulkanShader::Create(mContext.device);
		compiler.CompileGLSLShader("debug/debug_line", mpDebugLineShader);

		mpDebugLinePipelineSignature = VulkanPipelineSignature::Builder(mContext, mpDebugLineShader)
			.Build();

#ifdef SG_ENABLE_HDR
		ClearValue cv = {};
		cv.color = { 0.04f, 0.04f, 0.04f, 1.0f };
		AttachResource(0, { VK_RESOURCE()->GetRenderTarget("HDRColor"), mColorRtLoadStoreOp, cv, EResourceBarrier::efUndefined, EResourceBarrier::efShader_Resource });
#endif
		cv = {};
		cv.depthStencil.depth = 1.0f;
		cv.depthStencil.stencil = 0;
		AttachResource(1, { mContext.depthRt, mDepthRtLoadStoreOp, cv, EResourceBarrier::efUndefined, EResourceBarrier::efDepth_Stencil });

#ifndef SG_ENABLE_HDR
		ClearValue cv = {};
		cv.color = { 0.04f, 0.04f, 0.04f, 1.0f };
		AttachResource(0, { mContext.colorRts[frameIndex], mColorRtLoadStoreOp, cv });
#endif

		vector<float> dots;
		MeshGenerator::GenDebugBox(dots);

		BufferCreateDesc vbCreateInfo = {};
		vbCreateInfo.name = "debug_geo";
		vbCreateInfo.type = EBufferType::efVertex;
		vbCreateInfo.pInitData = dots.data();
		vbCreateInfo.bufferSize = static_cast<UInt32>(dots.size()) * sizeof(float);
		vbCreateInfo.memoryUsage = EGPUMemoryUsage::eGPU_Only;
		VK_RESOURCE()->CreateBuffer(vbCreateInfo);

		VK_RESOURCE()->FlushBuffers();
	}

	RGDebugNode::~RGDebugNode()
	{
		SG_PROFILE_FUNCTION();

		Delete(mpDebugLinePipeline);
	}

	void RGDebugNode::Update()
	{
		mMessageBusMember.ListenFor<Scene::TreeNode*>("OnSelectedEntityChanged", SG_BIND_MEMBER_FUNC(OnSelectedEntityChanged));
	}

	void RGDebugNode::Reset()
	{
		SG_PROFILE_FUNCTION();

#ifdef SG_ENABLE_HDR
		ClearValue cv = {};
		cv.color = { 0.04f, 0.04f, 0.04f, 1.0f };
		AttachResource(0, { VK_RESOURCE()->GetRenderTarget("HDRColor"), mColorRtLoadStoreOp, cv, EResourceBarrier::efUndefined, EResourceBarrier::efShader_Resource });
#endif
		cv = {};
		cv.depthStencil.depth = 1.0f;
		cv.depthStencil.stencil = 0;
		AttachResource(1, { mContext.depthRt, mDepthRtLoadStoreOp, cv, EResourceBarrier::efUndefined, EResourceBarrier::efDepth_Stencil });

#ifndef SG_ENABLE_HDR
		ClearValue cv = {};
		cv.color = { 0.04f, 0.04f, 0.04f, 1.0f };
		AttachResource(0, { mContext.colorRts[frameIndex], mColorRtLoadStoreOp, cv });
#endif
	}

	void RGDebugNode::Prepare(VulkanRenderPass* pRenderpass)
	{
		SG_PROFILE_FUNCTION();

		mpDebugLinePipeline = VulkanPipeline::Builder(mContext.device)
			.BindSignature(mpDebugLinePipelineSignature)
			.BindRenderPass(pRenderpass)
			.SetColorBlend(false)
			.SetDynamicStates()
			.SetRasterizer(ECullMode::eNone)
			.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
			.Build();
	}

	void RGDebugNode::Draw(DrawInfo& context)
	{
		SG_PROFILE_FUNCTION();

		auto DrawDebugBox = [this](VulkanCommandBuffer& cmd)
		{
			UInt64 offset[] = { 0 };
			auto& pVertexBuffer = *VK_RESOURCE()->GetBuffer("debug_geo");
			cmd.BindVertexBuffer(0, 1, pVertexBuffer, offset);
			cmd.BindPipelineSignatureNonDynamic(mpDebugLinePipelineSignature.get());
			cmd.BindPipeline(mpDebugLinePipeline);

			cmd.PushConstants(mpDebugLinePipelineSignature.get(), EShaderStage::efVert, sizeof(Matrix4f), 0, &mDebugObjectModelMat);

			cmd.Draw(12 * 2, 1, 0, 0);
		};

		auto& cmd = *context.pCmd;

		if (mpSelectedEntityTreeNode && mpSelectedEntityTreeNode->pEntity->HasComponent<MeshComponent>())
		{
			auto& mesh = mpSelectedEntityTreeNode->pEntity->GetComponent<MeshComponent>();
			mDebugObjectModelMat = glm::translate(Matrix4f(1.0f), AABBCenter(mesh.aabb)) *
				glm::scale(Matrix4f(1.0f), AABBExtent(mesh.aabb));

			DrawDebugBox(cmd);
		}
		else if (mpSelectedEntityTreeNode && mpSelectedEntityTreeNode->pEntity->HasComponent<DDGIVolumnComponent>())
		{
			auto& ddgiVolumn = mpSelectedEntityTreeNode->pEntity->GetComponent<DDGIVolumnComponent>();
			mDebugObjectModelMat = Matrix4f(1.0f) * glm::translate(Matrix4f(1.0f), AABBCenter(ddgiVolumn.volumn)) *
				glm::scale(Matrix4f(1.0f), AABBExtent(ddgiVolumn.volumn));

			DrawDebugBox(cmd);
		}
		else
		{
			auto sceneAABB = SSystem()->GetRenderDataBuilder()->GetSceneAABB();
			mDebugObjectModelMat = Matrix4f(1.0f) * glm::translate(Matrix4f(1.0f), AABBCenter(sceneAABB)) *
				glm::scale(Matrix4f(1.0f), AABBExtent(sceneAABB));

			DrawDebugBox(cmd);
		}
	}

	void RGDebugNode::OnSelectedEntityChanged(Scene::TreeNode* pTreeNode)
	{
		mpSelectedEntityTreeNode = pTreeNode;
	}

}