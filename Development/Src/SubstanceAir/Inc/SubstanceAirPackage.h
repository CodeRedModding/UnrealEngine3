//! @file SubstanceAirPackage.h
//! @brief Substance Air package definition
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_PACKAGE_H
#define _SUBSTANCE_AIR_PACKAGE_H

#include "SubstanceAirTypedefs.h"

class USubstanceAirInstanceFactory;

namespace SubstanceAir
{

namespace Details
{
	class LinkData;
}

	struct FPackage
	{
		FPackage();
		~FPackage();

		//! @brief Initialize the package using the given binary data
		//! @note Supports only SBSAR content
		void SetData(const BYTE*&, const INT, const FString&);

		//! @brief Tell is the package is valid
		UBOOL IsValid();

		//! @brief Read the xml description of the assembly and creates the matching outputs and inputs
		UBOOL ParseXml(const TArray<FString> & XmlContent);

		//! @brief Return the output instances with the given FGuid
		output_inst_t * GetOutputInst(const class FGuid) const;

		//! @return the number of existing instances (loaded + unloaded)
		INT GetInstanceCount();

		//! @brief Called when a Graph gains an instance
		void InstanceSubscribed();

		//! @brief Called when a Graph looses an instance
		void InstanceUnSubscribed();

		//! @brief The sbsar content
		//! @note can be accessed by the substance rendering thread
		//! use the GSubstanceAirRendererMutex for safe access
		std::tr1::shared_ptr<Details::LinkData> LinkData;

		//! @brief substance uid are not unique, someone could import a sbsar twice...
		TArray<uint_t>	SubstanceUids;

		//! @brief ...that's why we also have unreal GUID, this one is unique
		guid_t			Guid;

		FString			SourceFilePath;
		FString			SourceFileTimestamp;

		uint_t			LoadedInstancesCount;

		//! @brief The collection of graph descriptions
		//! @note This structure package owns those objects
		SubstanceAir::List<graph_desc_t*> Graphs;

		//! @brief Parent UObject
		//! @note Used as entry point for serialization and UI
		USubstanceAirInstanceFactory* Parent;

	private:
		//! @brief Disabling package copy
		FPackage(const FPackage&);
		const FPackage& operator=( const FPackage& );
	};

} // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_PACKAGE_H
