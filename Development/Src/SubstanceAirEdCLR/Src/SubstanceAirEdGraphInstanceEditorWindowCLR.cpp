//! @file SubstanceAirEdGraphInstanceEditorWindowCLR.cpp
//! @brief Substance Air Graph Instance editor implementation
//! @copyright Allegorithmic. All rights reserved.

#include "UnrealEdCLR.h"

#include "ManagedCodeSupportCLR.h"
#include "SubstanceAirEdGraphInstanceEditorWindowShared.h"
#include "WPFFrameCLR.h"
#include "ThumbnailToolsCLR.h"

#pragma unmanaged
#include "UnrealEd.h"
#include "Dialogs.h"

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirInput.h"
#include "SubstanceAirOutput.h"
#include "SubstanceAirImageInputClasses.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirEdHelpers.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirInstanceFactoryClasses.h"

#pragma pack ( push, 8 )
#include <substance/substance.h>
#pragma pack ( pop )

SubstanceAir::FGraphInstance* FGraphInstanceEditorWindow::GraphInstance = NULL;

IMPLEMENT_COMPARE_CONSTREF(input_desc_ptr, SubstanceAirEdGraphInstanceEditor,
{
	return (DWORD)(*A).Index > (DWORD)(*B).Index ? 1 : -1; 
})

IMPLEMENT_COMPARE_CONSTREF(input_inst_t_ptr, SubstanceAirEdGraphInstanceEditor,
{
	return (DWORD)(*A).Desc->Index > (DWORD)(*B).Desc->Index ? 1 : -1;
})

#pragma managed

#include "ConvertersCLR.h"
#include "ColorPickerShared.h"

using namespace System;
using namespace System::ComponentModel;
using namespace System::Windows::Controls;
using namespace System::Windows::Controls::Primitives;
using namespace System::Windows::Media::Imaging;
using namespace System::Windows::Input;
using namespace System::Deployment;


template< typename V = FLOAT >
ref class MInputDataContext : public System::Object
{
public:
	MNativePointer< input_inst_t > mInputPtr;
	MNativePointer< graph_inst_t > mGraphPtr;

	Button^ mPickButton;

	array< V >^ mValue;
	INT mIndex;
};


ref class MImageInputDataContext : public System::Object
{
public:
	MNativePointer< input_inst_t > mInputPtr;
	MNativePointer< graph_inst_t > mGraphPtr;

	System::Windows::Shapes::Rectangle^ mThumbnailRect;
};


ref class MOutputDataContext : public System::Object
{
public:
	MNativePointer< output_inst_t > mOutputPtr;
	MNativePointer< graph_inst_t > mGraphPtr;

	String^ mOutputName;
};


ref class MRandomizeDataContext : public System::Object
{
public:
	CustomControls::DragSlider^ mRandomSlider;
	MNativePointer< graph_inst_t > mGraphPtr;
};


ref class PickColorDataContext : public System::Object
{
public:
	CustomControls::DragSlider^ mSliderRed;
	CustomControls::DragSlider^ mSliderGreen;
	CustomControls::DragSlider^ mSliderBlue;
	CustomControls::DragSlider^ mSliderAlpha;

	double StartR;
	double StartG;
	double StartB;
	double StartA;

	MNativePointer< input_inst_t > mInputPtr;
	MNativePointer< graph_inst_t > mGraphPtr;

	unsigned int ColorPickerIndex;
	bool bSyncEnabled;
};


ref class MSizePow2Context : MInputDataContext< int >
{
public:
	CheckBox^ mRatioLock;
	ComboBox^ mOtherSize;
	bool mSkipUpdate;
};


#define POW2_MIN (5)	// 2^5  = 32
#define POW2_MAX (11)	// 2^11 = 2048
#define EDITOR_WIDTH (390)
#define INPUT_HEIGHT (24)
#define INPUT_WIDTH (256)
#define INPUT_MARGIN (5)
#define SLIDER_HEIGHT (18)
#define IMG_INP_BUTTON_SIZE (18)

/*
 * SubstanceAir Graph Instance editor window control (managed)
 */
