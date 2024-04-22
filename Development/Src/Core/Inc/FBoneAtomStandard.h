/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/******************************************************************************
 ******************************************************************************

     NOTE: If you modify this file, please also modify FBoneAtomVectorized.h!

 *****************************************************************************
 *****************************************************************************/

#define FBONEATOM_TRACK_NAN_ISSUES 0

/** 
 * FBoneAtom class for Quat/Translation/Scale.
 *
 */
MS_ALIGN(16) class FBoneAtom
{
protected:
	FQuat	Rotation;
	FVector	Translation;
	FLOAT	Scale;
public:
	/**
	 * The identity transformation (Rotation = FQuat::Identity, Translation = FVector::ZeroVector, Scale = 1.0f)
	 */
	static const FBoneAtom Identity;

#if FBONEATOM_TRACK_NAN_ISSUES
	void DiagnosticCheckNaN_Scale() const
	{
		ensure(!(appIsNaN(Scale) || !appIsFinite(Scale)));
	}

	void DiagnosticCheckNaN_Translate() const
	{
		ensure(!(appIsNaN(Translation.X) || !appIsFinite(Translation.X) ||
			appIsNaN(Translation.Y) || !appIsFinite(Translation.Y) ||
			appIsNaN(Translation.Z) || !appIsFinite(Translation.Z)));
	}

	void DiagnosticCheckNaN_Rotate() const
	{
		ensure(!(appIsNaN(Rotation.X) || !appIsFinite(Rotation.X) ||
			appIsNaN(Rotation.Y) || !appIsFinite(Rotation.Y) ||
			appIsNaN(Rotation.Z) || !appIsFinite(Rotation.Z) ||
			appIsNaN(Rotation.W) || !appIsFinite(Rotation.W)));
	}

	void DiagnosticCheckNaN_All() const
	{
		DiagnosticCheckNaN_Scale();
		DiagnosticCheckNaN_Rotate();
		DiagnosticCheckNaN_Translate();
	}
#else
	FORCEINLINE void DiagnosticCheckNaN_Translate() const {}
	FORCEINLINE void DiagnosticCheckNaN_Rotate() const {}
	FORCEINLINE void DiagnosticCheckNaN_Scale() const {}
	FORCEINLINE void DiagnosticCheckNaN_All() const {}
#endif

	/**
	 * Constructor with no components initialized (garbage values)
	 */
	FORCEINLINE FBoneAtom()
	{
		// Note: This can be used to track down initialization issues with bone atom arrays; but it will
		// cause issues with transient fields such as RootMotionDelta that get initialized to 0 by default
#if 0
		float qnan = appLog2(-5.3f);
		check(appIsNaN(qnan));
		Translation = FVector(qnan, qnan, qnan);
		Rotation = FQuat(qnan, qnan, qnan, qnan);
		Scale = qnan;
#endif
	}

	/**
	 * Constructor with all components initialized
	 *
	 * @param InRotation The value to use for rotation component
	 * @param InTranslation The value to use for the translation component
	 * @param InScale The value to use for the scale component
	 */
	FORCEINLINE FBoneAtom(const FQuat& InRotation, const FVector& InTranslation, FLOAT InScale=1.f) 
		: Rotation(InRotation), 
			Translation(InTranslation), 
			Scale(InScale)
	{
		DiagnosticCheckNaN_All();
	}

	/**
	 * Constructor taking a FRotator as the rotation component
	 *
	 * @param InRotation The value to use for rotation component (after being converted to a quaternion)
	 * @param InTranslation The value to use for the translation component
	 * @param InScale The value to use for the scale component
	 */
	FORCEINLINE FBoneAtom(const FRotator& InRotation, const FVector& InTranslation, FLOAT InScale=1.f) 
		:	Rotation(InRotation), 
			Translation(InTranslation), 
			Scale(InScale)
	{
		DiagnosticCheckNaN_All();
	}

	/**
	 * Copy-constructor
	 *
	 * @param InBoneAtom The source atom from which all components will be copied
	 */
	FORCEINLINE FBoneAtom(const FBoneAtom& InBoneAtom) : 
		Rotation(InBoneAtom.Rotation), 
		Translation(InBoneAtom.Translation), 
		Scale(InBoneAtom.Scale)
	{
		DiagnosticCheckNaN_All();
	}

	/**
	* Constructor for converting a Matrix into a bone atom. InMatrix should not contain any scaling info.
	*/
	FORCEINLINE FBoneAtom(const FMatrix& InMatrix)
		:	Rotation( FQuat(InMatrix) )
		,	Translation( InMatrix.GetOrigin() )
		,	Scale(1.f)
	{
		DiagnosticCheckNaN_All();
	}

	/**
	* Does a debugf of the contents of this BoneAtom.
	*/
	void DebugPrint() const;

	FString ToString() const;

#ifdef IMPLEMENT_ASSIGNMENT_OPERATOR_MANUALLY
	/**
	* Copy another Atom into this one
	*/
	FORCEINLINE FBoneAtom& operator=(const FBoneAtom& Other)
	{
		this->Rotation = Other.Rotation;
		this->Translation = Other.Translation;
		this->Scale = Other.Scale;

		return *this;
	}
#endif

	/**
	* Convert this Atom to a transformation matrix.
	*/
	FORCEINLINE FMatrix ToMatrix() const
	{
		FMatrix OutMatrix;

#if !FINAL_RELEASE && !CONSOLE
		// Make sure Rotation is normalized when we turn it into a matrix.
		check( IsRotationNormalized() );
#endif
		OutMatrix.M[3][0] = Translation.X;
		OutMatrix.M[3][1] = Translation.Y;
		OutMatrix.M[3][2] = Translation.Z;

		const FLOAT x2 = Rotation.X + Rotation.X;	
		const FLOAT y2 = Rotation.Y + Rotation.Y;  
		const FLOAT z2 = Rotation.Z + Rotation.Z;
		{
			const FLOAT xx2 = Rotation.X * x2;
			const FLOAT yy2 = Rotation.Y * y2;			
			const FLOAT zz2 = Rotation.Z * z2;

			OutMatrix.M[0][0] = (1.0f - (yy2 + zz2)) * Scale;	
			OutMatrix.M[1][1] = (1.0f - (xx2 + zz2)) * Scale;
			OutMatrix.M[2][2] = (1.0f - (xx2 + yy2)) * Scale;
		}
		{
			const FLOAT yz2 = Rotation.Y * z2;   
			const FLOAT wx2 = Rotation.W * x2;	

			OutMatrix.M[2][1] = (yz2 - wx2) * Scale;
			OutMatrix.M[1][2] = (yz2 + wx2) * Scale;
		}
		{
			const FLOAT xy2 = Rotation.X * y2;
			const FLOAT wz2 = Rotation.W * z2;

			OutMatrix.M[1][0] = (xy2 - wz2) * Scale;
			OutMatrix.M[0][1] = (xy2 + wz2) * Scale;
		}
		{
			const FLOAT xz2 = Rotation.X * z2;
			const FLOAT wy2 = Rotation.W * y2;   

			OutMatrix.M[2][0] = (xz2 + wy2) * Scale;
			OutMatrix.M[0][2] = (xz2 - wy2) * Scale;
		}

		OutMatrix.M[0][3] = 0.0f;
		OutMatrix.M[1][3] = 0.0f;
		OutMatrix.M[2][3] = 0.0f;
		OutMatrix.M[3][3] = 1.0f;

		return OutMatrix;
	}

	/**
	* Convert this Atom to the 3x4 transpose of the transformation matrix.
	*/
	FORCEINLINE void To3x4MatrixTranspose( FLOAT* Out ) const
	{
		FMatrix Transform = ToMatrix();

		const FLOAT * RESTRICT Src = &(Transform.M[0][0]);
		FLOAT * RESTRICT Dest = Out;

		Dest[0] = Src[0];   // [0][0]
		Dest[1] = Src[4];   // [1][0]
		Dest[2] = Src[8];   // [2][0]
		Dest[3] = Src[12];  // [3][0]

		Dest[4] = Src[1];   // [0][1]
		Dest[5] = Src[5];   // [1][1]
		Dest[6] = Src[9];   // [2][1]
		Dest[7] = Src[13];  // [3][1]

		Dest[8] = Src[2];   // [0][2]
		Dest[9] = Src[6];   // [1][2]
		Dest[10] = Src[10]; // [2][2]
		Dest[11] = Src[14]; // [3][2]
	}		

	/** Set this atom to the weighted blend of the supplied two atoms. */
	FORCEINLINE void Blend(const FBoneAtom& Atom1, const FBoneAtom& Atom2, FLOAT Alpha)
	{
#if !FINAL_RELEASE && !CONSOLE
		// Check that all bone atoms coming from animation are normalized
		check( Atom1.IsRotationNormalized() );
		check( Atom2.IsRotationNormalized() );
#endif
		if( Alpha <= ZERO_ANIMWEIGHT_THRESH )
		{
			// if blend is all the way for child1, then just copy its bone atoms
			(*this) = Atom1;
		}
		else if( Alpha >= 1.f - ZERO_ANIMWEIGHT_THRESH )
		{
			// if blend is all the way for child2, then just copy its bone atoms
			(*this) = Atom2;
		}
		else
		{
			// Simple linear interpolation for translation and scale.
			Translation = Lerp(Atom1.Translation, Atom2.Translation, Alpha);
			Scale		= Lerp(Atom1.Scale, Atom2.Scale, Alpha);
			Rotation	= LerpQuat(Atom1.Rotation, Atom2.Rotation, Alpha);

			// ..and renormalize
			Rotation.Normalize();
		}
	}

	/** Set this atom to the weighted blend of the supplied two atoms. */
	FORCEINLINE void BlendWith(const FBoneAtom& OtherAtom, FLOAT Alpha)
	{
#if !FINAL_RELEASE && !CONSOLE
		// Check that all bone atoms coming from animation are normalized
		check( IsRotationNormalized() );
		check( OtherAtom.IsRotationNormalized() );
#endif
		if( Alpha > ZERO_ANIMWEIGHT_THRESH )
		{
			if( Alpha >= 1.f - ZERO_ANIMWEIGHT_THRESH )
			{
				// if blend is all the way for child2, then just copy its bone atoms
				(*this) = OtherAtom;
			}
			else 
			{
				// Simple linear interpolation for translation and scale.
				Translation = Lerp(Translation, OtherAtom.Translation, Alpha);
				Scale		= Lerp(Scale, OtherAtom.Scale, Alpha);
				Rotation	= LerpQuat(Rotation, OtherAtom.Rotation, Alpha);

				// ..and renormalize
				Rotation.Normalize();
			}
		}
	}

	/** 
	* For quaternions, delta angles is done by multiplying the conjugate.
	* Result is normalized.
	*/
	FORCEINLINE FBoneAtom operator-(const FBoneAtom& Atom) const
	{
		return FBoneAtom(Rotation * (-Atom.Rotation), Translation - Atom.Translation, Scale - Atom.Scale);
	}

	/**
	* Quaternion addition is wrong here. This is just a special case for linear interpolation.
	* Use only within blends!!
	* Rotation part is NOT normalized!!
	*/
	FORCEINLINE FBoneAtom operator+(const FBoneAtom& Atom) const
	{
		return FBoneAtom(Rotation + Atom.Rotation, Translation + Atom.Translation, Scale + Atom.Scale);
	}

	FORCEINLINE FBoneAtom& operator+=(const FBoneAtom& Atom)
	{
		Translation += Atom.Translation;

		Rotation.X += Atom.Rotation.X;
		Rotation.Y += Atom.Rotation.Y;
		Rotation.Z += Atom.Rotation.Z;
		Rotation.W += Atom.Rotation.W;

		Scale += Atom.Scale;

		return *this;
	}

	FORCEINLINE FBoneAtom operator*(ScalarRegister Mult) const
	{
		return FBoneAtom(Rotation * Mult, Translation * Mult, Scale * Mult);
	}

	FORCEINLINE FBoneAtom& operator*=(ScalarRegister Mult)
	{
		Translation *= Mult;
		Rotation.X	*= Mult;
		Rotation.Y	*= Mult;
		Rotation.Z	*= Mult;
		Rotation.W	*= Mult;
		Scale		*= Mult;

		return *this;
	}

	FORCEINLINE FBoneAtom		operator*(const FBoneAtom& Other) const;
	FORCEINLINE void			operator*=(const FBoneAtom& Other);
	FORCEINLINE FBoneAtom		operator*(const FQuat& Other) const;
	FORCEINLINE void			operator*=(const FQuat& Other);

	// new functions: mostly I keep same name with FMatrix to support both bone transform
	FORCEINLINE void ScaleTranslation(const FVector& Scale3D);
	FORCEINLINE void RemoveScaling(FLOAT Tolerance=SMALL_NUMBER);
	/** same version of FMatrix::GetMaximumAxisScale function **/
	/** @return the maximum magnitude of any row of the matrix. */
	FORCEINLINE FLOAT GetMaximumAxisScale() const;
	FORCEINLINE FBoneAtom	Inverse() const;
	FORCEINLINE FBoneAtom	InverseSafe() const;
	FORCEINLINE FVector4	TransformFVector4(const FVector4& V) const;
	FORCEINLINE FVector		TransformFVector(const FVector& V) const;


	/** Inverts the matrix and then transforms V - correctly handles scaling in this matrix. */
	FORCEINLINE FVector InverseTransformFVector(const FVector &V) const;

	FORCEINLINE FVector		TransformNormal(const FVector& V) const;
	/** 
	 *	Transform a direction vector by the inverse of this matrix - will not take into account translation part.
	 *	If you want to transform a surface normal (or plane) and correctly account for non-uniform scaling you should use TransformByUsingAdjointT with adjoint of matrix inverse.
	 */
	FORCEINLINE FVector InverseTransformNormal(const FVector &V) const;

	FORCEINLINE FBoneAtom	ApplyScale(FLOAT Scale) const;
	FORCEINLINE FVector		GetAxis(INT i) const;
	FORCEINLINE void		Mirror(BYTE MirrorAxis, BYTE FlipAxis);

	// temp function for easy conversion
	FORCEINLINE FVector GetOrigin() const
	{
		return Translation;
	}

	// temp function for easy conversion
	FORCEINLINE void SetOrigin(const FVector& Origin)
	{
		Translation = Origin;
		DiagnosticCheckNaN_Translate();
	}

	/**
	 * Checks the components for NaN's
	 * @return Returns true if any component (rotation, translation, or scale) is a NAN
	 */
	UBOOL ContainsNaN() const
	{
		if(appIsNaN(Rotation.X) || !appIsFinite(Rotation.X) ||
		   appIsNaN(Rotation.Y) || !appIsFinite(Rotation.Y) ||
		   appIsNaN(Rotation.Z) || !appIsFinite(Rotation.Z) ||
		   appIsNaN(Rotation.W) || !appIsFinite(Rotation.W))
		{
			return TRUE;
		}

		if(appIsNaN(Translation.X) || !appIsFinite(Translation.X) ||
		   appIsNaN(Translation.Y) || !appIsFinite(Translation.Y) ||
		   appIsNaN(Translation.Z) || !appIsFinite(Translation.Z))
		{
			return TRUE;
		}

		if(appIsNaN(Scale) || !appIsFinite(Scale))
		{
			return TRUE;
		}

		return FALSE;
	}

	// Serializer.
	inline friend FArchive& operator<<(FArchive& Ar,FBoneAtom& M)
	{
		Ar << M.Rotation;
		Ar << M.Translation;
		Ar << M.Scale;

		return Ar;
	}

	// Binary comparison operators.
/*
	UBOOL operator==( const FBoneAtom& Other ) const
	{
		return Rotation==Other.Rotation && Translation==Other.Translation && Scale==Other.Scale;
	}
	UBOOL operator!=( const FBoneAtom& Other ) const
	{
		return Rotation!=Other.Rotation || Translation!=Other.Translation || Scale!=Other.Scale;
	}

	inline UBOOL Equals(const FBoneAtom& Other, FLOAT Tolerance=KINDA_SMALL_NUMBER) const
	{
		return Rotation.Equals(Other.Rotation, Tolerance) && Translation.Equals(Other.Translation, Tolerance) && Abs(Scale-Other.Scale) < Tolerance;
	}
*/

	static FORCEINLINE void Multiply(FBoneAtom& OutBoneAtom, const FBoneAtom& A, FLOAT Mult);
	static FORCEINLINE void Multiply(FBoneAtom * OutBoneAtom, const FBoneAtom * A, const FBoneAtom * B);

	/**
	 * Sets the components
	 * @param InRotation The new value for the Rotation component
	 * @param InTranslation The new value for the Translation component
	 * @param InScale The new value for the Scale component
	 */
	FORCEINLINE void SetComponents(const FQuat& InRotation, const FVector& InTranslation, FLOAT InScale = 1.0f) 
	{
		Rotation = InRotation;
		Translation = InTranslation;
		Scale = InScale;

		DiagnosticCheckNaN_All();
	}

	/**
	 * Sets the components to the identity transform:
	 *   Rotation = (0,0,0,1)
	 *   Translation = (0,0,0)
	 *   Scale = 1
	 */
	FORCEINLINE void SetIdentity()
	{
		Rotation = FQuat::Identity;
		Translation	= FVector::ZeroVector;
		Scale = 1.0f;
	}

	/**
	 * Scales the scale component by a new factor
	 * @param ScaleMultiplier The value to multiply scale with
	 */
	FORCEINLINE void MultiplyScale(FLOAT ScaleMultiplier)
	{
		Scale *= ScaleMultiplier;
		DiagnosticCheckNaN_Scale();
	}

	/**
	 * Sets the translation component
	 * @param NewTranslation The new value for the translation component
	 */
	FORCEINLINE void SetTranslation(const FVector& NewTranslation)
	{
		Translation = NewTranslation;
		DiagnosticCheckNaN_Translate();
	}

	/**
	 * Concatenates another rotation to this transformation 
	 * @param DeltaRotation The rotation to concatenate in the following fashion: Rotation = Rotation * DeltaRotation
	 */
	FORCEINLINE void ConcatenateRotation(const FQuat& DeltaRotation)
	{
		Rotation = Rotation * DeltaRotation;
		DiagnosticCheckNaN_Rotate();
	}

	/**
	 * Adjusts the translation component of this transformation 
	 * @param DeltaTranslation The translation to add in the following fashion: Translation += DeltaTranslation
	 */
	FORCEINLINE void AddToTranslation(const FVector& DeltaTranslation)
	{
		Translation += DeltaTranslation;
		DiagnosticCheckNaN_Translate();
	}

	/**
	 * Sets the rotation component
	 * @param NewRotation The new value for the rotation component
	 */
	FORCEINLINE void SetRotation(const FQuat& NewRotation)
	{
		Rotation = NewRotation;
		DiagnosticCheckNaN_Rotate();
	}

	/**
	 * Sets the scale component
	 * @param NewScale The new value for the scale component
	 */
	FORCEINLINE void SetScale(const FLOAT NewScale)
	{
		Scale = NewScale;
		DiagnosticCheckNaN_Scale();
	}

	/**
	 * Sets both the translation and scale components at the same time
	 * @param NewTranslation The new value for the translation component
	 * @param NewScale The new value for the scale component
	 */
	FORCEINLINE void SetTranslationAndScale(const FVector& NewTranslation, FLOAT NewScale = 1.0f)
	{
		Translation = NewTranslation;
		Scale = NewScale;

		DiagnosticCheckNaN_Translate();
		DiagnosticCheckNaN_Scale();
	}

	/**
	 * Flips the sign of the rotation quaternion's W component
	 */
	FORCEINLINE void FlipSignOfRotationW()
	{
		DiagnosticCheckNaN_Rotate();
		Rotation.W *= -1.0f;
	}

	/**
	 * Accumulates another transform with this one, with an optional blending weight
	 *
	 * Rotation is accumulated additively, in the shortest direction (Rotation = Rotation +/- DeltaAtom.Rotation * Weight)
	 * Translation is accumulated additively (Translation += DeltaAtom.Translation * Weight)
	 * Scale is accumulated additively (Scale += DeltaAtom.Scale * Weight)
	 *
	 * @param DeltaAtom The other transform to accumulate into this one
	 * @param Weight The weight to multiply DeltaAtom by before it is accumulated.
	 */
	FORCEINLINE void AccumulateWithShortestRotation(const FBoneAtom& DeltaAtom, float Weight = 1.0f)
	{
		FBoneAtom Atom(DeltaAtom * Weight);

		// To ensure the 'shortest route', we make sure the dot product between the accumulator and the incoming child atom is positive.
		if( (Atom.Rotation | Rotation) < 0.f )
		{
			Rotation.X -= Atom.Rotation.X;
			Rotation.Y -= Atom.Rotation.Y;
			Rotation.Z -= Atom.Rotation.Z;
			Rotation.W -= Atom.Rotation.W;
		}
		else
		{
			Rotation.X += Atom.Rotation.X;
			Rotation.Y += Atom.Rotation.Y;
			Rotation.Z += Atom.Rotation.Z;
			Rotation.W += Atom.Rotation.W;
		}

		Translation += Atom.Translation;
		Scale += Atom.Scale;

		DiagnosticCheckNaN_All();
	}

	/**
	 * Accumulates another transform with this one
	 *
	 * Rotation is accumulated multiplicatively (Rotation = SourceAtom.Rotation * Rotation)
	 * Translation is accumulated additively (Translation += SourceAtom.Translation)
	 * Scale is accumulated multiplicatively (Scale *= SourceAtom.Scale)
	 *
	 * @param SourceAtom The other transform to accumulate into this one
	 */
	FORCEINLINE void Accumulate(const FBoneAtom& SourceAtom)
	{
		// Add ref pose relative animation to base animation, only if rotation is significant.
		if( Square(SourceAtom.Rotation.W) < 1.f - DELTA * DELTA )
		{
			Rotation = SourceAtom.Rotation * Rotation;
		}

		Translation += SourceAtom.Translation;
		Scale *= SourceAtom.Scale;

		DiagnosticCheckNaN_All();

#ifdef _DEBUG
		check( IsRotationNormalized() );
#endif
	}

   /** Accumulates another transform with this one, with a blending weight
	*
	* Let SourceAtom = Atom * BlendWeight
	* Rotation is accumulated multiplicatively (Rotation = SourceAtom.Rotation * Rotation).
	* Translation is accumulated additively (Translation += SourceAtom.Translation)
	* Scale is accumulated multiplicatively (Scale *= SourceAtom.Scale)
	*
	* Note: Rotation will not be normalized! Will have to be done manually.
	*
	* @param Atom The other transform to accumulate into this one
	* @param BlendWeight The weight to multiply Atom by before it is accumulated.
	*/
	FORCEINLINE void Accumulate(const FBoneAtom& Atom, float BlendWeight)
	{
		FBoneAtom SourceAtom(Atom * BlendWeight);

		// Add ref pose relative animation to base animation, only if rotation is significant.
		if( Square(SourceAtom.Rotation.W) < 1.f - DELTA * DELTA )
		{
			Rotation = SourceAtom.Rotation * Rotation;
		}

		Translation += SourceAtom.Translation;
		Scale *= SourceAtom.Scale;

		DiagnosticCheckNaN_All();
	}

	/**
	 * Set the translation and scale components of this atom to a linearly interpolated combination of two other atoms
	 *
	 * Translation = Lerp(SourceAtom1.Translation, SourceAtom2.Translation, Alpha)
	 * Scale = Lerp(SourceAtom1.Scale, SourceAtom2.Scale, Alpha)
	 *
	 * @param SourceAtom1 The starting point source atom (used 100% if Alpha is 0)
	 * @param SourceAtom2 The ending point source atom (used 100% if Alpha is 1)
	 * @param Alpha The blending weight between SourceAtom1 and SourceAtom2
	 */
	FORCEINLINE void LerpTranslationScale(const FBoneAtom& SourceAtom1, const FBoneAtom& SourceAtom2, ScalarRegister Alpha)
	{
		Translation	= Lerp(SourceAtom1.Translation, SourceAtom2.Translation, Alpha);
		Scale = Lerp(SourceAtom1.Scale, SourceAtom2.Scale, Alpha);

		DiagnosticCheckNaN_Translate();
		DiagnosticCheckNaN_Scale();
	}

	/**
	 * Accumulates another transform with this one
	 *
	 * Rotation is accumulated multiplicatively (Rotation = SourceAtom.Rotation * Rotation)
	 * Translation is accumulated additively (Translation += SourceAtom.Translation)
	 * Scale is accumulated additively (Scale += SourceAtom.Scale)
	 *
	 * @param SourceAtom The other transform to accumulate into this one
	 */
	FORCEINLINE void AccumulateWithAdditiveScale(const FBoneAtom& SourceAtom)
	{
		if( Square(SourceAtom.Rotation.W) < 1.f - DELTA * DELTA )
		{
			Rotation = SourceAtom.Rotation * Rotation;
		}

		Translation += SourceAtom.Translation;
		Scale += SourceAtom.Scale;

		DiagnosticCheckNaN_All();
	}

	/**
	 * Normalize the rotation component of this transformation
	 */
	FORCEINLINE void NormalizeRotation()
	{
		Rotation.Normalize();
		DiagnosticCheckNaN_Rotate();
	}

	/**
	 * Checks whether the rotation component is normalized or not
	 *
	 * @return True if the rotation component is normalized, and false otherwise.
	 */
	FORCEINLINE UBOOL IsRotationNormalized() const
	{
		return Rotation.IsNormalized();
	}

	/**
	 * Blends the Identity atom with a weighted source atom and accumulates that into a destination atom
	 *
	 * SourceAtom = Blend(Identity, SourceAtom, BlendWeight)
	 * FinalAtom.Rotation = SourceAtom.Rotation * FinalAtom.Rotation
	 * FinalAtom.Translation += SourceAtom.Translation
	 * FinalAtom.Scale *= SourceAtom.Scale
	 *
	 * @param FinalAtom [in/out] The atom to accumulate the blended source atom into
	 * @param SourceAtom The target transformation (used when BlendWeight = 1); this is modified during the process
	 * @param BlendWeight The blend weight between Identity and SourceAtom
	 */
	FORCEINLINE static void BlendFromIdentityAndAccumulate(FBoneAtom& FinalAtom, FBoneAtom& SourceAtom, float BlendWeight)
	{
		const FBoneAtom IdentityAtom = FBoneAtom::Identity;

		// Scale delta by weight
		if( BlendWeight < (1.f - ZERO_ANIMWEIGHT_THRESH) )
		{
			SourceAtom.Blend(IdentityAtom, SourceAtom, BlendWeight);
		}

		// Add ref pose relative animation to base animation, only if rotation is significant.
		if( Square(SourceAtom.Rotation.W) < 1.f - DELTA * DELTA )
		{
			FinalAtom.Rotation = SourceAtom.Rotation * FinalAtom.Rotation;
		}

		FinalAtom.Translation += SourceAtom.Translation;
		FinalAtom.Scale *= SourceAtom.Scale;

		FinalAtom.DiagnosticCheckNaN_All();

#ifdef _DEBUG
		check( FinalAtom.IsRotationNormalized() );
#endif
	}

	/**
	 * Returns the rotation component
	 *
	 * @return The rotation component
	 */
	FORCEINLINE FQuat GetRotation() const
	{
		DiagnosticCheckNaN_Rotate();
		return Rotation;
	}

	/**
	 * Returns the translation component
	 *
	 * @return The translation component
	 */
	FORCEINLINE FVector GetTranslation() const
	{
		DiagnosticCheckNaN_Translate();
		return Translation;
	}

	/**
	 * Returns the scale component
	 *
	 * @return The scale component
	 */
	FORCEINLINE FLOAT GetScale() const
	{
		DiagnosticCheckNaN_Scale();
		return Scale;
	}

	/**
	 * Returns an opaque copy of the rotation component
	 * This method should be used when passing rotation from one FBoneAtom to another
	 *
	 * @return The rotation component
	 */
	FORCEINLINE FQuat GetRotationV() const
	{
		DiagnosticCheckNaN_Rotate();
		return Rotation;
	}

	/**
	 * Returns an opaque copy of the translation component
	 * This method should be used when passing translation from one FBoneAtom to another
	 *
	 * @return The translation component
	 */
	FORCEINLINE FVector GetTranslationV() const
	{
		DiagnosticCheckNaN_Translate();
		return Translation;
	}

	/**
	 * Returns an opaque copy of the scale component
	 * This method should be used when passing scale from one FBoneAtom to another
	 *
	 * @return The scale component
	 */
	FORCEINLINE ScalarRegister GetScaleV() const
	{
		DiagnosticCheckNaN_Scale();
		return Scale;
	}

	/**
	 * Sets the Rotation and Scale of this transformation from another atom
	 *
	 * @param SrcBA The atom to copy rotation and scale from
	 */
	FORCEINLINE void CopyRotationPart(const FBoneAtom& SrcBA)
	{
		Rotation = SrcBA.Rotation;
		Scale = SrcBA.Scale;

		DiagnosticCheckNaN_Rotate();
		DiagnosticCheckNaN_Scale();
	}

	/**
	 * Sets the Translation and Scale of this transformation from another atom
	 *
	 * @param SrcBA The atom to copy translation and scale from
	 */
	FORCEINLINE void CopyTranslationAndScale(const FBoneAtom& SrcBA)
	{
		Translation = SrcBA.Translation;
		Scale = SrcBA.Scale;

		DiagnosticCheckNaN_Translate();
		DiagnosticCheckNaN_Scale();
	}

	FORCEINLINE void SetMatrix(const FMatrix & InMatrix)
	{
		*this = FBoneAtom(InMatrix.GetMatrixWithoutScale());
	}

	FORCEINLINE void SetMatrixWithScale(const FMatrix & InMatrix)
	{
		// NOTE - This only gets maximum axis 
		// BoneAtom does not support non uniform scale
		Rotation = FQuat(InMatrix.GetMatrixWithoutScale());
		Translation = InMatrix.GetOrigin();
		Scale = InMatrix.GetMaximumAxisScale();
	}

} GCC_ALIGN(16);

