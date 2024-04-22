//! @file SubstanceAirOutput.h
//! @brief Substance Air output informations definition
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_OUTPUT_H
#define _SUBSTANCE_AIR_OUTPUT_H

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirTextureClasses.h"

#include "framework/renderresult.h"

#include <deque>

namespace SubstanceAir
{
	struct RenderResult;

	namespace Details
	{
		class RenderToken;
	}

	//! @brief Description of a Substance output
	struct FOutputDesc
	{
		FOutputDesc();
		FOutputDesc(const FOutputDesc &);

		FOutputInstance Instantiate() const;

		FString		Identifier;
		FString		Label;

		uint_t 		Uid;
		int_t 		Format;	//! @see SubstancePixelFormat enum in substance/pixelformat.h
		int_t 		Channel;	//! @see ChannelUse

		//! @brief The inputs modifying this output
		//! @note FOutput is not the owner of those objects
		SubstanceAir::List<uint_t> AlteringInputUids;
	};


	//! @brief Description of a Substance output instance
	struct FOutputInstance
	{
		typedef std::auto_ptr<RenderResult> Result;

		FOutputInstance();
		FOutputInstance(const FOutputInstance&);

		//! @brief Get the inputs altering this instance
		void GetInputParameterNames(
			TArray<class FName>&,
			TArray<class FGuid>&,
			UBOOL isNumerical=TRUE);

		//! @brief Return the graph instance this output belongs to
		//! @return Null is the output is unable to know, this
		//! happens when its texture has not been created yet.
		graph_inst_t* GetParentGraphInstance() const;
		graph_desc_t* GetParentGraph() const;
		output_desc_t* GetOutputDesc() const;
		USubstanceAirGraphInstance* GetOuter() const;

		Result grabResult();

		void flagAsDirty() { bIsDirty = TRUE; }

		UBOOL isDirty() const { return bIsDirty; }

		//! @brief Substance UID of the reference output desc
		uint_t		Uid;

		//! @brief Output of this specific instance
		int_t		Format;

		//! @brief Used to link host-engine texture to output
		FGuid		OutputGuid;

		//! @brief The user can disable the generation of some outputs
		BITFIELD	bIsEnabled:1;

		BITFIELD	bIsDirty:1;

		//! @brief Actual texture class of the host engine
		MS_ALIGN(16) std::tr1::shared_ptr<USubstanceAirTexture2D*> Texture;

		MS_ALIGN(16) USubstanceAirGraphInstance* ParentInstance;

		typedef std::tr1::shared_ptr<Details::RenderToken> Token; //!< Internal use only
		void push(const Token&);                               //!< Internal use only
		UBOOL queueRender();                                    //!< Internal use only
	protected:
		typedef SubstanceAir::Vector<Token> RenderTokens_t;

		RenderTokens_t RenderTokens;
	};

} // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_OUTPUT_H