ref class MGraphInstanceEditorWindow
	: public MWPFPanel
{
	Visual^ RootVisual;
	static SubstanceAir::FGraphInstance* ParentInstance;
	const FPickColorStruct& PickColorStruct;
	FLinearColor* PickColorData;

	PickColorDataContext^ CurrentColorEdited;

public:

	/**
	 * Initialize the Graph Instance editor  window
	 *
	 * @param	Instance				The instance being edited
	 * @param	InParentWindowHandle	Parent window handle
	 *
	 * @return	TRUE if successful
	 */
	MGraphInstanceEditorWindow(
		MWPFFrame^ InFrame, 
		SubstanceAir::FGraphInstance* Instance,
		FPickColorStruct& inPickColorStruct,
		FLinearColor* inPickColorData)
			: 
			MWPFPanel("SubstanceAirGraphInstanceEditorWindow.xaml"),
			PickColorStruct(inPickColorStruct),
			PickColorData(inPickColorData)
	{
		check(Instance != NULL);
		ParentInstance = Instance;

		// Setup bindings
		RootVisual = this;
		FrameworkElement^ WindowContentElement =
			safe_cast<FrameworkElement^>(RootVisual);

		WindowContentElement->DataContext = this;

		CurrentColorEdited = nullptr;
		CloseColorPickers();

		BuildGraphDesc();
		BuildOutputList();
		BuildImageInputList();
		BuildInputList();
		BuildDefaultActions();

		// Setup a handler for when the closed button is pressed
		Button^ CloseButton = safe_cast<Button^>( 
			LogicalTreeHelper::FindLogicalNode( 
				InFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler(
			this, &MGraphInstanceEditorWindow::OnClose );

		WindowContentElement->PreviewKeyDown +=  gcnew
			KeyEventHandler( this, &MGraphInstanceEditorWindow::PreviewKeyDownHandler );
	}

	/** Called when the close button on the title bar is pressed */
	void OnClose( Object^ Sender, RoutedEventArgs^ Args )
	{
		CloseColorPickers();
		ParentInstance = NULL;
		FGraphInstanceEditorWindow::GraphInstance = NULL;
	}

	virtual ~MGraphInstanceEditorWindow(void)
	{
		
	}


	void BuildOutputList()
	{
		Grid ^ Container = safe_cast< Grid ^ >(
			LogicalTreeHelper::FindLogicalNode(RootVisual, "Outputs"));

		Container->Children->Clear();

		SubstanceAir::List< output_inst_t >::TIterator 
			ItOut(ParentInstance->Outputs.itfront());

		size_t OutputIndex = 0;
		for (;ItOut;++ItOut)
			BuildOutputControl(ItOut, Container, OutputIndex++);
	}


	void BuildInputList()
	{
		StackPanel^ Inputs= safe_cast< StackPanel^ >(
			LogicalTreeHelper::FindLogicalNode(
				RootVisual, "Inputs"));

		Inputs->Children->Clear();

		SubstanceAir::List<std::tr1::shared_ptr<input_desc_t>>
			OrderedInputDesc(ParentInstance->Desc->InputDescs);

		SubstanceAir::List<std::tr1::shared_ptr<input_inst_t>>
			OrderedInputInst(ParentInstance->Inputs);

		Sort<input_desc_ptr, COMPARE_CONSTREF_CLASS(input_desc_ptr, SubstanceAirEdGraphInstanceEditor)>(
			&OrderedInputDesc(0), OrderedInputDesc.Num());

		Sort<input_inst_t_ptr, COMPARE_CONSTREF_CLASS(input_inst_t_ptr, SubstanceAirEdGraphInstanceEditor)>(
			&OrderedInputInst(0), OrderedInputInst.Num());
		
		SubstanceAir::List<std::tr1::shared_ptr<input_desc_t>>::TIterator 
			ItInputDesc(OrderedInputDesc.itfront());

		SubstanceAir::List<std::tr1::shared_ptr<input_inst_t>>::TIterator
			ItInputInst(OrderedInputInst.itfront());

		// build the list of inputs of enabled only outputs
		TArray< DWORD > ActiveOutputs;
		SubstanceAir::List< output_inst_t >::TIterator 
			ItOut(ParentInstance->Outputs.itfront());

		for (;ItOut;++ItOut)
		{
			//! @todo: expose to the user "the filter by enabled outputs",
			//!        in the meantime, display all inputs.
			//if ((*ItOut).bIsEnabled)
			{
				ActiveOutputs.AddItem((*ItOut).Uid);
			}
		}

		TMap<FString, INT> Groups;

		bool bHasInput = false;
		bool darkBackgroud = false;

		for (;ItInputDesc && ItInputInst;++ItInputDesc, ++ItInputInst)
		{	
			//! @todo: expose to the user "the filter by enabled outputs",
			//!        in the meantime, display all inputs.
			/*
			TArray< DWORD >::TIterator ItEnabledOut(ActiveOutputs);
			UBOOL Found = FALSE;
			
			for (;ItEnabledOut;++ItEnabledOut)
			{
				if (INDEX_NONE != (*ItInputDesc)->AlteredOutputUids.FindItemIndex(
					*ItEnabledOut))
				{
					Found = TRUE;
					break;
				}
			}

			if (!Found)
			{
				continue;
			}*/

			// do not build control for $pixelsize or $time
			if ((*ItInputDesc)->Identifier == TEXT("$pixelsize") ||
				(*ItInputDesc)->Identifier == TEXT("$time") ||
				(*ItInputDesc)->Identifier == TEXT("$normalformat"))
			{
				continue;
			}

			// controls for image input are in a separate list
			if ((*ItInputDesc)->Type == Substance_IType_Image)
			{
				continue;
			}

			bHasInput = true;

			if (0 == (*ItInputDesc)->Group.Len())
			{
				Inputs->Children->Add(
					BuildInputControl(ItInputDesc, ItInputInst, darkBackgroud, false));
				darkBackgroud = !darkBackgroud;
			}
			else 
			{
				// Group doesn't exists ? create one
				if (FALSE == Groups.HasKey((*ItInputDesc)->Group))
				{
					Expander^ expander = gcnew Expander();
					expander->Header = CLRTools::ToString((*ItInputDesc)->Group);
					
					expander->Foreground = safe_cast<Brush^>( 
						System::Windows::Application::
							Current->Resources["Slate_Control_Foreground"] );

					expander->Content = gcnew StackPanel();

					// Background alternation storage (per group case)
					expander->DataContext = false;
					expander->Margin = System::Windows::Thickness(5, 0, -1, 0);

					Inputs->Children->Add(expander);
					Groups.Set(
						(*ItInputDesc)->Group,
						Inputs->Children->IndexOf(expander));
				}

				Expander^ expander =
					safe_cast< Expander^ >(
						Inputs->Children[*Groups.Find((*ItInputDesc)->Group)]);

				StackPanel^ panel =
					safe_cast< StackPanel^ >(expander->Content);

				Grid ^ InputControl = BuildInputControl(
					ItInputDesc, ItInputInst, (bool)expander->DataContext, true);
				panel->Children->Add(InputControl);

				// Background alternation
				expander->DataContext = !(bool)expander->DataContext;
			}
		}

		if (!bHasInput)
		{
			Expander^ InputExpander = safe_cast< Expander^ >(
				LogicalTreeHelper::FindLogicalNode(
					RootVisual, "InputsExpander"));
			InputExpander->Visibility = System::Windows::Visibility::Collapsed;
			InputExpander->Height = 0;

			Separator^ InputsSep = safe_cast< Separator^ >(
				LogicalTreeHelper::FindLogicalNode(
					RootVisual, "InputsSep"));
			InputsSep->Visibility = System::Windows::Visibility::Collapsed;
		}
	}


	void BuildImageInputList()
	{
		StackPanel^ ImageInputs = safe_cast< StackPanel^ >(
			LogicalTreeHelper::FindLogicalNode(
			RootVisual, "ImageInputs"));

		ImageInputs->Children->Clear();

		SubstanceAir::List<std::tr1::shared_ptr<input_desc_t>>::TIterator 
			ItInputDesc(ParentInstance->Desc->InputDescs.itfront());
		SubstanceAir::List<std::tr1::shared_ptr<input_inst_t>>::TIterator
			ItInputInst(ParentInstance->Inputs.itfront());

		bool bHasImageInput = false;

		DockPanel^ RowContainer = nullptr;

		for (;ItInputDesc && ItInputInst;++ItInputDesc, ++ItInputInst)
		{
			// controls for image input are in a separate list
			if ((*ItInputDesc)->Type != Substance_IType_Image)
			{
				continue;
			}

			if (RowContainer)
			{
				RowContainer->Children->Add(BuildImageInputControl(ItInputDesc, ItInputInst));
				ImageInputs->Children->Add(RowContainer);
				RowContainer = nullptr;
			}
			else
			{
				RowContainer = gcnew DockPanel;
				RowContainer->Margin = System::Windows::Thickness(10, 5, 0, 5);
				RowContainer->Children->Add(BuildImageInputControl(ItInputDesc, ItInputInst));
			}

			bHasImageInput = true;
		}

		if (RowContainer)
		{
			ImageInputs->Children->Add(RowContainer);
		}

		if (false == bHasImageInput)
		{
			Expander^ ImageInputExpander = safe_cast< Expander^ >(
				LogicalTreeHelper::FindLogicalNode(
				RootVisual, "ImageInputsExpander"));
			ImageInputExpander->Visibility = System::Windows::Visibility::Collapsed;
			ImageInputExpander->Height = 0;

			Separator^ ImageInputSep = safe_cast< Separator^ >(
				LogicalTreeHelper::FindLogicalNode(
				RootVisual, "ImageInputsSep"));
			ImageInputSep->Visibility = System::Windows::Visibility::Collapsed;
		}
	}

	void SynchColorPicker()
	{
		// sync color picker and sliders
		if (CurrentColorEdited != nullptr)
		{
			SubstanceAir::FNumericalInputInstanceBase* InputInst =
				(SubstanceAir::FNumericalInputInstanceBase*)(CurrentColorEdited->mInputPtr.Get());

			if (InputInst->Desc->Type == Substance_IType_Float ||
				InputInst->Desc->Type == Substance_IType_Float2 ||
				InputInst->Desc->Type == Substance_IType_Float3 ||
			    InputInst->Desc->Type == Substance_IType_Float4)
			{
				TArray< float_t > Values;
				InputInst->GetValue(Values);
				check (Values.Num() != 0);
				PickColorData->R = Values(0);
				PickColorData->G = Values(1);
				PickColorData->B = Values(2);
				if (Values.Num() == 4)
				{
					PickColorData->A = Values(3);
				}
			}
			else
			{
				TArray< int_t > Values;
				InputInst->GetValue(Values);
				check (Values.Num() != 0);
				PickColorData->R = Values(0);
				PickColorData->G = Values(1);
				PickColorData->B = Values(2);
				if (Values.Num() == 4)
				{
					PickColorData->A = Values(3);
				}
			}

			// grab the last color picker and plug callbacks
			MColorPickerPanel^ TempPanel = 
				MColorPickerPanel::GetStaticColorPicker(CurrentColorEdited->ColorPickerIndex);

			// kind of a trick, this resets start color which is duplicated in CurrentColorEdited
			TempPanel->BindData();
		}
	}

protected:

	/** Array of dropped asset data for supporting drag-and-drop */
	MScopedNativePointer< TArray< FSelectedAssetInfo > > DroppedAssets;


	void PreviewKeyDownHandler( Object^ sender, KeyEventArgs^ e )
	{
		if (e->KeyboardDevice->Modifiers == ModifierKeys::Control &&
			e->KeyboardDevice->IsKeyDown(Key::Z))
		{
			GUnrealEd->Exec( TEXT("TRANSACTION UNDO") );
			e->Handled = true;
		}
	}


	bool CanEdit()
	{
		if (ParentInstance->ParentInstance->HasAnyFlags(RF_BeginDestroyed) ||
			FALSE == ParentInstance->ParentInstance->HasAnyFlags(RF_Standalone))
		{
			return false;
		}

		return true;
	}

	void BuildGraphDesc() 
	{
		Grid ^ GraphDetails = safe_cast< Grid ^ >(
			LogicalTreeHelper::FindLogicalNode(
			RootVisual, "GraphDetails"));

		// Add a new row
		GraphDetails->RowDefinitions->Add(gcnew RowDefinition);

		Label^ NameLabel = gcnew Label;
		NameLabel->Content = gcnew String("Name");
		GraphDetails->Children->Add(NameLabel);
		Grid::SetColumn(NameLabel, 0);

		Label^ Name = gcnew Label;
		Name->HorizontalAlignment =  System::Windows::HorizontalAlignment::Left;
		String ^ NameStr = CLRTools::ToString(ParentInstance->Desc->Label)->Replace("_", " ")->Trim();
		Name->Content = NameStr;
		GraphDetails->Children->Add(Name);
		Grid::SetColumn(Name, 1);

		Expander ^ GraphDetailsExp = safe_cast< Expander^ >(
			LogicalTreeHelper::FindLogicalNode(
			RootVisual, "GraphDetailsExp"));
		GraphDetailsExp->Header = 
			gcnew String("Graph Details (") + Name->Content + gcnew String(")");

		if (ParentInstance->Desc->Description.Len() == 0)
			return;

		// Add a new row
		GraphDetails->RowDefinitions->Add(gcnew RowDefinition);

		Label^ DescLabel = gcnew Label;
		DescLabel->Content = gcnew String("Description");
		

		TextBlock^ Desc = gcnew TextBlock;
		Desc->Text = CLRTools::ToString(ParentInstance->Desc->Description);
		Desc->Foreground =
			safe_cast<Brush^>( 
				System::Windows::Application::Current->Resources
					["Slate_Control_Foreground"]);
		Desc->TextWrapping = System::Windows::TextWrapping::Wrap;
		
		GraphDetails->Children->Add(DescLabel);
		Grid::SetColumn(DescLabel, 0);
		Grid::SetRow(DescLabel, 1);
		GraphDetails->Children->Add(Desc);
		Grid::SetColumn(Desc, 1);
		Grid::SetRow(Desc, 1);
	}

	
	void BuildOutputControl(TArray<output_inst_t>::TIterator ItOut, Grid ^ Container, size_t OutputIndex)
	{
		output_desc_t* Desc = (*ItOut).GetOutputDesc();
		if (!Desc)
		{
			return;
		}

		Label^ OutputName = gcnew Label;
		OutputName->Content = CLRTools::ToString(Desc->Label);

		// Names can be duplicated, check that it's not a problem
		CheckBox^ OutputCheckBox = gcnew CheckBox;
		OutputCheckBox->Foreground =
			safe_cast<Brush^>( 
				System::Windows::Application::Current->Resources
					["Slate_Control_Foreground"] );
		OutputCheckBox->IsChecked = (*ItOut).bIsEnabled ? true : false;

		OutputCheckBox->Style = 
			safe_cast<System::Windows::Style^>(
				OutputCheckBox->TryFindResource("OnOffCheckbox"));

		if (*(ItOut->Texture))
		{
			UObject* TheTexture = *(ItOut->Texture);
			UBOOL IsReferenced = UObject::IsReferenced(
				TheTexture,
				GARBAGE_COLLECTION_KEEPFLAGS );

			if (IsReferenced)
			{
				OutputName->ToolTip =
					CLRTools::ToString(TEXT("This texture is referenced and cannot be deleted."));
				OutputCheckBox->IsEnabled = false;
			}
			else
			{		
				OutputCheckBox->IsEnabled = true;
			}
		}
		else
		{
			OutputCheckBox->IsEnabled = true;
		}

		// Pass along the OutputCheckBox and the graph instance
		MOutputDataContext^ Thecontext = 
			gcnew MOutputDataContext;
		Thecontext->mOutputPtr.Reset(&(*ItOut));
		Thecontext->mGraphPtr.Reset((*ItOut).GetParentGraphInstance());
		Thecontext->mOutputName = CLRTools::ToString(Desc->Label);

		OutputCheckBox->DataContext = Thecontext;		

		OutputCheckBox->Checked += 
			gcnew System::Windows::RoutedEventHandler(
			this, &MGraphInstanceEditorWindow::CheckedOutput);
		OutputCheckBox->Unchecked += 
			gcnew System::Windows::RoutedEventHandler(
			this, &MGraphInstanceEditorWindow::UncheckedOutput);

		Container->RowDefinitions->Add(gcnew RowDefinition());
		
		Container->Children->Add(OutputName);
		Grid::SetColumn(OutputName, 0);
		Grid::SetRow(OutputName, OutputIndex);

		Container->Children->Add(OutputCheckBox);
		Grid::SetColumn(OutputCheckBox, 1);
		Grid::SetRow(OutputCheckBox, OutputIndex);
	}


	void BuildDefaultActions()
	{
		Grid ^ Actions= safe_cast< Grid ^ >(
			LogicalTreeHelper::FindLogicalNode(
			RootVisual, "Options"));

		// Bake outputs
		{
			Actions->RowDefinitions->Add(gcnew RowDefinition);

			Label^ OptName = gcnew Label;
			OptName->Content = "Bake outputs";

			CheckBox^ BakeCheckBox = gcnew CheckBox;
			BakeCheckBox->IsChecked = ParentInstance->bIsBaked;
			BakeCheckBox->Style = 
				safe_cast<System::Windows::Style^>(
				BakeCheckBox->TryFindResource("OnOffCheckbox"));

			BakeCheckBox->Checked += 
				gcnew System::Windows::RoutedEventHandler(
				this, &MGraphInstanceEditorWindow::CheckedBake);
			BakeCheckBox->Unchecked += 
				gcnew System::Windows::RoutedEventHandler(
				this, &MGraphInstanceEditorWindow::UncheckedBake);
			
			Actions->Children->Add(OptName);
			Grid::SetColumn(OptName, 0);
			Actions->Children->Add(BakeCheckBox);
			Grid::SetColumn(BakeCheckBox, 1);
		}


		// reset instance to default
		{
			Actions->RowDefinitions->Add(gcnew RowDefinition);
			
			Button^ ResetButton = gcnew Button;
			ResetButton->Content = "Reset to default Values";
			ResetButton->Height = SLIDER_HEIGHT;
			ResetButton->HorizontalAlignment = System::Windows::HorizontalAlignment::Stretch;
			ResetButton->Margin = System::Windows::Thickness(0, 0, INPUT_MARGIN, 0);
			ResetButton->Click += 
				gcnew System::Windows::RoutedEventHandler(
					this,
					&MGraphInstanceEditorWindow::ResetToDefaultPressed);

			Actions->Children->Add(ResetButton);
			Grid::SetColumn(ResetButton, 1);
			Grid::SetRow(ResetButton, 2);
		}
	}


	void CheckedBake(
		System::Object ^ sender, 
		System::Windows::RoutedEventArgs^ e)
	{
		ParentInstance->bIsBaked = TRUE;
	}


	void UncheckedBake(
		System::Object^ sender, 
		System::Windows::RoutedEventArgs^ e)
	{
		ParentInstance->bIsBaked = FALSE;
	}
	
	Grid ^ BuildGrid(size_t ColCount)
	{
		Grid ^ Container = gcnew Grid;
		
		for (size_t i=0; i<ColCount; i++)
		{
			ColumnDefinition ^ col = gcnew ColumnDefinition();
			col->Width = GridLength(1, GridUnitType::Star);
			Container->ColumnDefinitions->Add(col);
		}

		return Container;
	}

	Grid ^ BuildInputControl(
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc,
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst,
		bool DarkBackgroud, bool IsInGroup)
	{
		Grid ^ Container = gcnew Grid;
		{
			ColumnDefinition ^ col = gcnew ColumnDefinition();
			col->Width = GridLength(IsInGroup ? 110 - 1 : 120);
			Container->ColumnDefinitions->Add(col);
		}
		{
			ColumnDefinition ^ col = gcnew ColumnDefinition();
			col->Width = GridLength(1, GridUnitType::Star);
			Container->ColumnDefinitions->Add(col);
		}

		if (IsInGroup)
		{
			Container->Margin = System::Windows::Thickness(5, 0, 0, 0);
		}

		if (DarkBackgroud)
		{
			Container->Background =
				gcnew SolidColorBrush(Color::FromArgb(255.0f, 80.0f, 80.0f, 80.0f));
		}

		// special control for random seed
		if ((*ItInputDesc)->Identifier == TEXT("$randomseed"))
		{
			BuildRandomSeedControl(ItInputDesc, ItInputInst, Container);
			UIElement ^ elem = Container->Children[1];
			Grid::SetColumn(elem, 1);
			return Container;
		}

		// special control for OutputCheckBox size
		else if ((*ItInputDesc)->Identifier == TEXT("$outputsize"))
		{
			BuildSizePow2Control(ItInputDesc, ItInputInst, Container);
			UIElement ^ elem = Container->Children[1];
			Grid::SetColumn(elem, 1);
			return Container;
		}

		switch ((*ItInputDesc)->Widget)
		{
		default:
		case SubstanceAir::Input_NoWidget:
		case SubstanceAir::Input_Slider:
			BuildSliderControl(ItInputDesc, ItInputInst, Container, nullptr, false);
			break;
		case SubstanceAir::Input_Angle:
			BuildAngleControl(ItInputDesc, ItInputInst, Container);
			break;
		case SubstanceAir::Input_Combobox:
			BuildComboBoxControl(ItInputDesc, ItInputInst, Container);
			break;
		case SubstanceAir::Input_Togglebutton:
			BuildToggleButtonControl(ItInputDesc, ItInputInst, Container);
			break;
		case SubstanceAir::Input_Color:
			if ((*ItInputDesc)->Type == Substance_IType_Float)
			{
				BuildSliderControl(ItInputDesc, ItInputInst, Container, nullptr, false);
			}
			else
			{
				BuildColorControl(ItInputDesc, ItInputInst, Container);
			}
			break;

		case SubstanceAir::Input_SizePow2:
			BuildSizePow2Control(ItInputDesc, ItInputInst, Container);
			break;
		}

		UIElement ^ elem = Container->Children[1];
		Grid::SetColumn(elem, 1);
		return Container;
	}


	template< typename T, typename U> U GetMinComponent(input_desc_t* Desc, int Idx)
	{
		SubstanceAir::FNumericalInputDesc<T>* TypedDesc = 
			(SubstanceAir::FNumericalInputDesc<T>*)Desc;

		U val = TypedDesc->Min[Idx];

		return val;
	}


	template< typename T, typename U> U GetMaxComponent(input_desc_t* Desc, int Idx)
	{
		SubstanceAir::FNumericalInputDesc<T>* TypedDesc = 
			(SubstanceAir::FNumericalInputDesc<T>*)Desc;

		U val = TypedDesc->Max[Idx];

		return val;
	}


	void BuildSliderControl(
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc,
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst,
		Grid ^ Container,
		Button^ ColorPicker,
		bool PlusOne)
	{
		check((*ItInputDesc)->Type != Substance_IType_Image);
		
		SubstanceAir::FNumericalInputDesc<int>* NumericalDesc =
			(SubstanceAir::FNumericalInputDesc<int>*)(*ItInputDesc).get();

		INT SliderCount = 1;

		switch((*ItInputDesc)->Type)
		{
		case Substance_IType_Integer2:
		case Substance_IType_Float2:
			SliderCount = 2;
			break;
		case Substance_IType_Integer3:
		case Substance_IType_Float3:
			SliderCount = 3;
			break;
		case Substance_IType_Integer4:
		case Substance_IType_Float4:
			SliderCount = 4;
			break;
		}

		Label^ Input = gcnew Label;
		Input->Content = CLRTools::ToString((*ItInputDesc)->Label)->Replace("_", " ");
		Input->ToolTip = CLRTools::ToString((*ItInputDesc)->Identifier);
		Container->Children->Add(Input);

		System::Windows::Controls::Primitives::UniformGrid^ SliderContainer =
			gcnew System::Windows::Controls::Primitives::UniformGrid;
		SliderContainer->Columns = PlusOne ? SliderCount + 1 : SliderCount;
		SliderContainer->Rows = 1;
		SliderContainer->HorizontalAlignment = System::Windows::HorizontalAlignment::Stretch;

		for (INT i = 0 ; i < SliderCount ; ++i)
		{
			CustomControls::DragSlider^ Slider = gcnew CustomControls::DragSlider();
			Slider->HorizontalAlignment = System::Windows::HorizontalAlignment::Stretch;
			Slider->Height = SLIDER_HEIGHT;

			if (i != SliderCount - 1)
				Slider->Margin = System::Windows::Thickness(0, 0, INPUT_MARGIN, 0);

			Slider->AllowInlineEdit = true;

			MInputDataContext<>^ data_context =
				gcnew MInputDataContext<>;

			// the data context includes the instance and the graph instance
			data_context->mInputPtr.Reset(&(*(*ItInputInst)));
			data_context->mGraphPtr.Reset(ParentInstance);
			data_context->mIndex = i;
			data_context->mValue = gcnew array<FLOAT>(SliderCount);
			data_context->mPickButton = ColorPicker;

			switch((*ItInputDesc)->Type)
			{
				case Substance_IType_Integer:
				case Substance_IType_Integer2:
				case Substance_IType_Integer3:
				case Substance_IType_Integer4:
					Slider->Minimum = System::UInt32::MinValue;
					Slider->Maximum = System::UInt32::MaxValue;
					break;
				case Substance_IType_Float:
				case Substance_IType_Float2:
				case Substance_IType_Float3:
				case Substance_IType_Float4:
 					Slider->Minimum = System::Double::MinValue;
 					Slider->Maximum = System::Double::MaxValue;
					break;
			}

			switch((*ItInputDesc)->Type)
			{
			case Substance_IType_Integer:
				{
					SubstanceAir::FNumericalInputInstance<int_t>* TypedInst =
						(SubstanceAir::FNumericalInputInstance<int_t>*)&(*(*ItInputInst));
					Slider->Value = TypedInst->Value;
					data_context->mValue[0] = TypedInst->Value;
				}
				break;
			case Substance_IType_Float:
				{
					SubstanceAir::FNumericalInputInstance<float_t>* TypedInst =
						(SubstanceAir::FNumericalInputInstance<float_t>*)&(*(*ItInputInst));
					Slider->Value = TypedInst->Value;
					data_context->mValue[0] = TypedInst->Value;
				}
				break;

			case Substance_IType_Integer2:
				{
					SubstanceAir::FNumericalInputInstance<vec2int_t>* TypedInst =
						(SubstanceAir::FNumericalInputInstance<vec2int_t>*)&(*(*ItInputInst));
					Slider->Value = i == 0 ? TypedInst->Value.X : TypedInst->Value.Y;

					data_context->mValue[0] = TypedInst->Value.X;
					data_context->mValue[1] = TypedInst->Value.Y;
				}
				break;
			case Substance_IType_Float2:
				{
					SubstanceAir::FNumericalInputInstance<vec2float_t>* TypedInst =
						(SubstanceAir::FNumericalInputInstance<vec2float_t>*)&(*(*ItInputInst));
					Slider->Value = i == 0 ? TypedInst->Value.X : TypedInst->Value.Y;

					data_context->mValue[0] = TypedInst->Value.X;
					data_context->mValue[1] = TypedInst->Value.Y;
				}
				break;
			case Substance_IType_Integer3:
				{
					SubstanceAir::FNumericalInputInstance<vec3int_t>* TypedInst = 
						(SubstanceAir::FNumericalInputInstance<vec3int_t>*)&(*(*ItInputInst));
					Slider->Value =
						i == 0 ? TypedInst->Value.X :
						i == 1 ? TypedInst->Value.Y :
						TypedInst->Value.Z;

					data_context->mValue[0] = TypedInst->Value.X;
					data_context->mValue[1] = TypedInst->Value.Y;
					data_context->mValue[2] = TypedInst->Value.Z;
				}
				break;
			case Substance_IType_Float3:
				{
					SubstanceAir::FNumericalInputInstance<vec3float_t>* TypedInst = 
						(SubstanceAir::FNumericalInputInstance<vec3float_t>*)&(*(*ItInputInst));
					Slider->Value =
						i == 0 ? TypedInst->Value.X :
						i == 1 ? TypedInst->Value.Y :
						TypedInst->Value.Z;

					data_context->mValue[0] = TypedInst->Value.X;
					data_context->mValue[1] = TypedInst->Value.Y;
					data_context->mValue[2] = TypedInst->Value.Z;
				}
				break;
			case Substance_IType_Integer4:
				{
					SubstanceAir::FNumericalInputInstance<vec4int_t>* TypedInst = 
						(SubstanceAir::FNumericalInputInstance<vec4int_t>*)&(*(*ItInputInst));
					Slider->Value =
						i == 0 ? TypedInst->Value.X :
						i == 1 ? TypedInst->Value.Y :
						i == 2 ? TypedInst->Value.Z :
						TypedInst->Value.W;

					data_context->mValue[0] = TypedInst->Value.X;
					data_context->mValue[1] = TypedInst->Value.Y;
					data_context->mValue[2] = TypedInst->Value.Z;
					data_context->mValue[3] = TypedInst->Value.W;
				}
				break;
			case Substance_IType_Float4:
				{
					SubstanceAir::FNumericalInputInstance<vec4float_t>* TypedInst = 
						(SubstanceAir::FNumericalInputInstance<vec4float_t>*)&(*(*ItInputInst));
					Slider->Value =
						i == 0 ? TypedInst->Value.X :
						i == 1 ? TypedInst->Value.Y :
						i == 2 ? TypedInst->Value.Z :
						TypedInst->Value.W;

					data_context->mValue[0] = TypedInst->Value.X;
					data_context->mValue[1] = TypedInst->Value.Y;
					data_context->mValue[2] = TypedInst->Value.Z;
					data_context->mValue[3] = TypedInst->Value.W;
				}
				break;
			}
		
			switch((*ItInputDesc)->Type)
			{
			case Substance_IType_Integer:
				{
					SubstanceAir::FNumericalInputDesc<int_t>* TypedDesc =
						(SubstanceAir::FNumericalInputDesc<int_t>*)&(*(*ItInputDesc));

					Slider->SliderMin = TypedDesc->Min;
					Slider->SliderMax = TypedDesc->Max;
				}
				break;
			case Substance_IType_Float:
				{
					SubstanceAir::FNumericalInputDesc<float_t>* TypedDesc =
						(SubstanceAir::FNumericalInputDesc<float_t>*)&(*(*ItInputDesc));

					Slider->SliderMin = TypedDesc->Min;
					Slider->SliderMax = TypedDesc->Max;					
				}
				break;
			case Substance_IType_Integer2:
				{
					Slider->SliderMin = GetMinComponent<vec2int_t, int>(&(*(*ItInputDesc)),i);
					Slider->SliderMax = GetMaxComponent<vec2int_t, int>(&(*(*ItInputDesc)),i);
				}
				break;
			case Substance_IType_Float2:
				{
					Slider->SliderMin = GetMinComponent<vec2float_t, float>(&(*(*ItInputDesc)),i);
					Slider->SliderMax = GetMaxComponent<vec2float_t, float>(&(*(*ItInputDesc)),i);
				}
				break;
			case Substance_IType_Integer3:
				{
					Slider->SliderMin = GetMinComponent<vec3int_t, int>(&(*(*ItInputDesc)),i);
					Slider->SliderMax = GetMaxComponent<vec3int_t, int>(&(*(*ItInputDesc)),i);
				}
				break;
			case Substance_IType_Float3:
				{
					Slider->SliderMin = GetMinComponent<vec3float_t, float>(&(*(*ItInputDesc)),i);
					Slider->SliderMax = GetMaxComponent<vec3float_t, float>(&(*(*ItInputDesc)),i);
				}
				break;
			case Substance_IType_Integer4:
				{
					Slider->SliderMin = GetMinComponent<vec4int_t, int>(&(*(*ItInputDesc)),i);
					Slider->SliderMax = GetMaxComponent<vec4int_t, int>(&(*(*ItInputDesc)),i);
				}
				break;
			case Substance_IType_Float4:
				{
					Slider->SliderMin = GetMinComponent<vec4float_t, float>(&(*(*ItInputDesc)),i);
					Slider->SliderMax = GetMaxComponent<vec4float_t, float>(&(*(*ItInputDesc)),i);
				}
				break;
			}

			if (Slider->SliderMin == 0 && Slider->SliderMax == 0)
			{
				if ((*ItInputDesc)->Type == Substance_IType_Integer ||
					(*ItInputDesc)->Type == Substance_IType_Integer2 ||
					(*ItInputDesc)->Type == Substance_IType_Integer3 ||
					(*ItInputDesc)->Type == Substance_IType_Integer4)
				{
					Slider->SliderMin = 0;
					Slider->SliderMax =
						data_context->mValue[0] > 0 ?
						data_context->mValue[0] * 2 : 10;
				}
				else
				{
					Slider->SliderMin = 0;
					Slider->SliderMax = 1.0f;
				}
			}

			if ((*ItInputDesc)->Type == Substance_IType_Integer ||
				(*ItInputDesc)->Type == Substance_IType_Integer2 ||
				(*ItInputDesc)->Type == Substance_IType_Integer3 ||
				(*ItInputDesc)->Type == Substance_IType_Integer4)
			{
				Slider->ValuesPerDragPixel = 0.1f;
				Slider->DrawAsInteger = true;
			}
			else
			{
				Slider->ValuesPerDragPixel = (Slider->SliderMax - Slider->SliderMin) / 1000.0f;
				Slider->ValuesPerMouseWheelScroll = Slider->ValuesPerDragPixel * 10;
			}

			if (NumericalDesc->IsClamped)
			{
				Slider->Minimum = Slider->SliderMin;
				Slider->Maximum = Slider->SliderMax;
			}

			Slider->ValueChanged +=
				gcnew System::Windows::RoutedPropertyChangedEventHandler<double>(
					this, &MGraphInstanceEditorWindow::SliderValueChanged<float>);

			Slider->PreviewMouseDown +=
				gcnew MouseButtonEventHandler( this,
					&MGraphInstanceEditorWindow::SliderMouseDown );

			Slider->AbsoluteDrag = true;
			Slider->DataContext = data_context;
			Slider->DrawAsPercentage = false;

			SliderContainer->Children->Add(Slider);
		}

		Container->Children->Add(SliderContainer);
	}


	void BuildRandomSeedControl(
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc,
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst,
		Grid ^ Container)
	{
		check((*ItInputDesc)->Type == Substance_IType_Integer);

		SubstanceAir::FNumericalInputInstance<int_t>* TypedInst = 
			(SubstanceAir::FNumericalInputInstance<int_t>*)&(*(*ItInputInst));

		// name of input
		Label^ Name = gcnew Label;
		Name->Content = "Random Seed";

		Grid ^ WidgetContainer = BuildGrid(2);
		WidgetContainer->ColumnDefinitions[0]->Width = GridLength(90);
		WidgetContainer->ColumnDefinitions[1]->Width = GridLength(1, GridUnitType::Star);

		// slider built first because we need to access it from the button
		MInputDataContext< int >^ data_context =
			gcnew MInputDataContext< int >;
		data_context->mInputPtr.Reset(&(*(*ItInputInst)));
		data_context->mGraphPtr.Reset(ParentInstance);
		data_context->mValue = gcnew array< int >(1);
		data_context->mValue[0] = (TypedInst->Value);

		CustomControls::DragSlider^ Slider = gcnew CustomControls::DragSlider();
		Slider->HorizontalAlignment = 
			System::Windows::HorizontalAlignment::Stretch;
		Slider->Minimum = System::UInt32::MinValue; // yes, it's 0
		Slider->Maximum = System::UInt32::MaxValue;
		Slider->ValuesPerDragPixel = 1;
		Slider->AllowInlineEdit = true;
		Slider->DrawAsInteger = true;
		Slider->DataContext = data_context;
		Slider->Value = data_context->mValue[0];
		Slider->Height = SLIDER_HEIGHT;
		Slider->ValueChanged +=
			gcnew System::Windows::RoutedPropertyChangedEventHandler<double>(
				this, &MGraphInstanceEditorWindow::SliderValueChanged<int>);

		// button
		Button^ RandomizeButton = gcnew Button;
		RandomizeButton->Content = "Randomize";
		RandomizeButton->Height = SLIDER_HEIGHT;
		RandomizeButton->HorizontalAlignment = System::Windows::HorizontalAlignment::Stretch;
		RandomizeButton->Margin = System::Windows::Thickness(0, 0, INPUT_MARGIN, 0);

		MRandomizeDataContext^ randomize_data_context = gcnew MRandomizeDataContext;
		randomize_data_context->mRandomSlider = Slider;
		randomize_data_context->mGraphPtr.Reset(ParentInstance);

		RandomizeButton->DataContext = randomize_data_context;

		RandomizeButton->Click += 
			gcnew System::Windows::RoutedEventHandler(
				this,
				&MGraphInstanceEditorWindow::RandomizeButtonPressed);

		WidgetContainer->Children->Add(RandomizeButton);
		Grid::SetColumn(RandomizeButton, 0);
		
		WidgetContainer->Children->Add(Slider);
		Grid::SetColumn(Slider, 1);

		Container->Children->Add(Name);
		Container->Children->Add(WidgetContainer);
	}


	ComboBox^ BuildResolutionComboBox(	
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc, 
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst,
		int SizeValue,
		ComboBox^ ResolutionComboBox,
		ComboBox^ OtherResolutionComboBox,
		CheckBox^ RatioLock,
		const UBOOL IsWidth)
	{
		ResolutionComboBox->Width = 60;
		ResolutionComboBox->Height = SLIDER_HEIGHT;

		for (INT I = POW2_MIN ; I <= POW2_MAX ; ++I)
		{
			INT Size = appPow(2, I);
			ResolutionComboBox->Items->Add(
				CLRTools::ToString(FString::Printf(TEXT("%d"), Size)));
		}

		int index = Clamp(POW2_MIN, SizeValue, POW2_MAX) - POW2_MIN;
		ResolutionComboBox->SelectedIndex = index;

		ResolutionComboBox->SelectionChanged +=
			gcnew System::Windows::Controls::SelectionChangedEventHandler(
				this, &MGraphInstanceEditorWindow::Pow2Changed);

		MSizePow2Context^ data_context =
			gcnew MSizePow2Context;
		data_context->mInputPtr.Reset(&(*(*ItInputInst)));
		data_context->mGraphPtr.Reset(ParentInstance);
		data_context->mIndex = IsWidth ? 0 : 1;
		data_context->mRatioLock = RatioLock;
		data_context->mOtherSize = OtherResolutionComboBox;
		data_context->mSkipUpdate = false;
		ResolutionComboBox->DataContext = data_context;

		return ResolutionComboBox;
	}


	void BuildSizePow2Control(
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc, 
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst,
		Grid ^ Container)
	{
		check((*ItInputDesc)->Type == Substance_IType_Integer2);

		Label^ NameText = gcnew Label;
		NameText->Content = "Output size";
		NameText->ToolTip = "$outputsize";
	
		StackPanel ^ WidgetContainer = gcnew StackPanel();
		WidgetContainer->Orientation = Orientation::Horizontal;

		SubstanceAir::FNumericalInputInstance<vec2int_t>* TypedInst = 
			(SubstanceAir::FNumericalInputInstance<vec2int_t>*)&(*(*ItInputInst));
		INT WidthPow2 = TypedInst->Value.X;
		INT HeightPow2 = TypedInst->Value.Y;

		CheckBox^ LockButton = gcnew CheckBox;
		LockButton->Content = "Lock ratio";
		LockButton->VerticalAlignment = System::Windows::VerticalAlignment::Center;
		LockButton->IsChecked = TypedInst->LockRatio ? true : false;
		LockButton->Checked += 
			gcnew RoutedEventHandler(this, &MGraphInstanceEditorWindow::CheckedLockRatio);
		LockButton->Unchecked += 
			gcnew RoutedEventHandler(this, &MGraphInstanceEditorWindow::CheckedLockRatio);

		ComboBox^ WidthComboBox = gcnew ComboBox;
		ComboBox^ HeightComboBox = gcnew ComboBox;

		BuildResolutionComboBox(
			ItInputDesc, 
			ItInputInst,
			WidthPow2,
			WidthComboBox,
			HeightComboBox,
			LockButton, 
			TRUE);

		BuildResolutionComboBox(
			ItInputDesc, 
			ItInputInst,
			HeightPow2,
			HeightComboBox,
			WidthComboBox,
			LockButton,
			FALSE);

		LockButton->DataContext = WidthComboBox->DataContext;

		WidthComboBox->Margin = System::Windows::Thickness(0, 0, INPUT_MARGIN, 0);
		HeightComboBox->Margin = System::Windows::Thickness(0, 0, INPUT_MARGIN, 0);

		WidgetContainer->Children->Add(WidthComboBox);
		WidgetContainer->Children->Add(HeightComboBox);
		WidgetContainer->Children->Add(LockButton);

		Container->Children->Add(NameText);
		Container->Children->Add(WidgetContainer);
	}


	void CheckedLockRatio(System::Object ^ sender,System::Windows::RoutedEventArgs ^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		CheckBox^ check_box = safe_cast<CheckBox^>(sender);
		MSizePow2Context^ context =
			safe_cast< MSizePow2Context^ >(check_box->DataContext);

		MNativePointer< input_inst_t > InputPtr(context->mInputPtr.Get());
		MNativePointer< graph_inst_t > InstancePtr(context->mGraphPtr.Get());

		if (InputPtr.IsValid() && InstancePtr.IsValid())
		{
			SubstanceAir::FNumericalInputInstance<vec2int_t>* InputInst =
				(SubstanceAir::FNumericalInputInstance<vec2int_t>*)(InputPtr.Get());

			InputInst->LockRatio = check_box->IsChecked.Value ? 1 : 0;
		}
	}


	void BuildColorControl(
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc,
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst,
		Grid ^ Container)
	{
		check((*ItInputDesc)->Type == Substance_IType_Float3 || 
			(*ItInputDesc)->Type == Substance_IType_Float4);

		// button
		Button^ PickColorButton = gcnew Button;

		BuildSliderControl(ItInputDesc, ItInputInst, Container, PickColorButton, true);

		System::Windows::Controls::Primitives::UniformGrid^ SliderGrid = 
			safe_cast<System::Windows::Controls::Primitives::UniformGrid^>(Container->Children[1]);

		Image^ PickButtonImage = gcnew Image();
		PickButtonImage->Source =
			gcnew BitmapImage(gcnew Uri(
				gcnew System::String(*FString::Printf(
					TEXT("%swxres\\EyeDropperIcon.png"),
					*GetEditorResourcesDir())),
				UriKind::Absolute));

		PickColorButton->Content = PickButtonImage;
		PickColorButton->Height = SLIDER_HEIGHT;
		PickColorButton->HorizontalAlignment = System::Windows::HorizontalAlignment::Stretch;
		PickColorButton->Margin = System::Windows::Thickness(INPUT_MARGIN, 0, 0, 0);

		PickColorDataContext^ ColorDataContext = gcnew PickColorDataContext;
		ColorDataContext->bSyncEnabled = true;
		ColorDataContext->mInputPtr.Reset(&(*(*ItInputInst)));
		ColorDataContext->mGraphPtr.Reset(ParentInstance);

		ColorDataContext->mSliderRed  = safe_cast<CustomControls::DragSlider^>(SliderGrid->Children[0]);
		ColorDataContext->mSliderGreen= safe_cast<CustomControls::DragSlider^>(SliderGrid->Children[1]);
		ColorDataContext->mSliderBlue = safe_cast<CustomControls::DragSlider^>(SliderGrid->Children[2]);

		if((*ItInputDesc)->Type == Substance_IType_Float4)
		{
			ColorDataContext->mSliderAlpha = safe_cast<CustomControls::DragSlider^>(SliderGrid->Children[3]);
		}

		PickColorButton->Background = gcnew SolidColorBrush(Color::FromArgb(
			(System::Byte)(255),
			(System::Byte)(ColorDataContext->mSliderRed->Value * 255.0f),
			(System::Byte)(ColorDataContext->mSliderGreen->Value * 255.0f),
			(System::Byte)(ColorDataContext->mSliderBlue->Value * 255.0f)
			));

		PickColorButton->DataContext = ColorDataContext;
		PickColorButton->Click += 
			gcnew System::Windows::RoutedEventHandler(
				this,
				&MGraphInstanceEditorWindow::PickColorPressed);

		// Finally Add to container
		SliderGrid->Children->Add(PickColorButton);

		if (CurrentColorEdited != nullptr)
		{
			if (CurrentColorEdited->mInputPtr.Get() == (*ItInputInst).get())
			{
				float startR = CurrentColorEdited->StartR;
				float startG = CurrentColorEdited->StartG;
				float startB = CurrentColorEdited->StartB;
				float startA = CurrentColorEdited->StartA;
				CurrentColorEdited = ColorDataContext;
				CurrentColorEdited->StartR = startR; 
				CurrentColorEdited->StartG = startG;
				CurrentColorEdited->StartB = startB;
				CurrentColorEdited->StartA = startA;
			}
		}
	}
	

	void BuildAngleControl(
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc,
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst,
		Grid ^ Container)
	{
		check((*ItInputDesc)->Type == Substance_IType_Float);
		BuildSliderControl(ItInputDesc, ItInputInst, Container, nullptr, false);
	}


	void BuildComboBoxControl(
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc,
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst,
		Grid ^ Container)
	{
		check((*ItInputDesc)->Type == Substance_IType_Integer);
		
		Label^ InputName = gcnew Label;
		InputName->Content = CLRTools::ToString((*ItInputDesc)->Label);
		InputName->ToolTip = CLRTools::ToString((*ItInputDesc)->Identifier);
		InputName->VerticalAlignment = System::Windows::VerticalAlignment::Center;
		

		// build the combo box itself
		MInputDataContext< int >^ data_context =
			gcnew MInputDataContext< int >;
		data_context->mInputPtr.Reset(&(*(*ItInputInst)));
		data_context->mGraphPtr.Reset(ParentInstance);

		ComboBox^ InputComboBox = gcnew ComboBox;

		SubstanceAir::FNumericalInputDescComboBox* TypedDesc = 
			(SubstanceAir::FNumericalInputDescComboBox*)&(*(*ItInputDesc));

		SubstanceAir::FNumericalInputInstance<int_t>* TypedInst = 
			(SubstanceAir::FNumericalInputInstance<int_t>*)&(*(*ItInputInst));

		INT SelectedIdxValue = 0;
		
		// store the values of each element of the combobox
		// to be able to find back which each index's value
		// eg: int selection_value = context->mValue[combo_box->SelectedIndex];
		data_context->mValue = gcnew array<int>(TypedDesc->ValueText.Num());

		TMap< INT, FString >::TIterator ItValueText(TypedDesc->ValueText);

		for (INT IdxValue = 0; ItValueText ; ++ItValueText, ++IdxValue)
		{
			// look for the current value of the input
			if(TypedInst->Value == ItValueText.Key())
			{
				SelectedIdxValue = IdxValue;
			}

			// add the label of the value to the combo box
			InputComboBox->Items->Add(CLRTools::ToString(ItValueText.Value()));

			// map entry's idx/value in the data context
			data_context->mValue[IdxValue] = ItValueText.Key();
		}

		InputComboBox->DataContext = data_context;
		InputComboBox->SelectedIndex = SelectedIdxValue;
		InputComboBox->Height = 18;

		InputComboBox->SelectionChanged +=
			gcnew System::Windows::Controls::SelectionChangedEventHandler(
				this, &MGraphInstanceEditorWindow::ComboBoxChoiceChanged);

		Container->Children->Add(InputName);
		Container->Children->Add(InputComboBox);
	}


	void BuildToggleButtonControl(
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc,
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst,
		Grid ^ Container) 
	{
		check((*ItInputDesc)->Type == Substance_IType_Integer);

		// Label
		Label^ InputName = gcnew Label;
		InputName->Content = CLRTools::ToString((*ItInputDesc)->Label);
		InputName->ToolTip = CLRTools::ToString((*ItInputDesc)->Identifier);
		InputName->VerticalAlignment = System::Windows::VerticalAlignment::Center;

		CheckBox^ CheckboxControl = gcnew CheckBox;
		CheckboxControl->HorizontalAlignment = System::Windows::HorizontalAlignment::Right;
		CheckboxControl->Style = 
			safe_cast<System::Windows::Style^>(
				CheckboxControl->TryFindResource("OnOffCheckbox"));
		CheckboxControl->HorizontalAlignment = 
			System::Windows::HorizontalAlignment::Left;

		SubstanceAir::FNumericalInputInstance<int_t>* TypedInst =
			(SubstanceAir::FNumericalInputInstance<int_t>*)&(*(*ItInputInst));
		CheckboxControl->IsChecked = TypedInst->Value == 1 ? true : false;

		CheckboxControl->Checked += 
			gcnew System::Windows::RoutedEventHandler(
				this, &MGraphInstanceEditorWindow::ToggleButtonChecked);
		CheckboxControl->Unchecked += 
			gcnew System::Windows::RoutedEventHandler(
				this, &MGraphInstanceEditorWindow::ToggleButtonUnchecked);

		MInputDataContext< >^ data_context =
			gcnew MInputDataContext< >;

		// Pass along the instance and the graph instance
		data_context->mInputPtr.Reset(&(*(*ItInputInst)));
		data_context->mGraphPtr.Reset(ParentInstance);
		CheckboxControl->DataContext = data_context;	

		Container->Children->Add(InputName);
		Container->Children->Add(CheckboxControl);
	}


	Button^ ImageInputButtonFactory(String^ IconUrl)
	{
		Button^ TheButton = gcnew Button;
		TheButton->Width = IMG_INP_BUTTON_SIZE;
		TheButton->Height = IMG_INP_BUTTON_SIZE;
		TheButton->HorizontalAlignment = 
			System::Windows::HorizontalAlignment::Center;

		TheButton->Background = gcnew ImageBrush(
			gcnew BitmapImage(gcnew Uri(IconUrl,UriKind::Absolute)));

		return TheButton;
	}


	System::Windows::UIElement ^ BuildImageInputControl(
		SubstanceAir::List< std::tr1::shared_ptr<input_desc_t> >::TIterator& ItInputDesc,
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator& ItInputInst) 
	{
		StackPanel^ MainContainer = gcnew StackPanel;
		MainContainer->Margin = System::Windows::Thickness(0, 5, 0, 0);

		MainContainer->Width = 195;
		MainContainer->HorizontalAlignment = 
			System::Windows::HorizontalAlignment::Left;

		// image input data context
		MImageInputDataContext^ IIDataContext = gcnew MImageInputDataContext;
		IIDataContext->mInputPtr.Reset(&(*(*ItInputInst)));
		IIDataContext->mGraphPtr.Reset(ParentInstance);

		// thumb and label
		{
			System::Windows::Shapes::Rectangle^ IIThumbnail =
				gcnew System::Windows::Shapes::Rectangle;

			IIThumbnail->RadiusX = 15;
			IIThumbnail->RadiusY = 15;
			IIThumbnail->Width = 64;
			IIThumbnail->Height = 64;

			IIDataContext->mThumbnailRect = IIThumbnail;

				SubstanceAir::FImageInputInstance* TypedInst = 
					(SubstanceAir::FImageInputInstance*)&(*(*ItInputInst));

			if (TypedInst->ImageSource)
			{
				IIThumbnail->Fill = gcnew ImageBrush(
					ThumbnailToolsCLR::GetBitmapSourceForObject(
					(UObject *)TypedInst->ImageSource));
			}
			else
			{
				IIThumbnail->Fill = gcnew ImageBrush(gcnew BitmapImage(gcnew Uri(
					gcnew System::String(*FString::Printf(
						TEXT("%swxres\\SubstanceAir_NoImageInput.png"),
						*GetEditorResourcesDir())),
						UriKind::Absolute)));
			}

			IIThumbnail->DataContext = IIDataContext;
			IIThumbnail->AllowDrop = true;
			IIThumbnail->DragOver +=
				gcnew DragEventHandler(this, &MGraphInstanceEditorWindow::OnDragOver);
			IIThumbnail->Drop +=
				gcnew DragEventHandler(this, &MGraphInstanceEditorWindow::OnDrop);
			IIThumbnail->DragEnter +=
				gcnew DragEventHandler(this, &MGraphInstanceEditorWindow::OnDragEnter);
			IIThumbnail->DragLeave +=
				gcnew DragEventHandler(this, &MGraphInstanceEditorWindow::OnDragLeave);

			Label^ NameLabel = gcnew Label;
			NameLabel->Content = CLRTools::ToString(
				((SubstanceAir::FImageInputDesc*)(*ItInputDesc).get())->Label);
			NameLabel->ToolTip = CLRTools::ToString(
				((SubstanceAir::FImageInputDesc*)(*ItInputDesc).get())->Identifier);
			NameLabel->VerticalAlignment = 
				System::Windows::VerticalAlignment::Center;
			NameLabel->Margin.Bottom = 0;
			NameLabel->Margin.Left = 0;
			NameLabel->Margin.Top = 0;
			NameLabel->Margin.Right = 0;

			DockPanel^ TopContainer = gcnew DockPanel;

			TopContainer->Children->Add(IIThumbnail);
			TopContainer->Children->Add(NameLabel);

			MainContainer->Children->Add(TopContainer);
		}

		// controls
		{
			StackPanel ^ ControlsContainer = gcnew StackPanel;
			ControlsContainer->Orientation = Orientation::Horizontal;

			ControlsContainer->Margin = System::Windows::Thickness(0, 2, 0, 0);
			ControlsContainer->HorizontalAlignment = 
				System::Windows::HorizontalAlignment::Left;

			Button^ ReplaceButton = ImageInputButtonFactory(
				gcnew System::String(
					*FString::Printf(
						TEXT("%swxres\\SubstanceAir_ReplaceImageInput.png"),
						*GetEditorResourcesDir())));
			ReplaceButton->DataContext = IIDataContext;
			ReplaceButton->Click += 
				gcnew System::Windows::RoutedEventHandler(
					this,
					&MGraphInstanceEditorWindow::ImageInputReplaceButton_Click);
			ReplaceButton->Margin = System::Windows::Thickness(3, 0, 0, 0);
			ControlsContainer->Children->Add(ReplaceButton);

			Button^ RemoveButton = ImageInputButtonFactory(
				gcnew System::String(
					*FString::Printf(
						TEXT("%swxres\\SubstanceAir_DeleteImageInput.png"),
						*GetEditorResourcesDir())));
			RemoveButton->DataContext = IIDataContext;
			RemoveButton->Click += 
				gcnew System::Windows::RoutedEventHandler(
					this,
					&MGraphInstanceEditorWindow::ImageInputRemoveButton_Click);
			RemoveButton->Margin = System::Windows::Thickness(2, 0, 0, 0);
			ControlsContainer->Children->Add(RemoveButton);

			Button^ SyncButton = ImageInputButtonFactory(
				gcnew System::String(
					*FString::Printf(
						TEXT("%swxres\\SubstanceAir_SyncCB.png"),
						*GetEditorResourcesDir())));
			SyncButton->DataContext = IIDataContext;
			SyncButton->Click += 
				gcnew System::Windows::RoutedEventHandler(
					this,
					&MGraphInstanceEditorWindow::ImageInputSyncButton_Click);
			SyncButton->Margin = System::Windows::Thickness(2, 0, 0, 0);
			ControlsContainer->Children->Add(SyncButton);

			MainContainer->Children->Add(ControlsContainer);
		}

		return MainContainer;
	}


	void OnDragEnter(Object^ Sender, DragEventArgs^ Args)
	{
		if (!CanEdit())
		{
			return;
		}

		// Assets being dropped from content browser should be parsable from a string format
		if (Args->Data->GetDataPresent(DataFormats::StringFormat))
		{
			const TCHAR AssetDelimiter[] = 
				{ AssetMarshalDefs::AssetDelimiter, TEXT('\0') };

			// Extract the string being dragged over the panel
			String^ DroppedData = safe_cast<String^>(
				Args->Data->GetData(DataFormats::StringFormat));
			FString SourceData = CLRTools::ToFString(DroppedData);

			// Parse the dropped string into separate asset strings
			TArray<FString> DroppedAssetStrings;
			SourceData.ParseIntoArray(&DroppedAssetStrings, AssetDelimiter, TRUE);

			// Construct drop data info for each parsed asset string
			DroppedAssets.Reset(new TArray<FSelectedAssetInfo>());
			DroppedAssets->Empty(DroppedAssetStrings.Num());
			TArray<FString>::TConstIterator StringIter(DroppedAssetStrings);
			for (; StringIter; ++StringIter)
			{
				new(*(DroppedAssets.Get())) FSelectedAssetInfo(*StringIter);
			}
			Args->Handled = TRUE;
		}
	}


	void OnDragLeave(Object^ Sender, DragEventArgs^ Args)
	{
		if (!CanEdit())
		{
			return;
		}

		if(DroppedAssets.Get() != NULL)
		{
			DroppedAssets->Empty();
			DroppedAssets.Reset(NULL);
		}
		Args->Handled = TRUE;
	}


	void OnDragOver(Object^ Owner, DragEventArgs^ Args)
	{
		if (!CanEdit())
		{
			return;
		}

		Args->Effects = DragDropEffects::Copy;

		if (DroppedAssets.Get() != NULL)
		{
			TArray<FSelectedAssetInfo>::TConstIterator 
				DroppedAssetsIter(*(DroppedAssets.Get()));

			for (; DroppedAssetsIter; ++DroppedAssetsIter)
			{
				const FSelectedAssetInfo& CurInfo = *DroppedAssetsIter;
				if (Cast<USubstanceAirImageInput>(CurInfo.Object) == NULL && 
					Cast<USubstanceAirTexture2D>(CurInfo.Object) == NULL)
				{
					Args->Effects = DragDropEffects::None;
					break;
				}
			}
		}

		Args->Handled = TRUE;
	}


	void OnDrop(Object^ Owner, DragEventArgs^ Args)
	{
		if (!CanEdit())
		{
			return;
		}

		if (DroppedAssets.Get()!=NULL)
		{
			TArray<FSelectedAssetInfo>::TConstIterator
				DroppedAssetsIter(*(DroppedAssets.Get()));

			for (; DroppedAssetsIter; ++DroppedAssetsIter)
			{
				const FSelectedAssetInfo& CurInfo = *DroppedAssetsIter;

				UObject* ImageInput = 
					LoadObject<UObject>(
						NULL, 
						*CurInfo.ObjectPathName,
						NULL, LOAD_None, NULL);

				if (Cast<USubstanceAirImageInput>(ImageInput) != NULL ||
					Cast<USubstanceAirTexture2D>(ImageInput) != NULL)
				{
					MImageInputDataContext^ Context =
						safe_cast<MImageInputDataContext^>(
							safe_cast<System::Windows::Shapes::Rectangle^>(Owner)->DataContext);

					MNativePointer< input_inst_t > InputPtr(Context->mInputPtr.Get());
					MNativePointer< graph_inst_t > InstancePtr(Context->mGraphPtr.Get());

					if (InstancePtr->UpdateInput(
							InputPtr->Uid,
							ImageInput))
					{
						UObject* ActualImageInput =
							((SubstanceAir::FImageInputInstance*)
							InputPtr.Get())->ImageSource;

						if (ImageInput && 
							ImageInput == ActualImageInput)
						{
							Context->mThumbnailRect->Fill = 
								gcnew ImageBrush(
								ThumbnailToolsCLR::GetBitmapSourceForObject(
								(UObject *)ActualImageInput));
						}					
					}

					SubstanceAir::Helpers::RenderAsync(InstancePtr.Get());	
				}
			}
		}

		Args->Handled = TRUE;
	}


	void ImageInputReplaceButton_Click(Object^ Owner, RoutedEventArgs^ Args)
	{
		if (!CanEdit())
		{
			return;
		}

		// Get current selection from content browser
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
		USelection* SelectedSet = 
			GEditor->GetSelectedSet(UObject::StaticClass());
		UObject* SelectedImageInput = 
			SelectedSet->GetTop(UObject::StaticClass());

		if (Cast<USubstanceAirImageInput>(SelectedImageInput) != NULL ||
			Cast<USubstanceAirTexture2D>(SelectedImageInput) != NULL)
		{
			MImageInputDataContext^ Context = 
				safe_cast<MImageInputDataContext^>
					(safe_cast<Button^>(Owner)->DataContext);

			MNativePointer< input_inst_t > InputPtr(Context->mInputPtr.Get());
			MNativePointer< graph_inst_t > InstancePtr(Context->mGraphPtr.Get());

			if (InstancePtr->UpdateInput(
					InputPtr->Uid,
					SelectedImageInput))
			{
				UObject* ActualImageInput =
					((SubstanceAir::FImageInputInstance*)
						InputPtr.Get())->ImageSource;

				if (SelectedImageInput && 
					SelectedImageInput == ActualImageInput)
				{
					Context->mThumbnailRect->Fill = 
						gcnew ImageBrush(
							ThumbnailToolsCLR::GetBitmapSourceForObject(
							(UObject *)ActualImageInput));
				}					
			}

			SubstanceAir::Helpers::RenderAsync(InstancePtr.Get());
		}
	}


	void ImageInputSyncButton_Click(Object^ Owner, RoutedEventArgs^ Args)
	{
		if (!CanEdit())
		{
			return;
		}

		MImageInputDataContext^ Context = 
			safe_cast<MImageInputDataContext^>(
				safe_cast<Button^>(Owner)->DataContext);
		UObject* SelectedImageInput =
			((SubstanceAir::FImageInputInstance*)
				(Context->mInputPtr.Get()))->ImageSource;

		if (SelectedImageInput != NULL)
		{
			TArray<UObject*> Objects;
			Objects.AddItem(SelectedImageInput);
			GApp->EditorFrame->SyncBrowserToObjects(Objects);
		}
	}


	void ImageInputRemoveButton_Click(Object^ Owner, RoutedEventArgs^ Args)
	{
		if (!CanEdit())
		{
			return;
		}

		MImageInputDataContext^ Context = 
			safe_cast<MImageInputDataContext^>(
				safe_cast<Button^>(Owner)->DataContext);
		
		MNativePointer< input_inst_t > InputPtr(Context->mInputPtr.Get());
		MNativePointer< graph_inst_t > InstancePtr(Context->mGraphPtr.Get());

		InstancePtr->UpdateInput(
			InputPtr->Uid,
			NULL);

		Context->mThumbnailRect->Fill =
			gcnew ImageBrush(gcnew BitmapImage(gcnew Uri(
				gcnew System::String(*FString::Printf(
					TEXT("%swxres\\SubstanceAir_NoImageInput.png"),
					*GetEditorResourcesDir())),
					UriKind::Absolute)));

		SubstanceAir::Helpers::RenderAsync(
			Context->mGraphPtr.Get());
	}


	void CheckedOutput(
		System::Object ^ sender, 
		System::Windows::RoutedEventArgs^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		CheckBox^ checkbox = safe_cast<CheckBox^>(sender);
		MOutputDataContext^ context = 
			safe_cast< MOutputDataContext^ >(checkbox->DataContext);
		
		if (FALSE == context->mOutputPtr->bIsEnabled)
		{
			SubstanceAir::Helpers::CreateTexture2D(
				context->mOutputPtr.Get());

			SubstanceAir::List<graph_inst_t*> Instances;
			Instances.push(context->mGraphPtr.Get());

			SubstanceAir::Helpers::RenderAsync(Instances);

			SubstanceAir::Helpers::FlagRefreshContentBrowser();
		}

		e->Handled = true;
	}


	void UncheckedOutput(
		System::Object^ sender, 
		System::Windows::RoutedEventArgs^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		CheckBox^ checkbox = safe_cast<CheckBox^>(sender);
		MOutputDataContext^ context = 
			safe_cast< MOutputDataContext^ >(checkbox->DataContext);

		if (context->mOutputPtr->bIsEnabled &&
			*(context->mOutputPtr->Texture))
		{
			SubstanceAir::Helpers::RegisterForDeletion(
				*(context->mOutputPtr->Texture));

			SubstanceAir::Helpers::Tick();
		}

		e->Handled = true;
	}


	void ToggleButtonSetValue(
		MNativePointer< input_inst_t > InputPtr,
		MNativePointer< graph_inst_t > InstancePtr,
		bool isChecked)
	{
		if (!CanEdit())
		{
			return;
		}

		if (InputPtr.IsValid() && InstancePtr.IsValid())
		{
			TArray<INT> Values;
			Values.AddItem(isChecked ? 1 : 0);

			InstancePtr->UpdateInput< int >(
				InputPtr->Uid,
				Values);

			SubstanceAir::Helpers::RenderAsync(InstancePtr.Get());
		}
	}


	void ToggleButtonChecked(
		System::Object^ sender, 
		System::Windows::RoutedEventArgs^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		CheckBox^ LockedCheckbox = safe_cast<CheckBox^>(sender);
		MInputDataContext<  >^ context = 
			safe_cast< MInputDataContext<  >^ >(LockedCheckbox->DataContext);

		MNativePointer< graph_inst_t > InstancePtr(context->mGraphPtr.Get());

		if (!InstancePtr.IsValid())
		{
			return;
		}

		FScopedTransaction Transaction(
			*Localize(TEXT("Editor"), 
			TEXT("ModifiedInput"),
			TEXT("SubstanceAir"), NULL, 0));

		ToggleButtonSetValue(
			MNativePointer< input_inst_t >(context->mInputPtr.Get()),
			MNativePointer< graph_inst_t >(context->mGraphPtr.Get()), 
			true);

		InstancePtr->ParentInstance->Modify();

		e->Handled = true;
	}


	void ToggleButtonUnchecked(
		System::Object^ sender, 
		System::Windows::RoutedEventArgs^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		CheckBox^ LockedCheckbox = safe_cast<CheckBox^>(sender);
		MInputDataContext<  >^ context = 
			safe_cast< MInputDataContext<>^ >(LockedCheckbox->DataContext);

		MNativePointer< graph_inst_t > InstancePtr(context->mGraphPtr.Get());

		if (!InstancePtr.IsValid())
		{
			return;
		}

		FScopedTransaction Transaction(
			*Localize(TEXT("Editor"), 
			TEXT("ModifiedInput"),
			TEXT("SubstanceAir"), NULL, 0));

		ToggleButtonSetValue(
			MNativePointer< input_inst_t >(context->mInputPtr.Get()),
			MNativePointer< graph_inst_t >(context->mGraphPtr.Get()), 
			false);

		InstancePtr->ParentInstance->Modify();

		e->Handled = true;
	}


	void RandomizeButtonPressed(
		System::Object^ Sender,
		System::Windows::RoutedEventArgs^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		Button^ randomize_buttton = safe_cast<Button^>(Sender);

		MRandomizeDataContext^ context = 
			safe_cast< MRandomizeDataContext^ >(randomize_buttton->DataContext);

		MNativePointer< graph_inst_t > InstancePtr(context->mGraphPtr.Get());

		if (!InstancePtr.IsValid())
		{
			return;
		}

		FScopedTransaction Transaction(
			*Localize(TEXT("Editor"), 
			TEXT("ModifiedInput"),
			TEXT("SubstanceAir"), NULL, 0));

		InstancePtr->ParentInstance->Modify();

		context->mRandomSlider->Value = appRand();
		e->Handled = true;
	}


	void ResetToDefaultPressed(
		System::Object^ Sender,
		System::Windows::RoutedEventArgs^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		SubstanceAir::Helpers::ResetToDefault(ParentInstance);

		BuildInputList();
		BuildImageInputList();

		SubstanceAir::Helpers::RenderAsync(ParentInstance);
	}


	void PickColorPressed(
		System::Object^ Sender,
		System::Windows::RoutedEventArgs^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		Button^ PickButton = safe_cast<Button^>(Sender);
		CurrentColorEdited = safe_cast< PickColorDataContext^ >(PickButton->DataContext);

		MNativePointer< graph_inst_t > InstancePtr(CurrentColorEdited->mGraphPtr.Get());

		if (!InstancePtr.IsValid())
		{
			return;
		}

		FScopedTransaction Transaction(
			*Localize(TEXT("Editor"), 
				TEXT("ModifiedInput"),
				TEXT("SubstanceAir"), NULL, 0));
		InstancePtr->ParentInstance->Modify();

		// only one allowed at the same time
		CloseColorPickers();

		CurrentColorEdited->StartR = PickColorData->R = CurrentColorEdited->mSliderRed->Value;
		CurrentColorEdited->StartG = PickColorData->G = CurrentColorEdited->mSliderGreen->Value;
		CurrentColorEdited->StartB = PickColorData->B = CurrentColorEdited->mSliderBlue->Value;

		if (CurrentColorEdited->mSliderAlpha)
		{
			CurrentColorEdited->StartA = PickColorData->A = CurrentColorEdited->mSliderAlpha->Value;
		}

		ColorPickerConstants::ColorPickerResults res = PickColorWPF(PickColorStruct);
		CurrentColorEdited->ColorPickerIndex = MColorPickerPanel::GetNumColorPickerPanels()-1;

		// grab the last color picker and plug callbacks
		MColorPickerPanel^ TempPanel = 
			MColorPickerPanel::GetStaticColorPicker(CurrentColorEdited->ColorPickerIndex);

		TempPanel->AddCallbackDelegate(
			gcnew ColorPickerPropertyChangeFunction(
			this,
			&MGraphInstanceEditorWindow::UpdatePreviewColors));

		Button^ CancelButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( TempPanel, "CancelButton" ) );
		CancelButton->Click += gcnew RoutedEventHandler( this, &MGraphInstanceEditorWindow::CancelColorClicked );
		CancelButton->Click += gcnew RoutedEventHandler( this, &MGraphInstanceEditorWindow::OkColorClicked );

		e->Handled = true;
	}


	void OkColorClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		CurrentColorEdited = nullptr;
	}


	void CancelColorClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		PickColorDataContext^ ColorEdited = CurrentColorEdited;
		CurrentColorEdited = nullptr;

		ColorEdited->mSliderRed->Value = ColorEdited->StartR;
		ColorEdited->mSliderGreen->Value = ColorEdited->StartG;
		ColorEdited->mSliderBlue->Value = ColorEdited->StartB;

		if (ColorEdited->mSliderAlpha)
		{
			ColorEdited->mSliderAlpha->Value = ColorEdited->StartA;
		}
	}


	void UpdatePreviewColors()
	{
		MNativePointer< input_inst_t > InputPtr(CurrentColorEdited->mInputPtr.Get());
		MNativePointer< graph_inst_t > InstancePtr(CurrentColorEdited->mGraphPtr.Get());

			if (!InputPtr.IsValid() || !InstancePtr.IsValid())
			{
				return;
			}

			SubstanceAir::FNumericalInputInstanceBase* InputInst =
				(SubstanceAir::FNumericalInputInstanceBase*)(InputPtr.Get());
	
			TArray<FLOAT> Values(4);
			Values(0) = PickColorData->R;
			Values(1) = PickColorData->G;
			Values(2) = PickColorData->B;
			Values(3) = PickColorData->A;

		CurrentColorEdited->bSyncEnabled = false;

		CurrentColorEdited->mSliderRed->Value = Values(0);
		CurrentColorEdited->mSliderGreen->Value = Values(1);
		CurrentColorEdited->mSliderBlue->Value = Values(2);

		if (CurrentColorEdited->mSliderAlpha != nullptr)
		{
			CurrentColorEdited->mSliderAlpha->Value = Values(3);
		}

		CurrentColorEdited->bSyncEnabled = true;
	}
	

	void ComboBoxChoiceChanged(
		System::Object^ sender, 
		System::Windows::Controls::SelectionChangedEventArgs^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		ComboBox^ combo_box = safe_cast<ComboBox^>(sender);

		MInputDataContext< int >^ context =
			safe_cast< MInputDataContext< int >^ >(
				combo_box->DataContext);

		MNativePointer< input_inst_t > InputPtr(context->mInputPtr.Get());
		MNativePointer< graph_inst_t > InstancePtr(context->mGraphPtr.Get());

		if (InputPtr.IsValid() && InstancePtr.IsValid())
		{
			FScopedTransaction Transaction(
				*Localize(TEXT("Editor"), 
				TEXT("ModifiedInput"),
				TEXT("SubstanceAir"), NULL, 0));
			InstancePtr->ParentInstance->Modify();

			TArray<INT> Values;

			INT Value = context->mValue[combo_box->SelectedIndex];
			Values.AddItem(Value);

			InstancePtr->UpdateInput< INT >(
				InputPtr->Uid,
				Values);

			SubstanceAir::Helpers::RenderAsync(InstancePtr.Get());
		}

		e->Handled = true;
	}


	void Pow2Changed(
		System::Object^ sender, 
		System::Windows::Controls::SelectionChangedEventArgs^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		ComboBox^ combo_box = safe_cast<ComboBox^>(sender);
		MSizePow2Context^ context =
			safe_cast< MSizePow2Context^ >(combo_box->DataContext);

		MNativePointer< input_inst_t > InputPtr(context->mInputPtr.Get());
		MNativePointer< graph_inst_t > InstancePtr(context->mGraphPtr.Get());

		if (InputPtr.IsValid() && InstancePtr.IsValid())
		{
			if (false == context->mSkipUpdate)
			{
				SubstanceAir::FNumericalInputInstanceBase* InputInst =
					(SubstanceAir::FNumericalInputInstanceBase*)(InputPtr.Get());

				TArray<int> OutputSizeValue;
				InputInst->GetValue(OutputSizeValue);

				int thisIndex = context->mIndex;
				int otherIndex = thisIndex ? 0 : 1;

				int oldValuePow2 = OutputSizeValue(thisIndex);
				int newValuePow2 = Clamp<int>(
					combo_box->SelectedIndex + POW2_MIN,
					POW2_MIN, 
					POW2_MAX);

				OutputSizeValue(thisIndex) = newValuePow2;

				if (context->mRatioLock->IsChecked.Equals(true))
				{
					int deltaValue = newValuePow2 - oldValuePow2;
					int oldOtherValuePow2 = OutputSizeValue(otherIndex);
					int newOtherValuePow2 = Clamp<int>(
						oldOtherValuePow2 + deltaValue, 
						POW2_MIN, 
						POW2_MAX);

					OutputSizeValue(otherIndex) = newOtherValuePow2;

					MSizePow2Context^ otherContext =
						safe_cast< MSizePow2Context^ >(
							context->mOtherSize->DataContext);

					otherContext->mSkipUpdate = true;

					context->mOtherSize->SelectedIndex =
						newOtherValuePow2 - POW2_MIN;

					otherContext->mSkipUpdate = false;
				}

				int OutputCount = InstancePtr->UpdateInput< INT >(
					InputPtr->Uid,
					OutputSizeValue);

				SubstanceAir::Helpers::RenderAsync(InstancePtr.Get());
				SubstanceAir::Helpers::FlagRefreshContentBrowser(OutputCount);
			}
		}

		e->Handled = true;
	}
		

	void SliderMouseDown( Object^ Sender, MouseButtonEventArgs^ e )
	{
		CustomControls::DragSlider^ Slider = safe_cast<CustomControls::DragSlider^>(Sender);
		MInputDataContext< float >^ Context = 
			safe_cast< MInputDataContext< float >^ >(Slider->DataContext);

		MNativePointer< graph_inst_t > InstancePtr(Context->mGraphPtr.Get());

		if (InstancePtr.IsValid())
		{
			FScopedTransaction Transaction(
				*Localize(TEXT("Editor"), 
				TEXT("ModifiedInput"),
				TEXT("SubstanceAir"), NULL, 0));
			InstancePtr->ParentInstance->Modify();
		}
	}


	template< typename T > void SliderValueChanged(
		System::Object^ sender,
		System::Windows::RoutedPropertyChangedEventArgs<double>^ e)
	{
		if (!CanEdit())
		{
			return;
		}

		CustomControls::DragSlider^ Slider = safe_cast<CustomControls::DragSlider^>(sender);
		MInputDataContext< T >^ context = 
			safe_cast< MInputDataContext< T >^ >(Slider->DataContext);

		MNativePointer< input_inst_t > InputPtr(context->mInputPtr.Get());
		MNativePointer< graph_inst_t > InstancePtr(context->mGraphPtr.Get());

		if (InputPtr.IsValid() && InstancePtr.IsValid())
		{
			SubstanceAir::FNumericalInputInstanceBase* InputInst =
				(SubstanceAir::FNumericalInputInstanceBase*)(InputPtr.Get());

			TArray< T > Values;
			InputInst->GetValue(Values);

			if (Values.Num() == 0)
			{
				return;
			}

			Values(context->mIndex) = e->NewValue;

			if (InputInst->Desc->Widget == SubstanceAir::Input_Color && Values.Num() >= 3)
			{
				PickColorData->Component(context->mIndex) = e->NewValue;
				context->mPickButton->Background = gcnew SolidColorBrush(Color::FromArgb(
					(System::Byte)(255),
					(System::Byte)(PickColorData->Component(0) * 255.0f),
					(System::Byte)(PickColorData->Component(1) * 255.0f),
					(System::Byte)(PickColorData->Component(2) * 255.0f)
					));
			}

			InstancePtr->UpdateInput< T >(
				InputPtr->Uid,
				Values);

			SubstanceAir::Helpers::RenderAsync(InstancePtr.Get());

			if (CurrentColorEdited != nullptr && CurrentColorEdited->bSyncEnabled)
			{
				// grab the last color picker and plug callbacks
				MColorPickerPanel^ TempPanel = 
					MColorPickerPanel::GetStaticColorPicker(CurrentColorEdited->ColorPickerIndex);

				// kind of a trick, this resets start color which is duplicated in CurrentColorEdited
				TempPanel->BindData();
			}
		}

		e->Handled = true;
	}
};


