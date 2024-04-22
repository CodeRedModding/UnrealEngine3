//! @file SubstanceAirEdXmlHelper.cpp
//! @brief Substance Air description XML parsing helper
//! @author Gonzalez Antoine - Allegorithmic
//! @date 20101229
//! @copyright Allegorithmic. All rights reserved.

#include <UnrealEd.h>
#include <Engine.h>

#include <tinyxml.h>

#include <memory>	//std::auto_ptr
#include <stdlib.h>	//for strtoul

#pragma pack ( push, 8 )
#include <substance/handle.h>
#pragma pack ( pop )

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirHelpers.h"

#include "SubstanceAirEdPreset.h"
#include "SubstanceAirEdXmlHelper.h"


IMPLEMENT_COMPARE_CONSTREF(input_desc_ptr, SubstanceAirEdXmlHelper,
{
	return (DWORD)(*A).Uid > (DWORD)(*B).Uid ? 1 : -1; 
})


IMPLEMENT_COMPARE_CONSTREF(uint_t, SubstanceAirEdXmlHelper,
{
	return A > B ? 1 : -1;
})


IMPLEMENT_COMPARE_CONSTREF(output_desc_t,SubstanceAirEdXmlHelper,
{
	return A.Uid > B.Uid ? 1 : -1;
})


