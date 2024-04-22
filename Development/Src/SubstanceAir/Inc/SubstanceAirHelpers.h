//! @file SubstanceAirHelpers.h
//! @brief Substance Air helper function declaration
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20101229
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_HELPERS_H
#define _SUBSTANCE_AIR_HELPERS_H

#include "framework/imageinput.h"

#pragma pack ( push, 8 )
#include <substance/pixelformat.h>
#pragma pack ( pop )

class USubstanceAirInstanceFactory;
class USubstanceAirGraphInstance;
class USubstanceAirTexture2D;
class UObject;

namespace SubstanceAir
{
	//! @brief Contains helper functions to call from the main thread
	namespace Helpers
	{
		//! @brief Perform an asynchronous (non-blocking) rendering of those instances
		void RenderAsync(SubstanceAir::List<graph_inst_t*>&);
		void RenderAsync(graph_inst_t*);

		//! @brief Perform a blocking rendering of those instances
		void RenderSync(SubstanceAir::List<graph_inst_t*>&);
		void RenderSync(graph_inst_t*);

		//! @brief Subscribe the graph instance for rendering, without rendering
		void RenderPush(graph_inst_t*);

		//! @brief Queue the graph instance for later render
		//! @note Used during seekfreeloading
		void PushDelayedRender(graph_inst_t*);

		//! @brief Queue an image input until its source has been rendered
		//! @note This should be improve to handle recursive image input 
		void PushDelayedImageInput(img_input_inst_t* ImgInput, graph_inst_t* Instance);

		//! @brief Render queued graph instances
		void PerformDelayedRender();

		//! @brief Perform per frame Substance management
		void Tick();

		//! @brief Cancel pending renders etc.
		void CancelPendingActions();

		//! @brief Create a texture 2D object using an output instance desc.
		void CreateTexture2D(FOutputInstance* OutputInstance, FString Name=FString());

		//! @brief used to create a graph instance object
		USubstanceAirGraphInstance* CreateGraphInstanceContainer(
			UObject* Parent,
			const FString& BaseName, 
			UBOOL CreateSubPackage=TRUE);

		//! @brief Convert a PixelFormat to the closest supported PixelFormat
		//! @note If adding support to a new type, update SubstanceToUe3Format function
		SubstancePixelFormat ValidateFormat(const SubstancePixelFormat Format);

		//! @brief Convert from a Substance Format to UE3 equivalent
		//! @pre The input format is a format supported by the UE3
		//! @return The equivalent in UE3 enum or PF_Unknown if not supported
		EPixelFormat SubstanceToUe3Format(const SubstancePixelFormat Format);

		//! @brief Convert from a UE3 pixel format to a Substance Format
		SubstancePixelFormat Ue3FormatToSubstance(EPixelFormat Fmt);
		
		//! @brief Tell if the UE3 pixel format is supported by SubstanceAir
		//! @note Used for image inputs.
		UBOOL IsUe3FormatSupported(EPixelFormat Fmt);

		//! @brief Create textures of empty output instances of the given graph instance
		void CreateTextures(graph_inst_t* GraphInstance);

		//! @brief Update Instances's outputs
		UBOOL UpdateTextures(SubstanceAir::List<FGraphInstance*>& Instances);

		//! @brief Compare the values of two input instances.
		//! @todo implement this as a comparison operator ?
		//! @return True is the values are the same.
		UBOOL AreInputValuesEqual(
			std::tr1::shared_ptr<input_inst_t>&,
			std::tr1::shared_ptr<input_inst_t>&);

		//! @brief Return the string representation of the Input's value
		FString GetValueString(const std::tr1::shared_ptr<input_inst_t>& Input);

		TArray< FLOAT > GetValueFloat(const std::tr1::shared_ptr<input_inst_t>& Input);

		TArray< INT > GetValueInt(const std::tr1::shared_ptr<input_inst_t>& Input);

		//! @brief Setup the rendering system for commandlet or things like that
		void SetupSubstanceAir();

		//! @brief Opposite of SetupSubstanceAir
		void TearDownSubstanceAir();

		//! @brief Disable an output
		//! @pre It is ok to delete the texture, i.e. it is not used
		//! @port The output's texture is deleted
		void Disable(output_inst_t*);

		//! @brief Clears the texture properly
		void Clear(std::tr1::shared_ptr<USubstanceAirTexture2D*>& otherTexture);

		//! @brief Find which desc an instance belongs to
		graph_desc_t* FindParentGraph(
			SubstanceAir::List<graph_desc_t*>& Graphs,
			graph_inst_t*);

		graph_desc_t* FindParentGraph(
			SubstanceAir::List<graph_desc_t*>& Graphs,
			const FString& URL);

		//! @brief Decompress a jpeg buffer in RAW RGBA
		BYTE* DecompressJpeg(
			const BYTE* Buffer,
			const INT Length, INT* W, INT* H, INT NumComp=4);

		//! @brief Compress a jpeg buffer in RAW RGBA
		void CompressJpeg(
			const BYTE* InBuffer, const INT InLength, 
			INT W, INT H, INT NumComp,
			BYTE** OutBuffer, INT& OutLength);

		std::tr1::shared_ptr<ImageInput> PrepareImageInput(
			class UObject* InValue, 
			FImageInputInstance* Input,
			FGraphInstance* Parent);

		UBOOL IsSupportedImageInput(UObject*);

		void LinkImageInput(
			class USubstanceAirImageInput* BmpImageInput,
			struct FImageInputInstance* ImgInputInst);
		
		void Cleanup(USubstanceAirGraphInstance*);

		//! @brief Reset an instance's inputs to their default values
		//! @note Does no trigger rendering of the instance
		void ResetToDefault(graph_inst_t*);

		void RegisterOutputAsImageInput(USubstanceAirGraphInstance* G, USubstanceAirTexture2D* T);
		void UnregisterOutputAsImageInput(USubstanceAirTexture2D* T, UBOOL bUnregisterAll, UBOOL bUpdateGraphInstance);
		void UnregisterOutputAsImageInput(USubstanceAirGraphInstance* Graph);

	} // namespace Helpers
	
} // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_HELPERS_H