/** Static: Allocate and initialize graph instance editor window */
FGraphInstanceEditorWindow* FGraphInstanceEditorWindow::CreateGraphInstanceEditorWindow(
	SubstanceAir::FGraphInstance* InGraphInstance,
	const HWND InParentWindowHandle)
{
	FGraphInstanceEditorWindow* NewGraphInstanceEditorWindow = 
		new FGraphInstanceEditorWindow();

	if (!NewGraphInstanceEditorWindow->InitGraphInstanceEditorWindow(
			InGraphInstance,
			InParentWindowHandle))
	{
		delete NewGraphInstanceEditorWindow;
		return NULL;
	}

	GraphInstance = InGraphInstance;

	return NewGraphInstanceEditorWindow;
}


/** Constructor */
FGraphInstanceEditorWindow::FGraphInstanceEditorWindow():
	GraphInstanceEditorFrame(nullptr),
	GraphInstanceEditorPanel(nullptr)
{
	GCallbackEvent->Register(CALLBACK_PackageSaved, this);
	GCallbackEvent->Register(CALLBACK_PreEditorClose, this);
	GCallbackEvent->Register(CALLBACK_TextureModified, this);
	GCallbackEvent->Register(CALLBACK_ObjectPropertyChanged, this);
}


/** Destructor */
FGraphInstanceEditorWindow::~FGraphInstanceEditorWindow()
{
	GraphInstanceEditorFrame.reset();
	GraphInstanceEditorPanel.reset();
}


