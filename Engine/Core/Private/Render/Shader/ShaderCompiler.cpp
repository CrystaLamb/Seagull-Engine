#include "StdAfx.h"
#include "Render/Shader/ShaderComiler.h"

#include "System/System.h"
#include "System/FileSystem.h"
#include "System/Logger.h"
#include "Memory/Memory.h"
#include "Render/Shader/ShaderLibrary.h"
#include "Profile/Profile.h"

#include "spirv-cross/spirv_cross.hpp"

#include "eastl/stack.h"

namespace SG
{

	static EShaderDataType _SPIRVTypeToShaderDataType(const spirv_cross::SPIRType& type)
	{
		if (type.basetype == spirv_cross::SPIRType::Float)
		{
			if (type.columns == 1)
			{
				if (type.vecsize == 1)
					return EShaderDataType::eFloat;
				else if (type.vecsize == 2)
					return EShaderDataType::eFloat2;
				else if (type.vecsize == 3)
					return EShaderDataType::eFloat3;
				else if (type.vecsize == 4)
					return EShaderDataType::eUnorm4; // TODO: what is the different of eFloat4 and eUnorm4?
			}
			else if (type.columns == 3 && type.vecsize == 3)
				return EShaderDataType::eMat3;
			else if (type.columns == 4 && type.vecsize == 4)
				return EShaderDataType::eMat4;
		}
		else if (type.basetype == spirv_cross::SPIRType::Int)
		{
			if (type.vecsize == 1)
				return EShaderDataType::eInt;
			else if (type.vecsize == 2)
				return EShaderDataType::eInt2;
			else if (type.vecsize == 3)
				return EShaderDataType::eInt3;
			else if (type.vecsize == 4)
				return EShaderDataType::eInt4;
		}
		else if (type.basetype == spirv_cross::SPIRType::UInt)
			if (type.vecsize == 1)
				return EShaderDataType::eUInt4;
		else if (type.basetype == spirv_cross::SPIRType::Boolean)
			return EShaderDataType::eBool;

		SG_LOG_ERROR("Unsupported shader data type yet!");
		SG_ASSERT(false);
		return EShaderDataType::eUndefined;
	}

	bool ShaderCompiler::LoadSPIRVShader(const string& binShaderName, RefPtr<Shader> pShader)
	{
		SG_PROFILE_FUNCTION();

		UInt8 shaderBits = 0;
		if (!FileSystem::Exist(EResourceDirectory::eShader_Binarires, ""))
		{
			SG_LOG_WARN("You must put all the SPIRV binaries in the ShaderBin folder!");
			return false;
		}

		for (UInt32 i = 0; i < (UInt32)EShaderStage::NUM_STAGES; ++i)
		{
			string actualName = binShaderName;
			switch (i)
			{
			case 0: actualName += "-vert.spv"; break;
			case 1:	actualName += "-tesc.spv"; break;
			case 2:	actualName += "-tese.spv"; break;
			case 3:	actualName += "-geom.spv"; break;
			case 4:	actualName += "-frag.spv"; break;
			case 5:	actualName += "-comp.spv"; break;
			}

			ReadInShaderData(actualName, i, pShader, shaderBits);
			pShader->mShaderStages[EShaderStage(1 << i)].name = binShaderName;
		}

		if (shaderBits == 0)
		{
			SG_LOG_ERROR("No SPIRV shader is found! (%s)", binShaderName.c_str());
			return false;
		}
	
		return ReflectSPIRV(pShader);
	}

