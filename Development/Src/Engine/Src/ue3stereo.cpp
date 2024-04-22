
#include "EnginePrivate.h"
#include "ScenePrivate.h"

#define NO_STEREO_D3D9 1
#define NO_STEREO_D3D11 1
#include "ue3stereo.h"
#undef NO_STEREO_D3D9
#undef NO_STEREO_D3D11

#if !CONSOLE && !PLATFORM_MACOSX
namespace nv {
	namespace stereo {
	    bool GIsStereoEnabled = false;
	    
		bool IsStereoEnabled()
		{
		    static bool bFirstTime = true;
		    
		    if (bFirstTime) 
		    {
			    NvU8 stereoEnabled = 0;
			    // The call to NvAPI_Stereo_IsEnabled can be incredibly slow, so only make the call once and
			    // use the result for the rest of the application run. The side-effect to this is that 
			    // IsActivated needs to check IsEnabled now.
			    if (!GAllowNvidiaStereo3d || (NVAPI_OK != NvAPI_Stereo_IsEnabled(&stereoEnabled))) {
				    GIsStereoEnabled = false;
			    } else {
			        GIsStereoEnabled = (stereoEnabled != 0);
			    }
			    
			    if (!GIsStereoEnabled) { 
			        // Forcible disable to ensure that it doesn't turn on later during the application run
			        NvAPI_Stereo_Disable();
			    }
			    
			    bFirstTime = false;
			}

			return GIsStereoEnabled;
		}
	}
}
#endif