namespace SubstanceAir
{
namespace Helpers
{
	
//! @brief Parse an output node from the Substance Air xml desc file
//! @note Not reading the default size as we do not use it 
UBOOL ParseOutput(TiXmlElement* OutputNode, graph_desc_t* ParentGraph)
{
	output_desc_t Output;

	FString Attribute = FString(ANSI_TO_TCHAR(OutputNode->Attribute("uid")));
	if (Attribute.Len() <= 0)
	{
		return FALSE;
	}
	Output.Uid = appAtoul(*Attribute);

	Attribute = FString(UTF8_TO_TCHAR(OutputNode->Attribute("identifier")));
	if (Attribute.Len() <= 0)
	{
		return FALSE;
	}
	Output.Identifier = Attribute;

	Attribute = FString(ANSI_TO_TCHAR(OutputNode->Attribute("format")));
	if (Attribute.Len() <= 0)
	{
		return FALSE;
	}
	// Not every formats is supported, 
	// check it and force another is not supported
	Output.Format = Helpers::ValidateFormat((SubstancePixelFormat)appAtoi(*Attribute));

	// try to read the channel from the GUI info
	Output.Channel = CHAN_Undef;
	TiXmlElement* OutputGuiNode = OutputNode->FirstChildElement("outputgui");

	if (OutputGuiNode)
	{
		Attribute = FString(UTF8_TO_TCHAR(OutputGuiNode->Attribute("label")));
		if (Attribute.Len() <= 0)
		{
			return FALSE;
		}
		Output.Label = Attribute;


		TiXmlElement* ChannelsNode = OutputGuiNode->FirstChildElement("channels");

		if (ChannelsNode)
		{
			TiXmlElement* ChannelNode = ChannelsNode->FirstChildElement("channel");

			if (ChannelNode)
			{
				Attribute = FString(UTF8_TO_TCHAR(ChannelNode->Attribute("names")));

				if (FString(TEXT("diffuse")) == Attribute)
				{
					Output.Channel = CHAN_Diffuse;
				}
				else if (FString(TEXT("normal")) == Attribute)
				{
					Output.Channel = CHAN_Normal;
				}
				else if (FString(TEXT("opacity")) == Attribute)
				{
					Output.Channel = CHAN_Opacity;
				}
				else if (FString(TEXT("emissive")) == Attribute)
				{
					Output.Channel = CHAN_Emissive;
				}
				else if (FString(TEXT("ambient")) == Attribute)
				{
					Output.Channel = CHAN_Ambient;
				}
				else if (FString(TEXT("ambientOcclusion")) == Attribute)
				{
					Output.Channel = CHAN_AmbientOcclusion;
				}
				else if (FString(TEXT("mask")) == Attribute)
				{
					Output.Channel = CHAN_Mask;
				}
				else if (FString(TEXT("bump")) == Attribute)
				{
					Output.Channel = CHAN_Bump;
				}
				else if (FString(TEXT("height")) == Attribute)
				{
					Output.Channel = CHAN_Height;
				}
				else if (FString(TEXT("displacement")) == Attribute)
				{
					Output.Channel = CHAN_Displacement;
				}
				else if (FString(TEXT("specular")) == Attribute)
				{
					Output.Channel = CHAN_Specular;
				}
				else if (FString(TEXT("specularLevel")) == Attribute)
				{
					Output.Channel = CHAN_SpecularLevel;
				}
				else if (FString(TEXT("specularColor")) == Attribute)
				{
					Output.Channel = CHAN_SpecularColor;
				}
				else if (FString(TEXT("glossiness")) == Attribute)
				{
					Output.Channel = CHAN_Glossiness;
				}
				else if (FString(TEXT("roughness")) == Attribute)
				{
					Output.Channel = CHAN_Roughness;
				}
				else if (FString(TEXT("anisotropyLevel")) == Attribute)
				{
					Output.Channel = CHAN_AnisotropyLevel;
				}
				else if (FString(TEXT("anisotropyAngle")) == Attribute)
				{
					Output.Channel = CHAN_AnisotropyAngle;
				}
				else if (FString(TEXT("transmissive")) == Attribute)
				{
					Output.Channel = CHAN_Transmissive;
				}
				else if (FString(TEXT("reflection")) == Attribute)
				{
					Output.Channel = CHAN_Reflection;
				}
				else if (FString(TEXT("refraction")) == Attribute)
				{
					Output.Channel = CHAN_Refraction;
				}
				else if (FString(TEXT("environment")) == Attribute)
				{
					Output.Channel = CHAN_Environment;
				}
				else if (FString(TEXT("IOR")) == Attribute)
				{
					Output.Channel = CHAN_IOR;
				}
				else if (FString(TEXT("scattering0")) == Attribute)
				{
					Output.Channel = CU_SCATTERING0;
				}
				else if (FString(TEXT("scattering1")) == Attribute)
				{
					Output.Channel = CU_SCATTERING1;
				}
				else if (FString(TEXT("scattering2")) == Attribute)
				{
					Output.Channel = CU_SCATTERING2;
				}
				else if (FString(TEXT("scattering3")) == Attribute)
				{
					Output.Channel = CU_SCATTERING3;
				}
			}
		}
	}
	else
	{
		Output.Label = Output.Identifier;
	}

	// give the ownership to the parent graph
	ParentGraph->OutputDescs.push(Output);

	return TRUE;
}


//! @brief Parse an input node from the substance xml companion file
UBOOL ParseInput(TiXmlElement* InputNode, graph_desc_t* ParentGraph)
{
	std::tr1::shared_ptr<input_desc_t> Input;

	FString Attribute = FString(ANSI_TO_TCHAR(InputNode->Attribute("type")));
	if (Attribute.Len() <= 0)
	{
		return FALSE;
	}
	int_t Type = appAtoi(*Attribute);

	switch((SubstanceInputType)Type)
	{
	case Substance_IType_Float:
		Input = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<float_t>);
		break;
	case Substance_IType_Float2:
		Input = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec2float_t>);
		break;
	case Substance_IType_Float3:
		Input = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec3float_t>);
		break;
	case Substance_IType_Float4:
		Input = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec4float_t>);
		break;
	case Substance_IType_Integer:
		Input = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<int_t>);
		break;
	case Substance_IType_Integer2:
		Input = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec2int_t>);
		break;
	case Substance_IType_Integer3:
		Input = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec3int_t>);
		break;
	case Substance_IType_Integer4:
		Input = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec4int_t>);
		break;
	case Substance_IType_Image:
		Input = std::tr1::shared_ptr<input_desc_t>(new FImageInputDesc());
		break;
	default:
		return FALSE;
		break;
	}
	Input->Type = Type;

	Input->Identifier = 
		FString(UTF8_TO_TCHAR(InputNode->Attribute("identifier")));
	if (Input->Identifier.Len()<=0)
	{
		return FALSE;
	}

	// $outputsize, $pixelsize and $randomseed are heavyduty input
	if (InputNode->Attribute("identifier")[0] == '$')
	{
		Input->IsHeavyDuty = TRUE;
	}
	else
	{
		Input->IsHeavyDuty = FALSE;
	}

	Attribute = FString(ANSI_TO_TCHAR(InputNode->Attribute("uid")));
	if (Attribute.Len()<=0)
	{
		return FALSE;
	}
	Input->Uid = appAtoul(*Attribute);

	//Parse alter outputs list and match uid with outputs
	FString AlteroutputsNode(InputNode->Attribute("alteroutputs"));
	TArray<FString> AlteredOutputsArray;
	AlteroutputsNode.ParseIntoArray(&AlteredOutputsArray, TEXT(","), true); 

	for (int_t i = 0; i < AlteredOutputsArray.Num() ; ++i)
	{
		uint_t Uid = appAtoul(*AlteredOutputsArray(i));
		
		//push the output in the list of outputs altered by the input
		Input->AlteredOutputUids.push(Uid);

		List<output_desc_t>::TIterator ItDesc(ParentGraph->OutputDescs.itfront());

		for (; ItDesc ; ++ItDesc)
		{
			if ((*ItDesc).Uid == Uid)
			{
				//and push the modifying uid in the output's list
				(*ItDesc).AlteringInputUids.push(Input->Uid);
				break;
			}
		}
	}

	Sort<USE_COMPARE_CONSTREF(uint_t, SubstanceAirEdXmlHelper)>(
		&Input->AlteredOutputUids(0),
		Input->AlteredOutputUids.Num());

	if (InputNode->Attribute("default"))
	{
		FString DefaultValue = FString(InputNode->Attribute("default"));
		TArray<FString> DefaultValueArray;
		DefaultValue.ParseIntoArray(&DefaultValueArray,TEXT(","),TRUE);

		switch ((SubstanceInputType)Input->Type)
		{
		case Substance_IType_Float:
			((FNumericalInputDesc<float_t>*)Input.get())->DefaultValue =
				appAtof(*DefaultValueArray(0));
			break;
		case Substance_IType_Float2:
			((FNumericalInputDesc<vec2float_t>*)Input.get())->DefaultValue.X =
				appAtof(*DefaultValueArray(0));
			((FNumericalInputDesc<vec2float_t>*)Input.get())->DefaultValue.Y =
				appAtof(*DefaultValueArray(1));
			break;
		case Substance_IType_Float3:
			((FNumericalInputDesc<vec3float_t>*)Input.get())->DefaultValue.X =
				appAtof(*DefaultValueArray(0));
			((FNumericalInputDesc<vec3float_t>*)Input.get())->DefaultValue.Y =
				appAtof(*DefaultValueArray(1));
			((FNumericalInputDesc<vec3float_t>*)Input.get())->DefaultValue.Z =
				appAtof(*DefaultValueArray(2));
			break;
		case Substance_IType_Float4:
			((FNumericalInputDesc<vec4float_t>*)Input.get())->DefaultValue.X =
				appAtof(*DefaultValueArray(0));
			((FNumericalInputDesc<vec4float_t>*)Input.get())->DefaultValue.Y =
				appAtof(*DefaultValueArray(1));
			((FNumericalInputDesc<vec4float_t>*)Input.get())->DefaultValue.Z =
				appAtof(*DefaultValueArray(2));
			((FNumericalInputDesc<vec4float_t>*)Input.get())->DefaultValue.W =
				appAtof(*DefaultValueArray(3));
			break;
		case Substance_IType_Integer:
			((FNumericalInputDesc<int_t>*)Input.get())->DefaultValue =
				appAtoi(*DefaultValueArray(0));
			break;
		case Substance_IType_Integer2:
			((FNumericalInputDesc<vec2int_t>*)Input.get())->DefaultValue.X =
				appAtoi(*DefaultValueArray(0));
			((FNumericalInputDesc<vec2int_t>*)Input.get())->DefaultValue.Y =
				appAtoi(*DefaultValueArray(1));
			break;
		case Substance_IType_Integer3:
			((FNumericalInputDesc<vec3int_t>*)Input.get())->DefaultValue.X =
				appAtoi(*DefaultValueArray(0));
			((FNumericalInputDesc<vec3int_t>*)Input.get())->DefaultValue.Y =
				appAtoi(*DefaultValueArray(1));
			((FNumericalInputDesc<vec3int_t>*)Input.get())->DefaultValue.Z =
				appAtoi(*DefaultValueArray(2));
			break;
		case Substance_IType_Integer4:
			((FNumericalInputDesc<vec4int_t>*)Input.get())->DefaultValue.X =
				appAtoi(*DefaultValueArray(0));
			((FNumericalInputDesc<vec4int_t>*)Input.get())->DefaultValue.Y =
				appAtoi(*DefaultValueArray(1));
			((FNumericalInputDesc<vec4int_t>*)Input.get())->DefaultValue.Z =
				appAtoi(*DefaultValueArray(2));
			((FNumericalInputDesc<vec4int_t>*)Input.get())->DefaultValue.W =
				appAtoi(*DefaultValueArray(3));
			break;
		case Substance_IType_Image:
			break;
		default:
			// should have returned in the previous switch-case
			check(0);
			break;
		}
	}

	// parse GUI specific information: min, max, prefered widget
	TiXmlElement* InputguiNode = InputNode->FirstChildElement("inputgui");
	if (InputguiNode)
	{
		Input->Label = FString(UTF8_TO_TCHAR(InputguiNode->Attribute("label")));
		if (Input->Label.Len()<=0)
		{
			return FALSE;
		}

		if (InputguiNode->Attribute("group"))
		{
			Input->Group = FString(UTF8_TO_TCHAR(InputguiNode->Attribute("group")));
		}

		FString Widget = FString(UTF8_TO_TCHAR(InputguiNode->Attribute("widget")));

		// special case for old sbsar files where image input desc where not complete
		if (0 == Widget.Len() && (SubstanceInputType)Input->Type == Substance_IType_Image)
		{
			((FInputDescBase*)Input.get())->Widget =
				SubstanceAir::Input_Image;
			((FImageInputDesc*)Input.get())->Label =
				FString(UTF8_TO_TCHAR(InputguiNode->Attribute("label")));
			((FImageInputDesc*)Input.get())->Desc =
				FString(TEXT("No Description."));
		}
		else if (Widget == FString(TEXT("image")))
		{
			((FInputDescBase*)Input.get())->Widget =
				SubstanceAir::Input_Image;
			((FImageInputDesc*)Input.get())->Label =
				FString(UTF8_TO_TCHAR(InputguiNode->Attribute("label")));
			((FImageInputDesc*)Input.get())->Desc =
				FString(UTF8_TO_TCHAR(InputguiNode->Attribute("description")));
		}
		else if (Widget == FString(TEXT("combobox")))
		{
			((FInputDescBase*)Input.get())->Widget =
				SubstanceAir::Input_Combobox;

			std::tr1::shared_ptr<input_desc_t> NewInput(
				new FNumericalInputDescComboBox(
					((FNumericalInputDesc<int_t>*)Input.get())));

			FNumericalInputDescComboBox* NewDesc = 
				(FNumericalInputDescComboBox*)NewInput.get();

			// parse the combobox enum
			TiXmlElement* GuicomboboxNode = 
				InputguiNode->FirstChildElement("guicombobox");
			
			if (GuicomboboxNode)
			{
				TiXmlElement* GuicomboboxitemNode = 
					GuicomboboxNode->FirstChildElement("guicomboboxitem");
				
				while (GuicomboboxitemNode)
				{
					FString item_text(
						UTF8_TO_TCHAR(GuicomboboxitemNode->Attribute("text")));
					
					FString item_value_attribute = 
						FString(ANSI_TO_TCHAR(
							GuicomboboxitemNode->Attribute("value")));
					if (item_value_attribute.Len() <= 0)
					{
						continue;
					}
					int_t item_value = appAtoi(*item_value_attribute);

					NewDesc->ValueText.Set(item_value, item_text);

					GuicomboboxitemNode = 
						GuicomboboxitemNode->NextSiblingElement("guicomboboxitem");
				}

				Input = NewInput;
			}
		}
		else if (Widget == FString(TEXT("togglebutton")))
		{
			((FInputDescBase*)Input.get())->Widget =
				SubstanceAir::Input_Togglebutton;
		}
		else if (Widget == FString(TEXT("color")))
		{
			((FInputDescBase*)Input.get())->Widget =
				SubstanceAir::Input_Color;
		}
		else if (Widget == FString(TEXT("angle")))
		{
			((FInputDescBase*)Input.get())->Widget =
				SubstanceAir::Input_Angle;
		}
		else // default to ("slider")
		{
			((FInputDescBase*)Input.get())->Widget =
				SubstanceAir::Input_Slider;
		}

		TiXmlElement* InputsubguiNode = InputguiNode->FirstChildElement();
		if (InputsubguiNode)
		{
			if (InputsubguiNode->Attribute("min"))
			{
				FString MinValue(InputsubguiNode->Attribute("min"));
				TArray<FString> MinValueArray;
				MinValue.ParseIntoArray(&MinValueArray,TEXT(","),TRUE);
				INT IdxComp = 0;

				switch ((SubstanceInputType)Input->Type)
				{
				case Substance_IType_Float:
					((FNumericalInputDesc<float_t>*)Input.get())->Min =
						appAtof(*MinValueArray(IdxComp++));
					break;
				case Substance_IType_Float2:
					((FNumericalInputDesc<vec2float_t>*)Input.get())->Min.X =
						appAtof(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec2float_t>*)Input.get())->Min.Y =
						appAtof(*MinValueArray(IdxComp++));
					break;
				case Substance_IType_Float3:
					((FNumericalInputDesc<vec3float_t>*)Input.get())->Min.X =
						appAtof(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec3float_t>*)Input.get())->Min.Y =
						appAtof(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec3float_t>*)Input.get())->Min.Z =
						appAtof(*MinValueArray(IdxComp++));
					break;
				case Substance_IType_Float4:
					((FNumericalInputDesc<vec4float_t>*)Input.get())->Min.X =
						appAtof(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec4float_t>*)Input.get())->Min.Y =
						appAtof(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec4float_t>*)Input.get())->Min.Z =
						appAtof(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec4float_t>*)Input.get())->Min.W =
						appAtof(*MinValueArray(IdxComp++));
					break;
				case Substance_IType_Integer:
					((FNumericalInputDesc<int_t>*)Input.get())->Min =
						appAtoi(*MinValueArray(IdxComp++));
					break;
				case Substance_IType_Integer2:
					((FNumericalInputDesc<vec2int_t>*)Input.get())->Min.X =
						appAtoi(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec2int_t>*)Input.get())->Min.Y =
						appAtoi(*MinValueArray(IdxComp++));
					break;
				case Substance_IType_Integer3:
					((FNumericalInputDesc<vec3int_t>*)Input.get())->Min.X =
						appAtoi(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec3int_t>*)Input.get())->Min.Y =
						appAtoi(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec3int_t>*)Input.get())->Min.Z =
						appAtoi(*MinValueArray(IdxComp++));
					break;
				case Substance_IType_Integer4:
					((FNumericalInputDesc<vec4int_t>*)Input.get())->Min.X =
						appAtoi(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec4int_t>*)Input.get())->Min.Y =
						appAtoi(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec4int_t>*)Input.get())->Min.Z =
						appAtoi(*MinValueArray(IdxComp++));
					((FNumericalInputDesc<vec4int_t>*)Input.get())->Min.W =
						appAtoi(*MinValueArray(IdxComp++));
					break;
				default:
					// should have returned in the previous switch-case
					check(0);
					break;
				}
			}

			if (InputsubguiNode->Attribute("max"))
			{
				FString MaxValue(InputsubguiNode->Attribute("max"));
				TArray<FString> MaxValueArray;
				MaxValue.ParseIntoArray(&MaxValueArray,TEXT(","),TRUE);
				INT IdxComp = 0;

				switch ((SubstanceInputType)Input->Type)
				{
				case Substance_IType_Float:
					((FNumericalInputDesc<float_t>*)Input.get())->Max =
						appAtof(*MaxValueArray(IdxComp++));
					break;
				case Substance_IType_Float2:
					((FNumericalInputDesc<vec2float_t>*)Input.get())->Max.X =
						appAtof(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec2float_t>*)Input.get())->Max.Y =
						appAtof(*MaxValueArray(IdxComp++));
					break;
				case Substance_IType_Float3:
					((FNumericalInputDesc<vec3float_t>*)Input.get())->Max.X =
						appAtof(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec3float_t>*)Input.get())->Max.Y =
						appAtof(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec3float_t>*)Input.get())->Max.Z =
						appAtof(*MaxValueArray(IdxComp++));
					break;
				case Substance_IType_Float4:
					((FNumericalInputDesc<vec4float_t>*)Input.get())->Max.X =
						appAtof(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec4float_t>*)Input.get())->Max.Y =
						appAtof(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec4float_t>*)Input.get())->Max.Z =
						appAtof(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec4float_t>*)Input.get())->Max.W =
						appAtof(*MaxValueArray(IdxComp++));
					break;
				case Substance_IType_Integer:
					((FNumericalInputDesc<int_t>*)Input.get())->Max =
						appAtoi(*MaxValueArray(IdxComp++));
					break;
				case Substance_IType_Integer2:
					((FNumericalInputDesc<vec2int_t>*)Input.get())->Max.X =
						appAtoi(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec2int_t>*)Input.get())->Max.Y =
						appAtoi(*MaxValueArray(IdxComp++));
					break;
				case Substance_IType_Integer3:
					((FNumericalInputDesc<vec3int_t>*)Input.get())->Max.X =
						appAtoi(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec3int_t>*)Input.get())->Max.Y =
						appAtoi(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec3int_t>*)Input.get())->Max.Z =
						appAtoi(*MaxValueArray(IdxComp++));
					break;
				case Substance_IType_Integer4:
					((FNumericalInputDesc<vec4int_t>*)Input.get())->Max.X =
						appAtoi(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec4int_t>*)Input.get())->Max.Y =
						appAtoi(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec4int_t>*)Input.get())->Max.Z =
						appAtoi(*MaxValueArray(IdxComp++));
					((FNumericalInputDesc<vec4int_t>*)Input.get())->Max.W =
						appAtoi(*MaxValueArray(IdxComp++));
					break;
				default:
					// should have returned in the previous switch-case
					check(0);
					break;
				}
			}

			if (InputsubguiNode->Attribute("clamp"))
			{
				FString ClampValue(InputsubguiNode->Attribute("clamp"));

				((FNumericalInputDesc<float_t>*)Input.get())->IsClamped =
						ClampValue==FString(TEXT("on")) ? TRUE : FALSE;
			}
		}
	}
	
	if (0 == Input->Label.Len())
	{
		Input->Label = Input->Identifier;
	}

	Input->Index = ParentGraph->InputDescs.Num();

	ParentGraph->InputDescs.push(Input);

	return TRUE;
}