	bool ShaderCompiler::LoadSPIRVShader(const string& vertShaderName, const string& fragShaderName, RefPtr<Shader> pShader)
	{
		SG_PROFILE_FUNCTION();

		UInt8 shaderBits = 0;
		if (!FileSystem::Exist(EResourceDirectory::eShader_Binarires, ""))
		{
			SG_LOG_WARN("You must put all the SPIRV binaries in the ShaderBin folder!");
			return false;
		}

		string vertName = vertShaderName + "-vert.spv";
		string fragName = fragShaderName + "-frag.spv";

		ReadInShaderData(vertName, 0, pShader, shaderBits);
		ReadInShaderData(fragName, 4, pShader, shaderBits);

		if ((shaderBits & (1 << 0)) == 0 || (shaderBits & (1 << 4)) == 0) // if vert or frag stage is missing
		{
			pShader->mShaderStages.clear();

			SG_LOG_WARN("Necessary shader stages(vert or frag) is/are missing!");
			return false;
		}

		if (shaderBits == 0)
		{
			SG_LOG_ERROR("No SPIRV shader is found! (vert: %s, frag: %s)", vertShaderName.c_str(), fragShaderName.c_str());
			return false;
		}

		pShader->mShaderStages[EShaderStage::efVert].name = vertShaderName;
		pShader->mShaderStages[EShaderStage::efFrag].name = fragShaderName;
		return ReflectSPIRV(pShader);
	}

	bool ShaderCompiler::CompileGLSLShader(const string& binShaderName, RefPtr<Shader> pShader)
	{
		SG_PROFILE_FUNCTION();

		UInt8 shaderBits = 0;
		FileSystem::ExistOrCreate(EResourceDirectory::eShader_Binarires, ""); // create ShaderBin folder if it doesn't exist

		string folderPath = "";
		Size beginPos = 0;
		Size slashPos = binShaderName.find_first_of('/', beginPos);
		while (slashPos != string::npos) // this shader is inside a folder
		{
			folderPath += binShaderName.substr(beginPos, slashPos - beginPos + 1);
			beginPos = slashPos + 1;
			slashPos = binShaderName.find_first_of('/', beginPos);
		}
		FileSystem::ExistOrCreate(EResourceDirectory::eShader_Binarires, folderPath); // create folders in ShaderBin

		for (UInt32 i = 0; i < (UInt32)EShaderStage::NUM_STAGES; ++i)
		{
			string extension;
			switch (i)
			{
			case 0: extension = ".vert"; break;
			case 1:	extension = ".tesc"; break;
			case 2:	extension = ".tese"; break;
			case 3:	extension = ".geom"; break;
			case 4:	extension = ".frag"; break;
			case 5:	extension = ".comp"; break;
			}

			string actualName = binShaderName + extension;
			if (FileSystem::Exist(EResourceDirectory::eShader_Sources, actualName.c_str(), SG_ENGINE_DEBUG_BASE_OFFSET))
			{
				bool bForceToRecompile = false;
				// Get the time stamp of this shader to check if this shader should force to recompile.
				TimePoint prevTp = ShaderLibrary::GetInstance()->GetShaderTimeStamp(actualName);
				TimePoint currTp = FileSystem::GetFileLastWriteTime(EResourceDirectory::eShader_Sources, actualName.c_str(), SG_ENGINE_DEBUG_BASE_OFFSET);
				if (!prevTp.IsValid() || currTp > prevTp) // invalid means we can't find shader_env.ini, so you should recompile the shader anyway.
				{
					bForceToRecompile = true;
					SG_LOG_DEBUG("Force to compile: %s", actualName.c_str());
				}

				string compiledName = actualName;
				compiledName[actualName.find('.')] = '-';
				compiledName += ".spv";

				// Skip this to force recompile shader.
				if (!bForceToRecompile && FileSystem::Exist(EResourceDirectory::eShader_Binarires, compiledName.c_str())) // already compiled this shader once, skip it.
				{
					shaderBits |= (1 << i); // mark as exist.
					continue;
				}

				string pOut = FileSystem::GetResourceFolderPath(EResourceDirectory::eShader_Binarires) + binShaderName + "-" +
					extension.substr(1, extension.size() - 1) + "-compile.log";

#if SG_FORCE_USE_VULKAN_SDK
				if (CompileShaderVkSDK(actualName, compiledName, pOut))
#else
				if (CompileShaderVendor(actualName, compiledName, pOut))
#endif
				{
					Size splitPos = pOut.find_last_of('/');
					if (!CheckCompileError(actualName, folderPath + pOut.substr(splitPos + 1, pOut.size() - splitPos - 1)))
						shaderBits |= (1 << i); // record what shader stage we had compiled
				}
				else
					SG_LOG_ERROR("Failed to run shader compiler!");
			}
		}

		if (shaderBits == 0)
			return false;

		return LoadSPIRVShader(binShaderName, pShader);
	}

