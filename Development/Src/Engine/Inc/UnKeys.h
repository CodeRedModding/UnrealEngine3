/*=============================================================================
	UnKeys.h: Input key definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#ifndef DEFINE_KEY
#define DEFINE_KEY(Name) extern FName KEY_##Name;
#endif

DEFINE_KEY(MouseX);
DEFINE_KEY(MouseY);
DEFINE_KEY(MouseScrollUp);
DEFINE_KEY(MouseScrollDown);

DEFINE_KEY(LeftMouseButton);
DEFINE_KEY(RightMouseButton);
DEFINE_KEY(MiddleMouseButton);

DEFINE_KEY(BackSpace);
DEFINE_KEY(Tab);
DEFINE_KEY(Enter);
DEFINE_KEY(Pause);

DEFINE_KEY(CapsLock);
DEFINE_KEY(Escape);
DEFINE_KEY(SpaceBar);
DEFINE_KEY(PageUp);
DEFINE_KEY(PageDown);
DEFINE_KEY(End);
DEFINE_KEY(Home);

DEFINE_KEY(Left);
DEFINE_KEY(Up);
DEFINE_KEY(Right);
DEFINE_KEY(Down);

DEFINE_KEY(Insert);
DEFINE_KEY(Delete);

DEFINE_KEY(Zero);
DEFINE_KEY(One);
DEFINE_KEY(Two);
DEFINE_KEY(Three);
DEFINE_KEY(Four);
DEFINE_KEY(Five);
DEFINE_KEY(Six);
DEFINE_KEY(Seven);
DEFINE_KEY(Eight);
DEFINE_KEY(Nine);

DEFINE_KEY(A);
DEFINE_KEY(B);
DEFINE_KEY(C);
DEFINE_KEY(D);
DEFINE_KEY(E);
DEFINE_KEY(F);
DEFINE_KEY(G);
DEFINE_KEY(H);
DEFINE_KEY(I);
DEFINE_KEY(J);
DEFINE_KEY(K);
DEFINE_KEY(L);
DEFINE_KEY(M);
DEFINE_KEY(N);
DEFINE_KEY(O);
DEFINE_KEY(P);
DEFINE_KEY(Q);
DEFINE_KEY(R);
DEFINE_KEY(S);
DEFINE_KEY(T);
DEFINE_KEY(U);
DEFINE_KEY(V);
DEFINE_KEY(W);
DEFINE_KEY(X);
DEFINE_KEY(Y);
DEFINE_KEY(Z);

DEFINE_KEY(NumPadZero);
DEFINE_KEY(NumPadOne);
DEFINE_KEY(NumPadTwo);
DEFINE_KEY(NumPadThree);
DEFINE_KEY(NumPadFour);
DEFINE_KEY(NumPadFive);
DEFINE_KEY(NumPadSix);
DEFINE_KEY(NumPadSeven);
DEFINE_KEY(NumPadEight);
DEFINE_KEY(NumPadNine);

DEFINE_KEY(Multiply);
DEFINE_KEY(Add);
DEFINE_KEY(Subtract);
DEFINE_KEY(Decimal);
DEFINE_KEY(Divide);

DEFINE_KEY(F1);
DEFINE_KEY(F2);
DEFINE_KEY(F3);
DEFINE_KEY(F4);
DEFINE_KEY(F5);
DEFINE_KEY(F6);
DEFINE_KEY(F7);
DEFINE_KEY(F8);
DEFINE_KEY(F9);
DEFINE_KEY(F10);
DEFINE_KEY(F11);
DEFINE_KEY(F12);

DEFINE_KEY(NumLock);

DEFINE_KEY(ScrollLock);

DEFINE_KEY(LeftShift);
DEFINE_KEY(RightShift);
DEFINE_KEY(LeftControl);
DEFINE_KEY(RightControl);
DEFINE_KEY(LeftAlt);
DEFINE_KEY(RightAlt);

DEFINE_KEY(Semicolon);
DEFINE_KEY(Equals);
DEFINE_KEY(Comma);
DEFINE_KEY(Underscore);
DEFINE_KEY(Period);
DEFINE_KEY(Slash);
DEFINE_KEY(Tilde);
DEFINE_KEY(LeftBracket);
DEFINE_KEY(Backslash);
DEFINE_KEY(RightBracket);
DEFINE_KEY(Quote);

DEFINE_KEY(XboxTypeS_LeftX);
DEFINE_KEY(XboxTypeS_LeftY);
DEFINE_KEY(XboxTypeS_RightX);
DEFINE_KEY(XboxTypeS_RightY);

DEFINE_KEY(XboxTypeS_LeftThumbstick);
DEFINE_KEY(XboxTypeS_RightThumbstick);
DEFINE_KEY(XboxTypeS_Back);
DEFINE_KEY(XboxTypeS_Start);
DEFINE_KEY(XboxTypeS_A);
DEFINE_KEY(XboxTypeS_B);
DEFINE_KEY(XboxTypeS_X);
DEFINE_KEY(XboxTypeS_Y);
DEFINE_KEY(XboxTypeS_White);
DEFINE_KEY(XboxTypeS_Black);
DEFINE_KEY(XboxTypeS_LeftTrigger);
DEFINE_KEY(XboxTypeS_RightTrigger);
DEFINE_KEY(XboxTypeS_DPad_Up);
DEFINE_KEY(XboxTypeS_DPad_Down);
DEFINE_KEY(XboxTypeS_DPad_Right);
DEFINE_KEY(XboxTypeS_DPad_Left);

#undef DEFINE_KEY