//! @brief Parse Xml companion file
//! @param XmlContent The string containing the xml data
//! @return True for success, False for failure
UBOOL ParseSubstanceXml(
	const TArray<FString>& XmlContent,
	SubstanceAir::List<graph_desc_t*>& Graphs,
	TArray<uint_t>& assembly_uid)
{
	if (XmlContent.Num() <= 0)
	{
		// abort if invalid
		GWarn->Log( TEXT("#air: error, no xml content to parse"));
		return FALSE;
	}

	TArray<FString>::TConstIterator ItXml(XmlContent);

	for (;ItXml;++ItXml)
	{
		TiXmlDocument Document;

		Document.Parse(TCHAR_TO_UTF8(&(*ItXml)[0]), NULL, TIXML_ENCODING_UTF8);

		if (Document.Error())
		{
			GWarn->Log(TEXT("#air: error while parsing xml companion file"));
			return FALSE;
		}

		TiXmlElement* RootNode = Document.FirstChildElement("sbsdescription");
			
		if (!RootNode)
		{
			GWarn->Log((TEXT("#air: error, invalid xml content (sbsdescription)")));
			return FALSE;
		}
		else
		{
			FString Attribute = FString(ANSI_TO_TCHAR(RootNode->Attribute("asmuid")));
			if (Attribute.Len() <= 0)
			{
				GWarn->Log((TEXT("#air: error, invalid xml content (asmuid)")));
				return FALSE;
			}
			assembly_uid.AddItem(appAtoul(*Attribute));


			TiXmlElement* GlobalNode = RootNode->FirstChildElement("global");

			TiXmlElement* normalformatinputnode = NULL;
			TiXmlElement* timeinputnode = NULL;

			if (GlobalNode!=NULL)
			{
				TiXmlElement* inputlistnode = 
					GlobalNode->FirstChildElement("inputs");

				TiXmlElement* inputnode = 
					inputlistnode->FirstChildElement("input");

				while(inputnode)
				{
					FString identifier(UTF8_TO_TCHAR(inputnode->Attribute("identifier")));

					if (identifier==TEXT("$time"))
					{
						timeinputnode = inputnode;
					}
					else if (identifier==TEXT("$normalformat"))
					{
						normalformatinputnode = inputnode;
					}
					else
					{
						//! @todo global not handled
					}

					inputnode = inputnode->NextSiblingElement("input");
				}
			}

			TiXmlElement* GraphListNode = RootNode->FirstChildElement("graphs");
			if (!GraphListNode)
			{
				GWarn->Log((TEXT("#air: error, invalid xml content (graphs)")));
				return FALSE;
			}

			int_t GraphCount = 0;
			GraphListNode->QueryIntAttribute("count",&GraphCount);

			if (0 == GraphCount)
			{
				GWarn->Log((TEXT("#air: error, invalid content (graph count null)")));
				return FALSE;
			}

			TiXmlElement* GraphNode = GraphListNode->FirstChildElement("graph");

			// Parse all the graphs
			while (GraphNode)
			{
				// Create the graph object
				std::auto_ptr<graph_desc_t> OneGraph(new graph_desc_t);

				OneGraph->PackageUrl =
					FString(UTF8_TO_TCHAR(GraphNode->Attribute("pkgurl")));
				OneGraph->Label =
					FString(UTF8_TO_TCHAR(GraphNode->Attribute("label")));
				OneGraph->Description =
					FString(UTF8_TO_TCHAR(GraphNode->Attribute("description")));

				//! @todo store the usertag tag for later use in the content browser ?

				// Parse outputs
				{
					TiXmlElement* OutputListNode = GraphNode->FirstChildElement("outputs");
		
					if (!OutputListNode)
					{
						GWarn->Log((TEXT("#air: error, invalid xml content, missing \"outputs\" node for graph %s"), *OneGraph->Label));
						return FALSE;
					}

					int_t OutputCount;
					OutputListNode->QueryIntAttribute("count",&OutputCount);

					if (OutputCount<=0)
					{
						GWarn->Log(TEXT("#air: error parsing xml file, no outputs"));
						
						// temp: no failure as long as the Ticket #1418 is not fixed.
						GraphNode = GraphNode->NextSiblingElement("graph");
						continue;

						//return FALSE;
					}

					OneGraph->OutputDescs.Reserve(OutputCount);
					
					TiXmlElement* OutputNode = OutputListNode->FirstChildElement("output");

					while(OutputNode)
					{
						if ( !ParseOutput(OutputNode,OneGraph.get()))
						{
							GWarn->Log(TEXT("#air: error parsing xml, invalid output"));
							return FALSE;
						}

						OutputNode = OutputNode->NextSiblingElement("output");
					}
				}

				// Sort outpts by uid
				Sort<USE_COMPARE_CONSTREF(output_desc_t,SubstanceAirEdXmlHelper)>(
					&OneGraph->OutputDescs(0), OneGraph->OutputDescs.Num());

				// Parse inputs
				{
					TiXmlElement* InputListNode = GraphNode->FirstChildElement("inputs");

					if (!InputListNode)
					{
						GWarn->Log(TEXT("#air: error parsing xml, no inputs"));
						return FALSE;
					}

					int InputCount;
					InputListNode->QueryIntAttribute("count",&InputCount);

					if	(InputCount >= 0)
					{
						OneGraph->InputDescs.Reserve(InputCount);

						TiXmlElement* InputNode = InputListNode->FirstChildElement("input");

						while(InputNode)
						{
							// Parse input
							if ( !ParseInput(InputNode, OneGraph.get()))
							{
								GWarn->Log(TEXT("#air: error parsing xml, invalid input"));
								return FALSE;
							}

							InputNode = InputNode->NextSiblingElement("input");
						}

						if (normalformatinputnode)
						{
							// Parse input
							if ( !ParseInput(normalformatinputnode, OneGraph.get()))
							{
								GWarn->Log(TEXT("#air: error parsing xml, invalid input"));
								return FALSE;
							}
						}

						if (timeinputnode)
						{
							// Parse input
							if ( !ParseInput(timeinputnode, OneGraph.get()))
							{
								GWarn->Log(TEXT("#air: error parsing xml, invalid input"));
								return FALSE;
							}
						}
					}

					// Sort inputs by uid
					Sort<input_desc_ptr, COMPARE_CONSTREF_CLASS(input_desc_ptr,SubstanceAirEdXmlHelper)>(
						&OneGraph->InputDescs(0), OneGraph->InputDescs.Num());
				}

				// Add the graph to his package
				Graphs.push(OneGraph.release());

				// Seek next graph
				GraphNode = GraphNode->NextSiblingElement("graph");
			}

			SubstanceAir::List<graph_desc_t*>::TIterator itGraph(Graphs.itfront());
			for (;itGraph;++itGraph)
			{
				Sort<USE_COMPARE_CONSTREF(uint_t,SubstanceAirEdXmlHelper)>(
					&(*itGraph)->OutputDescs(0).AlteringInputUids(0),
					(*itGraph)->OutputDescs(0).AlteringInputUids.Num());
			}
		}
	}
	return TRUE;
}

