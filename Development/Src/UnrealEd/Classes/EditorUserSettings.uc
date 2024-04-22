/**
 * This class handles hotkey binding management for the editor.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class EditorUserSettings extends Object
	hidecategories(Object)
	config(EditorUserSettings)
	native;

enum WASDType
{
	WASD_Always,
	WASD_RMBOnly,
	WASD_Never,
};

/** Whether to automatically save after a time interval */
var(Options) config bool bAutoSaveEnable;

/** Whether to automatically save maps during an autosave */
var(Options) config bool bAutoSaveMaps;

/** Whether to automatically save content packages during an autosave */
var(Options) config bool bAutoSaveContent;

/** The time interval after which to auto save */
var(Options) config int AutoSaveTimeMinutes;

/** Enable the use of flight camera controls under various circumstances */
var(Options) config WASDType FlightCameraControlType;

/** The background color for material preview thumbnails in Generic Browser  */
var(Options) config Color PreviewThumbnailBackgroundColor;

/** The background color for translucent material preview thumbnails in Generic Browser */
var(Options) config Color PreviewThumbnailTranslucentMaterialBackgroundColor;

/** Controls whether packages which are checked-out are automatically fully loaded at startup */
var(Options) config	bool bAutoloadCheckedOutPackages;

/** If this is true, the user will not be asked to fully load a package before saving or before creating a new object */
var(Options) config bool bSuppressFullyLoadPrompt;

/** True if user should be allowed to select translucent objects in perspective viewports */
var(Options) config bool bAllowSelectTranslucent;

/** True if Play In Editor should only load currently-visible levels in PIE */
var(Options) config bool bOnlyLoadVisibleLevelsInPIE;

/** True if ortho-viewport box selection requires objects to be fully encompassed by the selection box to be selected */
var(Options) config bool bStrictBoxSelection;

/** Whether to automatically prompt for SCC checkout on package modification */
var(Options) config bool bPromptForCheckoutOnPackageModification;

/** If true audio will be enabled in the editor. Does not affect PIE **/
var(Options) config bool bEnableRealTimeAudio;

/** Global volume setting for the editor */
var(Options) config float EditorVolumeLevel;

/** True if we should move actors to their appropriate grid volume levels immediately after most operations */
var(Options) config bool bUpdateActorsInGridLevelsImmediately;

/** True if we should automatically restart playback Flash Movies that are reimported in the editor */
var(Options) config bool bAutoRestartReimportedFlashMovies;

/** True if we should automatically reimport textures when a change to source content is detected*/
var(Options) config bool bAutoReimportTextures;

/** True if we should automatically reimport apex assets when a change to source content is detected*/
var(Options) config bool bAutoReimportApexAssets;

/** True if we should automatically reimport animset assets when a change to source content is detected*/
var(Options) config bool bAutoReimportAnimSets;

/** If checked all orthographic viewports are linked to the same position and move together */
var(Options) config bool bUseLinkedOrthographicViewports;

/** If checked all show flags are more easily available in a menu straight off the viewport toolbar. */
var(Options) config bool bEnableShowFlagsShortcut;

/** If true perspective viewports will default to realtime mode. */
var(Options) config bool bStartInRealtimeMode;

/** How to constrain perspective viewport FOV */ 
var(Options) config EAspectRatioAxisConstraint AspectRatioAxisConstraint;
/** Whether to load a simple example map at startup */ 
var(Options) config bool bLoadSimpleLevelAtStartup;

/** Enables real-time hover feedback when mousing over objects in editor viewports */
var(Options) config bool bEnableViewportHoverFeedback;

/** Enables the editor perspective camera to be dropped at the last PlayInViewport cam position */
var(Options) config bool bEnableViewportCameraToUpdateFromPIV;

/** Toggles emulation of mobile input and rendering on PC (enables touch-based input, disables gamma correction, certain post-processes, directional light maps, etc.) */
var(Options) config bool bEmulateMobileFeatures;

/** When enabled, forces all content to be optimized for mobile platforms when possible (e.g. compress PVRTCs, flatten textures, etc) */
var(Options) config bool bAlwaysOptimizeContentForMobile;

/** List of packages to autosave if bAutoSaveContent is true, if none specified will saved all */
var(Options) config array<string> PackagesToSave;