/** Initialize the graph instance editor window */
UBOOL FGraphInstanceEditorWindow::InitGraphInstanceEditorWindow(
	SubstanceAir::FGraphInstance* InGraphInstance,
	const HWND InParentWindowHandle)
{
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle =  gcnew String("Graph Instance Editor");
	Settings->bShowCloseButton = TRUE;
	Settings->bUseWxDialog = TRUE;

	Settings->PositionX = -1;
	Settings->PositionY = -1;

	PickColorStruct.FLOATColorArray.AddItem(&(PickColorData));
	PickColorStruct.bModal = FALSE;
	PickColorStruct.bUseSrgb = FALSE;

	GraphInstanceEditorFrame = 
		gcnew MWPFFrame(GApp->EditorFrame , Settings, TEXT("GraphInstanceEditor"));

	GraphInstanceEditorPanel = 
		gcnew MGraphInstanceEditorWindow(
			GraphInstanceEditorFrame.get(),
			InGraphInstance,
			PickColorStruct,
			&PickColorData);

	GraphInstanceEditorFrame->SetContentAndShow(GraphInstanceEditorPanel.get());
	return TRUE;
}


/** Refresh all properties */
void FGraphInstanceEditorWindow::RefreshAllProperties()
{
	GraphInstanceEditorPanel->RefreshAllProperties();
}