UBOOL ParseSubstanceXmlPreset(
	presets_t& presets,
	const FString& XmlContent,
	graph_desc_t* graphDesc)
{
	TiXmlDocument Document;

	Document.Parse(TCHAR_TO_UTF8(&XmlContent[0]), NULL, TIXML_ENCODING_UTF8);

	if (Document.Error())
	{
		warnf(TEXT("#air: Invalid preset file (xml parsing error)."));
		return FALSE;
	}

	TiXmlElement* Rootnode = Document.FirstChildElement("sbspresets");

	TiXmlElement* sbspresetnode = 
		Rootnode->FirstChildElement("sbspreset");

	// Reserve presets array
	{
		SIZE_T presetsCount = 0;
		while (sbspresetnode)
		{
			sbspresetnode = 
				sbspresetnode->NextSiblingElement("sbspreset");
			++presetsCount;
		}
		presets.AddZeroed(presetsCount+presets.Num());
	}

	presets_t::TIterator ItPres(presets);

	for (sbspresetnode = Rootnode->FirstChildElement("sbspreset");
		sbspresetnode != NULL ;
		sbspresetnode = sbspresetnode->NextSiblingElement("sbspreset"))
	{
		FPreset &preset = *ItPres;

		// Parse preset
		preset.mPackageUrl = 
			FString(UTF8_TO_TCHAR(sbspresetnode->Attribute("pkgurl")));
		preset.mPackageUrl = graphDesc!=NULL ?
			graphDesc->PackageUrl :
			preset.mPackageUrl;
		preset.mLabel = 
			FString(UTF8_TO_TCHAR(sbspresetnode->Attribute("label")));
		preset.mDescription = 
			FString(UTF8_TO_TCHAR(sbspresetnode->Attribute("description")));

		// Loop on preset inputs
		for (TiXmlElement* presetinputnode = 
			sbspresetnode->FirstChildElement("presetinput");
			presetinputnode != NULL;
			presetinputnode = presetinputnode->NextSiblingElement("presetinput"))
		{
			INT Idx = preset.mInputValues.AddZeroed(1);
			FPreset::FInputValue& inpvalue = preset.mInputValues[Idx];

			inpvalue.mUid = 0;

			FString Attribute = 
				FString(ANSI_TO_TCHAR(presetinputnode->Attribute("uid")));
			if (Attribute.Len() <= 0)
			{
				return FALSE;
			}
			inpvalue.mUid = appAtoul(*Attribute);

			inpvalue.mValue = 
				FString(ANSI_TO_TCHAR(presetinputnode->Attribute("value")));

			int type = 0xFFFFu;

			if (graphDesc)
			{
				// Internal preset
				std::tr1::shared_ptr<input_desc_t> ite = FindInput(
					graphDesc->InputDescs,
					inpvalue.mUid);

				if (ite.get())
				{
					type = ite->Type;
					inpvalue.mIdentifier = ite->Identifier;
				}
				else
				{
					// TODO Warning: cannot match internal preset
					inpvalue.mType = (SubstanceInputType)0xFFFF;
				}
			}
			else
			{
				// External preset
				presetinputnode->QueryIntAttribute("type",&type);
				inpvalue.mIdentifier = 
					FString(UTF8_TO_TCHAR(
						presetinputnode->Attribute("identifier")));
			}

			inpvalue.mType = (SubstanceInputType)type;
		}
		
		++ItPres;
	}

	return TRUE;
}


