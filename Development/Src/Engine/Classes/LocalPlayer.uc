//=============================================================================
// LocalPlayer
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class LocalPlayer extends Player
	config(Engine)
	native
	noexport;

// Internal.
var native const noexport pointer	vfViewportClient;

var native const pointer			Viewport,
									MasterViewport;

var native const array<string>		EnabledStats;
var native const int				ViewMode;
var native const int				ShowFlags;

var native const int				UseAutomaticBrightness;

var native const pointer			ViewState;