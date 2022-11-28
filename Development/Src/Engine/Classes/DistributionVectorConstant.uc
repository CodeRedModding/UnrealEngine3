class DistributionVectorConstant extends DistributionVector
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

/** This vector will be returned for all input times. */
var()	vector	Constant;

/** If true, X == Y == Z ie. only one degree of freedom. If false, each axis is picked independently. */ 
var		bool							bLockAxes;
var()	EDistributionVectorLockFlags	LockedAxes;

cpptext
{
	virtual FVector GetValue( FLOAT F = 0.f );

	// UObject interface
	virtual void Serialize(FArchive& Ar);
	
	// FCurveEdInterface interface
	virtual INT		GetNumKeys();
	virtual INT		GetNumSubCurves();
	virtual FLOAT	GetKeyIn(INT KeyIndex);
	virtual FLOAT	GetKeyOut(INT SubIndex, INT KeyIndex);
	virtual FColor	GetKeyColor(INT SubIndex, INT KeyIndex, const FColor& CurveColor);
	virtual void	GetInRange(FLOAT& MinIn, FLOAT& MaxIn);
	virtual void	GetOutRange(FLOAT& MinOut, FLOAT& MaxOut);
	virtual BYTE	GetKeyInterpMode(INT KeyIndex);
	virtual void	GetTangents(INT SubIndex, INT KeyIndex, FLOAT& ArriveTangent, FLOAT& LeaveTangent);
	virtual FLOAT	EvalSub(INT SubIndex, FLOAT InVal);

	virtual INT		CreateNewKey(FLOAT KeyIn);
	virtual void	DeleteKey(INT KeyIndex);

	virtual INT		SetKeyIn(INT KeyIndex, FLOAT NewInVal);
	virtual void	SetKeyOut(INT SubIndex, INT KeyIndex, FLOAT NewOutVal);
	virtual void	SetKeyInterpMode(INT KeyIndex, EInterpCurveMode NewMode);
	virtual void	SetTangents(INT SubIndex, INT KeyIndex, FLOAT ArriveTangent, FLOAT LeaveTangent);
}