#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

static const INT DrawCollisionSides = 16;
static const INT DrawConeLimitSides = 40;

static const FLOAT DebugJointPosSize = 5.0f;
static const FLOAT DebugJointAxisSize = 20.0f;

static const FLOAT JointRenderSize = 5.0f;
static const FLOAT LimitRenderSize = 16.0f;

static const FColor JointUnselectedColor(255,0,255);
static const FColor	JointFrame1Color(255,0,0);
static const FColor JointFrame2Color(0,0,255);
static const FColor	JointLimitColor(0,255,0);
static const FColor	JointRefColor(255,255,0);
static const FColor JointLockedColor(255,128,10);

/////////////////////////////////////////////////////////////////////////////////////
// FKSphereElem
/////////////////////////////////////////////////////////////////////////////////////

// NB: ElemTM is assumed to have no scaling in it!
void FKSphereElem::DrawElemWire(FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color)
{
	FVector Center = ElemTM.GetOrigin();
	FVector X = ElemTM.GetAxis(0);
	FVector Y = ElemTM.GetAxis(1);
	FVector Z = ElemTM.GetAxis(2);

	PRI->DrawCircle(Center, X, Y, Color, Scale*Radius, DrawCollisionSides);
	PRI->DrawCircle(Center, X, Z, Color, Scale*Radius, DrawCollisionSides);
	PRI->DrawCircle(Center, Y, Z, Color, Scale*Radius, DrawCollisionSides);
}

void FKSphereElem::DrawElemSolid(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, FMaterial* Material, FMaterialInstance* MaterialInstance)
{
	PRI->DrawSphere( ElemTM.GetOrigin(), FVector( this->Radius ), DrawCollisionSides, DrawCollisionSides/2, Material, MaterialInstance );
}


/////////////////////////////////////////////////////////////////////////////////////
// FKBoxElem
/////////////////////////////////////////////////////////////////////////////////////

// NB: ElemTM is assumed to have no scaling in it!
void FKBoxElem::DrawElemWire(FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color)
{
	FVector	B[2], P, Q, Radii;

	// X,Y,Z member variables are LENGTH not RADIUS
	Radii.X = Scale*0.5f*X;
	Radii.Y = Scale*0.5f*Y;
	Radii.Z = Scale*0.5f*Z;

	B[0] = Radii; // max
	B[1] = -1.0f * Radii; // min

	for( INT i=0; i<2; i++ )
	{
		for( INT j=0; j<2; j++ )
		{
			P.X=B[i].X; Q.X=B[i].X;
			P.Y=B[j].Y; Q.Y=B[j].Y;
			P.Z=B[0].Z; Q.Z=B[1].Z;
			PRI->DrawLine( ElemTM.TransformFVector(P), ElemTM.TransformFVector(Q), Color);

			P.Y=B[i].Y; Q.Y=B[i].Y;
			P.Z=B[j].Z; Q.Z=B[j].Z;
			P.X=B[0].X; Q.X=B[1].X;
			PRI->DrawLine( ElemTM.TransformFVector(P), ElemTM.TransformFVector(Q), Color);

			P.Z=B[i].Z; Q.Z=B[i].Z;
			P.X=B[j].X; Q.X=B[j].X;
			P.Y=B[0].Y; Q.Y=B[1].Y;
			PRI->DrawLine( ElemTM.TransformFVector(P), ElemTM.TransformFVector(Q), Color);
		}
	}

}

void FKBoxElem::DrawElemSolid(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, FMaterial* Material, FMaterialInstance* MaterialInstance)
{
	PRI->DrawBox( ElemTM, 0.5f * FVector(X, Y, Z), Material, MaterialInstance );
}

/////////////////////////////////////////////////////////////////////////////////////
// FKSphylElem
/////////////////////////////////////////////////////////////////////////////////////

