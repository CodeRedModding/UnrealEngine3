/*=============================================================================
	TwoDeeShapeEditor : 2D Shape Editor

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

class F2DSEVector : public FVector
{
public:
	F2DSEVector();
	F2DSEVector( FLOAT x, FLOAT y, FLOAT z );
	~F2DSEVector();

	inline F2DSEVector operator=( F2DSEVector Other )
	{
		X = Other.X;
		Y = Other.Y;
		Z = Other.Z;
		bSelected = Other.bSelected;
		return *this;
	}

	inline F2DSEVector operator=( FVector Other )
	{
		X = Other.X;
		Y = Other.Y;
		Z = Other.Z;
		return *this;
	}

	inline UBOOL operator!=( F2DSEVector Other )
	{
		return (X != Other.X && Y != Other.Y && Z != Other.Z);
	}

	inline UBOOL operator==( F2DSEVector Other )
	{
		return (X == Other.X && Y == Other.Y && Z == Other.Z);
	}

	void SelectToggle();
	void Select( BOOL bSel );
	BOOL IsSel( void );
	void SetTempPos();

	FLOAT TempX, TempY;

private:
	UBOOL bSelected;
};

enum ESegType {
	ST_LINEAR	= 0,
	ST_BEZIER	= 1
};

class FSegment
{
public:
	FSegment();
	FSegment( F2DSEVector vtx1, F2DSEVector vtx2 );
	FSegment( FVector vtx1, FVector vtx2 );
	~FSegment();

	inline FSegment operator=( FSegment Other )
	{
		Vertex[0] = Other.Vertex[0];
		Vertex[1] = Other.Vertex[1];
		ControlPoint[0] = Other.ControlPoint[0];
		ControlPoint[1] = Other.ControlPoint[1];
		return *this;
	}
	inline UBOOL operator==( FSegment Other )
	{
		return( Vertex[0] == Other.Vertex[0] && Vertex[1] == Other.Vertex[1] );
	}

	FVector GetHalfwayPoint();
	void GenerateControlPoint();
	void SetSegType( INT InType );
	UBOOL IsSel();
	void GetBezierPoints( TArray<FVector>* pBezierPoints );

	F2DSEVector Vertex[2], ControlPoint[2];
	INT SegType,		// eSEGTYPE_
		DetailLevel;	// Detail of bezier curve
	UBOOL bUsed;
};

class FShape
{
public:
	FShape();
	~FShape();

	F2DSEVector Handle;
	TArray<FSegment> Segments;
	TArray<FVector> Verts;

	void ComputeHandlePosition();
	void Breakdown( F2DSEVector InOrigin, FPolyBreaker* InBreaker );
};

#define d2dSE_SELECT_TOLERANCE 4

// --------------------------------------------------------------
//
// W2DSHAPEEDITOR
//
// --------------------------------------------------------------

class W2DShapeEditor : public WWindow
{
	DECLARE_WINDOWCLASS(W2DShapeEditor,WWindow,Window)

	// Structors.
	W2DShapeEditor( FName InPersistentName, WWindow* InOwnerWindow );

	WToolTip* ToolTipCtrl;
	HWND hWndToolBar;
	FVector m_camera;			// The viewing camera position
	F2DSEVector m_origin;		// The origin point used for revolves and such
	FPoint BoxSelStart, BoxSelEnd;
	BOOL m_bDraggingCamera, m_bDraggingVerts, m_bBoxSel, m_bMouseHasMoved;
	FPoint m_pointOldPos;
	TCHAR MapFilename[512];
	HBITMAP hImage;
	RECT m_rcWnd;
	POINT m_ContextPos;
	INT Zoom;

	TArray<FShape> m_shapes;

	void OpenWindow();
	void OnDestroy();
	INT OnSetCursor();
	void OnCreate();
	void OnPaint();
	void ScaleShapes( FLOAT InScale );
	void OnCommand( INT Command );
	void OnRightButtonDown();
	void OnRightButtonUp();
	void GetSegmentDetails( int* InSegType, int* InDetailLevel );
	void OnMouseMove( DWORD Flags, FPoint MouseLocation );
	void ComputeHandlePositions( UBOOL bAlwaysCompute = FALSE );
	void ChangeSegmentTypes( INT InType );
	void SetSegmentDetail( INT InDetailLevel );
	void OnLeftButtonDown();
	void OnLeftButtonUp();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	void PositionChildControls( void );
	virtual void OnKeyDown( TCHAR Ch );
	void RotateShape( INT _Angle );
	void Flip( BOOL _bHoriz );
	void DeselectAllVerts();
	FPoint ToWorld( FPoint InPoint );
	BOOL ProcessHits( BOOL bJustChecking, BOOL _bCumulative );
	BOOL IsInsideBox( F2DSEVector InVtx );
	void ProcessBoxSelHits( BOOL _bCumulative );
	void New( void );
	void InsertNewShape();
	void DrawGrid( HDC _hdc );
	void DrawImage( HDC _hdc );
	void DrawOrigin( HDC _hdc );
	void DrawSegments( HDC _hdc );
	void DrawShapeHandles( HDC _hdc );
	void DrawVertices( HDC _hdc );
	void DrawBoxSel( HDC _hdc );
	void SplitSides( void );
	void Delete( void );
	void FileSaveAs( HWND hWnd );
	void FileSave( HWND hWnd );
	void FileOpen( HWND hWnd );
	void SetCaption( void );
	void WriteShape( TCHAR* Filename );
	void ReadShape( TCHAR* Filename );
	void SetOrigin( void );
	void RotateBuilderBrush( INT Axis );
	void ProcessSheet();
	void ProcessExtrude( INT Depth );
	void ProcessExtrudeToPoint( INT Depth );
	void ProcessExtrudeToBevel( INT Depth, INT CapHeight );
	void ProcessRevolve( INT TotalSides, INT Sides );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
