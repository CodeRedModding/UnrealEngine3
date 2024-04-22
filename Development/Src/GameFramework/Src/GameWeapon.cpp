/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "GameFramework.h"

IMPLEMENT_CLASS(UGameExplosion);
IMPLEMENT_CLASS(AGameExplosionActor);

/** 
  * Returns distance from bounding box to point
  */
FLOAT AGameExplosionActor::BoxDistanceToPoint(FVector Start, FBox BBox)
{
	return appSqrt(BBox.ComputeSquaredDistanceToPoint(Start));
}

void AGameExplosionActor::TickSpecial(FLOAT DeltaTime)
{
	if ( ExplosionRadialBlur && ExplosionRadialBlur->BlurScale > 0.f )
	{
		if (RadialBlurFadeTimeRemaining > 0.f)
		{
			FLOAT Pct = Square(RadialBlurFadeTimeRemaining/RadialBlurFadeTime);  // note that fades out as a square
			ExplosionRadialBlur->SetBlurScale(Pct*RadialBlurMaxBlurAmount);
			RadialBlurFadeTimeRemaining -= DeltaTime;
		}
		else
		{
			ExplosionRadialBlur->SetBlurScale(0.f);
		}
	}

	if (ExplosionLight && ExplosionLight->bEnabled)
	{
		if (LightFadeTimeRemaining > 0.f)
		{
			FLOAT Pct = Square(LightFadeTimeRemaining/LightFadeTime); // note that fades out as a square
			ExplosionLight->SetLightProperties(LightInitialBrightness * Pct, ExplosionLight->LightColor, ExplosionLight->Function);
			LightFadeTimeRemaining -= DeltaTime;
		}
		else
		{
			ExplosionLight->SetEnabled(FALSE);
		}
	}
}

