/**************************************************************************

Filename    :   GamePad.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

package scaleform.gfx
{   
    public final class GamePad
    {
        // The following constants will be filled in appropriately by 
        // Scaleform at runtime. Flash has no concept of a GamePad and
        // therefore the constants (and this class) has no use other
        // than for compiler sanity during content creation.
        
        public static const PAD_NONE    = 0;
        public static const PAD_BACK    = 0;
        public static const PAD_START   = 0;
        public static const PAD_A       = 0;
        public static const PAD_B       = 0;
        public static const PAD_X       = 0;
        public static const PAD_Y       = 0;
        public static const PAD_R1      = 0;  // RightShoulder;
        public static const PAD_L1      = 0;  // LeftShoulder;
        public static const PAD_R2      = 0;  // RightTrigger;
        public static const PAD_L2      = 0;  // LeftTrigger;
        public static const PAD_UP      = 0;
        public static const PAD_DOWN    = 0;
        public static const PAD_RIGHT   = 0;
        public static const PAD_LEFT    = 0;
        public static const PAD_PLUS    = 0;
        public static const PAD_MINUS   = 0;
        public static const PAD_1       = 0;
        public static const PAD_2       = 0;
        public static const PAD_H       = 0;
        public static const PAD_C       = 0;
        public static const PAD_Z       = 0;
        public static const PAD_O       = 0;
        public static const PAD_T       = 0;
        public static const PAD_S       = 0;
        public static const PAD_SELECT  = 0;
        public static const PAD_HOME    = 0;
        public static const PAD_RT      = 0;   // RightThumb;
        public static const PAD_LT      = 0;   // LeftThumb;
        
        // Will return true if the current platform supports emitting analog events
        // for specific game pad controls (such as RT - right thumb, etc.)
        public static function supportsAnalogEvents():Boolean    { return false; }
    }
}