class MaterialExpression extends Object within Material
	native
	abstract;

struct ExpressionInput
{
	var MaterialExpression	Expression;
	var int					Mask,
							MaskR,
							MaskG,
							MaskB,
							MaskA;
};

var int		EditorX,
			EditorY;

var bool			Compiling;		// Protects against cyclic expression connections.
var transient bool	PendingKill;	// Used to mark an expression which is about to be deleted so connections to this expression can be cleaned up.
var array<string>	Errors;

var() string		Desc;			// A description that level designers can add (shows in the material editor UI)

cpptext
{
	// UObject interface.

	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual UBOOL IsPendingKill() {return PendingKill;}

	// UMaterialExpression interface.

	virtual INT Compile(FMaterialCompiler* Compiler) { return INDEX_NONE; }
	virtual void GetOutputs(TArray<FExpressionOutput>& Outputs) const;
	virtual INT GetWidth() const;
	virtual INT GetHeight() const;
	virtual UBOOL UsesLeftGutter() const;
	virtual UBOOL UsesRightGutter() const;
	virtual FString GetCaption() const;
	virtual INT GetLabelPadding() { return 0; }
}