/////////////////////////////////////////////////////////////////////////////
// FBoneAtom implementation
// Mostly keep same name as FMatrix for easy toggling
/////////////////////////////////////////////////////////////////////////////
// Scale Translation
FORCEINLINE void FBoneAtom::ScaleTranslation(const FVector& Scale3D)
{
	Translation *= Scale3D;

	DiagnosticCheckNaN_Translate();
}

// this function is from matrix, and all it does is to normalize rotation portion
FORCEINLINE void FBoneAtom::RemoveScaling(FLOAT Tolerance/*=SMALL_NUMBER*/)
{
	Scale = 1.f;
	Rotation.Normalize();

	DiagnosticCheckNaN_Rotate();
	DiagnosticCheckNaN_Scale();
}

// Replacement of InverseSafe of FMatrix
FORCEINLINE FBoneAtom FBoneAtom::InverseSafe() const
{
	if (Scale == 0.f)
	{
		return FBoneAtom::Identity;
	}

	return Inverse();
}

/** Returns Inverse Transform of this FBoneAtom **/
FORCEINLINE FBoneAtom FBoneAtom::Inverse() const
{
	// Inverse QST (A) = QST (~A)
	// Since A*~A = Identity, 
	// A(P) = Q(A)*S(A)*P*-Q(A) + T(A)
	// ~A(A(P)) = Q(~A)*S(~A)*(Q(A)*S(A)*P*-Q(A) + T(A))*-Q(~A) + T(~A) = Identity
	// Q(~A)*Q(A)*S(~A)*S(A)*P*-Q(A)*-Q(~A) + Q(~A)*S(~A)*T(A)*-Q(~A) + T(~A) = Identity
	// [Q(~A)*Q(A)]*[S(~A)*S(A)]*P*-[Q(~A)*Q(A)] + [Q(~A)*S(~A)*T(A)*-Q(~A) + T(~A)] = I

	// Identity Q = (0, 0, 0, 1) = Q(~A)*Q(A)
	// Identity Scale = 1 = S(~A)*S(A)
	// Identity Translation = (0, 0, 0) = [Q(~A)*S(~A)*T(A)*-Q(~A) + T(~A)]

	//	Q(~A) = Q(~A)
	//	S(~A) = 1.f/S(A)
	//	T(~A) = - (Q(~A)*S(~A)*T(A)*Q(A))

	checkSlow(IsRotationNormalized());
	checkSlow(Scale!=0.f);

	FBoneAtom Result;

	Result.Rotation = Rotation.Inverse();
	Result.Scale = 1/Scale;
	
	FQuat QuatTrans = FQuat(Result.Scale*Translation.X, Result.Scale*Translation.Y, Result.Scale*Translation.Z, 0.f);
	// NOTE: if you change Rotation.Inverse() to Result.Rotation and compiling in VS 2008, you'll get crash in 64 bit. 
	QuatTrans = Rotation.Inverse()*QuatTrans*Rotation;
	Result.Translation = FVector(-QuatTrans.X, -QuatTrans.Y, -QuatTrans.Z);

	Result.DiagnosticCheckNaN_All();

	return Result;
}

