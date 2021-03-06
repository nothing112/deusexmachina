// Loads CTableT<CString> from Excel 2003 table saved in XML format.
// Use function declaration instead of header file where you want to call this loader.

#include <Data/TableT.h>
#include <Data/XMLDocument.h>
#include <Data/String.h>
#include <Data/Buffer.h>
#include <Data/XMLDocument.h>
#include <IO/IOServer.h>
#include <IO/Stream.h>

namespace Load
{

static void GetTextFromCell(tinyxml2::XMLElement* pCell, CString& Text)
{
	Text.Clear();

	tinyxml2::XMLHandle hCell(pCell);
	tinyxml2::XMLText* pText = hCell.FirstChildElement("Data").FirstChild().ToText();
	if (pText) Text = pText->Value();
	else
	{
		// Check for indirect text (hidden in Font-Statements)
		tinyxml2::XMLElement* pData = hCell.FirstChildElement("ss:Data").ToElement();
		if (!pData) return;

		tinyxml2::XMLElement* pFont = pData->FirstChildElement("Font");
		for (; pFont; pFont = pFont->NextSiblingElement("Font"))
		{
			tinyxml2::XMLHandle hFont(pFont);
			tinyxml2::XMLText* pFontText = hFont.FirstChild().ToText();
			if (pFontText) Text.Add(pFontText->Value());
		}
	}
}
//---------------------------------------------------------------------

bool StringTableFromExcelXML(Data::PXMLDocument Doc,
							 Data::CTableT<CString>& Out,
							 const char* pWorksheetName,
							 bool FirstRowAsColNames,
							 bool FirstColAsRowNames)
{
	if (Doc.IsNullPtr() || Doc->Error()) FAIL;

	tinyxml2::XMLHandle hDoc(Doc);
	tinyxml2::XMLElement* pSheet = hDoc.FirstChildElement("Workbook").FirstChildElement("Worksheet").ToElement();

	if (pWorksheetName && *pWorksheetName)
		for (; pSheet; pSheet = pSheet->NextSiblingElement("Worksheet"))
			if (!strcmp(pWorksheetName, pSheet->Attribute("ss:Name"))) break;

	if (!pSheet) FAIL;

	tinyxml2::XMLElement* pTable = pSheet->FirstChildElement("Table");
	n_assert(pTable);

	CString CellText;

	Out.Name = pSheet->Attribute("ss:Name");
	Out.ColMap.Clear();
	Out.RowMap.Clear();

	// Count columns and optionally save column name to index mapping
	UPTR ColCount = 0;
	UPTR TableW = 0;
	tinyxml2::XMLHandle hTable(pTable);
	tinyxml2::XMLElement* pCell = hTable.FirstChildElement("Row").FirstChildElement("Cell").ToElement();
	
	if (FirstColAsRowNames)
	{
		pCell = pCell->NextSiblingElement("Cell");
		++ColCount;
	}
	
	for (; pCell; pCell = pCell->NextSiblingElement("Cell"))
	{
		if (pCell->FirstChildElement("Data"))
		{
			if (FirstRowAsColNames)
			{
				GetTextFromCell(pCell, CellText);
				if (CellText.IsValid()) Out.ColMap.Add(CStrID(CellText.CStr()), TableW);
			}
			++ColCount;
			++TableW;
		}
	}

	// Count rows
	UPTR RowCount = 0;
	tinyxml2::XMLElement* pRow = pTable->FirstChildElement("Row");

	if (FirstRowAsColNames)
	{
		pRow = pRow->NextSiblingElement("Row");
		++RowCount;
	}

	for (; pRow; pRow = pRow->NextSiblingElement("Row"))
	{
		tinyxml2::XMLHandle hRow(pRow);
		if (hRow.FirstChildElement("Cell").FirstChildElement("Data").ToElement()) ++RowCount;
		else break;
	}

	UPTR TableH = FirstRowAsColNames ? RowCount - 1 : RowCount;

	if (!ColCount || !RowCount || !TableW || !TableH)
	{
		Out.SetSize(0, 0);
		OK;
	}

	Out.SetSize(TableW, TableH);

	pRow = pTable->FirstChildElement("Row");
	if (FirstRowAsColNames) pRow = pRow->NextSiblingElement("Row");

	for (UPTR CurrRow = 0; pRow && (CurrRow < TableH); ++CurrRow, pRow = pRow->NextSiblingElement("Row"))
	{
		tinyxml2::XMLElement* pCell = pRow->FirstChildElement("Cell");

		UPTR CurrCol = 0;
		if (FirstColAsRowNames)
		{
			GetTextFromCell(pCell, CellText);
			if (CellText.IsValid()) Out.RowMap.Add(CStrID(CellText.CStr()), CurrRow);
			pCell = pCell->NextSiblingElement("Cell");
			++CurrCol;
		}

		for (; pCell && (CurrCol < TableW); ++CurrCol, pCell = pCell->NextSiblingElement("Cell"))
		{
			// Check if cells are skipped
			int CellIdx;
			if (pCell->QueryIntAttribute("ss:Index", &CellIdx) == tinyxml2::XML_SUCCESS)
				CurrCol = CellIdx - 1;

			GetTextFromCell(pCell, CellText);
			Out.Set(FirstColAsRowNames ? CurrCol - 1 : CurrCol, CurrRow, CellText);
		}
	}

	OK;
}
//---------------------------------------------------------------------

bool StringTableFromExcelXML(const CString& FileName,
							 Data::CTableT<CString>& Out,
							 const char* pWorksheetName,
							 bool FirstRowAsColNames,
							 bool FirstColAsRowNames)
{
	Data::CBuffer Buffer;
	IO::PStream File = IOSrv->CreateStream(FileName);
	if (!File->Open(IO::SAM_READ, IO::SAP_SEQUENTIAL)) FAIL;
	const UPTR FileSize = static_cast<UPTR>(File->GetSize());
	Buffer.Reserve(FileSize);
	Buffer.Trim(File->Read(Buffer.GetPtr(), FileSize));
	if (Buffer.GetSize() != FileSize) FAIL;

	Data::PXMLDocument XML = n_new(Data::CXMLDocument);
	if (!XML->Parse((const char*)Buffer.GetPtr(), Buffer.GetSize()) == tinyxml2::XML_SUCCESS) FAIL;

	return StringTableFromExcelXML(XML, Out, pWorksheetName, FirstRowAsColNames, FirstColAsRowNames);
}
//---------------------------------------------------------------------

}
