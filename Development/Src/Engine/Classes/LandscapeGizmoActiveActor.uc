/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class LandscapeGizmoActiveActor extends LandscapeGizmoActor
	notplaceable
	native(Terrain);

enum ELandscapeGizmoType
{
	LGT_None,
	LGT_Height,
	LGT_Weight
};

struct native GizmoSelectData
{
	var editoronly float Ratio;
	var editoronly float HeightData;
	var editoronly native map{FName, FLOAT} WeightDataMap;

	structcpptext
	{
		FGizmoSelectData()
			#if WITH_EDITORONLY_DATA
			: Ratio(0.f), HeightData(0.f)
			#endif
		{
		}
	}
};

var transient editoronly ELandscapeGizmoType DataType;
var const private native map{QWORD, FGizmoSelectData} SelectedData;

var editoronly Texture2D GizmoTexture;
var editoronly Vector2D TextureScale;
var editoronly array<Vector> SampledHeight;
var editoronly array<Vector> SampledNormal;
var editoronly int SampleSizeX;
var editoronly int SampleSizeY;
var editoronly float CachedWidth;
var editoronly float CachedHeight;
var editoronly float CachedScaleXY;
var editoronly transient vector FrustumVerts[8];

var editoronly Material GizmoMaterial;
var editoronly MaterialInstance GizmoDataMaterial;
var editoronly Material GizmoMeshMaterial;
var editoronly Material GizmoMeshMaterial2;

var() editoronly editconst array<Name> LayerNames; // only for showing LayerNames currently contained...

cpptext
{
#if WITH_EDITOR
	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	// AActor interface.
	virtual void PostEditMove(UBOOL bFinished);
	virtual void PostLoad();

	ALandscapeGizmoActor* SpawnGizmoActor();

	virtual void Serialize(FArchive& Ar);
	void ClearGizmoData();
	void FitToSelection();
	void FitMinMaxHeight();
	void SetTargetLandscape(ULandscapeInfo* LandscapeInfo);

	void CalcNormal();
	void SampleData(INT SizeX, INT SizeY);

	void ExportToClipboard();
	void ImportFromClipboard();
	void Import(INT VertsX, INT VertsY, WORD* HeightData, TArray<FName> ImportLayerNames, BYTE* LayerDataPointers[] );
	void Export(INT Index, TArray<FString>& Filenames);

	FLOAT GetNormalizedHeight(WORD LandscapeHeight);
	FLOAT GetLandscapeHeight(FLOAT NormalizedHeight);

	FLOAT GetWidth() { return Width * DrawScale * DrawScale3D.X; }
	FLOAT GetHeight() { return Height * DrawScale * DrawScale3D.Y; }
	FLOAT GetLength() { return LengthZ * DrawScale * DrawScale3D.Z; }

	void SetLength(FLOAT WorldLength) { LengthZ = WorldLength / (DrawScale * DrawScale3D.Z); }

	static const INT DataTexSize;
private:
	FORCEINLINE FLOAT GetWorldHeight(FLOAT NormalizedHeight);
#endif
}

defaultproperties
{
	//TickGroup=TG_DuringAsyncWork

	Components.Remove(Sprite)

	GizmoMaterial = Material'EditorLandscapeResources.LandscapeGizmo_Mat'
	GizmoDataMaterial = MaterialInstanceConstant'EditorLandscapeResources.LandscapeGizmo_Mat_Copied'
	GizmoMeshMaterial = Material'EditorLandscapeResources.LandscapeGizmoHeight_Mat'
	GizmoMeshMaterial2 = Material'EditorLandscapeResources.LandscapeGizmoHeight_UnderMat'

	Begin Object Class=LandscapeGizmoRenderComponent Name=GizmoRenderer
	End Object
	Components.Add(GizmoRenderer)

	bStatic=true
	bMovable=false
	Width=1280
	Height=1280
	LengthZ=1280
	MarginZ=512
	DataType=LGT_None
	bEditable=true
	SampleSizeX=0
	SampleSizeY=0
	CachedWidth=0
	CachedHeight=0
	CachedScaleXY=0
}
