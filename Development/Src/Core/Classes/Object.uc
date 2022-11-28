//=============================================================================
// Object: The base class all objects.
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Object
	native
	noexport;

//=============================================================================
// UObject variables.

// Internal variables.
var native private const int ObjectInternal[6];
var native const object Outer;
var native const int ObjectFlags;
var(Object) native const editconst name Name;
var native const editconst class Class;

//=============================================================================
// Unreal base structures.

// Object flags.
const RF_Transactional	= 0x00000001; // Supports editor undo/redo.
const RF_Public         = 0x00000004; // Can be referenced by external package files.
const RF_Transient      = 0x00004000; // Can't be saved or loaded.
const RF_NotForClient	= 0x00100000; // Don't load for game client.
const RF_NotForServer	= 0x00200000; // Don't load for game server.
const RF_NotForEdit		= 0x00400000; // Don't load for editor.

// Temporary UnrealScript->C++ mirrors.

struct pointer
{
	var native const int Dummy;
};

struct double
{
	var native const int A, B;
};

struct qword
{
	var native const int A, B;
};

struct Map_Mirror
{
	var native const pointer	Pairs;
	var native const int		PairNum,
								PairMax;
	var native const pointer	Hash;
	var native const int		HashNum;
};

struct MultiMap_Mirror
{
	var native const pointer	Pairs;
	var native const int		PairNum,
								PairMax;
	var native const pointer	Hash;
	var native const int		HashNum;
};

struct LazyArray_Mirror
{
	var native const pointer	VfTable;
	var native const pointer	Data;
	var native const int		ArrayNum;
	var native const int		ArrayMax;
	var native const pointer	SavedAr;
	var native const int		SavedPos;
};

// A globally unique identifier.
struct Guid
{
	var int A, B, C, D;
};

// A point or direction vector in 3d space.
struct Vector
{
	var() config float X, Y, Z;
};

struct Vector2D
{
	var() config float X, Y;
};

// A plane definition in 3d space.
struct Plane extends Vector
{
	var() config float W;
};

// An orthogonal rotation in 3d space.
struct Rotator
{
	var() config int Pitch, Yaw, Roll;
};

// Quaternion
struct Quat
{
	var() config float X, Y, Z, W;
};

// A scale and sheering.
struct Scale
{
	var() config vector Scale;
	var() config float SheerRate;
	var() config enum ESheerAxis
	{
		SHEER_None,
		SHEER_XY,
		SHEER_XZ,
		SHEER_YX,
		SHEER_YZ,
		SHEER_ZX,
		SHEER_ZY,
	} SheerAxis;
};

// Generic axis enum.
enum EAxis
{
	AXIS_NONE, // = 0
	AXIS_X, // = 1
	AXIS_Y, // = 2
	AXIS_BLANK, // = 3. Need this because AXIS enum is used as bitfield in C++...
	AXIS_Z // 4
};

// A color.
struct Color
{
	var() config byte B, G, R, A;
};

// A linear color.
struct LinearColor
{
	var() config float R, G, B, A;

	structdefaults
	{
		A=1.f
	}
};

// A bounding box.
struct Box
{
	var vector Min, Max;
	var byte IsValid;
};

// A bounding box and bounding sphere with the same origin.
struct BoxSphereBounds
{
	var vector	Origin;
	var vector	BoxExtent;
	var float	SphereRadius;
};

// a 4x4 matrix
struct Matrix
{
	var() Plane XPlane;
	var() Plane YPlane;
	var() Plane ZPlane;
	var() Plane WPlane;
};

// Interpolation data types.
enum EInterpCurveMode
{
	CIM_Linear,
	CIM_CurveAuto,
	CIM_Constant,
	CIM_CurveUser,
	CIM_CurveBreak
};

struct InterpCurvePointFloat
{
	var() float InVal;
	var() float OutVal;
	var() float ArriveTangent;
	var() float LeaveTangent;
	var() EInterpCurveMode InterpMode;
};
struct InterpCurveFloat
{
	var() array<InterpCurvePointFloat>	Points;
};

struct InterpCurvePointVector
{
	var() float InVal;
	var() vector OutVal;
	var() vector ArriveTangent;
	var() vector LeaveTangent;
	var() EInterpCurveMode InterpMode;
};
struct InterpCurveVector
{
	var() array<InterpCurvePointVector>	Points;
};

struct InterpCurvePointQuat
{
	var() float InVal;
	var() quat OutVal;
	var() quat ArriveTangent;
	var() quat LeaveTangent;
	var() EInterpCurveMode InterpMode;
};
struct InterpCurveQuat
{
	var() array<InterpCurvePointQuat> Points;
};

struct CompressedPosition
{
	var vector Location;
	var rotator Rotation;
	var vector Velocity;
};

//=============================================================================
// Constants.

