//-----------------------------------------------------------
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//-----------------------------------------------------------
class MobileDebugCameraHUD extends MobileHUD
	config(Game);

/** When true the debug text is drawn.  This is passed through from CheatManager's ToggleDebugCamera exec function. */
var bool bDrawDebugText;

simulated event PostBeginPlay()
{
	super.PostBeginPlay();
}

function bool DisplayMaterials( float X, out float Y, float DY, MeshComponent MeshComp )
{
	local int	MaterialIndex;
	local bool	bDisplayedMaterial;
	local MaterialInterface	Material;

	bDisplayedMaterial = false;
	if ( MeshComp != None )
	{
		for ( MaterialIndex = 0; MaterialIndex < MeshComp.GetNumElements(); ++MaterialIndex )
		{
			Material = MeshComp.GetMaterial(MaterialIndex);
			if ( Material != None )
			{
				Y += DY;
				Canvas.SetPos( X + DY, Y );
				Canvas.DrawText("Material: '" $ Material.Name $ "'" );
				bDisplayedMaterial = true;
			}
		}
	}
	return bDisplayedMaterial;
}

event PostRender()
{
	local MobileDebugCameraController DCC;
	local float				xl,yl,X,Y;
	local String			MyText;
	local vector			CamLoc, ZeroVec;
	local rotator			CamRot;
	local TraceHitInfo		HitInfo;
	local Actor				HitActor;
	local MeshComponent		MeshComp;
	local vector			HitLoc, HitNormal;
	local bool				bFoundMaterial;
	local bool				bOldForceMobileHUD;

	bOldForceMobileHUD = bForceMobileHUD;
	bForceMobileHUD = true;

	super.PostRender();

	DCC = MobileDebugCameraController( PlayerOwner );
	if( DCC != none && bDrawDebugText )
	{
		Canvas.SetDrawColor(0, 0, 255, 255);
		MyText = "MobileDebugCameraHUD";
		Canvas.Font = class'Engine'.Static.GetSmallFont();
		Canvas.StrLen(MyText, XL, YL);
		X = Canvas.SizeX * 0.05f;
		Y = YL;//*1.67;
		YL += 2*Y;
		Canvas.SetPos( X, YL);
		Canvas.DrawText(MyText, true);

		Canvas.SetDrawColor(128, 128, 128, 255);
		//DCC.GetPlayerViewPoint( CamLoc, CamRot );
		CamLoc = DCC.PlayerCamera.CameraCache.POV.Location;
		CamRot = DCC.PlayerCamera.CameraCache.POV.Rotation;

		YL += Y;
		Canvas.SetPos(X,YL);
		Canvas.DrawText("CamLoc:" $ CamLoc @ "CamRot:" $ CamRot );

		HitActor = Trace(HitLoc, HitNormal, vector(camRot) * 5000 * 20 + CamLoc, CamLoc, true, ZeroVec, HitInfo);
		if( HitActor != None)
		{
			YL += Y;
			Canvas.SetPos(X,YL);
			Canvas.DrawText("HitLoc:" $ HitLoc @ "HitNorm:" $ HitNormal );
			YL += Y;
			Canvas.SetPos(X,YL);
			Canvas.DrawText("HitDist:" @ VSize(CamLoc - HitLoc) );
			YL += Y;
			Canvas.SetPos(X,YL);
			Canvas.DrawText("HitActor: '" $ HitActor.Name $ "'" );
			YL += Y;
			Canvas.SetPos(X,YL);
			Canvas.DrawText("PhysMat: '" $ HitInfo.PhysMaterial $ "'" );

			bFoundMaterial = false;
			if ( HitInfo.Material != None )
			{
				YL += Y;
				Canvas.SetPos(X + Y,YL);
				Canvas.DrawText("Material:" $ HitInfo.Material.Name );
				bFoundMaterial = true;
			}
			else if ( HitInfo.HitComponent != None )
			{
				bFoundMaterial = DisplayMaterials( X, YL, Y, MeshComponent(HitInfo.HitComponent) );
			}
			else
			{
				foreach HitActor.AllOwnedComponents( class'MeshComponent', MeshComp )
				{
					bFoundMaterial = bFoundMaterial || DisplayMaterials( X, YL, Y, MeshComp );	
				}
			}
			if ( bFoundMaterial == false )
			{
				YL += Y;
				Canvas.SetPos( X + Y, YL );
				Canvas.DrawText("Material: NONE" );
			}
			DrawDebugLine( HitLoc, HitLoc+HitNormal*30, 255,255,1255 );
		}
		else
		{
			YL += Y;
			Canvas.SetPos(X,YL);
			Canvas.DrawText( "Not trace hit" );
		}

		if ( DCC.bShowSelectedInfo == true && DCC.SelectedActor != None )
		{
			YL += Y;
			Canvas.SetPos( X, YL );
			Canvas.DrawText( "Selected actor: '" $ DCC.SelectedActor.Name $ "'" );
			DisplayMaterials( X, YL, Y, MeshComponent(DCC.SelectedComponent) );
		}
	}

	bForceMobileHUD = bOldForceMobileHUD;
}

DefaultProperties
{
	bHidden=false
	bDrawDebugText=true
	JoystickBackground=Texture2D'MobileResources.T_MobileControls_texture'
	JoystickBackgroundUVs=(U=0,V=0,UL=126,VL=126)
	JoystickHat=Texture2D'MobileResources.T_MobileControls_texture'
	JoystickHatUVs=(U=128,V=0,UL=78,VL=78)
}