/** Returns Multiplied Transform of 2 FBoneAtoms **/
FORCEINLINE void FBoneAtom::Multiply(FBoneAtom * OutBoneAtom, const FBoneAtom * A, const FBoneAtom * B)
{
	A->DiagnosticCheckNaN_All();
	B->DiagnosticCheckNaN_All();

	checkSlow( A->IsRotationNormalized() );
	checkSlow( B->IsRotationNormalized() );

	//	When Q = quaternion, S = single scalar scale, and T = translation
	//	QST(A) = Q(A), S(A), T(A), and QST(B) = Q(B), S(B), T(B)

	//	QST (AxB) 
	
	// QST(A) = Q(A)*S(A)*P*-Q(A) + T(A)
	// QST(AxB) = Q(B)*S(B)*QST(A)*-Q(B) + T(B)
	// QST(AxB) = Q(B)*S(B)*[Q(A)*S(A)*P*-Q(A) + T(A)]*-Q(B) + T(B)
	// QST(AxB) = Q(B)*S(B)*Q(A)*S(A)*P*-Q(A)*-Q(B) + Q(B)*S(B)*T(A)*-Q(B) + T(B)
	// QST(AxB) = [Q(B)*Q(A)]*[S(B)*S(A)]*P*-[Q(B)*Q(A)] + Q(B)*S(B)*T(A)*-Q(B) + T(B)

	//	Q(AxB) = Q(B)*Q(A)
	//	S(AxB) = S(A)*S(B)
	//	T(AxB) = Q(B)*S(B)*T(A)*-Q(B) + T(B)

	FBoneAtom Ret;
	Ret.Rotation = B->Rotation*A->Rotation;
	Ret.Scale = A->Scale*B->Scale;

	FQuat Tmp = B->Rotation*FQuat(B->Scale*A->Translation.X, B->Scale*A->Translation.Y, B->Scale*A->Translation.Z, 0)*B->Rotation.Inverse() + FQuat(B->Translation.X, B->Translation.Y, B->Translation.Z, 0);
	Ret.Translation = FVector(Tmp.X, Tmp.Y, Tmp.Z);
	*OutBoneAtom = Ret;

	Ret.DiagnosticCheckNaN_All();
}