const MaxInt		= 0x7fffffff;
const Pi			= 3.1415926535897932;
const RadToDeg		= 57.295779513082321600;	// 180 / Pi
const DegToRad		= 0.017453292519943296;		// Pi / 180

//=============================================================================
// Basic native operators and functions.

// Bool operators.
native(129) static final preoperator  bool  !  ( bool A );
native(242) static final operator(24) bool  == ( bool A, bool B );
native(243) static final operator(26) bool  != ( bool A, bool B );
native(130) static final operator(30) bool  && ( bool A, skip bool B );
native(131) static final operator(30) bool  ^^ ( bool A, bool B );
native(132) static final operator(32) bool  || ( bool A, skip bool B );

// Byte operators.
native(133) static final operator(34) byte *= ( out byte A, byte B );
native(134) static final operator(34) byte /= ( out byte A, byte B );
native(135) static final operator(34) byte += ( out byte A, byte B );
native(136) static final operator(34) byte -= ( out byte A, byte B );
native(137) static final preoperator  byte ++ ( out byte A );
native(138) static final preoperator  byte -- ( out byte A );
native(139) static final postoperator byte ++ ( out byte A );
native(140) static final postoperator byte -- ( out byte A );

// Integer operators.
native(141) static final preoperator  int  ~  ( int A );
native(143) static final preoperator  int  -  ( int A );
native(144) static final operator(16) int  *  ( int A, int B );
native(145) static final operator(16) int  /  ( int A, int B );
native(146) static final operator(20) int  +  ( int A, int B );
native(147) static final operator(20) int  -  ( int A, int B );
native(148) static final operator(22) int  << ( int A, int B );
native(149) static final operator(22) int  >> ( int A, int B );
native(196) static final operator(22) int  >>>( int A, int B );
native(150) static final operator(24) bool <  ( int A, int B );
native(151) static final operator(24) bool >  ( int A, int B );
native(152) static final operator(24) bool <= ( int A, int B );
native(153) static final operator(24) bool >= ( int A, int B );
native(154) static final operator(24) bool == ( int A, int B );
native(155) static final operator(26) bool != ( int A, int B );
native(156) static final operator(28) int  &  ( int A, int B );
native(157) static final operator(28) int  ^  ( int A, int B );
native(158) static final operator(28) int  |  ( int A, int B );
native(159) static final operator(34) int  *= ( out int A, float B );
native(160) static final operator(34) int  /= ( out int A, float B );
native(161) static final operator(34) int  += ( out int A, int B );
native(162) static final operator(34) int  -= ( out int A, int B );
native(163) static final preoperator  int  ++ ( out int A );
native(164) static final preoperator  int  -- ( out int A );
native(165) static final postoperator int  ++ ( out int A );
native(166) static final postoperator int  -- ( out int A );

// Integer functions.
native(167) static final Function     int  Rand  ( int Max );
native(249) static final function     int  Min   ( int A, int B );
native(250) static final function     int  Max   ( int A, int B );
native(251) static final function     int  Clamp ( int V, int A, int B );

// Float operators.
native(169) static final preoperator  float -  ( float A );
native(170) static final operator(12) float ** ( float A, float B );
native(171) static final operator(16) float *  ( float A, float B );
native(172) static final operator(16) float /  ( float A, float B );
native(173) static final operator(18) float %  ( float A, float B );
native(174) static final operator(20) float +  ( float A, float B );
native(175) static final operator(20) float -  ( float A, float B );
native(176) static final operator(24) bool  <  ( float A, float B );
native(177) static final operator(24) bool  >  ( float A, float B );
native(178) static final operator(24) bool  <= ( float A, float B );
native(179) static final operator(24) bool  >= ( float A, float B );
native(180) static final operator(24) bool  == ( float A, float B );
native(210) static final operator(24) bool  ~= ( float A, float B );
native(181) static final operator(26) bool  != ( float A, float B );
native(182) static final operator(34) float *= ( out float A, float B );
native(183) static final operator(34) float /= ( out float A, float B );
native(184) static final operator(34) float += ( out float A, float B );
native(185) static final operator(34) float -= ( out float A, float B );

// Float functions.
native(186) static final function     float Abs   ( float A );
native(187) static final function     float Sin   ( float A );
native      static final function	  float Asin  ( float A );
native(188) static final function     float Cos   ( float A );
native      static final function     float Acos  ( float A );
native(189) static final function     float Tan   ( float A );
native(190) static final function     float Atan  ( float A, float B ); 
native(191) static final function     float Exp   ( float A );
native(192) static final function     float Loge  ( float A );
native(193) static final function     float Sqrt  ( float A );
native(194) static final function     float Square( float A );
native(195) static final function     float FRand ();
native(244) static final function     float FMin  ( float A, float B );
native(245) static final function     float FMax  ( float A, float B );
native(246) static final function     float FClamp( float V, float A, float B );
native(247) static final function     float Lerp  ( float Alpha, float A, float B );
native(248) static final function     float Smerp ( float Alpha, float A, float B );

