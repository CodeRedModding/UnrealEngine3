/*=============================================================================
	Light actor creation from FBX data.
=============================================================================*/

#include "UnrealEd.h"

#if WITH_FBX

#include "Factories.h"
#include "Engine.h"
#include "EngineMaterialClasses.h"

#include "UnFbxImporter.h"

using namespace UnFbx;

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
ALight* CFbxImporter::CreateLight(FbxLight* Light)
{
	ALight* UnrealLight = NULL;
	FString ActorName = ANSI_TO_TCHAR(MakeName(Light->GetName()));

	// create the light actor
	switch (Light->LightType.Get())
	{
	case FbxLight::ePOINT:
		UnrealLight = Cast<ALight>(GWorld->SpawnActor(APointLight::StaticClass(),*ActorName));
		break;
	case FbxLight::eDIRECTIONAL:
		UnrealLight = Cast<ALight>(GWorld->SpawnActor(ADirectionalLight::StaticClass(),*ActorName));
		break;
	case FbxLight::eSPOT:
		UnrealLight = Cast<ALight>(GWorld->SpawnActor(ASpotLight::StaticClass(),*ActorName));
		break;
	}

	if (UnrealLight)
	{
		FillLightComponent(Light,UnrealLight->LightComponent);	
	}
	
	return UnrealLight;
}

UBOOL CFbxImporter::FillLightComponent(FbxLight* Light, ULightComponent* UnrealLightComponent)
{
	FbxDouble3 Color = Light->Color.Get();
	FColor UnrealColor( BYTE(255.0*Color[0]), BYTE(255.0*Color[1]), BYTE(255.0*Color[2]) );
	UnrealLightComponent->LightColor = UnrealColor;

	FbxDouble Intensity = Light->Intensity.Get();
	UnrealLightComponent->Brightness = (FLOAT)Intensity/100.f;

	UnrealLightComponent->CastShadows = Light->CastShadows.Get();

	switch (Light->LightType.Get())
	{
	// point light properties
	case FbxLight::ePOINT:
		{
			UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(UnrealLightComponent);
			if (PointLightComponent)
			{
				FbxDouble DecayStart = Light->DecayStart.Get();
				PointLightComponent->Radius = Converter.ConvertDist(DecayStart);

				FbxLight::EDecayType Decay = Light->DecayType.Get();
				if (Decay == FbxLight::eNONE)
				{
					PointLightComponent->Radius = FBXSDK_FLOAT_MAX;
				}
			}
			else
			{
				warnf(NAME_Error,TEXT("FBX Light type 'Point' does not match unreal light component"));
			}
		}
		break;
	// spot light properties
	case FbxLight::eSPOT:
		{
			USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(UnrealLightComponent);
			if (SpotLightComponent)
			{
				FbxDouble DecayStart = Light->DecayStart.Get();
				SpotLightComponent->Radius = Converter.ConvertDist(DecayStart);
				FbxLight::EDecayType Decay = Light->DecayType.Get();
				if (Decay == FbxLight::eNONE)
				{
					SpotLightComponent->Radius = FBXSDK_FLOAT_MAX;
				}
				SpotLightComponent->InnerConeAngle = Light->InnerAngle.Get();
				SpotLightComponent->OuterConeAngle = Light->OuterAngle.Get();
			}
			else
			{
				warnf(NAME_Error,TEXT("FBX Light type 'Spot' does not match unreal light component"));
			}
		}
		break;
	// directional light properties 
	case FbxLight::eDIRECTIONAL:
		{
			// nothing specific
		}
		break;
	}

	return TRUE;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
ACameraActor* CFbxImporter::CreateCamera(FbxCamera* Camera)
{
	ACameraActor* UnrealCamera = NULL;
	FString ActorName = ANSI_TO_TCHAR(MakeName(Camera->GetName()));
	UnrealCamera = Cast<ACameraActor>(GWorld->SpawnActor(ACameraActor::StaticClass(),*ActorName));
	if (UnrealCamera)
	{
		UnrealCamera->FOVAngle = Camera->FieldOfView.Get();
	}
	return UnrealCamera;
}

#endif // WITH_FBX
