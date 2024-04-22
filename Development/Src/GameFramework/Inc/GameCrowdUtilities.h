//=============================================================================
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

UBOOL GDrawCrowdPath = 0;

// 1 radian == 10430 unreal rotator units
#define CONV_RADIANS_TO_ROTATOR 10430

void GetDebugCrowdColor( INT i, FColor& C )
{
	switch(i)
	{
	case 0: C=FColor(255,0,0); break;
	case 1: C=FColor(0,255,0); break;
	case 2: C=FColor(0,0,255); break;
	case 3: C=FColor(255,255,0); break;
	case 4: C=FColor(255,0,255); break;
	case 5: C=FColor(0,255,255); break;
	case 6: C=FColor(255,255,255); break;
	case 7: C=FColor(255,128,0); break;
	}
}

struct FVelocityObstacleInfo
{
	UBOOL	  bValid;
	UBOOL	  bOverlap;
	FVector2D ObstacleConeApex;
	FVector2D ObstacleConeDir;
	FLOAT	  AngleCos;

	UBOOL     bPlanesValid;
	FPlane	  RightPlane;
	FPlane    LeftPlane;

	FVector	  PositionA;
	FVector	  VelocityA;
	FLOAT	  RadiusA;
	FVector	  PositionB;
	FVector	  VelocityB;
	FLOAT	  RadiusB;

	FVelocityObstacleInfo()
	{
		bValid = FALSE;
		bPlanesValid = FALSE;
	}

	void CalcVO( const FVector& PosA, const FVector& VelA, FLOAT RadA, const FVector& PosB, const FVector& VelB, FLOAT RadB )
	{
		PositionA = PosA;
		PositionB = PosB;
		RadiusA	  = RadA;
		RadiusB	  = RadB;
		VelocityA = VelA;
		VelocityB = VelB;

		bValid = ComputeAngleCos();
	}

	UBOOL ComputeAngleCos()
	{
		const FVector2D Pa(PositionA), Pb(PositionB), Va(VelocityA), Vb(VelocityB);

		const FLOAT DistAtoB = (Pb - Pa).Size();
		if( DistAtoB > KINDA_SMALL_NUMBER )
		{
			const FLOAT RadiusSum = RadiusA + RadiusB;
			const FLOAT Discrimiant = (DistAtoB * DistAtoB) - (RadiusSum * RadiusSum);
			if( Discrimiant >= 0.f )
			{
				AngleCos = appSqrt(Discrimiant) / DistAtoB;
				ObstacleConeApex = Pa + Vb;
				ObstacleConeDir  = (Pb - Pa);
				ObstacleConeDir /= DistAtoB;

				return TRUE;
			}
			else
			{
				AngleCos = 0;
				ObstacleConeApex = Pa;
				ObstacleConeDir = (Pb - Pa);
				ObstacleConeDir /= DistAtoB;
				bOverlap = TRUE;

				return TRUE;
			}
		}
		return FALSE;
	}

	void ComputePlanes()
	{
		if( bValid && !bPlanesValid )
		{
			const FLOAT AngleRad = appAcos(AngleCos) + HALF_PI;
			const FRotator RotTrans( 0, appFloor(AngleRad * CONV_RADIANS_TO_ROTATOR), 0 );

			const FRotationMatrix RotMatrix(RotTrans);
			const FVector Dir(ObstacleConeDir, 0);
			const FVector Apex(ObstacleConeApex,0);
			RightPlane = FPlane(Apex, RotMatrix.TransformNormal(Dir));
			LeftPlane  = FPlane(Apex, RotMatrix.Transpose().TransformNormal(Dir));

			bPlanesValid = TRUE;
		}
	}

	UBOOL IsVelocityWithinObstacleBounds( const FVector& PosA, const FVector& VelA )
	{
		if( !bValid )
		{
			return FALSE;
		}

		const FVector VelPos = PosA + VelA;
		const FVector Apex = FVector(ObstacleConeApex,PosA.Z);
		const FVector DirApexToVelTip = (VelPos - Apex).SafeNormal();
		const FLOAT ConeDirDotRelVel = DirApexToVelTip | FVector(ObstacleConeDir,0);

		return (ConeDirDotRelVel >= AngleCos);
	}

	void DebugDrawVelocityObstacle( FColor C )
	{
		AWorldInfo* Info = GWorld->GetWorldInfo();
		check(Info);

		if( bValid )
		{
			const FVector Apex = FVector(ObstacleConeApex.X, ObstacleConeApex.Y, PositionA.Z );
			Info->DrawDebugCylinder( PositionB, PositionB, RadiusB, 20, C.R, C.G, C.B );

			FVector Dir(ObstacleConeDir, 0);
			Info->DrawDebugLine( Apex, Apex + Dir * 128.f, C.R, C.G, C.B );

			FLOAT AngleRad = appAcos(AngleCos);
			FRotator RotTrans( 0, appFloor(AngleRad * CONV_RADIANS_TO_ROTATOR), 0 );

			const FVector Rt = FRotationMatrix(RotTrans).TransformNormal(Dir);
			const FVector Lt = FRotationMatrix(RotTrans).Transpose().TransformNormal(Dir);
			Info->DrawDebugLine( Apex, Apex + Rt * 1024.f, C.R, C.G, C.B );
			Info->DrawDebugLine( Apex, Apex + Lt * 1024.f, C.R, C.G, C.B );

			ComputePlanes();
			const FVector pRt = Apex + Rt * 128.f;
			const FVector pLt = Apex + Lt * 128.f;
			const FVector nRt = RightPlane;
			const FVector nLt = LeftPlane;

			Info->DrawDebugLine( pRt, pRt + nRt * 32.f, C.R, C.G, C.B );
			Info->DrawDebugLine( pLt, pLt + nLt * 32.f, C.R, C.G, C.B );
		}
	}
};

struct FReciprocalVelocityObjectInfo : public FVelocityObstacleInfo
{
	void CalcRVO( const FVector& PosA, const FVector& VelA, FLOAT RadA, const FVector& PosB, const FVector& VelB, FLOAT RadB, FLOAT InfluencePct )
	{
		FVelocityObstacleInfo::CalcVO( PosA, VelA, RadA, PosB, VelB, RadB );
		if( !bOverlap )
		{
			ObstacleConeApex = FVector2D(PosA) + ((1.f - InfluencePct) * FVector2D(VelocityA)) + (InfluencePct * FVector2D(VelocityB));
		}
	}
};

struct FRVOAgentPair
{
	IInterface_RVO*				  OtherAgent;
	FReciprocalVelocityObjectInfo RVO;
};