// Vector operators.
native(211) static final preoperator  vector -     ( vector A );
native(212) static final operator(16) vector *     ( vector A, float B );
native(213) static final operator(16) vector *     ( float A, vector B );
native(296) static final operator(16) vector *     ( vector A, vector B );
native(214) static final operator(16) vector /     ( vector A, float B );
native(215) static final operator(20) vector +     ( vector A, vector B );
native(216) static final operator(20) vector -     ( vector A, vector B );
native(275) static final operator(22) vector <<    ( vector A, rotator B );
native(276) static final operator(22) vector >>    ( vector A, rotator B );
native(217) static final operator(24) bool   ==    ( vector A, vector B );
native(218) static final operator(26) bool   !=    ( vector A, vector B );
native(219) static final operator(16) float  Dot   ( vector A, vector B );
native(220) static final operator(16) vector Cross ( vector A, vector B );
native(221) static final operator(34) vector *=    ( out vector A, float B );
native(297) static final operator(34) vector *=    ( out vector A, vector B );
native(222) static final operator(34) vector /=    ( out vector A, float B );
native(223) static final operator(34) vector +=    ( out vector A, vector B );
native(224) static final operator(34) vector -=    ( out vector A, vector B );

// Vector functions.
native(225)		static final function	float	VSize	( vector A );
native			static final function	float	VSize2D	( vector A );
native			static final function	float	VSizeSq	( vector A );
native(226)		static final function	vector	Normal	( vector A );
native			static final function	vector	VLerp	( float Alpha, vector A, vector B );
native			static final function	vector	VSmerp	( float Alpha, vector A, vector B );
native(252)		static final function	vector	VRand	( );
native(300)		static final function	vector	MirrorVectorByNormal( vector Vect, vector Normal );
native(1500)	static final function	Vector	ProjectOnTo( Vector x, Vector y );
native(1501)	static final function	bool	IsZero( Vector A );

// Rotator operators and functions.
native(142) static final operator(24) bool ==     ( rotator A, rotator B );
native(203) static final operator(26) bool !=     ( rotator A, rotator B );
native(287) static final operator(16) rotator *   ( rotator A, float    B );
native(288) static final operator(16) rotator *   ( float    A, rotator B );
native(289) static final operator(16) rotator /   ( rotator A, float    B );
native(290) static final operator(34) rotator *=  ( out rotator A, float B  );
native(291) static final operator(34) rotator /=  ( out rotator A, float B  );
native(316) static final operator(20) rotator +   ( rotator A, rotator B );
native(317) static final operator(20) rotator -   ( rotator A, rotator B );
native(318) static final operator(34) rotator +=  ( out rotator A, rotator B );
native(319) static final operator(34) rotator -=  ( out rotator A, rotator B );
native(229) static final function GetAxes         ( rotator A, out vector X, out vector Y, out vector Z );
native(230) static final function GetUnAxes       ( rotator A, out vector X, out vector Y, out vector Z );
native(320) static final function rotator RotRand ( optional bool bRoll );
native      static final function rotator OrthoRotation( vector X, vector Y, vector Z );
native      static final function rotator Normalize( rotator Rot );
native		static final operator(24) bool ClockwiseFrom( int A, int B );
native		static final function Rotator RLerp( float Alpha, Rotator A, Rotator B );
native		static final function Rotator RSmerp( float Alpha, Rotator A, Rotator B );

// String operators.
native(112) static final operator(40) string $  ( coerce string A, coerce string B );
native(168) static final operator(40) string @  ( coerce string A, coerce string B );
native(115) static final operator(24) bool   <  ( string A, string B );
native(116) static final operator(24) bool   >  ( string A, string B );
native(120) static final operator(24) bool   <= ( string A, string B );
native(121) static final operator(24) bool   >= ( string A, string B );
native(122) static final operator(24) bool   == ( string A, string B );
native(123) static final operator(26) bool   != ( string A, string B );
native(124) static final operator(24) bool   ~= ( string A, string B );

// String functions.
native(125) static final function int    Len    ( coerce string S );
native(126) static final function int    InStr  ( coerce string S, coerce string t );
native(127) static final function string Mid    ( coerce string S, int i, optional int j );
native(128) static final function string Left   ( coerce string S, int i );
native(234) static final function string Right  ( coerce string S, int i );
native(235) static final function string Caps   ( coerce string S );
native(236) static final function string Chr    ( int i );
native(237) static final function int    Asc    ( string S );

// Object operators.
native(114) static final operator(24) bool == ( Object A, Object B );
native(119) static final operator(26) bool != ( Object A, Object B );

// Name operators.
native(254) static final operator(24) bool == ( name A, name B );
native(255) static final operator(26) bool != ( name A, name B );