void WriteSubstanceXmlPreset(
	preset_t& preset,
	FString& XmlContent)
{
	XmlContent += TEXT("<sbspresets formatversion=\"1.0\" count=\"1\">");
	XmlContent += TEXT("<sbspreset pkgurl=\"");
	XmlContent += preset.mPackageUrl;
	XmlContent += TEXT("\" label=\"");
	XmlContent += preset.mLabel;
	XmlContent += TEXT("\" >\n");

	TArray<SubstanceAir::FPreset::FInputValue>::TIterator 
		ItV(preset.mInputValues.itfront());

	for (; ItV ; ++ItV)
	{
		XmlContent += TEXT("<presetinput identifier=\"");
		XmlContent += (*ItV).mIdentifier;
		XmlContent += TEXT("\" uid=\"");
		XmlContent += FString::Printf(TEXT("%u"),(*ItV).mUid);
		XmlContent += TEXT("\" type=\"");
		XmlContent += FString::Printf(TEXT("%d"),(INT)(*ItV).mType);
		XmlContent += TEXT("\" value=\"");
		XmlContent += (*ItV).mValue;
		XmlContent += TEXT("\" />\n");
	}

	XmlContent += TEXT("</sbspreset>\n");
	XmlContent += TEXT("</sbspresets>");
}

} // namespace SubstanceAir
} // namespace Helpers
