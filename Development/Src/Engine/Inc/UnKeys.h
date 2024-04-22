/*=============================================================================
	UnKeys.h: Input key definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef DEFINE_KEY
#define DEFINE_KEY(Name, Unused) extern FName KEY_##Name;
#endif

DEFINE_KEY(MouseX, SIE_Axis);
DEFINE_KEY(MouseY, SIE_Axis);
DEFINE_KEY(MouseScrollUp, SIE_PressedOnly);
DEFINE_KEY(MouseScrollDown, SIE_PressedOnly);

DEFINE_KEY(LeftMouseButton, SIE_MouseButton);
DEFINE_KEY(RightMouseButton, SIE_MouseButton);
DEFINE_KEY(MiddleMouseButton, SIE_MouseButton);
DEFINE_KEY(ThumbMouseButton, SIE_MouseButton);
DEFINE_KEY(ThumbMouseButton2, SIE_MouseButton);

DEFINE_KEY(BackSpace, SIE_Keyboard);
DEFINE_KEY(Tab, SIE_Keyboard);
DEFINE_KEY(Enter, SIE_Keyboard);
DEFINE_KEY(Pause, SIE_Keyboard);

DEFINE_KEY(CapsLock, SIE_Keyboard);
DEFINE_KEY(Escape, SIE_Keyboard);
DEFINE_KEY(SpaceBar, SIE_Keyboard);
DEFINE_KEY(PageUp, SIE_Keyboard);
DEFINE_KEY(PageDown, SIE_Keyboard);
DEFINE_KEY(End, SIE_Keyboard);
DEFINE_KEY(Home, SIE_Keyboard);

DEFINE_KEY(Left, SIE_Keyboard);
DEFINE_KEY(Up, SIE_Keyboard);
DEFINE_KEY(Right, SIE_Keyboard);
DEFINE_KEY(Down, SIE_Keyboard);

DEFINE_KEY(Insert, SIE_Keyboard);
DEFINE_KEY(Delete, SIE_Keyboard);

DEFINE_KEY(Zero, SIE_Keyboard);
DEFINE_KEY(One, SIE_Keyboard);
DEFINE_KEY(Two, SIE_Keyboard);
DEFINE_KEY(Three, SIE_Keyboard);
DEFINE_KEY(Four, SIE_Keyboard);
DEFINE_KEY(Five, SIE_Keyboard);
DEFINE_KEY(Six, SIE_Keyboard);
DEFINE_KEY(Seven, SIE_Keyboard);
DEFINE_KEY(Eight, SIE_Keyboard);
DEFINE_KEY(Nine, SIE_Keyboard);

DEFINE_KEY(A, SIE_Keyboard);
DEFINE_KEY(B, SIE_Keyboard);
DEFINE_KEY(C, SIE_Keyboard);
DEFINE_KEY(D, SIE_Keyboard);
DEFINE_KEY(E, SIE_Keyboard);
DEFINE_KEY(F, SIE_Keyboard);
DEFINE_KEY(G, SIE_Keyboard);
DEFINE_KEY(H, SIE_Keyboard);
DEFINE_KEY(I, SIE_Keyboard);
DEFINE_KEY(J, SIE_Keyboard);
DEFINE_KEY(K, SIE_Keyboard);
DEFINE_KEY(L, SIE_Keyboard);
DEFINE_KEY(M, SIE_Keyboard);
DEFINE_KEY(N, SIE_Keyboard);
DEFINE_KEY(O, SIE_Keyboard);
DEFINE_KEY(P, SIE_Keyboard);
DEFINE_KEY(Q, SIE_Keyboard);
DEFINE_KEY(R, SIE_Keyboard);
DEFINE_KEY(S, SIE_Keyboard);
DEFINE_KEY(T, SIE_Keyboard);
DEFINE_KEY(U, SIE_Keyboard);
DEFINE_KEY(V, SIE_Keyboard);
DEFINE_KEY(W, SIE_Keyboard);
DEFINE_KEY(X, SIE_Keyboard);
DEFINE_KEY(Y, SIE_Keyboard);
DEFINE_KEY(Z, SIE_Keyboard);

DEFINE_KEY(NumPadZero, SIE_Keyboard);
DEFINE_KEY(NumPadOne, SIE_Keyboard);
DEFINE_KEY(NumPadTwo, SIE_Keyboard);
DEFINE_KEY(NumPadThree, SIE_Keyboard);
DEFINE_KEY(NumPadFour, SIE_Keyboard);
DEFINE_KEY(NumPadFive, SIE_Keyboard);
DEFINE_KEY(NumPadSix, SIE_Keyboard);
DEFINE_KEY(NumPadSeven, SIE_Keyboard);
DEFINE_KEY(NumPadEight, SIE_Keyboard);
DEFINE_KEY(NumPadNine, SIE_Keyboard);

DEFINE_KEY(Multiply, SIE_Keyboard);
DEFINE_KEY(Add, SIE_Keyboard);
DEFINE_KEY(Subtract, SIE_Keyboard);
DEFINE_KEY(Decimal, SIE_Keyboard);
DEFINE_KEY(Divide, SIE_Keyboard);

DEFINE_KEY(F1, SIE_Keyboard);
DEFINE_KEY(F2, SIE_Keyboard);
DEFINE_KEY(F3, SIE_Keyboard);
DEFINE_KEY(F4, SIE_Keyboard);
DEFINE_KEY(F5, SIE_Keyboard);
DEFINE_KEY(F6, SIE_Keyboard);
DEFINE_KEY(F7, SIE_Keyboard);
DEFINE_KEY(F8, SIE_Keyboard);
DEFINE_KEY(F9, SIE_Keyboard);
DEFINE_KEY(F10, SIE_Keyboard);
DEFINE_KEY(F11, SIE_Keyboard);
DEFINE_KEY(F12, SIE_Keyboard);

DEFINE_KEY(NumLock, SIE_Keyboard);

DEFINE_KEY(ScrollLock, SIE_Keyboard);

DEFINE_KEY(LeftShift, SIE_Keyboard);
DEFINE_KEY(RightShift, SIE_Keyboard);
DEFINE_KEY(LeftControl, SIE_Keyboard);
DEFINE_KEY(RightControl, SIE_Keyboard);
DEFINE_KEY(LeftAlt, SIE_Keyboard);
DEFINE_KEY(RightAlt, SIE_Keyboard);

DEFINE_KEY(Semicolon, SIE_Keyboard);
DEFINE_KEY(Equals, SIE_Keyboard);
DEFINE_KEY(Comma, SIE_Keyboard);
DEFINE_KEY(Underscore, SIE_Keyboard);
DEFINE_KEY(Period, SIE_Keyboard);
DEFINE_KEY(Slash, SIE_Keyboard);
DEFINE_KEY(Tilde, SIE_Keyboard);
DEFINE_KEY(LeftBracket, SIE_Keyboard);
DEFINE_KEY(Backslash, SIE_Keyboard);
DEFINE_KEY(RightBracket, SIE_Keyboard);
DEFINE_KEY(Quote, SIE_Keyboard);

DEFINE_KEY(XboxTypeS_LeftX, SIE_Axis);
DEFINE_KEY(XboxTypeS_LeftY, SIE_Axis);
DEFINE_KEY(XboxTypeS_RightX, SIE_Axis);
DEFINE_KEY(XboxTypeS_RightY, SIE_Axis);
DEFINE_KEY(XboxTypeS_LeftTriggerAxis, SIE_Axis);
DEFINE_KEY(XboxTypeS_RightTriggerAxis, SIE_Axis);

DEFINE_KEY(GameCaster_LeftThumbX, SIE_Axis);
DEFINE_KEY(GameCaster_LeftThumbY, SIE_Axis);
DEFINE_KEY(GameCaster_RightThumb, SIE_Axis);
DEFINE_KEY(GameCaster_RollAxis,   SIE_Axis);
DEFINE_KEY(GameCaster_PitchAxis,  SIE_Axis);
DEFINE_KEY(GameCaster_YawAxis,    SIE_Axis);
DEFINE_KEY(GameCaster_Zoom,    SIE_Axis);

DEFINE_KEY(XboxTypeS_LeftThumbstick, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_RightThumbstick, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_Back, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_Start, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_A, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_B, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_X, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_Y, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_LeftShoulder, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_RightShoulder, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_LeftTrigger, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_RightTrigger, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_DPad_Up, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_DPad_Down, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_DPad_Right, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_DPad_Left, SIE_Keyboard);
DEFINE_KEY(XboxTypeS_BigButton, SIE_Keyboard);

// Virtual key codes used for input axis button press/release emulation
DEFINE_KEY(Gamepad_LeftStick_Up, SIE_Keyboard);
DEFINE_KEY(Gamepad_LeftStick_Down, SIE_Keyboard);
DEFINE_KEY(Gamepad_LeftStick_Right, SIE_Keyboard);
DEFINE_KEY(Gamepad_LeftStick_Left, SIE_Keyboard);

DEFINE_KEY(Gamepad_RightStick_Up, SIE_Keyboard);
DEFINE_KEY(Gamepad_RightStick_Down, SIE_Keyboard);
DEFINE_KEY(Gamepad_RightStick_Right, SIE_Keyboard);
DEFINE_KEY(Gamepad_RightStick_Left, SIE_Keyboard);

// This special key code is used by the UI system to indicate that the widget wishes to respond to InputChar events
DEFINE_KEY(Character, SIE_Keyboard);

// Vector axes (FVector, not FLOAT)
DEFINE_KEY(Tilt, SIE_Axis);
DEFINE_KEY(RotationRate, SIE_Axis);
DEFINE_KEY(Gravity, SIE_Axis);
DEFINE_KEY(Acceleration, SIE_Axis);

// Special PS3 axes
DEFINE_KEY(SIXAXIS_AccelX, SIE_Axis);
DEFINE_KEY(SIXAXIS_AccelY, SIE_Axis);
DEFINE_KEY(SIXAXIS_AccelZ, SIE_Axis);
DEFINE_KEY(SIXAXIS_Gyro, SIE_Axis);

// Special NGP controls
DEFINE_KEY(NGP_LeftX, SIE_Axis);
DEFINE_KEY(NGP_LeftY, SIE_Axis);
DEFINE_KEY(NGP_RightX, SIE_Axis);
DEFINE_KEY(NGP_RightY, SIE_Axis);

DEFINE_KEY(NGP_Triangle, SIE_Keyboard);
DEFINE_KEY(NGP_Square, SIE_Keyboard);
DEFINE_KEY(NGP_X, SIE_Keyboard);
DEFINE_KEY(NGP_Circle, SIE_Keyboard);
DEFINE_KEY(NGP_Start, SIE_Keyboard);
DEFINE_KEY(NGP_Select, SIE_Keyboard);
DEFINE_KEY(NGP_DPad_Up, SIE_Keyboard);
DEFINE_KEY(NGP_DPad_Down, SIE_Keyboard);
DEFINE_KEY(NGP_DPad_Left, SIE_Keyboard);
DEFINE_KEY(NGP_DPad_Right, SIE_Keyboard);
DEFINE_KEY(NGP_LeftShoulder, SIE_Keyboard);
DEFINE_KEY(NGP_RightShoulder, SIE_Keyboard);

// Special WiiU controls
DEFINE_KEY(WiiU_PointerX, SIE_Axis);
DEFINE_KEY(WiiU_PointerY, SIE_Axis);

#undef DEFINE_KEY