// Quaternion functions
native		static final function Quat QuatProduct( Quat A, Quat B );
native		static final function Quat QuatInvert( Quat A );
native		static final function vector QuatRotateVector( Quat A, vector B );
native		static final function Quat QuatFindBetween( Vector A, Vector B );
native		static final function Quat QuatFromAxisAndAngle( Vector Axis, Float Angle );
native		static final function Quat QuatFromRotator( rotator A );
native		static final function rotator QuatToRotator( Quat A );

//
// Vector2D functions

/**
 * Returns the value in the Range, relative to Pct.
 * Examples:
 * - GetRangeValueByPct( Range, 0.f ) == Range.X
 * - GetRangeValueByPct( Range, 1.f ) == Range.Y
 * - GetRangeValueByPct( Range, 0.5 ) == (Range.X+Range.Y)/2
 *
 * @param	Range	Range of values. [Range.X,Range.Y]
 * @param	Pct		Relative position in range in percentage. [0,1]
 *
 * @return	the value in the Range, relative to Pct.
 */
static final simulated function float GetRangeValueByPct( Vector2D Range, float Pct )
{
	return ( Range.X + (Range.Y-Range.X) * Pct );
}

/**
 * Returns the relative percentage position Value is in the Range.
 * Examples:
 * - GetRangeValueByPct( Range, Range.X ) == 0
 * - GetRangeValueByPct( Range, Range.Y ) == 1
 * - GetRangeValueByPct( Range, (Range.X+Range.Y)/2 ) == 0.5
 *
 * @param	Range	Range of values. [Range.X,Range.Y]
 * @param	Value	Value between Range.
 *
 * @return	relative percentage position Value is in the Range.
 */
static final simulated function float GetRangePctByValue( Vector2D Range, float Value )
{
	return (Value - Range.X) / (Range.Y - Range.X);	
}

//
// Color functions
//

/** Create a Color from independant RGBA components */
static final function Color MakeColor(byte R, byte G, byte B, optional byte A)
{
	local Color C;

	C.R = R;
	C.G = G;
	C.B = B;
	C.A = A;
	return C;
}

//
// Linear Color Functions
//

/** Create a LinearColor from independant RGBA components. */
static final function LinearColor	MakeLinearColor( float R, float G, float B, float A )
{
	local LinearColor	LC;

	LC.R = R;
	LC.G = G;
	LC.B = B;
	LC.A = A;
	return LC;
}

//=============================================================================
// General functions.

// Logging.
native(231) final static function Log( coerce string S, optional name Tag );
native(232) final static function Warn( coerce string S );
native static function string Localize( string SectionName, string KeyName, string PackageName );

// Goto state and label.

/**
 * Transitions to the desired state and label if specified,
 * generating the EndState event in the current state if applicable
 * and BeginState in the new state, unless transitioning to the same
 * state.
 * 
 * @param	NewState - new state to transition to
 * 
 * @param	Label - optional Label to jump to
 * 
 * @param	bForceEvents - optionally force EndState/BeginState to be
 * 			called even if transitioning to the same state.
 */
native(113) final function GotoState( optional name NewState, optional name Label, optional bool bForceEvents );

/**
 * Checks the current state and determines whether or not this object
 * is actively in the specified state.  Note: This *does* work with
 * inherited states.
 * 
 * @param	TestState - state to check for
 * 
 * @return	True if currently in TestState
 */
native(281) final function bool IsInState( name TestState );

/**
 * Returns the current state name, useful for determining current
 * state similar to IsInState.  Note:  This *doesn't* work with
 * inherited states, in that it will only compare at the lowest
 * state level.
 * 
 * @return	Name of the current state
 */
native(284) final function name GetStateName();

/**
 * Returns the current calling function's name, useful for
 * debugging.
 */
native final function Name GetFuncName();

/**
 * Dumps the current script function stack to the log file, useful
 * for debugging.
 */
native final function ScriptTrace();

/**
 * Pushes the new state onto the state stack, setting it as the 
 * current state until a matching PopState() is called.  Note that
 * multiple states may be pushed on top of each other.
 * 
 * @param	NewState - name of the state to push on the stack
 * 
 * @param	NewLabel - optional name of the state label to jump to
 */
native final function PushState(Name NewState, optional Name NewLabel);

/**
 * Pops the current pushed state, returning execution to the previous
 * state at the same code point.  Note: PopState() will have no effect
 * if no state has been pushed onto the stack.
 * 
 * @param	bPopAll - optionally pop all states on the stack to the
 * 			originally executing one
 */
native final function PopState(optional bool bPopAll);

// Objects.
native(258) static final function bool ClassIsChildOf( class TestClass, class ParentClass );
native(303) final function bool IsA( name ClassName );

// Probe messages.
native(117) final function Enable( name ProbeFunc );
native(118) final function Disable( name ProbeFunc );