/** Apply Scale to this transform
 * Simpley just apply to Scale
 */
FORCEINLINE FBoneAtom FBoneAtom::ApplyScale(FLOAT InScale) const
{
	FBoneAtom A(*this);
	A.Scale *= InScale;

	A.DiagnosticCheckNaN_Scale();

	return A;
}

/** Transform FVector4 **/
FORCEINLINE FVector4 FBoneAtom::TransformFVector4(const FVector4& V) const
{
	DiagnosticCheckNaN_All();

	// if not, this won't work
	checkSlow (V.W == 0.f || V.W == 1.f);

	//Transform using QST is following
	//QST(P) = Q*S*P*-Q + T where Q = quaternion, S = scale, T = translation
	FQuat Transform = Rotation*FQuat(Scale*V.X, Scale*V.Y, Scale*V.Z, 0.f)*Rotation.Inverse();
	if (V.W == 1.f)
	{
		Transform += FQuat(Translation.X, Translation.Y, Translation.Z, V.W);
	}

	return FVector4(Transform.X, Transform.Y, Transform.Z, Transform.W);
}



FORCEINLINE FVector FBoneAtom::TransformFVector(const FVector& V) const
{
	return TransformFVector4(FVector4(V.X,V.Y,V.Z,1.0f));
}

/** Inverts the matrix and then transforms V - correctly handles scaling in this matrix. */
FORCEINLINE FVector FBoneAtom::InverseTransformFVector(const FVector &V) const
{
	FBoneAtom InvSelf = this->Inverse();
	return InvSelf.TransformFVector(V);
}

