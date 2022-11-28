class ParticleSystem extends Object
	native(Particle)
	hidecategories(Object);

var()		enum EParticleSystemUpdateMode
{
	EPSUM_RealTime,
	EPSUM_FixedTime
} SystemUpdateMode;

var()	float	UpdateTime_FPS;
var		float	UpdateTime_Delta;
var()	float	WarmupTime;

var		export array<ParticleEmitter> Emitters;

// Used in Editor for drawing thumbnail preview. Initialised on first thumbnail render.
var	transient ParticleSystemComponent	PreviewComponent;

var		rotator	ThumbnailAngle;
var		float	ThumbnailDistance;
var()	float	ThumbnailWarmup;	


// Used for curve editor to remember curve-editing setup.
var		export InterpCurveEdSetup	CurveEdSetup;

cpptext
{
	// UObject interface.
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void PostLoad();
	
	// Browser window.
	void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	INT GetThumbnailLabels( TArray<FString>* InLabels );
}

defaultproperties
{
	ThumbnailDistance=200.0
	ThumbnailWarmup=1.0

	UpdateTime_FPS=60.0
	UpdateTime_Delta=1.0/60.0
	WarmupTime=0.0
}