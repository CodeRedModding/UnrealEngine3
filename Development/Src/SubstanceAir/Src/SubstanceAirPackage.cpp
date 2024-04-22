//! @file SubstanceAirPackage.cpp
//! @brief Implementation of the USubstanceAirPackage class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include <Engine.h>

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirHelpers.h"

#if WITH_EDITOR
#include "SubstanceAirEdPreset.h"
#include "SubstanceAirEdXmlHelper.h"
#endif

#pragma pack ( push, 8 )
#include <substance/linker/linker.h>
#pragma pack ( pop )

#include "framework/details/detailslinkdata.h"

namespace
{
	TArray<FString> linkerArchiveXmlContent;
}

namespace SubstanceAir
{

FPackage::FPackage():
	Guid(0, 0, 0, 0),
	Parent(0),
	LoadedInstancesCount(0)
{

}


FPackage::~FPackage()
{
	for (int_t IdxGraph=0 ; IdxGraph<Graphs.Num() ; ++IdxGraph)
	{
		delete Graphs(IdxGraph);
		Graphs(IdxGraph) = 0;
	}

	Graphs.Empty();
	Parent = 0;
}


SUBSTANCE_EXTERNC
static void SUBSTANCE_CALLBACK linkerCallbackArchiveXml(  
	SubstanceLinkerHandle *handle,
	const unsigned short* basename,
	const char* xmlContent)
{
	linkerArchiveXmlContent.AddItem(UTF8_TO_TCHAR(xmlContent));
}


void FPackage::SetData(
	const BYTE*& BinaryBuffer,
	const INT BinaryBufferSize,
	const FString& FilePath)
{
	// save the sbsar
	LinkData.reset(
		new Details::LinkDataAssembly(
			BinaryBuffer,
			BinaryBufferSize));

	{
		INT Err = 0;

		// grab the xmls using the linker
		SubstanceLinkerContext *context;
		
		Err = substanceLinkerContextInit(
			&context,
			SUBSTANCE_LINKER_API_VERSION,
			Substance_EngineID_sse2);
		if (Err)
		{
			check(0);
			appDebugMessagef(*FString::Printf(TEXT("Substance: error while initializing the linker context(%d)"), Err));
			return;
		}

		Err = substanceLinkerContextSetCallback(
			context,
			Substance_Linker_Callback_ArchiveXml,
			(void*)linkerCallbackArchiveXml);
		if (Err)
		{
			check(0);
			appDebugMessagef(*FString::Printf(TEXT("Substance: linker error (%d)"), Err));
			return;
		}

		if (Err)
		{
			check(0);
			appDebugMessagef(*FString::Printf(TEXT("Substance: linker error (%d)"), Err));
			return;
		}

		SubstanceLinkerHandle *handle;
		Err = substanceLinkerHandleInit(&handle,context);
		if (Err)
		{
			check(0);
			appDebugMessagef(*FString::Printf(TEXT("Substance: linker error (%d)"), Err));
			return;
		}

		linkerArchiveXmlContent.Empty();

		Err	= substanceLinkerHandlePushAssemblyMemory(
			handle,
			(const char*)BinaryBuffer,
			BinaryBufferSize);
		if (Err)
		{
			check(0);
			appDebugMessagef(*FString::Printf(TEXT("Substance: linker error while pushing assemblies(%d)"), Err));
			return;
		}

		// Release
		Err = substanceLinkerHandleRelease(handle);
		if (Err)
		{
			check(0);
			appDebugMessagef(*FString::Printf(TEXT("Substance: linker error (%d)"), Err));
			return;
		}

		Err = substanceLinkerContextRelease(context);
		if (Err)
		{
			check(0);
			appDebugMessagef(*FString::Printf(TEXT("Substance: linker error (%d)"), Err));
			return;
		}
	}

	// try to parse the xml file
	if (FALSE == ParseXml(linkerArchiveXmlContent))
	{
		// abort if invalid
		GWarn->Log( TEXT("Substance: error while parsing xml companion file."));
		return;
	}

	SubstanceAir::List<graph_desc_t*>::TIterator itG(Graphs.itfront());
	for (; itG ; ++itG)
	{
		(*itG)->Parent = this;
	}

	itG.Reset();

	for (; itG ; ++itG)
	{
		SubstanceAir::List<output_desc_t>::TIterator itO((*itG)->OutputDescs.itfront());
		for (; itO ; ++itO)
		{
			Details::LinkDataAssembly *linkdata = 
				(Details::LinkDataAssembly *)LinkData.get();

			linkdata->setOutputFormat(
				(*itO).Uid,
				(*itO).Format);
		}
	}

	SourceFilePath = FilePath;

	FFileManager::FTimeStamp Timestamp;
	if (GFileManager->GetTimestamp( *FilePath, Timestamp ))
	{
		FFileManager::FTimeStamp::TimestampToFString(
			Timestamp, SourceFileTimestamp);
	}
}


UBOOL FPackage::IsValid()
{
	return Graphs.Num() && LinkData->getSize() && guid_t(0,0,0,0) != Guid;
}


UBOOL FPackage::ParseXml(const TArray<FString>& XmlContent)
{
#if WITH_EDITOR
	return Helpers::ParseSubstanceXml(XmlContent, Graphs, SubstanceUids);
#else
	return FALSE;
#endif
}


output_inst_t* FPackage::GetOutputInst(const class FGuid OutputUid) const
{
	for (int_t IdxGraph=0 ; IdxGraph<Graphs.Num() ; ++IdxGraph)
	{
		SubstanceAir::List<graph_inst_t*>::TIterator
			InstanceIt(Graphs(IdxGraph)->LoadedInstances.itfront());

		for (; InstanceIt ; ++InstanceIt)
		{
			for (int_t IdxOut=0 ; IdxOut<(*InstanceIt)->Outputs.Num() ; ++IdxOut)
			{
				if (OutputUid == (*InstanceIt)->Outputs(IdxOut).OutputGuid)
				{
					return &(*InstanceIt)->Outputs(IdxOut);
				}
			}
		}
	}

	return NULL;
}


//! @brief Called when a Graph gains an instance
void FPackage::InstanceSubscribed()
{
	if (0 == LoadedInstancesCount)
	{
		// do nothing for the moment...
	}

	++LoadedInstancesCount;
}


//! @brief Called when a Graph loses an instance
void FPackage::InstanceUnSubscribed()
{
	--LoadedInstancesCount;

	if (0 == LoadedInstancesCount)
	{
		// should we clean RenderGroups when there are
		// not any instance left from a Package ?
	}
}


INT FPackage::GetInstanceCount()
{
	INT InstanceCount = 0;

	SubstanceAir::List<graph_desc_t*>::TIterator 
		ItGraph(Graphs.itfront());
	for (; ItGraph ; ++ItGraph)
	{
		InstanceCount += (*ItGraph)->InstanceUids.Num();		
	}

	return InstanceCount;
}

} // namespace SubstanceAir
