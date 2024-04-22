//! @file SubstanceAirEdXmlHelper.h
//! @brief Substance Air description XML parsing helper
//! @author Gonzalez Antoine - Allegorithmic
//! @date 20101229
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_ED_XMLHELPER_H__
#define _SUBSTANCE_AIR_ED_XMLHELPER_H__

namespace SubstanceAir
{
namespace Helpers
{

//! @brief Parse a Substance Air xml comp
//! @param[in] InOuter The unreal package in which to create SbsOutput structures
//! @param[in] XmlFilePath The path of the xml file, must be valid
//! @param[in/out] The package to store Graph in
//! @param[out] IdxOutputMap The substance index (valid only in the 
//! loaded substance package) to output structure map
UBOOL ParseSubstanceXml(
	const TArray<FString>& XmlContent,
	SubstanceAir::List<graph_desc_t*>& graphs,
	TArray<uint_t>& assembly_uid);

UBOOL ParseSubstanceXmlPreset(
	presets_t& presets,
	const FString& XmlContent,
	graph_desc_t* graphDesc);

void WriteSubstanceXmlPreset(
	preset_t& preset,
	FString& XmlContent);

} //namespace SubstanceAir
} //namespace Helpers

FORCEINLINE uint_t appAtoul( const TCHAR* String ) { return strtoul( TCHAR_TO_ANSI(String), NULL, 10 ); }

#endif // _SUBSTANCE_AIR_ED_XMLHELPER_H__
