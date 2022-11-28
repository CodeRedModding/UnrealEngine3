class EditorViewportInput extends Input
	transient
	config(Input)
	native;

var EditorEngine	Editor;

cpptext
{
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);
}