	bool ShaderCompiler::CompileGLSLShader(const string& vertShaderName, const string& fragShaderName, RefPtr<Shader> pShader)
	{
		SG_PROFILE_FUNCTION();

		UInt8 shaderBits = 0;
		FileSystem::ExistOrCreate(EResourceDirectory::eShader_Binarires, ""); // create ShaderBin folder if it doesn't exist

		string vertActualName = vertShaderName + ".vert";
		if (FileSystem::Exist(EResourceDirectory::eShader_Sources, vertActualName.c_str(), SG_ENGINE_DEBUG_BASE_OFFSET))
		{
			string folderPath = "";
			Size beginPos = 0;
			Size slashPos = vertShaderName.find_first_of('/', beginPos);
			while (slashPos != string::npos) // this shader is inside a folder
			{
				folderPath += vertShaderName.substr(beginPos, slashPos - beginPos + 1);
				beginPos = slashPos + 1;
				slashPos = vertShaderName.find_first_of('/', beginPos);
			}
			FileSystem::ExistOrCreate(EResourceDirectory::eShader_Binarires, folderPath); // create folders in ShaderBin

			bool bForceToRecompile = false;
			// Get the time stamp of this shader to check if this shader should force to recompile.
			TimePoint prevTp = ShaderLibrary::GetInstance()->GetShaderTimeStamp(vertActualName);
			TimePoint currTp = FileSystem::GetFileLastWriteTime(EResourceDirectory::eShader_Sources, vertActualName.c_str(), SG_ENGINE_DEBUG_BASE_OFFSET);
			if (!prevTp.IsValid() || currTp > prevTp) // invalid means we can't find shader_env.ini, so you should recompile the shader anyway.
			{
				bForceToRecompile = true;
				SG_LOG_DEBUG("Force to compile: %s", vertActualName.c_str());
			}

			string vertCompiledName = vertActualName;
			vertCompiledName[vertActualName.find('.')] = '-';
			vertCompiledName += ".spv";

			if (!bForceToRecompile && FileSystem::Exist(EResourceDirectory::eShader_Binarires, vertCompiledName.c_str())) // already compiled this shader once, skip it.
			{
				shaderBits |= (1 << 0); // mark as exist.
			}
			else
			{
				string pOut = FileSystem::GetResourceFolderPath(EResourceDirectory::eShader_Binarires) + vertShaderName + "-vert-compile.log";

#if SG_FORCE_USE_VULKAN_SDK
				if (CompileShaderVkSDK(vertActualName, vertCompiledName, pOut))
#else
				if (CompileShaderVendor(vertActualName, vertCompiledName, pOut))
#endif
				{
					Size splitPos = pOut.find_last_of('/');
					if (!CheckCompileError(vertActualName, folderPath + pOut.substr(splitPos + 1, pOut.size() - splitPos - 1)))
						shaderBits |= (1 << 0); // record what shader stage we had compiled
				}
				else
					SG_LOG_ERROR("Failed to run shader compiler!");
			}
		}

		string fragActualName = fragShaderName + ".frag";
		if (FileSystem::Exist(EResourceDirectory::eShader_Sources, fragActualName.c_str(), SG_ENGINE_DEBUG_BASE_OFFSET))
		{
			string folderPath = "";
			Size beginPos = 0;
			Size slashPos = fragActualName.find_first_of('/', beginPos);
			while (slashPos != string::npos) // this shader is inside a folder
			{
				folderPath += fragActualName.substr(beginPos, slashPos - beginPos + 1);
				beginPos = slashPos + 1;
				slashPos = fragActualName.find_first_of('/', beginPos);
			}
			FileSystem::ExistOrCreate(EResourceDirectory::eShader_Binarires, folderPath); // create folders in ShaderBin

			bool bForceToRecompile = false;
			// Get the time stamp of this shader to check if this shader should force to recompile.
			TimePoint prevTp = ShaderLibrary::GetInstance()->GetShaderTimeStamp(fragActualName);
			TimePoint currTp = FileSystem::GetFileLastWriteTime(EResourceDirectory::eShader_Sources, fragActualName.c_str(), SG_ENGINE_DEBUG_BASE_OFFSET);
			if (currTp > prevTp)
			{
				bForceToRecompile = true;
				SG_LOG_DEBUG("Force to compile: %s", fragActualName.c_str());
			}

			string fragCompiledName = fragActualName;
			fragCompiledName[fragActualName.find('.')] = '-';
			fragCompiledName += ".spv";

			if (!bForceToRecompile && FileSystem::Exist(EResourceDirectory::eShader_Binarires, fragCompiledName.c_str())) // already compiled this shader once, skip it.
				shaderBits |= (1 << 4); // mark as exist.
			else
			{
				string pOut = FileSystem::GetResourceFolderPath(EResourceDirectory::eShader_Binarires) + fragShaderName + "-frag-compile.log";

#if SG_FORCE_USE_VULKAN_SDK
				if (CompileShaderVkSDK(fragActualName, fragCompiledName, pOut))
#else
				if (CompileShaderVendor(fragActualName, fragCompiledName, pOut))
#endif
				{
					Size splitPos = pOut.find_last_of('/');
					if (!CheckCompileError(fragActualName, folderPath + pOut.substr(splitPos + 1, pOut.size() - splitPos - 1)))
						shaderBits |= (1 << 4); // record what shader stage we had compiled
				}
				else
					SG_LOG_ERROR("Failed to run shader compiler!");
			}
		}

		if (shaderBits == 0)
			return false;

		return LoadSPIRVShader(vertShaderName, fragShaderName, pShader);
	}

