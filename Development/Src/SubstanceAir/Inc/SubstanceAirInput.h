//! @file SubstanceAirInput.h
//! @brief Substance Air input definitions
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_INPUT_H
#define _SUBSTANCE_AIR_INPUT_H

#include "SubstanceAirTypedefs.h"

#include "framework/imageinput.h"

#pragma pack ( push, 8 )
#include <substance/substance.h>
#pragma pack ( pop )

class USubstanceAirImageInput;

namespace SubstanceAir
{
	//! @brief The list of input widgets available in Designer
	enum InputWidget
	{
		Input_NoWidget,         //!< Widget not defined
		Input_Slider,           //!< Simple widget slider 
		Input_Angle,            //!< Angle widget
		Input_Color,            //!< Color widget
		Input_Togglebutton,     //!< On/Off toggle button or checkbox
		Input_Combobox,         //!< Drop down list
		Input_SizePow2,         //!< Size Pow 2 list
		Input_Image,            //!< Image input widget
		Input_INTERNAL_COUNT
	};


	//! @brief Substance input basic description
	struct FInputDescBase 
	{
		//! @brief Return an instance of the Input
		std::tr1::shared_ptr< input_inst_t > Instantiate();
		
		virtual const void* getRawDefault() const { return NULL; }

		UBOOL IsNumerical() const;
		UBOOL IsImage() const {return !IsNumerical();}

		//! @brief Internal identifier of the output
		FString			Identifier;

		//! @brief User-readable identifier of the output
		FString			Label;

		//! @brief User-readable identifier of the output
		FString			Group;

		//! @brief Preferred widget for edition
		InputWidget		Widget;

		//! @brief Substance UID of the reference output desc
		uint_t			Uid;

		//! @brief Index of the input, used for display order
		uint_t			Index;

		//! @brief Type of the input
		int_t			Type;

		//! @brief list of output UIDSs altered by this input
		SubstanceAir::List< uint_t >  AlteredOutputUids;

		//! @brief Separate the inputs that cause a big change in the texture (random seed...)
		BITFIELD		IsHeavyDuty:1;
	};


	std::tr1::shared_ptr< FInputDescBase > FindInput(
		SubstanceAir::List< std::tr1::shared_ptr<FInputDescBase> >& Inputs, 
		const uint_t uid);


	//! @brief Numerical Substance input description
	template< typename T > struct FNumericalInputDesc : public FInputDescBase
	{
		FNumericalInputDesc():IsClamped(FALSE)
		{
			appMemzero((void*)&DefaultValue, sizeof(T));
			appMemzero((void*)&Min, sizeof(T));
			appMemzero((void*)&Max, sizeof(T));
		}

		const void* getRawDefault() const { return &DefaultValue; }

		//! @brief whether the value should be clamped in the UI
		UBOOL IsClamped;

		T DefaultValue;
		T Min;
		T Max;
	};


	//! @brief Special desc for combobox items
	//! @note Used to store value - text map
	struct FNumericalInputDescComboBox : public FNumericalInputDesc<int_t>
	{
		FNumericalInputDescComboBox(FNumericalInputDesc< int_t >*);

		TMap< INT, FString > ValueText;
	};


	//! @brief Image Substance input description
	struct FImageInputDesc : public FInputDescBase
	{
		ChannelUse Usage;
		FString Label;
		FString Desc;
	};


	//! @brief Base Instance of a Substance input
	struct FInputInstanceBase
	{
		FInputInstanceBase(FInputDescBase* Input=NULL);
		virtual ~FInputInstanceBase(){}
		
		std::tr1::shared_ptr< input_inst_t > Clone();
		UBOOL IsNumerical() const;
		virtual void Reset() = 0;

		//	//! @brief Internal use only
		virtual UBOOL isModified(const void*) const = 0;

		uint_t	Uid;
		int_t	Type;

		BITFIELD	IsHeavyDuty:1;
		BITFIELD	UseCache:1;

		input_desc_t* Desc;
		graph_inst_t* Parent;
	};


	std::tr1::shared_ptr< FInputInstanceBase > FindInput(
		SubstanceAir::List< std::tr1::shared_ptr< FInputInstanceBase > >& Inputs, 
		const uint_t uid);


	//! @brief Numerical Instance of a Substance input
	struct FNumericalInputInstanceBase : public FInputInstanceBase
	{
		FNumericalInputInstanceBase(FInputDescBase* Input=NULL):
			FInputInstanceBase(Input){}

		UBOOL isModified(const void*) const;

		template < typename T > void GetValue(TArray< T >& OutValue);
		template < typename T > void SetValue(const TArray< T >& InValue);

		virtual const void* getRawData() const = 0;
		virtual SIZE_T getRawSize() const = 0;
	};


	//! @brief Numerical Instance of a Substance input
	template< typename T = INT > struct FNumericalInputInstance : 
		public FNumericalInputInstanceBase
	{
		FNumericalInputInstance(FInputDescBase* Input=NULL):
			FNumericalInputInstanceBase(Input),LockRatio(TRUE){}

		T Value;
		void Reset();
		BITFIELD LockRatio:1; //!< @brief used only for $outputsize inputs

		const void* getRawData() const { return &Value; }
		SIZE_T getRawSize() const { return sizeof(Value); }
	};

	//! @brief Image Instance of a Substance input
	struct FImageInputInstance : public FInputInstanceBase
	{
		FImageInputInstance(FInputDescBase* Input=NULL);
		FImageInputInstance(const FImageInputInstance&);
		FImageInputInstance& operator = (const FImageInputInstance&);

		void Reset()
		{
			SetImageInput(NULL, NULL);
		}

		UBOOL isModified(const void*) const;

		//! @brief Set a new image content
		void SetImageInput(UObject*, FGraphInstance* Parent, UBOOL unregisterOutput = TRUE, UBOOL isTransacting=FALSE);

		//! @brief Accessor on current image content (can return NULL shared ptr)
		const std::tr1::shared_ptr< SubstanceAir::ImageInput >& GetImage() const { return ImageInput; }

		//! @brief Update this when the image is modified
		mutable UBOOL PtrModified;

		std::tr1::shared_ptr< SubstanceAir::ImageInput > ImageInput;

		UObject* ImageSource;
	};

	SIZE_T getComponentsCount(SubstanceInputType type);

} // namespace SubstanceAir

// template impl
#include "SubstanceAirInput.inl"

#endif //_SUBSTANCE_AIR_INPUT_H