static void DrawHalfCircle(FPrimitiveRenderInterface* PRI, const FVector& Base, const FVector& X, const FVector& Y, const FColor Color, FLOAT Radius)
{
	FLOAT	AngleDelta = 2.0f * (FLOAT)PI / ((FLOAT)DrawCollisionSides);
	FVector	LastVertex = Base + X * Radius;

	for(INT SideIndex = 0; SideIndex < (DrawCollisionSides/2); SideIndex++)
	{
		FVector	Vertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * Radius;
		PRI->DrawLine(LastVertex, Vertex, Color);
		LastVertex = Vertex;
	}	
}

// NB: ElemTM is assumed to have no scaling in it!
void FKSphylElem::DrawElemWire(FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color)
{
	FVector Origin = ElemTM.GetOrigin();
	FVector XAxis = ElemTM.GetAxis(0);
	FVector YAxis = ElemTM.GetAxis(1);
	FVector ZAxis = ElemTM.GetAxis(2);

	// Draw top and bottom circles
	FVector TopEnd = Origin + Scale*0.5f*Length*ZAxis;
	FVector BottomEnd = Origin - Scale*0.5f*Length*ZAxis;

	PRI->DrawCircle(TopEnd, XAxis, YAxis, Color, Scale*Radius, DrawCollisionSides);
	PRI->DrawCircle(BottomEnd, XAxis, YAxis, Color, Scale*Radius, DrawCollisionSides);

	// Draw domed caps
	DrawHalfCircle(PRI, TopEnd, YAxis, ZAxis, Color,Scale* Radius);
	DrawHalfCircle(PRI, TopEnd, XAxis, ZAxis, Color, Scale*Radius);

	FVector NegZAxis = -ZAxis;

	DrawHalfCircle(PRI, BottomEnd, YAxis, NegZAxis, Color, Scale*Radius);
	DrawHalfCircle(PRI, BottomEnd, XAxis, NegZAxis, Color, Scale*Radius);

	// Draw connecty lines
	PRI->DrawLine(TopEnd + Scale*Radius*XAxis, BottomEnd + Scale*Radius*XAxis, Color);
	PRI->DrawLine(TopEnd - Scale*Radius*XAxis, BottomEnd - Scale*Radius*XAxis, Color);
	PRI->DrawLine(TopEnd + Scale*Radius*YAxis, BottomEnd + Scale*Radius*YAxis, Color);
	PRI->DrawLine(TopEnd - Scale*Radius*YAxis, BottomEnd - Scale*Radius*YAxis, Color);
}

