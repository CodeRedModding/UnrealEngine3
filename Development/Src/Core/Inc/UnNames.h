/*=============================================================================
	UnNames.h: Header file registering global hardcoded Unreal names.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Macros.
-----------------------------------------------------------------------------*/

/** index of highest hardcoded name to be replicated by index by the networking code
 * @warning: changing this number or making any change to the list of hardcoded names with index
 * less than this value breaks network compatibility (update GEngineMinNetVersion)
 * @note: names with a greater value than this can still be replicated, but they are sent as
 * strings instead of an efficient index
 */
#define MAX_NETWORKED_HARDCODED_NAME 1250

// Define a message as an enumeration.
#ifndef REGISTER_NAME
	#define REGISTER_NAME(num,name) NAME_##name = num,
	#define REGISTERING_ENUM
	enum EName {
#endif

/*-----------------------------------------------------------------------------
	Hardcoded names which are not messages.
-----------------------------------------------------------------------------*/

// Special zero value, meaning no name.
REGISTER_NAME(   0, None             )

// Class property types
REGISTER_NAME(   1, ByteProperty     )
REGISTER_NAME(   2, IntProperty      )
REGISTER_NAME(   3, BoolProperty     )
REGISTER_NAME(   4, FloatProperty    )
REGISTER_NAME(   5, ObjectProperty   )
REGISTER_NAME(   6, NameProperty     )
REGISTER_NAME(   7, DelegateProperty )
REGISTER_NAME(   8, ClassProperty    )
REGISTER_NAME(   9, ArrayProperty    )
REGISTER_NAME(  10, StructProperty   )
REGISTER_NAME(  11, VectorProperty   )
REGISTER_NAME(  12, RotatorProperty  )
REGISTER_NAME(  13, StrProperty      )
REGISTER_NAME(  14, MapProperty      )
REGISTER_NAME(	15,	InterfaceProperty)

// Packages.
REGISTER_NAME(  20, Core			 )
REGISTER_NAME(  21, Engine			 )
REGISTER_NAME(  22, Editor           )
REGISTER_NAME(  23, Gameplay         )

// UnrealScript types.
REGISTER_NAME(  80, Byte		     )
REGISTER_NAME(  81, Int			     )
REGISTER_NAME(  82, Bool		     )
REGISTER_NAME(  83, Float		     )
REGISTER_NAME(  84, Name		     )
REGISTER_NAME(  85, String		     )
REGISTER_NAME(  86, Struct			 )
REGISTER_NAME(  87, Vector		     )
REGISTER_NAME(  88, Rotator	         )
REGISTER_NAME(  89, SHVector         )
REGISTER_NAME(  90, Color            )
REGISTER_NAME(  91, Plane            )
REGISTER_NAME(  92, Button           )
REGISTER_NAME(  94, Matrix			 )
REGISTER_NAME(	95,	LinearColor		 )
REGISTER_NAME(	96, QWord			 )
REGISTER_NAME(	97, Pointer			 )
REGISTER_NAME(	98, Double			 )
REGISTER_NAME(	99, Quat			 )
REGISTER_NAME(1250, Vector4          )

// Keywords.
REGISTER_NAME( 100, Begin			 )
REGISTER_NAME( 102, State            )
REGISTER_NAME( 103, Function         )
REGISTER_NAME( 104, Self             )
REGISTER_NAME( 105, TRUE             )
REGISTER_NAME( 106, FALSE            )
REGISTER_NAME( 107, Transient        )
REGISTER_NAME( 117, Enum			 )
REGISTER_NAME( 119, Replication      )
REGISTER_NAME( 120, Reliable         )
REGISTER_NAME( 121, Unreliable       )
REGISTER_NAME( 122, Always           )
REGISTER_NAME( 123, Server			 )
REGISTER_NAME( 124, Client			 )

// Object class names.
REGISTER_NAME( 150, Field            )
REGISTER_NAME( 151, Object           )
REGISTER_NAME( 152, TextBuffer       )
REGISTER_NAME( 153, Linker           )
REGISTER_NAME( 154, LinkerLoad       )
REGISTER_NAME( 155, LinkerSave       )
REGISTER_NAME( 156, Subsystem        )
REGISTER_NAME( 157, Factory          )
REGISTER_NAME( 158, TextBufferFactory)
REGISTER_NAME( 159, Exporter         )
REGISTER_NAME( 160, StackNode        )
REGISTER_NAME( 161, Property         )
REGISTER_NAME( 162, Camera           )
REGISTER_NAME( 163, PlayerInput      )
REGISTER_NAME( 164, Actor			 )
REGISTER_NAME( 165, ObjectRedirector )
REGISTER_NAME( 166, ObjectArchetype  )

// Constants.
REGISTER_NAME( 600, Vect)
REGISTER_NAME( 601, Rot)
REGISTER_NAME( 605, ArrayCount)
REGISTER_NAME( 606, EnumCount)
REGISTER_NAME( 607, Rng)
REGISTER_NAME( 608, NameOf)

// Flow control.
REGISTER_NAME( 620, Else)
REGISTER_NAME( 621, If)
REGISTER_NAME( 622, Goto)
REGISTER_NAME( 623, Stop)
REGISTER_NAME( 625, Until)
REGISTER_NAME( 626, While)
REGISTER_NAME( 627, Do)
REGISTER_NAME( 628, Break)
REGISTER_NAME( 629, For)
REGISTER_NAME( 630, ForEach)
REGISTER_NAME( 631, Assert)
REGISTER_NAME( 632, Switch)
REGISTER_NAME( 633, Case)
REGISTER_NAME( 634, Default)
REGISTER_NAME( 635, Continue)
REGISTER_NAME( 636, ElseIf)
REGISTER_NAME( 637, FilterEditorOnly)

// Variable overrides.
REGISTER_NAME( 640, Private)
REGISTER_NAME( 641, Const)
REGISTER_NAME( 642, Out)
REGISTER_NAME( 643, Export)
REGISTER_NAME( 644, DuplicateTransient )
REGISTER_NAME( 645, NoImport )
REGISTER_NAME( 646, Skip)
REGISTER_NAME( 647, Coerce)
REGISTER_NAME( 648, Optional)
REGISTER_NAME( 649, Input)
REGISTER_NAME( 650, Config)
REGISTER_NAME( 651, EditorOnly)
REGISTER_NAME( 652, NotForConsole)
REGISTER_NAME( 653, EditConst)
REGISTER_NAME( 654, Localized)
REGISTER_NAME( 655, GlobalConfig)
REGISTER_NAME( 656, SafeReplace)
REGISTER_NAME( 657, New)
REGISTER_NAME( 658, Protected)
REGISTER_NAME( 659, Public)
REGISTER_NAME( 660, EditInline)
REGISTER_NAME( 661, EditInlineUse)
REGISTER_NAME( 662, Deprecated)
REGISTER_NAME( 663, Atomic)
REGISTER_NAME( 664, Immutable)
REGISTER_NAME( 665, Automated )
REGISTER_NAME( 666, RepNotify )
REGISTER_NAME( 667, Interp )
REGISTER_NAME( 668, NoClear )
REGISTER_NAME( 669, NonTransactional )
REGISTER_NAME( 670, EditFixedSize)
REGISTER_NAME( 940, ImmutableWhenCooked )
REGISTER_NAME( 941, RepRetry )
REGISTER_NAME( 942, PrivateWrite )
REGISTER_NAME( 943, ProtectedWrite )
REGISTER_NAME( 944, EditHide)
REGISTER_NAME( 945, EditTextBox)

// Class overrides.
REGISTER_NAME( 671, Intrinsic)
REGISTER_NAME( 672, Within)
REGISTER_NAME( 673, Abstract)
REGISTER_NAME( 674, Package)
REGISTER_NAME( 675, Guid)
REGISTER_NAME( 676, Parent)
REGISTER_NAME( 677, Class)
REGISTER_NAME( 678, Extends)
REGISTER_NAME( 679, NoExport)
REGISTER_NAME( 680, Placeable)
REGISTER_NAME( 681, PerObjectConfig)
REGISTER_NAME( 682, NativeReplication)
REGISTER_NAME( 683, NotPlaceable)
REGISTER_NAME( 684,	EditInlineNew)
REGISTER_NAME( 685,	NotEditInlineNew)
REGISTER_NAME( 686,	HideCategories)
REGISTER_NAME( 687,	ShowCategories)
REGISTER_NAME( 688,	CollapseCategories)
REGISTER_NAME( 689,	DontCollapseCategories)
REGISTER_NAME( 698, DependsOn)
REGISTER_NAME( 699, HideDropDown)

REGISTER_NAME( 950, Implements )
REGISTER_NAME( 951, Interface )
REGISTER_NAME( 952, Inherits)
REGISTER_NAME( 953, PerObjectLocalized)
REGISTER_NAME( 954, NonTransient)
REGISTER_NAME( 955, Archetype)

REGISTER_NAME( 956, StrictConfig )
REGISTER_NAME( 957, UnusedStructKeyword1 )
REGISTER_NAME( 958, UnusedStructKeyword2 )
REGISTER_NAME( 959, SerializeText)
REGISTER_NAME( 960, CrossLevel)
REGISTER_NAME( 961, CrossLevelActive)
REGISTER_NAME( 962, CrossLevelPassive)
REGISTER_NAME( 963, ClassGroup)

// State overrides.
REGISTER_NAME( 690, Auto)
REGISTER_NAME( 691, Ignores)

// Auto-instanced subobjects
REGISTER_NAME( 692, Instanced)

// Components.
REGISTER_NAME( 693, Component)
REGISTER_NAME( 694, Components)

// Calling overrides.
REGISTER_NAME( 695, Global)
REGISTER_NAME( 696, Super)
REGISTER_NAME( 697, Outer)

// Function overrides.
REGISTER_NAME( 700, Operator)
REGISTER_NAME( 701, PreOperator)
REGISTER_NAME( 702, PostOperator)
REGISTER_NAME( 703, Final)
REGISTER_NAME( 704, Iterator)
REGISTER_NAME( 705, Latent)
REGISTER_NAME( 706, Return)
REGISTER_NAME( 707, Singular)
REGISTER_NAME( 708, Simulated)
REGISTER_NAME( 709, Exec)
REGISTER_NAME( 710, Event)
REGISTER_NAME( 711, Static)
REGISTER_NAME( 712, Native)
REGISTER_NAME( 713, Invariant)
REGISTER_NAME( 714, Delegate)
REGISTER_NAME( 715, Virtual)
REGISTER_NAME( 716, NoExportHeader)
REGISTER_NAME( 717, DLLImport)
REGISTER_NAME( 718, NativeOnly)
REGISTER_NAME( 719, UnusedFunctionKeyword3)

// Variable declaration.
REGISTER_NAME( 720, Var)
REGISTER_NAME( 721, Local)
REGISTER_NAME( 722, Import)
REGISTER_NAME( 723, From)

// Special commands.
REGISTER_NAME( 730, Spawn)
REGISTER_NAME( 731, Array)
REGISTER_NAME( 732, Map)
REGISTER_NAME( 733, AutoExpandCategories)
REGISTER_NAME( 734, AutoCollapseCategories)
REGISTER_NAME( 735, DontAutoCollapseCategories)
REGISTER_NAME( 736, DontSortCategories)

// Misc.
REGISTER_NAME( 740, Tag)
REGISTER_NAME( 742, Role)
REGISTER_NAME( 743, RemoteRole)
REGISTER_NAME( 744, System)
REGISTER_NAME( 745, User)
REGISTER_NAME( 746, PersistentLevel)
REGISTER_NAME( 747, TheWorld)
REGISTER_NAME( 748, Benchmark)

// Platforms
REGISTER_NAME( 750, Windows)
REGISTER_NAME( 751, XBox)
REGISTER_NAME( 752, PlayStation)
REGISTER_NAME( 753, Linux)
REGISTER_NAME( 754, MacOSX)
REGISTER_NAME( 755, PC)

// Log messages.
REGISTER_NAME( 756, DevDecals)
REGISTER_NAME( 757, PerfWarning)
REGISTER_NAME( 758, DevStreaming)
REGISTER_NAME( 759, DevLive)
REGISTER_NAME( 760, Log)
REGISTER_NAME( 761, Critical)
REGISTER_NAME( 762, Init)
REGISTER_NAME( 763, Exit)
REGISTER_NAME( 764, Cmd)
REGISTER_NAME( 765, Play)
REGISTER_NAME( 766, Console)
REGISTER_NAME( 767, Warning)
REGISTER_NAME( 768, ExecWarning)
REGISTER_NAME( 769, ScriptWarning)
REGISTER_NAME( 770, ScriptLog)
REGISTER_NAME( 771, Dev)
REGISTER_NAME( 772, DevNet)
REGISTER_NAME( 773, DevPath)
REGISTER_NAME( 774, DevNetTraffic)
REGISTER_NAME( 775, DevAudio)
REGISTER_NAME( 776, DevLoad)
REGISTER_NAME( 777, DevSave)
REGISTER_NAME( 778, DevGarbage)
REGISTER_NAME( 779, DevKill)
REGISTER_NAME( 780, DevReplace)
REGISTER_NAME( 781, DevUI)
REGISTER_NAME( 782, DevSound)
REGISTER_NAME( 783, DevCompile)
REGISTER_NAME( 784, DevBind)
REGISTER_NAME( 785, Localization)
REGISTER_NAME( 786, Compatibility)
REGISTER_NAME( 787, NetComeGo)
REGISTER_NAME( 788, Title)
REGISTER_NAME( 789, Error)
REGISTER_NAME( 790, Heading)
REGISTER_NAME( 791, SubHeading)
REGISTER_NAME( 792, FriendlyError)
REGISTER_NAME( 793, Progress)
REGISTER_NAME( 794, UserPrompt)
REGISTER_NAME( 795, SourceControl)
REGISTER_NAME( 796, DevPhysics)
REGISTER_NAME( 797, DevTick)
REGISTER_NAME( 798, DevStats)
REGISTER_NAME( 799, DevComponents)
REGISTER_NAME( 809, DevMemory)
REGISTER_NAME( 810, XMA)
REGISTER_NAME( 811, WAV)
REGISTER_NAME( 812, AILog)
REGISTER_NAME( 813, DevParticle)
REGISTER_NAME( 814, PerfEvent )
REGISTER_NAME( 815, LocalizationWarning )
REGISTER_NAME( 816, DevUIStyles )
REGISTER_NAME( 817, DevUIStates )
REGISTER_NAME( 818, DevUIFocus )
REGISTER_NAME( 819, ParticleWarn )
REGISTER_NAME( 854, UTrace )
REGISTER_NAME( 855, DevCollision )
REGISTER_NAME( 856, DevSHA )
REGISTER_NAME( 857, DevSpawn )
REGISTER_NAME( 858, DevAnim )
REGISTER_NAME( 859, Hack ) // useful for putting this where we have have hacks so we can see often they are spamming out
REGISTER_NAME( 1118, DevShaders )
REGISTER_NAME( 1119, DevDataBase )
REGISTER_NAME( 1120, DevDataStore )
REGISTER_NAME( 1121, DevAudioVerbose )
REGISTER_NAME( 1125, DevUIAnimation )
REGISTER_NAME( 1126, DevHDDCaching )
REGISTER_NAME( 1127, DevMovie )
REGISTER_NAME( 1128, DevShadersDetailed )
REGISTER_NAME( 1129, PlayerManagement )		// log messages related to creating and removing local players
REGISTER_NAME( 1130, DevPatch)		// log messages related to script patching and other console patching
REGISTER_NAME( 1131, DevLightmassSolver )
REGISTER_NAME( 1132, DevAssetDataBase )
REGISTER_NAME( 200, GameStats ) //log messages related to the recording of game stats
REGISTER_NAME( 201, DevFaceFX ) // log messages related to FaceFX
REGISTER_NAME( 202, DevCrossLevel ) // log messages relating to cross level references
REGISTER_NAME( 203, DevConfig ) // log messages relating to cross level references
REGISTER_NAME( 204, DevCamera ) // log messages relating to camera
REGISTER_NAME( 205, DebugState )
REGISTER_NAME( 206, DevAbsorbFuncs ) // log messages relating to absorbing non-simulated functions on clients
REGISTER_NAME( 207, DevLevelTools ) // log messages relating to level management tools
REGISTER_NAME( 208, DevGFxUI ) // log messages relating to the GFx UI
REGISTER_NAME( 209, DevGFxUIWarning ) // Warning log messages relating to the GFx UI
REGISTER_NAME( 210, DevNavMesh) // log messages relating to NavMesh
REGISTER_NAME( 211, DevNavMeshWarning ) // Warning log messages relating to NavMesh
REGISTER_NAME( 212, DevMCP ) // log messages relating to MCP/LSP tasks
REGISTER_NAME( 213, DevBeacon ) // log messages for beacon functionality
REGISTER_NAME( 214, DevHTTP ) // log messages for http requests
REGISTER_NAME( 215, DevMovieCapture ) // log messages for movie capture requests
REGISTER_NAME( 216, DevHttpRequest ) // log messages for HttpRequest diagnostics
REGISTER_NAME( 217, DevNetTrafficDetail) // log extended net traffic (keeps NetTraffic usable)

// Console text colors.
REGISTER_NAME( 800, White)
REGISTER_NAME( 801, Black)
REGISTER_NAME( 802, Red)
REGISTER_NAME( 803, Green)
REGISTER_NAME( 804, Blue)
REGISTER_NAME( 805, Cyan)
REGISTER_NAME( 806, Magenta)
REGISTER_NAME( 807, Yellow)
REGISTER_NAME( 808, DefaultColor)

// Misc.
REGISTER_NAME( 820, KeyType)
REGISTER_NAME( 821, KeyEvent)
REGISTER_NAME( 822, Write)
REGISTER_NAME( 823, Message)
REGISTER_NAME( 824, InitialState)
REGISTER_NAME( 825, Texture)
REGISTER_NAME( 826, Sound)
REGISTER_NAME( 827, FireTexture)
REGISTER_NAME( 828, IceTexture)
REGISTER_NAME( 829, WaterTexture)
REGISTER_NAME( 830, WaveTexture)
REGISTER_NAME( 831, WetTexture)
REGISTER_NAME( 832, Main)
REGISTER_NAME( 834, VideoChange)
REGISTER_NAME( 835, SendText)
REGISTER_NAME( 836, SendBinary)
REGISTER_NAME( 837, ConnectFailure)
REGISTER_NAME( 838, Length)
REGISTER_NAME( 839, Insert)
REGISTER_NAME( 840, Remove)
REGISTER_NAME( 1200, Add)
REGISTER_NAME( 1201, AddItem)
REGISTER_NAME( 1202, RemoveItem)
REGISTER_NAME( 1203, InsertItem)
REGISTER_NAME( 1204, Sort)
REGISTER_NAME( 841, Game)
REGISTER_NAME( 842, SequenceObjects)
REGISTER_NAME( 843, PausedState)
REGISTER_NAME( 844, ContinuedState)
REGISTER_NAME( 845, SelectionColor)
REGISTER_NAME( 846, Find)
REGISTER_NAME( 847, UI)
REGISTER_NAME( 848, DevCooking)
REGISTER_NAME( 849, DevOnline)
REGISTER_NAME( 850, DataBinding )
REGISTER_NAME( 851, OptionMusic)
REGISTER_NAME( 852, OptionSFX)
REGISTER_NAME( 853, OptionVoice)
REGISTER_NAME( 1122, Linear )
REGISTER_NAME( 1123, Point )
REGISTER_NAME( 1124, Aniso )
REGISTER_NAME( 860, Master )
REGISTER_NAME( 861, Ambient )
REGISTER_NAME( 862, UnGrouped )
REGISTER_NAME( 863, VoiceChat )
REGISTER_NAME( 1208, Brush )

/** Virtual data store names */
REGISTER_NAME( 865, Attributes)	// virtual data store for specifying text attributes (italic, bold, underline, etc.)
REGISTER_NAME( 866, Strings )	// virtual data store for looking up localized string values
REGISTER_NAME( 867, Images)		// virtual data store for specifying direct object references
REGISTER_NAME( 868, SceneData)	// virtual data store for per-scene data stores

// Script Preprocessor
REGISTER_NAME( 870, EndIf)
REGISTER_NAME( 871, Include)
REGISTER_NAME( 872, Define)
REGISTER_NAME( 873, Undefine)
REGISTER_NAME( 874, IsDefined)
REGISTER_NAME( 875, NotDefined)
REGISTER_NAME( 876, Debug)
REGISTER_NAME( 877, Counter)
REGISTER_NAME( 878, SetCounter)
REGISTER_NAME( 879, GetCounter)
REGISTER_NAME( 880, EngineVersion)
REGISTER_NAME( 881, IfCondition)

//@compatibility - class names that have property remapping (reserve 900 - 999)
REGISTER_NAME( 900, FontCharacter )
REGISTER_NAME( 901,	SourceState )
REGISTER_NAME( 902, InitChild2StartBone )
REGISTER_NAME( 903, SourceStyle )
REGISTER_NAME( 904, SoundCueLocalized )
REGISTER_NAME( 905, SoundCue )
REGISTER_NAME( 906, RawDistributionFloat )
REGISTER_NAME( 907, RawDistributionVector )
REGISTER_NAME( 908, UIDockingSet )
REGISTER_NAME( 909, DockPadding )
REGISTER_NAME( 912, ScaleType )
REGISTER_NAME( 913, EvalType)
REGISTER_NAME( 914, AutoSizePadding )

// PlayerController states added for efficient replication
REGISTER_NAME( 915, PlayerWalking )
REGISTER_NAME( 916, PlayerClimbing )
REGISTER_NAME( 917, PlayerDriving )
REGISTER_NAME( 918, PlayerSwimming )
REGISTER_NAME( 919, PlayerFlying )
REGISTER_NAME( 920, Spectating )
REGISTER_NAME( 921, PlayerWaiting )
REGISTER_NAME( 922, WaitingForPawn )
REGISTER_NAME( 923, RoundEnded )
REGISTER_NAME( 924, Dead )

// Game specific Logging Categories
REGISTER_NAME( 1000, Gear_General )

REGISTER_NAME( 1001, Gear_ActiveReload )
REGISTER_NAME( 1002, Gear_MiniGames )
REGISTER_NAME( 1003, Gear_ResurrectionSystem )
REGISTER_NAME( 1004, Gear_VehicleSystem )
REGISTER_NAME( 1005, Gear_CheckpointSystem )
REGISTER_NAME( 1006, Cover )
REGISTER_NAME( 1007, AICommand )
REGISTER_NAME( 1008, Success )
REGISTER_NAME( 1009, Failure )
REGISTER_NAME( 1010, Aborted )
REGISTER_NAME( 1011, PlayerTakingCover )
REGISTER_NAME( 1012, Engaging )
REGISTER_NAME( 1013, PlayerTurreting )
REGISTER_NAME( 1014, Reviving )


REGISTER_NAME( 1080, PlayerID )
REGISTER_NAME( 1081, TeamID )
REGISTER_NAME( 1082, HearSoundFinished )
REGISTER_NAME( 1083, OnParticleSystemFinished )

// Needed for post time/locale deletion to export with proper case
REGISTER_NAME( 1100, Time )
// Post processing volume support.
REGISTER_NAME( 1101, PPVolume_BloomEffect )
REGISTER_NAME( 1102, PPVolume_DOFEffect )
REGISTER_NAME( 1103, PPVolume_MotionBlurEffect )
REGISTER_NAME( 1104, PPVolume_SceneEffect )
REGISTER_NAME( 1105, PPVolume_DOFAndBloomEffect )
REGISTER_NAME( 1106, Desaturation )
REGISTER_NAME( 1107, HighLights )
REGISTER_NAME( 1108, MidTones )
REGISTER_NAME( 1109, Shadows )
REGISTER_NAME( 1110, PPVolume_UberPostProcessEffect )

// Script uses both cases which causes *Classes.h problems
REGISTER_NAME( 1111, DeviceID )

// Needed for interpolation curve fixes.
REGISTER_NAME( 1112, InterpCurveFloat )
REGISTER_NAME( 1113, InterpCurveVector2D )
REGISTER_NAME( 1114, InterpCurveVector )
REGISTER_NAME( 1115, InterpCurveTwoVectors )
REGISTER_NAME( 1116, InterpCurveQuat )

REGISTER_NAME( 1117, UniqueNetId )

REGISTER_NAME( 1133, PopUp )

REGISTER_NAME( 1134, Team )

REGISTER_NAME( 1135, DevDlc )

// For Landscape DebugColorMode
REGISTER_NAME( 1140, Landscape_RedTexture )
REGISTER_NAME( 1141, Landscape_GreenTexture )
REGISTER_NAME( 1142, Landscape_BlueTexture )
REGISTER_NAME( 1143, Landscape_RedMask )
REGISTER_NAME( 1144, Landscape_GreenMask )
REGISTER_NAME( 1145, Landscape_BlueMask )

// Mobile material parameter group names
REGISTER_NAME( 1150, Base )
REGISTER_NAME( 1151, Specular )
REGISTER_NAME( 1152, Emissive )
REGISTER_NAME( 1153, Environment )
REGISTER_NAME( 1154, RimLighting )
REGISTER_NAME( 1155, BumpOffset )
REGISTER_NAME( 1156, Masking )
REGISTER_NAME( 1157, TextureBlending )
REGISTER_NAME( 1158, ColorBlending )
REGISTER_NAME( 1159, TextureTransform )
REGISTER_NAME( 1160, VertexAnimation )

// Mobile material parameter names
REGISTER_NAME( 1165, MobileSpecularPower )
REGISTER_NAME( 1166, MobileEnvironmentAmount )
REGISTER_NAME( 1167, MobileEnvironmentFresnelAmount )
REGISTER_NAME( 1168, MobileEnvironmentFresnelExponent )
REGISTER_NAME( 1169, MobileRimLightingStrength )
REGISTER_NAME( 1170, MobileRimLightingExponent )
REGISTER_NAME( 1171, MobileBumpOffsetReferencePlane )
REGISTER_NAME( 1172, MobileBumpOffsetHeightRatio )
REGISTER_NAME( 1173, MobileTransformCenterX )
REGISTER_NAME( 1174, MobileTransformCenterY )
REGISTER_NAME( 1175, MobilePannerSpeedX )
REGISTER_NAME( 1176, MobilePannerSpeedY )
REGISTER_NAME( 1177, MobileRotateSpeed )
REGISTER_NAME( 1178, MobileFixedScaleX )
REGISTER_NAME( 1179, MobileFixedScaleY )
REGISTER_NAME( 1180, MobileSineScaleX )
REGISTER_NAME( 1181, MobileSineScaleY )
REGISTER_NAME( 1182, MobileSineScaleFrequencyMultipler )
REGISTER_NAME( 1183, MobileFixedOffsetX )
REGISTER_NAME( 1184, MobileFixedOffsetY )
REGISTER_NAME( 1185, MobileTangentVertexFrequencyMultiplier )
REGISTER_NAME( 1186, MobileVerticalFrequencyMultiplier )
REGISTER_NAME( 1187, MobileMaxVertexMovementAmplitude )
REGISTER_NAME( 1188, MobileSwayFrequencyMultiplier )
REGISTER_NAME( 1189, MobileSwayMaxAngle )
REGISTER_NAME( 1190, MobileSpecularColor )
REGISTER_NAME( 1191, MobileEmissiveColor )
REGISTER_NAME( 1192, MobileEnvironmentColor )
REGISTER_NAME( 1193, MobileRimLightingColor )
REGISTER_NAME( 1194, MobileDefaultUniformColor )
REGISTER_NAME( 1195, MobileOpacityMultiplier )

REGISTER_NAME( 1230, MobileBaseTexture )
REGISTER_NAME( 1231, MobileNormalTexture )
REGISTER_NAME( 1232, MobileEmissiveTexture )
REGISTER_NAME( 1233, MobileMaskTexture )
REGISTER_NAME( 1234, MobileDetailTexture )
REGISTER_NAME( 1235, MobileDetailTexture2 )
REGISTER_NAME( 1236, MobileDetailTexture3 )
REGISTER_NAME( 1237, MobileEnvironmentTexture )

// Fortress specific names

REGISTER_NAME( 1056, Fortress_MCP )


/*-----------------------------------------------------------------------------
	Special engine-generated probe messages.
-----------------------------------------------------------------------------*/

//
//warning: All probe entries must be filled in, otherwise non-probe names might be mapped
// to probe name indices.
//
#define NAME_PROBEMIN ((EName)300)
#define NAME_PROBEMAX ((EName)332)

// Creation and destruction.
REGISTER_NAME( 300, Destroyed        ) // Called immediately before actor is removed from actor list.

// Gaining/losing actors.
REGISTER_NAME( 301, GainedChild		 ) // Sent to a parent actor when another actor attaches to it.
REGISTER_NAME( 302, LostChild		 ) // Sent to a parent actor when another actor detaches from it.

// Physics & world interaction.
REGISTER_NAME( 303, HitWall			 ) // Ran into a wall.
REGISTER_NAME( 304, Falling			 ) // Actor is falling.
REGISTER_NAME( 305, Landed			 ) // Actor has landed.
REGISTER_NAME( 306, Touch			 ) // Actor was just touched by another actor.
REGISTER_NAME( 307, UnTouch			 ) // Actor touch just ended, always sent sometime after Touch.
REGISTER_NAME( 308, Bump			 ) // Actor was just touched and blocked. No interpenetration. No UnBump.
REGISTER_NAME( 309, BeginState		 ) // Just entered a new state.
REGISTER_NAME( 310, EndState		 ) // About to leave the current state.
REGISTER_NAME( 311, BaseChange		 ) // Sent to actor when its floor changes.
REGISTER_NAME( 312, Attach			 ) // Sent to actor when it's stepped on by another actor.
REGISTER_NAME( 313, Detach			 ) // Sent to actor when another actor steps off it. 
REGISTER_NAME( 314, EncroachingOn    ) // Encroaching on another actor.
REGISTER_NAME( 315, EncroachedBy     ) // Being encroached by another actor.
REGISTER_NAME( 316, MayFall 		 )

// Updates.
REGISTER_NAME( 317, Tick			 ) // Clock tick update for nonplayer.

// AI.
REGISTER_NAME( 318, SeePlayer        ) // Can see player.
REGISTER_NAME( 319, EnemyNotVisible  ) // Current Enemy is not visible.
REGISTER_NAME( 320, HearNoise        ) // Noise nearby.
REGISTER_NAME( 321, UpdateEyeHeight  ) // Update eye level (after physics).
REGISTER_NAME( 322, SeeMonster       ) // Can see non-player.
REGISTER_NAME( 323, SpecialHandling	 ) // Navigation point requests special handling.
REGISTER_NAME( 324, BotDesireability ) // Value of this inventory to bot.

// Controller notifications
REGISTER_NAME( 325, NotifyBump		 )
REGISTER_NAME( 326, NotifyLanded	 )
REGISTER_NAME( 327, NotifyHitWall	 )
REGISTER_NAME( 328, PreBeginPlay	 )
REGISTER_NAME( 329, PostBeginPlay	 )

REGISTER_NAME( 330, UnusedProbe		 )

// Special tag meaning 'All probes'.
REGISTER_NAME( 331, All				 ) // Special meaning, not a message.

REGISTER_NAME(398, PoppedState)
REGISTER_NAME(399, PushedState)

// Misc. Make sure this starts after NAME_ProbeMax
REGISTER_NAME( 400, MeshEmitterVertexColor )
REGISTER_NAME( 401, TextureOffsetParameter )
REGISTER_NAME( 402, TextureScaleParameter )
REGISTER_NAME( 403, ImpactVel )
REGISTER_NAME( 404, SlideVel )
REGISTER_NAME( 405, TextureOffset1Parameter )
REGISTER_NAME( 406, MeshEmitterDynamicParameter )
REGISTER_NAME( 407, ExpressionInput )

REGISTER_NAME(1205, OnAudioFinished )

REGISTER_NAME( 1206, ForceScriptOrder)
REGISTER_NAME( 1207, Mobile)
REGISTER_NAME( 1209, Untitled)
REGISTER_NAME( 1210, Timer)
REGISTER_NAME( 1211, PS3 )


/*-----------------------------------------------------------------------------
	Closing.
-----------------------------------------------------------------------------*/

#ifdef REGISTERING_ENUM
	};
	#undef REGISTER_NAME
	#undef REGISTERING_ENUM
#endif