// Properties.
native final function string GetPropertyText( string PropName );
native final function SetPropertyText( string PropName, string PropValue );
native static final function name GetEnum( object E, int i );
native static final function object DynamicLoadObject( string ObjectName, class ObjectClass, optional bool MayFail );
native static final function object FindObject( string ObjectName, class ObjectClass );

// Configuration.
native(536) final function SaveConfig();
native static final function StaticSaveConfig();

// Return a random number within the given range.
static final simulated function  float RandRange( float Min, float Max )
{
    return Min + (Max - Min) * FRand();
}

/**
 * Returns the relative percentage position Value is in the range [Min,Max].
 * Examples:
 * - GetRangeValueByPct( 2, 4, 2 ) == 0
 * - GetRangeValueByPct( 2, 4, 4 ) == 1
 * - GetRangeValueByPct( 2, 4, 3 ) == 0.5
 *
 * @param	Min		Min limit
 * @param	Max		Max limit
 * @param	Value	Value between Range.
 *
 * @return	relative percentage position Value is in the range [Min,Max].
 */
static final simulated function float FPctByRange( float Value, float Min, float Max )
{
	return (Value - Min) / (Max - Min);	
}

//=============================================================================
// Engine notification functions.

/**
 * Called immediately when entering a state, while within the
 * GotoState() call that caused the state change (before any
 * state code is executed).
 */
event BeginState();

/**
 * Called immediately before going out of the current state, while
 * within the GotoState() call that caused the state change, and
 * before BeginState() is called within the new state.
 */
event EndState();

/**
 * Called immediately in the new state that was pushed onto the 
 * state stack, before any state code is executed.
 */
event PushedState();

/**
 * Called immediately in the current state that is being popped off
 * of the state stack, before the new state is activated.
 */
event PoppedState();

/**
 * Called on the state that is being paused because of a PushState().
 */
event PausedState();

/**
 * Called on the state that is no longer paused because of a PopState().
 */
event ContinuedState();

/**
 * Tries to reach TargetLocation based on distance from ActualLocation, giving a nice
 * smooth feeling when tracking an object. 
 * (Doesn't work well when target teleports )
 * 
 * @param		ActualLocation
 * @param		TargetLocation
 * @param		fDeltaTime - time since last tick
 * @param		InterpolationSpeed
 * @return		new interpolated vector
 */
static final simulated function vector VectorProportionalMoveTo
(	
	Vector	ActualLocation, 
	Vector	TargetLocation, 
	float	fDeltaTime, 
	float	InterpolationSpeed 
)
{
	local vector	vDist, vDeltaMove, vAccel;

	// if DeltaTime is 0, do not perform any interpolation (Location was already calculated for that frame)
	if ( fDeltaTime == 0.f )
		return ActualLocation;

	// If distance is too small, just set the desired location
	if ( VSize(TargetLocation - ActualLocation) < 0.001 )
		return TargetLocation;

	vDist		= TargetLocation - ActualLocation;		// Distance to reach
	vAccel		= InterpolationSpeed * vDist;			// Acceleration proportional to distance
	vDeltaMove	= vAccel * fDeltaTime;					// resulting delta move

	// Check boundaries, so we don't go past TargetLocation
	if ( Square(vDeltaMove.X) > Square(VDist.X) )
		vDeltaMove.X = VDist.X;

	if ( Square(vDeltaMove.Y) > Square(VDist.Y) )
		vDeltaMove.Y = VDist.Y;

	if ( Square(vDeltaMove.Z) > Square(VDist.Z) )
		vDeltaMove.Z = VDist.Z;

	// If delta move is significant, then perform it
	if ( VSize(vDeltaMove) > 0.001 )
		return ActualLocation + vDeltaMove;
	
	// othewise, just return target location
	return TargetLocation;
}

/**
 * Tries to reach TargetValue based on the delta to ActualValue, giving a nice
 * smooth feeling when tracking a TargetValue.
 * 
 * @param		ActualValue, value for current frame.
 * 
 * @param		TargetValue, target value to reach
 * 
 * @param		fDeltaTime - time since last tick
 * 
 * @param		InterpolationSpeed
 * 
 * @return		new interpolated value
 */
static final simulated function float DeltaInterpolationTo
(	
	float	ActualValue, 
	float	TargetValue, 
	float	fDeltaTime, 
	float	InterpolationSpeed 
)
{
	local float	fDelta, fVel, fAccel;

	// if DeltaTime is 0, do not perform any interpolation (assume interpolation was already calculated for that frame)
	if ( fDeltaTime == 0.f )
		return ActualValue;

	// If delta is too small, just set the target value
	if ( Abs(TargetValue - ActualValue) < 0.001 )
		return TargetValue;

	fDelta	= TargetValue - ActualValue;		// Distance to reach
	fAccel	= InterpolationSpeed * fDelta;		// Acceleration proportional to distance
	fVel	= fAccel * fDeltaTime;				// resulting delta move

	// Check boundaries, so we don't go past Target Value
	if ( Square(fVel) > Square(fDelta) )
		fVel = fDelta;

	// If velocity is significant, then perform it
	if ( Abs(fVel) > 0.001 )
		return ActualValue + fVel;
	
	// othewise, just return target
	return TargetValue;
}