void FKSphylElem::DrawElemSolid(FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, FMaterial* Material, FMaterialInstance* MaterialInstance)
{
	INT NumSides = DrawCollisionSides;
	INT NumRings = (DrawCollisionSides/2) + 1;

	// The first/last arc are on top of each other.
	INT NumVerts = (NumSides+1) * (NumRings+1);
	FRawTriangleVertex* Verts = (FRawTriangleVertex*)appMalloc( NumVerts * sizeof(FRawTriangleVertex) );

	// Calculate verts for one arc
	FRawTriangleVertex* ArcVerts = (FRawTriangleVertex*)appMalloc( (NumRings+1) * sizeof(FRawTriangleVertex) );

	for(INT i=0; i<NumRings+1; i++)
	{
		FRawTriangleVertex* ArcVert = &ArcVerts[i];

		FLOAT Angle;
		FLOAT ZOffset;
		if( i <= DrawCollisionSides/4 )
		{
			Angle = ((FLOAT)i/(NumRings-1)) * PI;
			ZOffset = 0.5 * Scale * Length;
		}
		else
		{
			Angle = ((FLOAT)(i-1)/(NumRings-1)) * PI;
			ZOffset = -0.5 * Scale * Length;
		}

		// Note- unit sphere, so position always has mag of one. We can just use it for normal!		
		FVector SpherePos;
		SpherePos.X = 0.0f;
		SpherePos.Y = Scale * Radius * appSin(Angle);
		SpherePos.Z = Scale * Radius * appCos(Angle);

		ArcVert->Position = SpherePos + FVector(0,0,ZOffset);

		ArcVert->TangentX = FVector(1,0,0);

		ArcVert->TangentY.X = 0.0f;
		ArcVert->TangentY.Y = -SpherePos.Z;
		ArcVert->TangentY.Z = SpherePos.Y;

		ArcVert->TangentZ = SpherePos; 

		ArcVert->TexCoord.X = 0.0f;
		ArcVert->TexCoord.Y = ((FLOAT)i/NumRings);
	}

	// Then rotate this arc NumSides+1 times.
	for(INT s=0; s<NumSides+1; s++)
	{
		FRotator ArcRotator(0, 65535 * ((FLOAT)s/NumSides), 0);
		FRotationMatrix ArcRot( ArcRotator );
		FLOAT XTexCoord = ((FLOAT)s/NumSides);

		for(INT v=0; v<NumRings+1; v++)
		{
			INT VIx = (NumRings+1)*s + v;

			Verts[VIx].Position = ArcRot.TransformFVector( ArcVerts[v].Position );
			Verts[VIx].TangentX = ArcRot.TransformNormal( ArcVerts[v].TangentX );
			Verts[VIx].TangentY = ArcRot.TransformNormal( ArcVerts[v].TangentY );
			Verts[VIx].TangentZ = ArcRot.TransformNormal( ArcVerts[v].TangentZ );
			Verts[VIx].TexCoord.X = XTexCoord;
			Verts[VIx].TexCoord.Y = ArcVerts[v].TexCoord.Y;
		}
	}

	FTriangleRenderInterface* TRI = PRI->GetTRI(ElemTM, Material, MaterialInstance);

	for(INT s=0; s<NumSides; s++)
	{
		INT a0start = (s+0) * (NumRings+1);
		INT a1start = (s+1) * (NumRings+1);

		for(INT r=0; r<NumRings; r++)
		{
			TRI->DrawQuad( Verts[a0start + r + 0], Verts[a1start + r + 0], Verts[a1start + r + 1], Verts[a0start + r + 1] );
		}
	}

	TRI->Finish();

	appFree(Verts);
	appFree(ArcVerts);

}

/////////////////////////////////////////////////////////////////////////////////////
// FKConvexElem
/////////////////////////////////////////////////////////////////////////////////////

// NB: ElemTM is assumed to have no scaling in it!
void FKConvexElem::DrawElemWire(FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, const FVector& Scale3D, const FColor Color)
{

}

void FKConvexElem::DrawElemSolid(FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, const FVector& Scale3D, FMaterial* Material, FMaterialInstance* MaterialInstance)
{

}


/////////////////////////////////////////////////////////////////////////////////////
// FKAggregateGeom
/////////////////////////////////////////////////////////////////////////////////////

// NB: ParentTM is assumed to have no scaling in it!
void FKAggregateGeom::DrawAggGeom(FPrimitiveRenderInterface* PRI, const FMatrix& ParentTM, const FVector& Scale3D, const FColor Color, UBOOL bNoConvex)
{
	if( Scale3D.IsUniform() )
	{
		for(INT i=0; i<SphereElems.Num(); i++)
		{
			FMatrix ElemTM = SphereElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= ParentTM;
			SphereElems(i).DrawElemWire(PRI, ElemTM, Scale3D.X, Color);
		}

		for(INT i=0; i<BoxElems.Num(); i++)
		{
			FMatrix ElemTM = BoxElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= ParentTM;
			BoxElems(i).DrawElemWire(PRI, ElemTM, Scale3D.X, Color);
		}

		for(INT i=0; i<SphylElems.Num(); i++)
		{
			FMatrix ElemTM = SphylElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= ParentTM;
			SphylElems(i).DrawElemWire(PRI, ElemTM, Scale3D.X, Color);
		}
	}

	if(!bNoConvex)
	{
		for(INT i=0; i<ConvexElems.Num(); i++)
		{
			ConvexElems(i).DrawElemWire(PRI, ParentTM, Scale3D, Color);
		}
	}

}

/////////////////////////////////////////////////////////////////////////////////////
// UPhysicsAsset
/////////////////////////////////////////////////////////////////////////////////////