	bool ShaderCompiler::ReadInShaderData(const string& name, UInt32 stage, RefPtr<Shader> pShader, UInt8& checkFlag)
	{
		SG_PROFILE_FUNCTION();

		if (FileSystem::Open(EResourceDirectory::eShader_Binarires, name.c_str(), EFileMode::efRead_Binary))
		{
			const Size sizeInByte = FileSystem::FileSize();
			pShader->mShaderStages[(EShaderStage)(1 << stage)] = {}; // insert a null vector
			auto& shaderBinary = pShader->mShaderStages[(EShaderStage)(1 << stage)];
			shaderBinary.binary.resize(sizeInByte);

			FileSystem::Read(shaderBinary.binary.data(), sizeInByte);

			checkFlag |= (1 << stage);
			FileSystem::Close();
			return true;
		}
		return false;
	}

	bool ShaderCompiler::CompileShaderVkSDK(const string& actualName, const string& compiledName, const string& pOut)
	{
		SG_PROFILE_FUNCTION();

		char* glslc = "";
		Size num = 1;
		_dupenv_s(&glslc, &num, "VULKAN_SDK");
		string exePath = glslc;
		exePath += "\\Bin32\\glslc.exe ";

		string shaderPath = FileSystem::GetResourceFolderPath(EResourceDirectory::eShader_Sources, SG_ENGINE_DEBUG_BASE_OFFSET);
		shaderPath += actualName;
		string outputPath = FileSystem::GetResourceFolderPath(EResourceDirectory::eShader_Binarires) + compiledName;

		exePath += shaderPath;
		exePath += " -o ";
		exePath += outputPath;

		const char* args[3] = { shaderPath.c_str(), "-o", outputPath.c_str() };

		// create a process to use vulkanSDK to compile shader sources to binary (spirv)
		if (SSystem()->RunProcess(exePath, pOut.c_str()) != 0)
			return false;
		return true;
	}

