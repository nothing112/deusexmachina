#pragma once
#ifndef __DEM_TOOLS_DEM_SHADER_COMPILER_DLL_H__
#define __DEM_TOOLS_DEM_SHADER_COMPILER_DLL_H__

#include <DEMShaderCompiler/ShaderCompiler.h>
#include <DEMShaderCompiler/ShaderReflection.h>

// Convenient wrappers for calling shader compiler DLL functions from an application.
// Include accompanying DEMShaderCompilerDLL.cpp file into a project from where a DLL will be called.

bool			InitDEMShaderCompilerDLL(const char* pDLLPath, const char* pDBFilePath, const char* pOutputDirectory);
bool			TermDEMShaderCompilerDLL();
const char*		DLLGetLastOperationMessages();
int				DLLCompileShader(const char* pSrcPath, EShaderType ShaderType, U32 Target, const char* pEntryPoint,
								 const char* pDefines, bool Debug, bool OnlyMetadata, U32& ObjectFileID, U32& InputSignatureFileID);
bool			DLLLoadShaderMetadataByObjectFileID(U32 ID, U32& OutTarget, CSM30ShaderMeta*& pOutD3D9Meta, CUSMShaderMeta*& pOutUSMMeta);
bool			DLLFreeShaderMetadata(CSM30ShaderMeta* pDLLAllocD3D9Meta, CUSMShaderMeta* pDLLAllocUSMMeta);
unsigned int	DLLPackShaders(const char* pCommaSeparatedShaderIDs, const char* pLibraryFilePath);

#endif