void UPhysicsAsset::DrawCollision(FPrimitiveRenderInterface* PRI, USkeletalMeshComponent* SkelComp, FLOAT Scale)
{
	for( INT i=0; i<BodySetup.Num(); i++)
	{
		INT BoneIndex = SkelComp->MatchRefBone( BodySetup(i)->BoneName );
		
		FColor* BoneColor = (FColor*)( &BodySetup(i) );
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		BoneMatrix.RemoveScaling();

		BodySetup(i)->AggGeom.DrawAggGeom( PRI, BoneMatrix, FVector(Scale, Scale, Scale), *BoneColor );
	}

}

void UPhysicsAsset::DrawConstraints(FPrimitiveRenderInterface* PRI, USkeletalMeshComponent* SkelComp, FLOAT Scale)
{
	for( INT i=0; i<ConstraintSetup.Num(); i++ )
	{
		URB_ConstraintSetup* cs = ConstraintSetup(i);

		// Get each constraint frame in world space.
		FMatrix Con1Frame = FMatrix::Identity;
		INT Bone1Index = SkelComp->MatchRefBone(cs->ConstraintBone1);
		if(Bone1Index != INDEX_NONE)
		{	
			FMatrix Body1TM = SkelComp->GetBoneMatrix(Bone1Index);
			Body1TM.RemoveScaling();
			Con1Frame = cs->GetRefFrameMatrix(0) * Body1TM;
		}

		FMatrix Con2Frame = FMatrix::Identity;
		INT Bone2Index = SkelComp->MatchRefBone(cs->ConstraintBone2);
		if(Bone2Index != INDEX_NONE)
		{	
			FMatrix Body2TM = SkelComp->GetBoneMatrix(Bone2Index);
			Body2TM.RemoveScaling();
			Con2Frame = cs->GetRefFrameMatrix(1) * Body2TM;
		}

		if(!SkelComp->LimitMaterial)
			SkelComp->LimitMaterial = LoadObject<UMaterialInstance>(NULL, TEXT("EditorMaterials.PhAT_JointLimitMaterial"), NULL, LOAD_NoFail, NULL);

		cs->DrawConstraint(PRI, Scale, true, true, SkelComp->LimitMaterial, Con1Frame, Con2Frame);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// URB_ConstraintSetup
/////////////////////////////////////////////////////////////////////////////////////

static void DrawOrientedStar(FPrimitiveRenderInterface* PRI, const FMatrix& Matrix, FLOAT Size, const FColor Color)
{
	FVector Position = Matrix.GetOrigin();

	PRI->DrawLine(Position + Size * Matrix.GetAxis(0), Position - Size * Matrix.GetAxis(0), Color);
	PRI->DrawLine(Position + Size * Matrix.GetAxis(1), Position - Size * Matrix.GetAxis(1), Color);
	PRI->DrawLine(Position + Size * Matrix.GetAxis(2), Position - Size * Matrix.GetAxis(2), Color);
}

static void DrawArc(const FVector& Base, const FVector& X, const FVector& Y, FLOAT MinAngle, FLOAT MaxAngle, FLOAT Radius, INT Sections, const FColor Color, FPrimitiveRenderInterface* PRI)
{
	FLOAT AngleStep = (MaxAngle - MinAngle)/((FLOAT)(Sections));
	FLOAT CurrentAngle = MinAngle;

	FVector LastVertex = Base + Radius * ( appCos(CurrentAngle * (PI/180.0f)) * X + appSin(CurrentAngle * (PI/180.0f)) * Y );
	CurrentAngle += AngleStep;

	for(INT i=0; i<Sections; i++)
	{
		FVector ThisVertex = Base + Radius * ( appCos(CurrentAngle * (PI/180.0f)) * X + appSin(CurrentAngle * (PI/180.0f)) * Y );
		PRI->DrawLine( LastVertex, ThisVertex, Color );
		LastVertex = ThisVertex;
		CurrentAngle += AngleStep;
	}
}

static void DrawLinearLimit(FPrimitiveRenderInterface* PRI, const FVector& Origin, const FVector& Axis, const FVector& Orth, FLOAT LinearLimitRadius, UBOOL bLinearLimited)
{
	if(bLinearLimited)
	{
		FVector Start = Origin - LinearLimitRadius * Axis;
		FVector End = Origin + LinearLimitRadius * Axis;

		PRI->DrawLine(  Start, End, JointLimitColor );

		// Draw ends indicating limit.
		PRI->DrawLine(  Start - (0.2f * LimitRenderSize * Orth), Start + (0.2f * LimitRenderSize * Orth), JointLimitColor );
		PRI->DrawLine(  End - (0.2f * LimitRenderSize * Orth), End + (0.2f * LimitRenderSize * Orth), JointLimitColor );
	}
	else
	{
		FVector Start = Origin - 1.5f * LimitRenderSize * Axis;
		FVector End = Origin + 1.5f * LimitRenderSize * Axis;

		PRI->DrawLine(  Start, End, JointRefColor );

		// Draw arrow heads.
		PRI->DrawLine(  Start, Start + (0.2f * LimitRenderSize * Axis) + (0.2f * LimitRenderSize * Orth), JointLimitColor );
		PRI->DrawLine(  Start, Start + (0.2f * LimitRenderSize * Axis) - (0.2f * LimitRenderSize * Orth), JointLimitColor );

		PRI->DrawLine(  End, End - (0.2f * LimitRenderSize * Axis) + (0.2f * LimitRenderSize * Orth), JointLimitColor );
		PRI->DrawLine(  End, End - (0.2f * LimitRenderSize * Axis) - (0.2f * LimitRenderSize * Orth), JointLimitColor );
	}
}

void URB_ConstraintSetup::DrawConstraint(FPrimitiveRenderInterface* PRI, 
										 FLOAT Scale, UBOOL bDrawLimits, UBOOL bDrawSelected, UMaterialInstance* LimitMaterial,
										 const FMatrix& Con1Frame, const FMatrix& Con2Frame)
{

	FVector Con1Pos = Con1Frame.GetOrigin();
	FVector Con2Pos = Con2Frame.GetOrigin();

	if(bDrawSelected)
	{
		DrawOrientedStar( PRI, Con1Frame, JointRenderSize, JointFrame1Color );
		DrawOrientedStar( PRI, Con2Frame, JointRenderSize, JointFrame2Color );
	}
	else
	{
		DrawOrientedStar( PRI, Con1Frame, JointRenderSize, JointUnselectedColor );
		DrawOrientedStar( PRI, Con2Frame, JointRenderSize, JointUnselectedColor );
	}

	//////////////////////////////////////////////////////////////////////////
	// LINEAR DRAWING

	UBOOL bLinearXLocked = LinearXSetup.bLimited && (LinearXSetup.LimitSize < RB_MinSizeToLockDOF);
	UBOOL bLinearYLocked = LinearYSetup.bLimited && (LinearYSetup.LimitSize < RB_MinSizeToLockDOF);
	UBOOL bLinearZLocked = LinearZSetup.bLimited && (LinearZSetup.LimitSize < RB_MinSizeToLockDOF);

	// APE_TODO: Change this once proper linear limits are done.
	FLOAT LinearLimitRadius = 0.f;
	if( LinearXSetup.bLimited && (LinearXSetup.LimitSize > LinearLimitRadius) )
		LinearLimitRadius = LinearXSetup.LimitSize;
	if( LinearYSetup.bLimited && (LinearYSetup.LimitSize > LinearLimitRadius) )
		LinearLimitRadius = LinearYSetup.LimitSize;
	if( LinearZSetup.bLimited && (LinearZSetup.LimitSize > LinearLimitRadius) )
		LinearLimitRadius = LinearZSetup.LimitSize;

	UBOOL bLinearLimited = false;
	if( LinearLimitRadius > RB_MinSizeToLockDOF )
		bLinearLimited = true;

	if(!bLinearXLocked)
		DrawLinearLimit(PRI, Con2Frame.GetOrigin(), Con2Frame.GetAxis(0), Con2Frame.GetAxis(2), LinearLimitRadius, bLinearLimited);

	if(!bLinearYLocked)
		DrawLinearLimit(PRI, Con2Frame.GetOrigin(), Con2Frame.GetAxis(1), Con2Frame.GetAxis(2), LinearLimitRadius, bLinearLimited);

	if(!bLinearZLocked)
		DrawLinearLimit(PRI, Con2Frame.GetOrigin(), Con2Frame.GetAxis(2), Con2Frame.GetAxis(0), LinearLimitRadius, bLinearLimited);


	if(!bDrawLimits)
		return;


	//////////////////////////////////////////////////////////////////////////
	// ANGULAR DRAWING

	UBOOL bLockTwist = bTwistLimited && (TwistLimitAngle < RB_MinAngleToLockDOF);
	UBOOL bLockSwing1 = bSwingLimited && (Swing1LimitAngle < RB_MinAngleToLockDOF);
	UBOOL bLockSwing2 = bSwingLimited && (Swing2LimitAngle < RB_MinAngleToLockDOF);
	UBOOL bLockAllSwing = bLockSwing1 && bLockSwing2;

	UBOOL bDrawnAxisLine = false;
	FVector RefLineEnd = Con1Frame.GetOrigin() + (1.2f * LimitRenderSize * Con1Frame.GetAxis(0));

	// If swing is limited (but not locked) - draw the limit cone.
	if(bSwingLimited)
	{
		FMatrix ConeLimitTM;
		ConeLimitTM = Con2Frame;
		ConeLimitTM.SetOrigin( Con1Frame.GetOrigin() );

		if(bLockAllSwing)
		{
			// Draw little red 'V' to indicate locked swing.
			PRI->DrawLine( ConeLimitTM.GetOrigin(), ConeLimitTM.TransformFVector( 0.3f * LimitRenderSize * FVector(1,1,0) ), JointLockedColor);
			PRI->DrawLine( ConeLimitTM.GetOrigin(), ConeLimitTM.TransformFVector( 0.3f * LimitRenderSize * FVector(1,-1,0) ), JointLockedColor);
		}
		else
		{
			FLOAT ang1 = Swing1LimitAngle;
			if(ang1 < RB_MinAngleToLockDOF)
				ang1 = 0.f;
			else
				ang1 *= ((FLOAT)PI/180.f); // convert to radians

			FLOAT ang2 = Swing2LimitAngle;
			if(ang2 < RB_MinAngleToLockDOF)
				ang2 = 0.f;
			else
				ang2 *= ((FLOAT)PI/180.f);

			ang1 = Clamp<FLOAT>(ang1, 0.01f, (FLOAT)PI - 0.01f);
			ang2 = Clamp<FLOAT>(ang2, 0.01f, (FLOAT)PI - 0.01f);

			FLOAT sinX_2 = appSin(0.5f * ang1);
			FLOAT sinY_2 = appSin(0.5f * ang2);

			FLOAT sinSqX_2 = sinX_2 * sinX_2;
			FLOAT sinSqY_2 = sinY_2 * sinY_2;

			FLOAT tanX_2 = appTan(0.5f * ang1);
			FLOAT tanY_2 = appTan(0.5f * ang2);

			TArray<FVector> ConeVerts(DrawConeLimitSides);

			for(INT i = 0; i < DrawConeLimitSides; i++)
			{
				FLOAT Fraction = (FLOAT)i/(FLOAT)(DrawConeLimitSides);
				FLOAT thi = 2.f*PI*Fraction;
				FLOAT phi = appAtan2(appSin(thi)*sinY_2, appCos(thi)*sinX_2);
				FLOAT sinPhi = appSin(phi);
				FLOAT cosPhi = appCos(phi);
				FLOAT sinSqPhi = sinPhi*sinPhi;
				FLOAT cosSqPhi = cosPhi*cosPhi;

				FLOAT rSq, r, Sqr, alpha, beta;

				rSq = sinSqX_2*sinSqY_2/(sinSqX_2*sinSqPhi + sinSqY_2*cosSqPhi);
				r = appSqrt(rSq);
				Sqr = appSqrt(1-rSq);
				alpha = r*cosPhi;
				beta  = r*sinPhi;

				ConeVerts(i).X = (1-2*rSq);
				ConeVerts(i).Y = 2*Sqr*alpha;
				ConeVerts(i).Z = 2*Sqr*beta;
			}

			FTriangleRenderInterface* TRI = PRI->GetTRI( FScaleMatrix( FVector(LimitRenderSize) ) * ConeLimitTM, LimitMaterial->GetMaterialInterface(0), LimitMaterial->GetInstanceInterface() );

			for(INT i=0; i < DrawConeLimitSides; i++)
			{
				FRawTriangleVertex V0, V1, V2;

				FVector TriTangentZ = ConeVerts((i+1)%DrawConeLimitSides) ^ ConeVerts( i ); // aka triangle normal
				FVector TriTangentY = ConeVerts(i);
				FVector TriTangentX = TriTangentZ ^ TriTangentY;

				V0.Position = FVector(0);
				V0.TexCoord.X = 0.0f;
				V0.TexCoord.Y = (FLOAT)i/DrawConeLimitSides;
				V0.TangentX = TriTangentX;
				V0.TangentY = TriTangentY;
				V0.TangentZ = TriTangentZ;	

				V1.Position = ConeVerts(i);
				V1.TexCoord.X = 1.0f;
				V1.TexCoord.Y = (FLOAT)i/DrawConeLimitSides;
				V1.TangentX = TriTangentX;
				V1.TangentY = TriTangentY;
				V1.TangentZ = TriTangentZ;	

				V2.Position = ConeVerts((i+1)%DrawConeLimitSides);
				V2.TexCoord.X = 1.0f;
				V2.TexCoord.Y = (FLOAT)((i+1)%DrawConeLimitSides)/DrawConeLimitSides;
				V2.TangentX = TriTangentX;
				V2.TangentY = TriTangentY;
				V2.TangentZ = TriTangentZ;	

				TRI->DrawTriangle(V0, V1, V2);
			}

			TRI->Finish();

			// Draw lines down major directions
			for(INT i=0; i<4; i++)
			{
				PRI->DrawLine( ConeLimitTM.GetOrigin(), ConeLimitTM.TransformFVector( LimitRenderSize * ConeVerts( (i*DrawConeLimitSides/4)%DrawConeLimitSides ) ), JointLimitColor );
			}

			// Draw reference line
			PRI->DrawLine( Con1Frame.GetOrigin(), RefLineEnd, JointRefColor );
			bDrawnAxisLine = true;
		}
	}

	// If twist is limited, but not completely locked, draw 
	if(bTwistLimited)
	{
		if(bLockTwist)
		{
			// If there is no axis line draw already - add a little one now to sit the 'twist locked' cross on
			if(!bDrawnAxisLine)
				PRI->DrawLine( Con1Frame.GetOrigin(), RefLineEnd, JointLockedColor );

			PRI->DrawLine(  RefLineEnd + Con1Frame.TransformNormal( 0.3f * LimitRenderSize * FVector(0.f,-0.5f,-0.5f) ), 
							RefLineEnd + Con1Frame.TransformNormal( 0.3f * LimitRenderSize * FVector(0.f, 0.5f, 0.5f) ), JointLockedColor);

			PRI->DrawLine(  RefLineEnd + Con1Frame.TransformNormal( 0.3f * LimitRenderSize * FVector(0.f, 0.5f,-0.5f) ), 
							RefLineEnd + Con1Frame.TransformNormal( 0.3f * LimitRenderSize * FVector(0.f,-0.5f, 0.5f) ), JointLockedColor);
		}
		else
		{
			// If no axis yet drawn - do it now
			if(!bDrawnAxisLine)
				PRI->DrawLine( Con1Frame.GetOrigin(), RefLineEnd, JointRefColor );

			// Draw twist limit.
			FVector ChildTwistRef = Con1Frame.GetAxis(1);
			FVector ParentTwistRef = Con2Frame.GetAxis(1);
			PRI->DrawLine( RefLineEnd, RefLineEnd + ChildTwistRef * LimitRenderSize, JointRefColor );

			// Rotate parent twist ref axis
			FQuat ParentToChildRot = FQuatFindBetween( Con2Frame.GetAxis(0), Con1Frame.GetAxis(0) );
			FVector ChildTwistLimit = ParentToChildRot.RotateVector( ParentTwistRef );

			FQuat RotateLimit = FQuat( Con1Frame.GetAxis(0), TwistLimitAngle * (PI/180.0f) );
			FVector WLimit = RotateLimit.RotateVector(ChildTwistLimit);
			PRI->DrawLine( RefLineEnd, RefLineEnd + WLimit * LimitRenderSize, JointLimitColor );

			RotateLimit = FQuat( Con1Frame.GetAxis(0), -TwistLimitAngle * (PI/180.0f) );
			WLimit = RotateLimit.RotateVector(ChildTwistLimit);
			PRI->DrawLine( RefLineEnd, RefLineEnd + WLimit * LimitRenderSize, JointLimitColor );

			DrawArc(RefLineEnd, ChildTwistLimit, -ChildTwistLimit ^ Con1Frame.GetAxis(0), -TwistLimitAngle, TwistLimitAngle, 0.8f * LimitRenderSize, 8, JointLimitColor, PRI);
		}
	}
	else
	{
		// For convenience, in the hinge case where swing is locked and twist is unlimited, draw the twist axis.
		if(bLockAllSwing)
			PRI->DrawLine(  Con2Frame.GetOrigin() - LimitRenderSize * Con2Frame.GetAxis(0), 
							Con2Frame.GetOrigin() + LimitRenderSize * Con2Frame.GetAxis(0), JointLimitColor );
	}


}

/////////////////////////////////////////////////////////////////////////////////////
// URB_ConstraintDrawComponent
/////////////////////////////////////////////////////////////////////////////////////

UBOOL URB_ConstraintDrawComponent::Visible(FSceneView* View)
{
	if(View->ShowFlags & SHOW_Constraints)
		return true;

	return Super::Visible(View);
}

void URB_ConstraintDrawComponent::Render(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI)
{
	Super::Render(Context, PRI);

	ARB_ConstraintActor* CA = Cast<ARB_ConstraintActor>(Owner);
	if(!CA)
		return;

	FMatrix Con1Frame = CA->ConstraintSetup->GetRefFrameMatrix(0) * FindBodyMatrix(CA->ConstraintActor1, CA->ConstraintSetup->ConstraintBone1);
	FMatrix Con2Frame = CA->ConstraintSetup->GetRefFrameMatrix(1) * FindBodyMatrix(CA->ConstraintActor2, CA->ConstraintSetup->ConstraintBone2);

	if(!LimitMaterial)
		LimitMaterial = LoadObject<UMaterialInstance>(NULL, TEXT("EditorMaterials.PhAT_JointLimitMaterial"), NULL, LOAD_NoFail, NULL);

	// Draw full limits if in game or if selected in editor.
	UBOOL bDrawLimits = !GIsEditor || GSelectionTools.IsSelected( Owner );

	CA->ConstraintSetup->DrawConstraint(PRI, 1.f, bDrawLimits, true, LimitMaterial, Con1Frame, Con2Frame);


	if(bDrawLimits)
	{
		FBox Body1Box = FindBodyBox(CA->ConstraintActor1, CA->ConstraintSetup->ConstraintBone1);
		if(Body1Box.IsValid)
		{
			PRI->DrawLine( Con1Frame.GetOrigin(), Body1Box.GetCenter(), JointFrame1Color );
			PRI->DrawWireBox( Body1Box, JointFrame1Color );
		}

		FBox Body2Box = FindBodyBox(CA->ConstraintActor2, CA->ConstraintSetup->ConstraintBone2);
		if(Body2Box.IsValid)
		{
			PRI->DrawLine( Con2Frame.GetOrigin(), Body2Box.GetCenter(), JointFrame2Color );
			PRI->DrawWireBox( Body2Box, JointFrame2Color );
		}
	}
}