	bool ShaderCompiler::CompileShaderVendor(const string& actualName, const string& compiledName, const string& pOut)
	{
		SG_PROFILE_FUNCTION();

		string exePath = FileSystem::GetResourceFolderPath(EResourceDirectory::eVendor, SG_ENGINE_DEBUG_BASE_OFFSET);
		exePath += "glslc.exe ";

		string shaderPath = FileSystem::GetResourceFolderPath(EResourceDirectory::eShader_Sources, SG_ENGINE_DEBUG_BASE_OFFSET);
		shaderPath += actualName;
		string outputPath = FileSystem::GetResourceFolderPath(EResourceDirectory::eShader_Binarires) + compiledName;

		exePath += shaderPath;
		exePath += " -o ";
		exePath += outputPath;

		const char* args[3] = { shaderPath.c_str(), "-o", outputPath.c_str() };

		// create a process to use vulkanSDK to compile shader sources to binary (spirv)
		if (SSystem()->RunProcess(exePath, pOut.c_str()) != 0)
			return false;
		return true;
	}

	bool ShaderCompiler::CheckCompileError(const string& actualName, const string& outputMessage)
	{
		SG_PROFILE_FUNCTION();

		bool bHaveError = false;
		if (FileSystem::FileSize(EResourceDirectory::eShader_Binarires, outputMessage) != 0) // failed to compile shader, log out the error message
		{
			bHaveError = true;
			if (FileSystem::Open(EResourceDirectory::eShader_Binarires, outputMessage.c_str(), EFileMode::efRead))
			{
				string errorMessage = "";
				errorMessage.resize(FileSystem::FileSize());
				FileSystem::Read(errorMessage.data(), errorMessage.size() * sizeof(char));
				SG_LOG_ERROR("Failed to compile shader: %s\n%s", actualName.c_str(), errorMessage.c_str());
				FileSystem::Close();
			}
		}
		if (!FileSystem::RemoveFile(EResourceDirectory::eShader_Binarires, outputMessage))
			SG_LOG_DEBUG("Failed to remove file: %s", outputMessage.c_str());
		return bHaveError;
	}

