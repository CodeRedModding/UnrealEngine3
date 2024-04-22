//! @file SubstanceAirGraphInstance.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief the interface to access a Substance Air Graph Instance

class SubstanceAirGraphInstance extends Object
	native(Texture)
	hidecategories(Object);

enum SubstanceAirInputType
{
	SIT_Float    , /**< = 0x0, Float (scalar) type */
	SIT_Float2   , /**< = 0x1, 2D Float (vector) type */
	SIT_Float3   , /**< = 0x2, 3D Float (vector) type */
	SIT_Float4   , /**< = 0x3, 4D Float (vector) type (e.g. color) */
	SIT_Integer  , /**< = 0x4, Integer type (int 32bits, enum or bool) */
	SIT_Image    , /**< = 0x5, bitmap/texture data */
	SIT_Unused_6 , /** adding some padding in the enum to match the native one*/
	SIT_Unused_7 , /** adding some padding in the enum to match the native one*/
	SIT_Integer2 , /**< = 0x8, 2D Integer (vector) type */
	SIT_Integer3 , /**< = 0x9, 3D Integer (vector) type */
	SIT_Integer4 , /**< = 0xA, 4D Integer (vector) type */	
};

// Substance graph instance, owned by this object
var native pointer Instance{struct SubstanceAir::FGraphInstance};

// Substance Air Instance factory parent object
var native SubstanceAirInstanceFactory Parent;


// retrieve the input list
native final function array<string> GetInputNames();

// retrieve an input's type
native final function SubstanceAirInputType GetInputType(const string InputName);

// modify the input value of the specified input (by name)
native final function bool SetInputInt(const string InputName, const array<int> Value);
native final function bool SetInputFloat(const string InputName, const array<float> Value);

// modify an image input, object must be a SubstanceAirImageInput or a SubstanceAirTexture2D
native final function bool SetInputImg(const string InputName, Object Value);

// get the input value of the specified input (by name)
native final function array<int> GetInputInt(const string InputName);
native final function array<float> GetInputFloat(const string InputName);
native final function Object GetInputImg(const string InputName);

cpptext
{
public:
	virtual void InitializeIntrinsicPropertyValues();
	virtual void Serialize(FArchive& Ar);
	virtual void BeginDestroy();
	virtual void PostLoad();
	virtual void PostDuplicate();
	virtual void PreEditUndo();
	virtual void PostEditUndo();
}
