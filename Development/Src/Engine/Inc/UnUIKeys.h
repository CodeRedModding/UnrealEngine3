/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


#ifndef DEFINE_UIKEY
	#define DEFINE_UIKEY(Name) extern FName UIKEY_##Name;
#endif

// ScreenObject
DEFINE_UIKEY(Consume);	// Consumes any bound input without executing a action.

DEFINE_UIKEY(NextControl);
DEFINE_UIKEY(PrevControl);

DEFINE_UIKEY(NavFocusUp);
DEFINE_UIKEY(NavFocusDown);
DEFINE_UIKEY(NavFocusLeft);
DEFINE_UIKEY(NavFocusRight);


// UIObject
DEFINE_UIKEY(ShowContextMenu);
DEFINE_UIKEY(HideContextMenu);

// Scene
DEFINE_UIKEY(CloseScene);

// Listbox
DEFINE_UIKEY(MoveSelectionUp);
DEFINE_UIKEY(MoveSelectionDown);
DEFINE_UIKEY(MoveSelectionLeft);
DEFINE_UIKEY(MoveSelectionRight);
DEFINE_UIKEY(SelectFirstElement);
DEFINE_UIKEY(SelectLastElement);
DEFINE_UIKEY(SelectAllItems);
DEFINE_UIKEY(PageUp);
DEFINE_UIKEY(PageDown);
DEFINE_UIKEY(SubmitListSelection);
DEFINE_UIKEY(ResizeColumn);

// Button/List
DEFINE_UIKEY(Clicked);

// EditBox
DEFINE_UIKEY(Char);
DEFINE_UIKEY(Backspace);
DEFINE_UIKEY(DeleteCharacter);
DEFINE_UIKEY(MoveCursorLeft);
DEFINE_UIKEY(MoveCursorRight);
DEFINE_UIKEY(MoveCursorToLineStart);
DEFINE_UIKEY(MoveCursorToLineEnd);
DEFINE_UIKEY(SubmitText);
DEFINE_UIKEY(MouseSelect);

// Slider
DEFINE_UIKEY(DragSlider);
DEFINE_UIKEY(IncrementSliderValue);
DEFINE_UIKEY(DecrementSliderValue);

// Numeric EditBox
DEFINE_UIKEY(IncrementNumericValue);
DEFINE_UIKEY(DecrementNumericValue);

// Tab Control
DEFINE_UIKEY(NextPage)
DEFINE_UIKEY(PreviousPage)


// UIScrollFrame
DEFINE_UIKEY(ScrollUp)
DEFINE_UIKEY(ScrollDown)
DEFINE_UIKEY(ScrollLeft)
DEFINE_UIKEY(ScrollRight)
DEFINE_UIKEY(ScrollTop)
DEFINE_UIKEY(ScrollBottom)

#undef DEFINE_UIKEY

