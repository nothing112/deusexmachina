#include "Main.h"

#include <IO/IOServer.h>
#include <IO/FSBrowser.h>
#include <IO/PathUtils.h>
#include <Data/StringUtils.h>
#include <Data/ParamsUtils.h>
#include <DEMShaderCompiler/DEMShaderCompilerDLL.h>

CString			ProjectDir;
bool			ExportDescs;
bool			ExportResources;
bool			IncludeSM30ShadersAndEffects;	// For legacy D3D9 API
bool			DebugShaders;
int				Verbose = VL_ERROR;
int				ExternalVerbosity = VL_ALWAYS;	// Only always printed messages by default

CArray<CString>	FilesToPack;
CArray<U32>		ShadersToPack;
//!!!control duplicates! (immediately after mangle path, forex)
CToolFileLists	InFileLists;
CToolFileLists	OutFileLists;

CClassCodeMap	ClassToFOURCC;
CDict<CStrID, Data::PDataScheme> Schemes;

// Debug command line:
// -export -waitkey -v 5 -sm3 -ds -proj ../../../../InsanePoet/Content -build ../../../../InsanePoet/Bin

int main(int argc, const char** argv)
{
	nCmdLineArgs Args(argc, argv);

	// If true, will re-export files from Src to Export before packing
	ExportDescs = Args.GetBoolArg("-er") || Args.GetBoolArg("-export");
	ExportResources = Args.GetBoolArg("-ed") || Args.GetBoolArg("-export");
	IncludeSM30ShadersAndEffects = Args.GetBoolArg("-sm3");
	DebugShaders = Args.GetBoolArg("-ds");

	// If true, application will wait for key before exit
	bool WaitKey = Args.GetBoolArg("-waitkey");

	// Verbosity level, where 0 is silence
	Verbose = Args.GetIntArg("-v");

	// Verbosity level for external tools
	ExternalVerbosity = Args.GetIntArg("-ev");

	// Project directory, where all content is placed. Will be a base directory for all data.
	ProjectDir = CString(Args.GetStringArg("-proj"));
	ProjectDir.Trim(" \r\n\t\\/", false);
	if (ProjectDir.IsEmpty()) EXIT_APP_FAIL;

	// Build directory, to where final data will be saved.
	CString BuildDir(Args.GetStringArg("-build"));
	BuildDir.Trim(" \r\n\t\\/", false);
	if (BuildDir.IsEmpty()) EXIT_APP_FAIL;

	n_msg(VL_ALWAYS, SEP_LINE TOOL_NAME" v"VERSION" for DeusExMachina engine\n(c) Vladimir \"Niello\" Orlov 2011-2015\n"SEP_LINE"\n");

	IO::CIOServer IOServer;
	CString WorkingDir;
	Sys::GetWorkingDirectory(WorkingDir);
	ProjectDir = IOSrv->ResolveAssigns(ProjectDir);
	BuildDir = IOSrv->ResolveAssigns(BuildDir);
	IOSrv->SetAssign("Proj", PathUtils::GetAbsolutePath(WorkingDir, ProjectDir));
	IOSrv->SetAssign("Build", PathUtils::GetAbsolutePath(WorkingDir, BuildDir));

	n_msg(VL_INFO, "Project directory: %s\nBuild directory: %s\n", ProjectDir.CStr(), BuildDir.CStr());

	Data::PParams PathList;
	ParamsUtils::LoadParamsFromHRD("Proj:SrcPathList.hrd", PathList);
	if (PathList.IsValidPtr())
	{
		for (UPTR i = 0; i < PathList->GetCount(); ++i)
			IOSrv->SetAssign(PathList->Get(i).GetName().CStr(), IOSrv->ResolveAssigns(PathList->Get<CString>(i)));
		PathList = NULL;
	}

	ParamsUtils::LoadParamsFromHRD("Proj:PathList.hrd", PathList);
	if (PathList.IsValidPtr())
	{
		for (UPTR i = 0; i < PathList->GetCount(); ++i)
			IOSrv->SetAssign(PathList->Get(i).GetName().CStr(), IOSrv->ResolveAssigns(PathList->Get<CString>(i)));
		PathList = NULL;

		IOSrv->CopyFile("Proj:PathList.hrd", "Build:PathList.hrd");
	}

	//???rewrite HRD/PRM to return CData? change root symbol in grammar, so could store array and anything else
	Data::PParams ClassToFOURCCDesc;
	ParamsUtils::LoadParamsFromHRD("Proj:ClassToFOURCC.hrd", ClassToFOURCCDesc);
	Data::PDataArray ClassToFOURCCArray;
	if (ClassToFOURCCDesc.IsValidPtr() &&
		ClassToFOURCCDesc->Get<Data::PDataArray>(ClassToFOURCCArray, CStrID("List")) &&
		ClassToFOURCCArray->GetCount())
	{
		for (UPTR i = 0; i < ClassToFOURCCArray->GetCount(); ++i)
		{
			Data::PParams Prm = ClassToFOURCCArray->Get<Data::PParams>(i);
			ClassToFOURCC.Add(Prm->Get<CString>(CStrID("Name")), Data::CFourCC(Prm->Get<CString>(CStrID("Code")).CStr()));
		}
	}
	ClassToFOURCCArray = NULL;
	ClassToFOURCCDesc = NULL;

	if (!ParamsUtils::LoadDataSerializationSchemesFromDSS("Home:DataSchemes/SceneNodes.dss", Schemes))
	{
		n_msg(VL_ERROR, "BBuilder: Failed to read 'Home:DataSchemes/SceneNodes.dss'\n");
		EXIT_APP_FAIL;
	}

	// Process levels

	Sys::Log("\n"SEP_LINE"Processing levels and entities:\n"SEP_LINE);

	CString ExportFilePath("Game:Main.prm");
	Data::PParams Desc;
	if (ExportDescs)
	{
		IOSrv->CreateDirectory("Levels:");
		ParamsUtils::LoadParamsFromHRD("SrcGame:Main.hrd", Desc);
		ParamsUtils::SaveParamsToPRM(ExportFilePath, *Desc);
	}
	else ParamsUtils::LoadParamsFromPRM(ExportFilePath, Desc);

	if (!Desc.IsValidPtr())
	{
		n_msg(VL_ERROR, "Error loading main game desc\n");
		EXIT_APP_FAIL;
	}

	FilesToPack.InsertSorted(ExportFilePath);

	IO::CFSBrowser Browser;
	if (!Browser.SetAbsolutePath(ExportDescs ? "SrcLevels:" : "Levels:"))
	{
		n_msg(VL_ERROR, "Could not open directory '%s' for reading!\n", Browser.GetCurrentPath().CStr());
		EXIT_APP_FAIL;
	}

	if (!Browser.IsCurrDirEmpty()) do
	{
		if (Browser.IsCurrEntryFile())
		{
			if (!PathUtils::CheckExtension(Browser.GetCurrEntryName(), ExportDescs ? "hrd" : "prm")) continue;

			CString FileNoExt = PathUtils::ExtractFileNameWithoutExtension(Browser.GetCurrEntryName());
			n_msg(VL_INFO, "Processing level '%s'...\n", FileNoExt.CStr());

			ExportFilePath = "Levels:" + FileNoExt + ".prm";
			Data::PParams LevelDesc;
			if (ExportDescs)
			{
				ParamsUtils::LoadParamsFromHRD("SrcLevels:" + Browser.GetCurrEntryName(), LevelDesc);

				// Convert entity props from names to FourCC
				Data::PParams SubDesc;
				if (LevelDesc->Get<Data::PParams>(SubDesc, CStrID("Entities")))
				{
					for (UPTR i = 0; i < SubDesc->GetCount(); ++i)
					{
						Data::PParams EntityDesc = SubDesc->Get<Data::PParams>(i);
						Data::PDataArray Props;
						if (EntityDesc->Get<Data::PDataArray>(Props, CStrID("Props")) && Props->GetCount())
							ConvertPropNamesToFourCC(Props);
					}
				}

				ParamsUtils::SaveParamsToPRM(ExportFilePath, *LevelDesc);
			}
			else ParamsUtils::LoadParamsFromPRM(ExportFilePath, LevelDesc);

			if (!LevelDesc.IsValidPtr())
			{
				n_msg(VL_ERROR, "Error loading level '%s' desc\n", FileNoExt.CStr());
				continue;
			}

			FilesToPack.InsertSorted(ExportFilePath);

			if (!ProcessLevel(*LevelDesc, FileNoExt))
			{
				n_msg(VL_ERROR, "Error processing level '%s'\n", FileNoExt.CStr());
				continue;
			}
		}
	}
	while (Browser.NextCurrDirEntry());

	Sys::Log("\n"SEP_LINE"Processing entity templates:\n"SEP_LINE);

	if (!ProcessEntityTplsInFolder(IOSrv->ResolveAssigns("SrcEntityTpls:"), IOSrv->ResolveAssigns("EntityTpls:")))
	{
		n_msg(VL_ERROR, "Error procesing entity templates!\n");
		EXIT_APP_FAIL;
	}

	Sys::Log("\n"SEP_LINE"Processing items:\n"SEP_LINE);

	if (!ProcessDescsInFolder(IOSrv->ResolveAssigns("SrcItems:"), IOSrv->ResolveAssigns("Items:")))
	{
		n_msg(VL_ERROR, "Error procesing items!\n");
		EXIT_APP_FAIL;
	}

	Sys::Log("\n"SEP_LINE"Processing quests:\n"SEP_LINE);

	if (!ProcessQuestsInFolder(IOSrv->ResolveAssigns("SrcQuests:"), IOSrv->ResolveAssigns("Quests:")))
	{
		n_msg(VL_ERROR, "Error procesing quests!\n");
		EXIT_APP_FAIL;
	}

	Sys::Log("\n"SEP_LINE"Processing system data and resources:\n"SEP_LINE);

	if (!ProcessUISettingsDesc(CString("Src:UI/UI.hrd"), CString("Export:UI/UI.prm")))
	{
		n_msg(VL_ERROR, "Error procesing UI settings desc!\n");
		EXIT_APP_FAIL;
	}

	if (!ProcessDesc(CString("SrcInput:Layouts.hrd"), CString("Input:Layouts.prm")))
	{
		n_msg(VL_ERROR, "Error procesing input layouts desc!\n");
		EXIT_APP_FAIL;
	}

	// Process render path descs (must be processed after effects)

	if (!Browser.SetAbsolutePath(ExportDescs ? "SrcShaders:" : "Shaders:"))
	{
		n_msg(VL_ERROR, "Could not open directory '%s' for reading!\n", Browser.GetCurrentPath().CStr());
		EXIT_APP_FAIL;
	}

	if (ExportDescs) IOSrv->CreateDirectory("Shaders:");

	if (!Browser.IsCurrDirEmpty()) do
	{
		if (Browser.IsCurrEntryFile())
		{
			if (!PathUtils::CheckExtension(Browser.GetCurrEntryName(), ExportDescs ? "hrd" : "rp")) continue;

			CString FileNoExt = PathUtils::ExtractFileNameWithoutExtension(Browser.GetCurrEntryName());
			n_msg(VL_INFO, "Processing render path '%s'...\n", FileNoExt.CStr());

			if (ExportDescs)
			{
				CString SrcFilePath = Browser.GetCurrentPath() + Browser.GetCurrEntryName();

				// Export referenced effects (EffectsWithGlobals and per-phase effect overrides)
				if (!ProcessRenderPathDesc(SrcFilePath))
				{
					n_msg(VL_ERROR, "Error processing render path desc '%s'\n", FileNoExt.CStr());
					EXIT_APP_FAIL;
				}

				CString RealExportFilePath = IOSrv->ResolveAssigns("Shaders:USM/" + FileNoExt + ".rp");
				if (!IsFileAdded(RealExportFilePath))
				{
					if (!ExportRenderPath(SrcFilePath, RealExportFilePath, false, DebugShaders)) EXIT_APP_FAIL;
					FilesToPack.InsertSorted(RealExportFilePath);
				}

				if (IncludeSM30ShadersAndEffects)
				{
					RealExportFilePath = IOSrv->ResolveAssigns("Shaders:SM_3_0/" + FileNoExt + ".rp");
					if (!IsFileAdded(RealExportFilePath))
					{
						if (!ExportRenderPath(SrcFilePath, RealExportFilePath, true, DebugShaders)) EXIT_APP_FAIL;
						FilesToPack.InsertSorted(RealExportFilePath);
					}
				}
			}
		}
	}
	while (Browser.NextCurrDirEntry());

	Sys::Log("\n"SEP_LINE"Running external tools:\n"SEP_LINE);

	if (RunExternalToolBatch(CStrID("CFCopy"), ExternalVerbosity, NULL, WorkingDir.CStr()) != 0) EXIT_APP_FAIL;
	if (RunExternalToolBatch(CStrID("CFLua"), ExternalVerbosity, NULL, WorkingDir.CStr()) != 0) EXIT_APP_FAIL;

	if (ShadersToPack.GetCount())
	{
		Sys::Log("\nPacking shader binaries...\n");

#ifdef _DEBUG
		CString DLLPath = IOSrv->ResolveAssigns("Home:../DEMShaderCompiler/DEMShaderCompiler_d.dll");
#else
		CString DLLPath = IOSrv->ResolveAssigns("Home:../DEMShaderCompiler/DEMShaderCompiler.dll");
#endif
		CString DBFilePath = IOSrv->ResolveAssigns("Shaders:ShaderDB.db3");
		CString OutputDir = PathUtils::GetAbsolutePath(IOSrv->ResolveAssigns("Home:"), IOSrv->ResolveAssigns("Shaders:Bin/"));
		if (!InitDEMShaderCompilerDLL(DLLPath, DBFilePath, OutputDir))
		{
			n_msg(VL_ERROR, "Error initializing DEMShaderCompiler.dll\n");
			EXIT_APP_FAIL;
		}

		CString ShaderIDs = StringUtils::FromInt(ShadersToPack[0]);
		for (UPTR i = 1; i < ShadersToPack.GetCount(); ++i)
		{
			ShaderIDs += ',';
			ShaderIDs += StringUtils::FromInt(ShadersToPack[i]);
		}
		CString LibPath = PathUtils::GetAbsolutePath(IOSrv->ResolveAssigns("Home:"), IOSrv->ResolveAssigns("Shaders:Shaders.slb"));
		UPTR ShadersPacked = DLLPackShaders(ShaderIDs.CStr(), LibPath.CStr());

		if (ShadersToPack.GetCount() > ShadersPacked)
			Sys::Log("Due to an error or lack of files only %d of %d shader binaries were packed\n", ShadersPacked, ShadersToPack.GetCount());
		else
			Sys::Log("%d shader binaries were packed successfully (%d requested + input signatures)\n", ShadersPacked, ShadersToPack.GetCount());

		FilesToPack.InsertSorted(LibPath);

		TermDEMShaderCompilerDLL();
	}

	Sys::Log("\n"SEP_LINE"Packing:\n"SEP_LINE);

	for (UPTR i = 0; i < FilesToPack.GetCount(); ++i)
	{
		 FilesToPack[i] = IOSrv->ResolveAssigns(FilesToPack[i]);
		 FilesToPack[i].ToLower();
	}
	FilesToPack.Sort();

	CString DestFile("Build:Export.npk");
	if (PackFiles(FilesToPack, DestFile, ProjectDir, CString("Export")))
	{
		n_msg(VL_INFO, "\nNPK file:      %s\nNPK file size: %.3f MB\n",
			IOSrv->ResolveAssigns(DestFile).CStr(),
			IOSrv->GetFileSize(DestFile) / (1024.f * 1024.f));
	}
	else
	{
		n_msg(VL_ERROR, "ERROR IN FILE GENERATION, DELETING NPK FILE\n");
		IOSrv->DeleteFile(DestFile);
		EXIT_APP_FAIL;
	}

	EXIT_APP_OK;
}
//---------------------------------------------------------------------

int ExitApp(bool NoError, bool WaitKey)
{
	if (!NoError) n_msg(VL_ERROR, "Building aborted due to errors.\n");
	if (WaitKey)
	{
		Sys::Log("\nPress any key to exit...\n");
		_getch();
	}

	Schemes.Clear();

	return NoError ? 0 : 1;
}
//---------------------------------------------------------------------
