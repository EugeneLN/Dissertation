#include "RuntimeMesh/EDGEMeshDataProvider.h"

#include <iostream>
#include <fstream>
#include <direct.h>

//#include "../../../../../../../../UE_4.26/Engine/Plugins/Experimental/AlembicImporter/Source/AlembicLibrary/Public/AbcFile.h"


bool GetDir(const string& FullName, string& DirName)
{
	size_t Pos = FullName.find_last_of("\\/");
	if (string::npos == Pos)
	{
		return false;
	}
	DirName = FullName.substr(0, Pos);
	return true;
}

bool CheckDir(const string& Dir)
{
	if (_chdir(Dir.c_str()) != 0)
	{
		string PrevDir;
		if (GetDir(Dir, PrevDir))
		{
			if (CheckDir(PrevDir))
			{
				_mkdir(Dir.c_str());
				return true;
			}
		}
		return false;
	}
	return true;
}


bool EDGEMeshDataProvider::WriteToFile(const string& FileName, const vector<EDGEMeshSectionData>& MeshData, string& OutErrorString)
{
	string FileDir;
	if (GetDir(FileName, FileDir))
	{
		if (!CheckDir(FileDir))
		{
			OutErrorString = "Can't recover directory from file name <" + FileName + ">.";
			return false;
		}
	}
	
	ofstream OutFile(FileName);
	
	if (!OutFile.is_open())
	{
		OutErrorString = "Can't create/open output file <" + FileName + ">.";
		return false;
	}

	// Get amount of sections in mesh
	const int SectionsCount = MeshData.size();
	OutFile << SectionsCount;

	// Write sections info: min/max indices, used material slot
	for (auto& Section : MeshData)
	{
		WriteVector(OutFile, Section.SectionData);
		OutFile << " " << Section.MaterialName;
	}

	// Write remained vertex data per section
	for (auto& Section : MeshData)
	{
		WriteVector(OutFile, Section.Vertices);
		WriteVector(OutFile, Section.Normals);
		WriteVector(OutFile, Section.Tangents);
		WriteVector(OutFile, Section.UVs);
		WriteVector(OutFile, Section.Indices);
	}

	OutFile.close();

	return true;
}

bool EDGEMeshDataProvider::ReadFromFile(const string& FileName, vector<EDGEMeshSectionData>& OutMeshData, string& OutErrorString)
{
	ifstream InFile(FileName);

	if (!InFile.is_open())
	{
		OutErrorString = "Can't open file <" + FileName + ">.";
		return false;
	}

	string InStr;

	string AllStr;
	getline(InFile, AllStr);
	int prevPtr = 0;
	int nxtPtr = 0;
	
	InFile.close();
	
	auto GetPart = [&](string& OutStr)
	{
		nxtPtr = AllStr.find(' ', prevPtr);
		OutStr = AllStr.substr(prevPtr, nxtPtr - prevPtr);
		prevPtr = nxtPtr + 1;
	};

	// Get number of Sections
	GetPart(InStr);
	const int SectionsCount = stoi(InStr);

	// Get Sections info
	for (int SectionIdx = 0; SectionIdx < SectionsCount; SectionIdx++)
	{
		OutMeshData.push_back(EDGEMeshSectionData());
		for (int nIdx = 0; nIdx < 4; nIdx++)
		{
			GetPart(InStr);
			OutMeshData[SectionIdx].SectionData.push_back(stoi(InStr));
		}
		GetPart(OutMeshData[SectionIdx].MaterialName);
	}

	// Get remained vertices info
	for (int SectionIdx = 0; SectionIdx < SectionsCount; SectionIdx++)
	{
		int MinVertIndex = OutMeshData[SectionIdx].SectionData[0];
		int MaxVertIndex = OutMeshData[SectionIdx].SectionData[1];
		int FirstTriIndex = OutMeshData[SectionIdx].SectionData[2];
		int NumTriangles = OutMeshData[SectionIdx].SectionData[3];
		
		for (int Idx = MinVertIndex; Idx <= MaxVertIndex; Idx++)
		{
			for (int nIdx = 0; nIdx < 3; nIdx++)
			{
				GetPart(InStr);
				OutMeshData[SectionIdx].Vertices.push_back(stof(InStr));
			}
		}
		for (int Idx = MinVertIndex; Idx <= MaxVertIndex; Idx++)
		{
			for (int nIdx = 0; nIdx < 3; nIdx++)
			{
				GetPart(InStr);
				OutMeshData[SectionIdx].Normals.push_back(stof(InStr));
			}
		}
		for (int Idx = MinVertIndex; Idx <= MaxVertIndex; Idx++)
		{
			for (int nIdx = 0; nIdx < 3; nIdx++)
			{
				GetPart(InStr);
				OutMeshData[SectionIdx].Tangents.push_back(stof(InStr));
			}
		}
		for (int Idx = MinVertIndex; Idx <= MaxVertIndex; Idx++)
		{
			for (int nIdx = 0; nIdx < 2; nIdx++)
			{
				GetPart(InStr);
				OutMeshData[SectionIdx].UVs.push_back(stof(InStr));
			}
		}
		for (int nIdx = 0; nIdx < NumTriangles * 3; nIdx++)
		{
			GetPart(InStr);
			OutMeshData[SectionIdx].Indices.push_back(stoi(InStr));
		}
	}
	
	return true;
}

bool EDGEMeshDataProvider::RemoveFile(const string& FileName, string& OutErrorString)
{
	
	if (std::remove(FileName.c_str()) == 0)
	{
		return true;
	}
	else
	{
		OutErrorString = "Can't delete file <" + FileName + ">.";
		return false;
	}
}

void EDGEMeshDataProvider::ClearAllMeshDataFiles(const wstring& Dir)
{

}

template <typename T>
void EDGEMeshDataProvider::WriteVector(ofstream& File, const vector<T>& Vector)
{
	for (auto& Data : Vector)
	{
		File << " " << Data;
	}
}

