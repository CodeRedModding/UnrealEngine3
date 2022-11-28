class DistributionVector extends Component
	noexport
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew
	abstract;
	
/** FCurveEdInterface virtual function table. */
var private native noexport pointer	CurveEdVTable; 

enum EDistributionVectorLockFlags
{
    EDVLF_None,
    EDVLF_XY,
    EDVLF_XZ,
    EDVLF_YZ,
    EDVLF_XYZ
};

enum EDistributionVectorMirrorFlags
{
	EDVMF_Same,
	EDVMF_Different,
	EDVMF_Mirror
};