/**
 * Returns a Rotator axis within the [-32768,+32767] range in float
 *
 * @param	RotAxis, axis of the rotator
 * 
 * @return	Normalized axis value, within the [-32768,+32767] range in float
 */
static final function float FNormalizedRotAxis( int RotAxis )
{
	local float	fAxis;

	fAxis = float(RotAxis & 65535);
	if ( fAxis >= 32768 )
	{
		fAxis -= 65536;
	}
	return fAxis;
}

/**
 * Clamp a rotation Axis.
 * The ViewAxis rotation component must be normalized (within the [-32768,+32767] range).
 * This function will set out_DeltaViewAxis to the delta needed to bring ViewAxis within the [MinLimit,MaxLimit] range.
 *
 * @param	ViewAxis			Rotation Axis to clamp
 * @input	out_DeltaViewAxis	Delta Rotation Axis to be added to ViewAxis rotation (from ProcessViewRotation). 
 *								Set to be the Delta to bring ViewAxis within the [MinLimit,MaxLimit] range.
 * @param	MaxLimit			Maximum for Clamp. ViewAxis will not exceed this.
 * @param	MinLimit			Minimum for Clamp. ViewAxis will not go below this.
 */
static final simulated function ClampRotAxis
( 
		int		ViewAxis, 
	out int		out_DeltaViewAxis,
		int		MaxLimit,
		int		MinLimit
)
{
	local int	DesiredViewAxis;

	ViewAxis		= FNormalizedRotAxis( ViewAxis );
	DesiredViewAxis = ViewAxis + out_DeltaViewAxis;

	if ( DesiredViewAxis > MaxLimit )
		DesiredViewAxis = MaxLimit;

	if ( DesiredViewAxis < MinLimit )
		DesiredViewAxis = MinLimit;

	out_DeltaViewAxis = DesiredViewAxis - ViewAxis;
}

/**
 * Smooth clamp a rotator axis.
 * This is mainly used to bring smoothly a rotator component within a certain range [MinLimit,MaxLimit].
 * For example to limit smoothly the player's ViewRotation Pitch or Yaw component.
 *
 * @param	fDeltaTime			Elapsed time since this function was last called, for interpolation.
 * @param	ViewAxis			Rotator's Axis' current angle.
 * @input	out_DeltaViewAxis	Delta Value of Axis to be added to ViewAxis (through PlayerController::ProcessViewRotation(). 
 *								This value gets modified.
 * @param	MaxLimit			Up angle limit.
 * @param	MinLimit			Negative angle limit (value must be negative)
 * @param	InterpolationSpeed	Interpolation Speed to bring ViewAxis within the [MinLimit,MaxLimit] range.
 */
static final simulated function SClampRotAxis
( 
		float	fDeltaTime, 
		int		ViewAxis, 
	out int		out_DeltaViewAxis,
		int		MaxLimit,
		int		MinLimit,
		float	InterpolationSpeed
)
{
	local float		fViewAxis, fDeltaViewAxis;

	// normalize rotation components values
	fDeltaViewAxis	= FNormalizedRotAxis( out_DeltaViewAxis );
	fViewAxis		= FNormalizedRotAxis( ViewAxis );

	if ( fViewAxis <= MaxLimit && (fViewAxis + fDeltaViewAxis) >= MaxLimit )
	{	// forbid player from going beyond limits through fDeltaViewAxis
		fDeltaViewAxis = MaxLimit - fViewAxis;
	}
	else if ( fViewAxis > MaxLimit )
	{
		if ( fDeltaViewAxis > 0 )
		{	// if players goes counter interpolation, ignore input
			fDeltaViewAxis = 0.f;
		}
		if ( (fViewAxis + fDeltaViewAxis) > MaxLimit )
		{	// if above limit, interpolate back to within limits
			fDeltaViewAxis = DeltaInterpolationTo(fViewAxis, MaxLimit, fDeltaTime, InterpolationSpeed ) - fViewAxis - 1;
		}
	}
	else if ( fViewAxis >= MinLimit && (fViewAxis + fDeltaViewAxis) <= MinLimit )
	{	// forbid player from going beyond limits through fDeltaViewAxis
		fDeltaViewAxis = MinLimit - fViewAxis;
	}
	else if ( fViewAxis < MinLimit )
	{
		if ( fDeltaViewAxis < 0 )
		{	// if players goes counter interpolation, ignore input
			fDeltaViewAxis = 0.f;
		}
		if ( (fViewAxis + fDeltaViewAxis) < MinLimit )
		{	// if above limit, interpolate back to within limits
			fDeltaViewAxis += DeltaInterpolationTo(fViewAxis, MinLimit, fDeltaTime, InterpolationSpeed ) - fViewAxis + 1;
		}
	}

	// Overwrite new delta pitch
	out_DeltaViewAxis = fDeltaViewAxis;
}