	bool ShaderCompiler::ReflectSPIRV(RefPtr<Shader> pShader)
	{
		SG_PROFILE_FUNCTION();

		for (auto& beg = pShader->mShaderStages.begin(); beg != pShader->mShaderStages.end(); ++beg)
		{
			auto& shaderData = beg->second;
			if (shaderData.binary.empty())
				continue;

			spirv_cross::Compiler compiler(reinterpret_cast<const UInt32*>(shaderData.binary.data()), shaderData.binary.size() / sizeof(UInt32));
			auto& stageData = compiler.get_entry_points_and_stages();

			// we only have one shader stage compile once for now.
			spv::ExecutionModel executionModel = {};
			for (auto& data : stageData)
			{
				pShader->mEntryPoint = data.name.c_str();
				executionModel = data.execution_model;
			}

			spirv_cross::ShaderResources shaderResources = compiler.get_shader_resources();

			// shader stage input collection
			if (executionModel == spv::ExecutionModelVertex) // for now, we only collect the attributes of vertex stage
			{
				OrderSet<ShaderAttributesLayout::ElementType> orderedInputLayout;
				for (auto& input : shaderResources.stage_inputs) // collect shader stage input info
				{
					const auto& type = compiler.get_type(input.type_id);
					const auto location = compiler.get_decoration(input.id, spv::DecorationLocation);
					orderedInputLayout.emplace(location, ShaderAttributesLayout::ElementType{ _SPIRVTypeToShaderDataType(type), input.name.c_str() });
				}

				for (auto& element : orderedInputLayout)
					shaderData.stageInputLayout.Emplace(eastl::move(element.second));
			}

			// shader push constants collection
			for (auto& pushConstant : shaderResources.push_constant_buffers)
			{
				const auto& type = compiler.get_type(pushConstant.type_id);
				if (type.basetype == spirv_cross::SPIRType::Struct)
				{
					for (UInt32 i = 0; i < type.member_types.size(); ++i)
					{
						const auto memberTypeID = type.member_types[i];
						const char* name = compiler.get_member_name(type.self, i).c_str();
						const auto& memberType = compiler.get_type(memberTypeID);
						shaderData.pushConstantLayout.Emplace(_SPIRVTypeToShaderDataType(memberType), name);
					}
				}
				else
				{
					const char* name = compiler.get_name(pushConstant.id).c_str();
					shaderData.pushConstantLayout.Emplace(_SPIRVTypeToShaderDataType(type), name);
				}
			}

			// shader uniform buffers collection
			for (auto& ubo : shaderResources.uniform_buffers)
			{
				const auto& type = compiler.get_type(ubo.type_id);
				const char* name = compiler.get_name(ubo.id).c_str();
				const auto set = compiler.get_decoration(ubo.id, spv::DecorationDescriptorSet);
				pShader->mSetIndices.emplace(set);
				const auto binding = compiler.get_decoration(ubo.id, spv::DecorationBinding);
				const UInt32 key = set * 10 + binding; // calculate key value for the set and binding

				if (pShader->mUniformBufferLayout.Exist(name)) // may be another stage is using it, too.
				{
					auto& data = pShader->mUniformBufferLayout.Get(name);
					data.stage = data.stage | beg->first;
					continue;
				}

				ShaderAttributesLayout layout = {};
				eastl::stack<spirv_cross::SPIRType> memberTypes;
				memberTypes.push(type);
				while (!memberTypes.empty())
				{
					auto currentType = memberTypes.top();
					memberTypes.pop();

					for (UInt32 i = 0; i < currentType.member_types.size(); ++i)
					{
						const auto memberTypeID = currentType.member_types[i];
						const char* memberName = compiler.get_member_name(currentType.self, i).c_str();
						const auto& memberType = compiler.get_type(memberTypeID);

						if (memberType.basetype != spirv_cross::SPIRType::Struct)
							layout.Emplace(_SPIRVTypeToShaderDataType(memberType), memberName);
						else
							memberTypes.push(memberType);
					}
				}

				pShader->mUniformBufferLayout.Emplace(name, { key, layout, beg->first });
			}

			// shader storage buffer collection
			for (auto& ssbo : shaderResources.storage_buffers)
			{
				const auto& type = compiler.get_type(ssbo.type_id);
				const char* name = compiler.get_name(ssbo.id).c_str();
				const auto set = compiler.get_decoration(ssbo.id, spv::DecorationDescriptorSet);
				pShader->mSetIndices.emplace(set);
				const auto binding = compiler.get_decoration(ssbo.id, spv::DecorationBinding);
				const UInt32 key = set * 10 + binding; // calculate key value for the set and binding

				if (pShader->mStorageBufferLayout.Exist(name)) // may be another stage is using it, too.
				{
					auto& data = pShader->mStorageBufferLayout.Get(name);
					data.stage = data.stage | beg->first;
					continue;
				}

				ShaderAttributesLayout layout = {};
				eastl::stack<spirv_cross::SPIRType> memberTypes;
				memberTypes.push(type);
				while (!memberTypes.empty())
				{
					auto currentType = memberTypes.top();
					memberTypes.pop();

					for (UInt32 i = 0; i < currentType.member_types.size(); ++i)
					{
						const auto memberTypeID = currentType.member_types[i];
						const char* memberName = compiler.get_member_name(currentType.self, i).c_str();
						const auto& memberType = compiler.get_type(memberTypeID);

						if (memberType.basetype != spirv_cross::SPIRType::Struct)
							layout.Emplace(_SPIRVTypeToShaderDataType(memberType), memberName);
						else
							memberTypes.push(memberType);
					}
				}

				pShader->mStorageBufferLayout.Emplace(name, { key, layout, beg->first });
			}

			// shader combine sampler image collection
			for (auto& image : shaderResources.sampled_images)
			{
				const char* name = compiler.get_name(image.id).c_str();
				const auto set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);
				const auto binding = compiler.get_decoration(image.id, spv::DecorationBinding);
				const UInt32 key = set * 10 + binding; // calculate key value for the set and binding
				pShader->mSetIndices.emplace(set);

				shaderData.sampledImageLayout.Emplace(name, key);
			}
		}

		return true;
	}

}