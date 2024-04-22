//! @file SubstanceAirEdHelpers.h
//! @brief Substance Air helper function declaration
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20101229
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_ED_HELPERS_H
#define _SUBSTANCE_AIR_ED_HELPERS_H

#include "SubstanceAirTypedefs.h"

namespace SubstanceAir
{
	//! @brief Contains helper functions to call from the main thread
	namespace Helpers
	{
		//! @brief Create an Unreal Material for the given graph-instance
		//! @brief It will be created in the same outer package as the GraphInstance
		UMaterial* CreateMaterial(
			USubstanceAirInstanceFactory* Parent,
			graph_inst_t* GraphInstance, 
			const FString& MatName,
			UBOOL bFocusInObjectBrowser = TRUE);

		//! @brief Create instance of a graph desc
		graph_inst_t* InstantiateGraph(
			graph_desc_t* Graph,
			UObject* Outer,
			UBOOL bUseDefaultPackage,
			UBOOL bCreateOutputs,
			FString InstanceName = FString(),
			UBOOL bShowCreateMaterialCheckBox=TRUE);

		//! @brief Create instances of a list of graph desc
		SubstanceAir::List<graph_inst_t*> InstantiateGraphs(
			SubstanceAir::List<graph_desc_t*>&,
			UObject* Outer=NULL,
			UBOOL bUseDefaultPackage=FALSE,
			UBOOL bCreateOutputs=TRUE,
			UBOOL bShowCreateMaterialCheckBox=TRUE);

		void FlagRefreshContentBrowser(int_t OutputsNeedingRefresh = 1);

		FString GetSuitableName(output_inst_t* Instance, UObject* Outer);

		//! @brief Find a unique name for a given object
		template< class T > FString GetSuitableNameT(const FString & BaseName, UObject* Outer)
		{
			int_t Count = 0;
			FString Name;

			// increment the instance number as long as the name is already used
			do
			{
				Name = BaseName + FString::Printf(TEXT("_%i"),Count++);
			}
			while (UObject::StaticFindObjectFast(	
				T::StaticClass(),
				Outer,
				*Name));

			return Name;
		}

		void CreateImageInput(UTexture2D* Texture);

		FString ImportPresetFile();

		void SavePresetFile(preset_t& Preset);

		void RegisterForDeletion( class USubstanceAirGraphInstance* InstanceContainer );

		void RegisterForDeletion( class USubstanceAirTexture2D* InstanceContainer );

		void RegisterForDeletion( class USubstanceAirInstanceFactory* InstanceContainer );

		void PerformDelayedDeletion();

		void DuplicateGraphInstance( 
			graph_inst_t* RefInstance, 
			SubstanceAir::List<graph_inst_t*> &OutInstances,
			UObject* Outer = NULL,
			UBOOL bUseDefaultPackage = TRUE,
			UBOOL bCopyOutputs = TRUE);
		
		void CopyInstance( 
			graph_inst_t* RefInstance,
			graph_inst_t* NewInstance,
			UBOOL bCopyOutputs = TRUE);

		void SaveInitialValues(graph_inst_t *Graph);

		void RestoreGraphInstances();

		void FullyLoadInstance(USubstanceAirGraphInstance* Instance);

	} // namespace Helpers
} // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_ED_HELPERS_H