/**
 * Calculates the distance of a given Point in world space to a given line, 
 * defined by the vector couple (Origin, Direction).
 * 
 * @param	Point				point to check distance to Axis
 * @param	Direction			unit vector indicating the direction to check against
 * @param	Origin				point of reference used to calculate distance
 * @param	out_ClosestPoint	optional point that represents the closest point projected onto Axis
 * 
 * @return	distance of Point from line defined by (Origin, Direction)
 */
simulated final function float PointDistToLine(vector Point, vector Direction, vector Origin, optional out vector out_ClosestPoint)
{
	local vector	SafeDirection;
	local float		ProjectedDist;

	SafeDirection		= Normal(Direction);
	ProjectedDist		= (Point-Origin) Dot SafeDirection;
	out_ClosestPoint	= Origin + (SafeDirection * ProjectedDist);

	return VSize(out_ClosestPoint-Point);
}

/**
 * Calculates the distance of a given point to the given plane. (defined by a combination of vector and rotator)
 * Rotator.AxisX = U, Rotator.AxisY = Normal, Rotator.AxisZ = V
 * 
 * @param	Point				Point to check distance to Orientation
 * @param	Orientation			Rotator indicating the direction to check against
 * @param	Origin				Point of reference used to calculate distance
 * @param	out_ClosestPoint	Optional point that represents the closest point projected onto Plane defined by the couple (Origin, Orientation)
 * 
 * @return	distance of Point to plane
 */
simulated final function float PointDistToPlane( Vector Point, Rotator Orientation, Vector Origin, optional out vector out_ClosestPoint )
{
	local vector	AxisX, AxisY, AxisZ, PointNoZ, OriginNoZ;
	local float		fPointZ, fProjDistToAxis;

	// Get orientation axis'
	GetAxes(Orientation, AxisX, AxisY, AxisZ);

	// Remove Z component of Point Location
	fPointZ		= Point dot AxisZ;
	PointNoZ	= Point - fPointZ * AxisZ;

	// Remove Z component of Origin
	OriginNoZ	= Origin - (Origin dot AxisZ) * AxisZ;

	// Projected distance of Point onto AxisX.
	fProjDistToAxis		= (PointNoZ - OriginNoZ) Dot AxisX;
	out_ClosestPoint	= OriginNoZ + fProjDistToAxis * AxisX + fPointZ * AxisZ;

	// return distance to closest point
	return VSize(out_ClosestPoint-Point);
}

/** 
 * Determines if point is inside given box
 *
 * @param	Point		- Point to check
 * 
 * @param	Location	- Center of box
 * 
 * @param	Extent		- Size of box
 * 
 * @return	bool - TRUE if point is inside box, FALSE otherwise
 */
static final function bool PointInBox( Vector Point, Vector Location, Vector Extent )
{
	local Vector MinBox, MaxBox;

	MinBox = Location - Extent;
	MaxBox = Location + Extent;

	if( Point.X >= MinBox.X &&
		Point.X <= MaxBox.X )
	{
		if( Point.Y >= MinBox.Y &&
			Point.Y <= MaxBox.Y )
		{
			if( Point.Z >= MinBox.Z &&
				Point.Z <= MaxBox.Z )
			{
				return true;
			}
		}
	}

	return false;
}

/**
 * Converts a float value to a 0-255 byte, assuming a range of
 * 0.f to 1.f.
 * 
 * @param	inputFloat - float to convert
 * 
 * @param	bSigned - optional, assume a range of -1.f to 1.f
 * 
 * @return	byte value 0-255
 */
simulated final function byte FloatToByte(float inputFloat, optional bool bSigned)
{
	if (bSigned)
	{
		// handle a 0.02f threshold so we can guarantee valid 0/255 values
		if (inputFloat > 0.98f)
		{
			return 255;
		}
		else
		if (inputFloat < -0.98f)
		{
			return 0;
		}
		else
		{
			return byte((inputFloat+1.f)*128.f);
		}
	}
	else
	{
		if (inputFloat > 0.98f)
		{
			return 255;
		}
		else
		if (inputFloat < 0.02f)
		{
			return 0;
		}
		else
		{
			return byte(inputFloat*255.f);
		}
	}
}

/**
 * Converts a 0-255 byte to a float value, to a range of 0.f
 * to 1.f.
 * 
 * @param	inputByte - byte to convert
 * 
 * @param	bSigned - optional, spit out -1.f to 1.f instead
 * 
 * @return	newly converted value
 */