/** Saves window settings to the Graph Instance settings structure */
void FGraphInstanceEditorWindow::SaveWindowSettings()
{

}


/** Returns true if the mouse cursor is over the window */
UBOOL FGraphInstanceEditorWindow::IsMouseOverWindow()
{
	if(GraphInstanceEditorPanel.get() != nullptr)
	{
		FrameworkElement^ WindowContentElement = 
			safe_cast<FrameworkElement^>(
				GraphInstanceEditorFrame->GetRootVisual());

		if(WindowContentElement->IsMouseOver)
		{
			return TRUE;
		}
	}

	return FALSE;
}


/** Override from FCallbackEventDevice to handle events */
void FGraphInstanceEditorWindow::Send(ECallbackEventType Event)
{
	switch (Event)
	{
	case CALLBACK_PreEditorClose:
		CloseColorPickers();
		GCallbackEvent->UnregisterAll(this);
		GraphInstanceEditorFrame.reset();
		GraphInstanceEditorPanel.reset();
		FGraphInstanceEditorWindow::GraphInstance = NULL;
		break;
	}
}


void FGraphInstanceEditorWindow::Send(ECallbackEventType Event, UObject* InObject)
{
	if (Event == CALLBACK_ObjectPropertyChanged && GetEditedInstance())
	{
		if (InObject == GetEditedInstance()->ParentInstance)
		{
			GraphInstanceEditorPanel->BuildInputList();
			GraphInstanceEditorPanel->BuildImageInputList();
			GraphInstanceEditorPanel->SynchColorPicker();
		}
	}
}

void FGraphInstanceEditorWindow::Send(ECallbackEventType Event, const FString& InString, UObject* InObject)
{
	switch (Event)
	{
	case CALLBACK_PackageSaved:
		if (GetEditedInstance() && GetEditedInstance()->ParentInstance->GetOutermost() == InObject &&
			!(GApp->AutosaveState == WxUnrealEdApp::AUTOSAVE_Saving))
		{
			GraphInstanceEditorFrame.reset();
			GraphInstanceEditorPanel.reset();
			FGraphInstanceEditorWindow::GraphInstance = NULL;
		}
		break;
	}
}


SubstanceAir::FGraphInstance* FGraphInstanceEditorWindow::GetEditedInstance()
{
	return FGraphInstanceEditorWindow::GraphInstance;
}
