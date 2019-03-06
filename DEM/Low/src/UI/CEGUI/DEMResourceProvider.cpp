#include <StdCfg.h>
#include "DEMResourceProvider.h"

#include <IO/IOServer.h>
#include <IO/FileSystem.h>
#include <IO/Stream.h>
#include <Data/StringUtils.h>

namespace CEGUI
{

void CDEMResourceProvider::loadRawDataContainer(const String& filename, RawDataContainer& output,
												const String& resourceGroup)
{
	n_assert2(!filename.empty(), "Filename supplied for data loading must be valid");

	String FinalFilename;
	if (resourceGroup.empty()) FinalFilename = d_defaultResourceGroup + filename;
	else
	{
		IPTR Idx = ResourceGroups.FindIndex(resourceGroup);
		if (Idx != INVALID_INDEX) FinalFilename = ResourceGroups.ValueAt(Idx) + filename;
		else FinalFilename = filename;
	}

#if CEGUI_STRING_CLASS == CEGUI_STRING_CLASS_UTF_32
	std::string Filename = String::convertUtf32ToUtf8(FinalFilename.getString());
#else
	const std::string& Filename = FinalFilename.getString();
#endif

	IO::PStream File = IOSrv->CreateStream(Filename.c_str());

	if (File && File->Open(IO::SAM_READ))
	{
		const UPTR Size = (UPTR)File->GetSize();
		unsigned char* const pBuffer = n_new_array(unsigned char, Size);
		const size_t BytesRead = File->Read(pBuffer, Size);
		if (BytesRead != Size)
		{
			n_delete_array(pBuffer);
			Sys::Error("CDEMResourceProvider::loadRawDataContainer() > a problem occurred while reading file: %s", Filename.c_str());
		}
		File->Close();

		output.setData(pBuffer);
		output.setSize(Size);
	}
	else
	{
		Sys::Error("CDEMResourceProvider::loadRawDataContainer() > can't open file: %s", Filename.c_str());
	}
}
//---------------------------------------------------------------------

void CDEMResourceProvider::unloadRawDataContainer(RawDataContainer& data)
{
	data.release();
}
//---------------------------------------------------------------------

void CDEMResourceProvider::setResourceGroupDirectory(const String& resourceGroup, const String& directory)
{
	if (directory.length() == 0) return;
	const String separators("\\/");
	if (String::npos == separators.find(directory[directory.length() - 1]))
		ResourceGroups.Add(resourceGroup, directory + '/');
	else ResourceGroups.Add(resourceGroup, directory);
}
//---------------------------------------------------------------------

const String& CDEMResourceProvider::getResourceGroupDirectory(const String& resourceGroup)
{
	return ResourceGroups[resourceGroup];
}
//---------------------------------------------------------------------

void CDEMResourceProvider::clearResourceGroupDirectory(const String& resourceGroup)
{
	ResourceGroups.Remove(resourceGroup);
}
//---------------------------------------------------------------------

size_t CDEMResourceProvider::getResourceGroupFileNames(std::vector<String>& out_vec,
													   const String& file_pattern,
													   const String& resource_group)
{
	String DirName;
	if (resource_group.empty()) DirName = d_defaultResourceGroup;
	else
	{
		IPTR Idx = ResourceGroups.FindIndex(resource_group);
		if (Idx != INVALID_INDEX) DirName = ResourceGroups.ValueAt(Idx);
		else DirName = "./";
	}

	std::string Pattern;
	std::string DirNameStr;
#if CEGUI_STRING_CLASS == CEGUI_STRING_CLASS_UTF_32
	DirNameStr = String::convertUtf32ToUtf8(DirName.getString());
	Pattern = DirNameStr + String::convertUtf32ToUtf8(file_pattern.c_str());
#else
	DirNameStr = DirName.getString();
	Pattern = DirNameStr + file_pattern;
#endif

    size_t Entries = 0;

	IO::PFileSystem FS;
	CString EntryName;
	IO::EFSEntryType EntryType;
	void* hDir = IOSrv->OpenDirectory(DirNameStr.c_str(), CString::Empty, FS, EntryName, EntryType);
	if (hDir)
	{
		if (EntryType != IO::FSE_NONE) do
		{
			if (EntryType == IO::FSE_FILE)
			{
				CString FullEntryName = DirNameStr.c_str() + EntryName;
				if (StringUtils::MatchesPattern(FullEntryName.CStr(), Pattern.c_str()))
				{
					out_vec.push_back(String(FullEntryName.CStr()));
					++Entries;
				}
			}
		}
		while (FS->NextDirectoryEntry(hDir, EntryName, EntryType));

		FS->CloseDirectory(hDir);
	}

	return Entries;
}
//---------------------------------------------------------------------

}