simulated final function float ByteToFloat(byte inputByte, optional bool bSigned)
{
	if (bSigned)
	{
		return ((float(inputByte)/128.f)-1.f);
	}
	else
	{
		return (float(inputByte)/255.f);
	}
}


/**
 * Calculates the dotted distance of vector 'Direction' to coordinate system O(AxisX,AxisY,AxisZ).
 *
 * Orientation: (consider 'O' the first person view of the player, and 'Direction' a vector pointing to an enemy)
 * - positive azimuth means enemy is on the right of crosshair. (negative means left).
 * - positive elevation means enemy is on top of crosshair, negative means below.
 *
 * Note: 'Azimuth' (.X) sign is changed to represent left/right and not front/behind. front/behind is the funtion's return value.
 *
 * @param	out_DotDist			.X = 'Direction' dot AxisX relative to plane (AxisX,AxisZ). (== Cos(Azimuth))
 *								.Y = 'Direction' dot AxisX relative to plane (AxisX,AxisY). (== Sin(Elevation))
 * @param	Direction			direction of target.
 * @param	AxisX				X component of reference system.
 * @param	AxisY				Y component of reference system.
 * @param	AxisZ				Z component of reference system.
 *
 * @output	true if 'Direction' is facing AxisX (Direction dot AxisX >= 0.f)
 */
static final simulated function bool GetDotDistance
( 
	out	vector2D	out_DotDist, 
		vector		Direction, 
		vector		AxisX, 
		vector		AxisY, 
		vector		AxisZ 
)
{
	local vector	NormalDir, NoZProjDir;
	local float		DirDotX;
	local int		AzimuthSign;

	NormalDir = Normal(Direction);

	// Find projected point (on AxisX and AxisY, remove AxisZ component)
	NoZProjDir = Normal( NormalDir - (NormalDir dot AxisZ) * AxisZ );

	// Figure out on which Azimuth dot is on right or left.
	AzimuthSign	= 1;
	if( (NoZProjDir dot AxisY) < 0.f )
	{
		AzimuthSign	= -1;
	}

	out_DotDist.Y	= NormalDir dot AxisZ;
	DirDotX			= NoZProjDir dot AxisX;
	out_DotDist.X	= AzimuthSign * Abs(DirDotX);

	return (DirDotX >= 0.f );
}

/**
 * Calculates the angular distance of vector 'Direction' to coordinate system O(AxisX,AxisY,AxisZ).
 *
 * Orientation: (consider 'O' the first person view of the player, and 'Direction' a vector pointing to an enemy)
 * - positive azimuth means enemy is on the right of crosshair. (negative means left).
 * - positive elevation means enemy is on top of crosshair, negative means below.
 *
 * @param	out_AngularDist	.X = Azimuth angle (in radians) of 'Direction' vector compared to plane (AxisX,AxisZ).
 *							.Y = Elevation angle (in radians) of 'Direction' vector compared to plane (AxisX,AxisY).
 * @param	Direction		Direction of target.
 * @param	AxisX			X component of reference system.
 * @param	AxisY			Y component of reference system.
 * @param	AxisZ			Z component of reference system.
 *
 * @output	true if 'Direction' is facing AxisX (Direction dot AxisX >= 0.f)
 */
static final simulated function bool GetAngularDistance
(
	out	vector2D	out_AngularDist, 
		vector		Direction, 
		vector		AxisX, 
		vector		AxisY, 
		vector		AxisZ 	
)
{
	local Vector2D	DotDist;
	local bool		bIsInFront;

	// Get Dotted distance
	bIsInFront = GetDotDistance( DotDist, Direction, AxisX, AxisY, AxisZ );
	GetAngularFromDotDist(out_AngularDist, DotDist );

	return bIsInFront;
}

/**
 * Converts Dot distance to angular distance.
 * @see	GetAngularDistance() and GetDotDistance().
 *
 * @param	out_AngularDist		Angular distance in radians.
 * @param	DotDist				Dot distance.
 */
static final simulated function GetAngularFromDotDist
(
	out	vector2D	out_AngularDist, 
		vector2D	DotDist
)
{
	local int	AzimuthSign;

	if ( DotDist.X < 0.f )
	{
		DotDist.X	*= -1.f;
		AzimuthSign	 = -1;
	}
	else
	{
		AzimuthSign = 1;
	}
	
	// Convert to angles.
	// Because of mathematical imprecision, make sure dot values are within the [-1.f,1.f] range.
	out_AngularDist.X = ACos( FClamp(DotDist.X, -1.f, 1.f) ) * AzimuthSign;
	out_AngularDist.Y = ASin( FClamp(DotDist.Y, -1.f, 1.f) );
}

/* transforms angular distance in radians to degrees */
static final simulated function GetAngularDegreesFromRadians( out vector2d out_FOV )
{
	out_FOV.X = out_FOV.X*RadToDeg;
	out_FOV.Y = out_FOV.Y*RadToDeg;
}

defaultproperties
{
}
