//=============================================================================
// Engine: The base class of the global application object classes.
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Engine extends Subsystem
	native
	config(Engine)
	noexport
	transient;

// Fonts.
var Font	TinyFont;
var Font	SmallFont;
var Font	MediumFont;
var Font	LargeFont;

// Drivers.
var(Drivers) config class<NetDriver>      NetworkDriverClass;

/** The material used when no material is explicitly applied. */
var globalconfig Material	DefaultMaterial;

/** The material used to render surfaces in the LightingOnly viewmode. */
var globalconfig Material	LightingOnlyMaterial;

/** A solid colored material with an instance parameter for the color. */
var globalconfig Material	SolidColorMaterial;

/** A material used to render colored BSP nodes. */
var globalconfig Material	ColoredNodeMaterial;

/** A translucent material used to render things in geometry mode. */
var globalconfig Material	GeomMaterial;

/** Material used for drawing a tick mark. */
var globalconfig Material	TickMaterial;

/** Material used for drawing a cross mark. */
var globalconfig Material	CrossMaterial;

/** The colors used to rendering light complexity. */
var globalconfig array<color> LightComplexityColors;

/** A material used to render the sides of the builder brush/volumes/etc. */
var globalconfig Material	EditorBrushMaterial;

// Variables.
var const client	Client;
var array<player>	Players;

var int TickCycles, GameCycles, ClientCycles;
var(Settings) config bool UseSound;

/** Whether to use dynamic streaming or not */
var(Settings) config bool UseStreaming;

/** Maximum number of miplevels being streamed in */
var(Settings) config int MaxStreamedInMips;
/** Minimum number of miplevels being streamed in */
var(Settings) config int MinStreamedInMips;

/** Global debug manager helper object that stores configuration and state used during development */
var const DebugManager DebugManager;

// Color preferences.
var(Colors) color
	C_WorldBox,
	C_GroundPlane,
	C_GroundHighlight,
	C_BrushWire,
	C_Pivot,
	C_Select,
	C_Current,
	C_AddWire,
	C_SubtractWire,
	C_GreyWire,
	C_BrushVertex,
	C_BrushSnap,
	C_Invalid,
	C_ActorWire,
	C_ActorHiWire,
	C_Black,
	C_White,
	C_Mask,
	C_SemiSolidWire,
	C_NonSolidWire,
	C_WireBackground,
	C_WireGridAxis,
	C_ActorArrow,
	C_ScaleBox,
	C_ScaleBoxHi,
	C_ZoneWire,
	C_OrthoBackground,
	C_Volume,
	C_ConstraintLine,
	C_AnimMesh,
	C_TerrainWire;

/** Fudge factor for tweaking the distance based miplevel determination */
var(Settings)	float			StreamingDistanceFactor;

/** Mapping between PhysicalMaterial class name and physics-engine specific MaterialIndex. */
var	native const pointer		MaterialMap;

/** Class name of the scout to use for path building */
var const config string ScoutClassName;

defaultproperties
{
	TinyFont=Font'EngineFonts.TinyFont'
	SmallFont=Font'EngineFonts.SmallFont'
	MediumFont=Font'EngineFonts.MediumFont'
	LargeFont=Font'EngineFonts.LargeFont'
	UseSound=True
	C_WorldBox=(R=0,G=0,B=107,A=255)
	C_GroundPlane=(R=0,G=0,B=63,A=255)
	C_GroundHighlight=(R=0,G=0,B=127,A=255)
	C_BrushWire=(R=192,G=0,B=0,A=255)
	C_Pivot=(R=0,G=255,B=0,A=255)
	C_Select=(R=0,G=0,B=127,A=255)
	C_AddWire=(R=127,G=127,B=255,A=255)
	C_SubtractWire=(R=255,G=192,B=63,A=255)
	C_GreyWire=(R=163,G=163,B=163,A=255)
	C_Invalid=(R=163,G=163,B=163,A=255)
	C_ActorWire=(R=127,G=63,B=0,A=255)
	C_ActorHiWire=(R=255,G=127,B=0,A=255)
	C_White=(R=255,G=255,B=255,A=255)
	C_SemiSolidWire=(R=127,G=255,B=0,A=255)
	C_NonSolidWire=(R=63,G=192,B=32,A=255)
	C_WireGridAxis=(R=119,G=119,B=119,A=255)
	C_ActorArrow=(R=163,G=0,B=0,A=255)
	C_ScaleBox=(R=151,G=67,B=11,A=255)
	C_ScaleBoxHi=(R=223,G=149,B=157,A=255)
	C_OrthoBackground=(R=163,G=163,B=163,A=255)
	C_Current=(R=0,G=0,B=0,A=255)
	C_BrushVertex=(R=0,G=0,B=0,A=255)
	C_BrushSnap=(R=0,G=0,B=0,A=255)
	C_Black=(R=0,G=0,B=0,A=255)
	C_Mask=(R=0,G=0,B=0,A=255)
	C_WireBackground=(R=0,G=0,B=0,A=255)
	C_ZoneWire=(R=0,G=0,B=0,A=255)
	C_Volume=(R=255,G=196,B=225,A=255)
	C_AnimMesh=(R=221,G=221,B=28,A=255)
	C_ConstraintLine=(R=0,G=255,B=0,A=255)
	C_TerrainWire=(R=255,G=255,B=255,A=255)
	ScoutClassName="Engine.Scout"
	UseStreaming=False
	MinStreamedInMips=5
	MaxStreamedInMips=14
}