FORCEINLINE FVector FBoneAtom::TransformNormal(const FVector& V) const
{
	return TransformFVector4(FVector4(V.X,V.Y,V.Z,0.0f));
}

/** Faster version of InverseTransformNormal that assumes no scaling. WARNING: Will NOT work correctly if there is scaling in the matrix. */
FORCEINLINE FVector FBoneAtom::InverseTransformNormal(const FVector &V) const
{
	FBoneAtom InvSelf = this->Inverse();
	return InvSelf.TransformNormal(V);
}

FORCEINLINE FBoneAtom FBoneAtom::operator*(const FBoneAtom& Other) const
{
	FBoneAtom Output;
	Multiply(&Output, this, &Other);
	return Output;
}

FORCEINLINE void FBoneAtom::operator*=(const FBoneAtom& Other)
{
	Multiply(this, this, &Other);
}

FORCEINLINE FBoneAtom FBoneAtom::operator*(const FQuat& Other) const
{
	FBoneAtom Output, OtherBoneAtom(Other, FVector(0.f), 1.f);
	Multiply(&Output, this, &OtherBoneAtom);
	return Output;
}

FORCEINLINE void FBoneAtom::operator*=(const FQuat& Other)
{
	FBoneAtom OtherBoneAtom(Other, FVector(0.f), 1.f);
	Multiply(this, this, &OtherBoneAtom);
}

// x = 0, y = 1, z = 2
FORCEINLINE FVector FBoneAtom::GetAxis(INT i) const
{
	if (i==0)
	{
		return TransformNormal(FVector(1.f, 0.f, 0.f));
	}
	else if (i==1)
	{
		return TransformNormal(FVector(0.f, 1.f, 0.f));
	}

	return TransformNormal(FVector(0.f, 0.f, 1.f));
}

FORCEINLINE void FBoneAtom::Mirror(BYTE MirrorAxis, BYTE FlipAxis)
{
	// We do convert to Matrix for mirroring. 
	FMatrix M = ToMatrix();
	M.Mirror(MirrorAxis, FlipAxis);

	*this = FBoneAtom(M);
	RemoveScaling();
}

/** same version of FMatrix::GetMaximumAxisScale function **/
/** @return the maximum magnitude of any row of the matrix. */
inline FLOAT FBoneAtom::GetMaximumAxisScale() const
{
	DiagnosticCheckNaN_Scale();
	return Scale;
}
