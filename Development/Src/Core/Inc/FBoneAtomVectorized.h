/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/******************************************************************************
 ******************************************************************************

     NOTE: If you modify this file, please also modify FBoneAtomStandard.h!

 *****************************************************************************
 *****************************************************************************/

/** This returns Quaternion Inverse of X **/
#define MAKE_QUATINV_VECTORREGISTER(X) VectorMultiply(GlobalVectorConstants::QINV_SIGN_MASK, X)

/** 
 * FBoneAtom class for Quat/Translation/Scale
 */
MS_ALIGN(16) class FBoneAtom
{
protected:
	VectorRegister Rotation;
	VectorRegister TranslationScale;
public:
	/**
	 * The identity transformation (Rotation = FQuat::Identity, Translation = FVector::ZeroVector, Scale = 1.0f)
	 */
	static const FBoneAtom Identity;

	/**
	 * Constructor with no components initialized (garbage values)
	 */
	FORCEINLINE FBoneAtom()
	{
	}

	/**
	 * Constructor with all components initialized (default scale)
	 *
	 * @param InRotation The value to use for rotation component
	 * @param InTranslation The value to use for the translation component
	 * Scale will be initialized to 1.0f
	 */
	FORCEINLINE FBoneAtom(const FQuat& InRotation, const FVector& InTranslation) 
	{
		Rotation = VectorLoadAligned(&InRotation);
		TranslationScale = VectorLoadFloat3_W1(&InTranslation);
	}

	/**
	 * Constructor with all components initialized
	 *
	 * @param InRotation The value to use for rotation component
	 * @param InTranslation The value to use for the translation component
	 * @param InScale The value to use for the scale component
	 */
	FORCEINLINE FBoneAtom(const FQuat& InRotation, const FVector& InTranslation, FLOAT InScale)
	{
		Rotation = VectorLoadAligned(&InRotation);
		TranslationScale = VectorMergeVecXYZ_VecW(VectorLoadFloat3(&InTranslation), VectorLoadFloat1(&InScale));
	}

	/**
	 * Constructor taking a FRotator as the rotation component (default scale)
	 *
	 * @param InRotation The value to use for rotation component (after being converted to a quaternion)
	 * @param InTranslation The value to use for the translation component
	 * Scale will be initialized to 1.0f
	 */
	FORCEINLINE FBoneAtom(const FRotator& InRotation, const FVector& InTranslation) 
	{
		FQuat Q(InRotation);
		Rotation = VectorLoadAligned(&Q);
		TranslationScale = VectorLoadFloat3_W1(&InTranslation);
	}

	/**
	 * Constructor taking a FRotator as the rotation component
	 *
	 * @param InRotation The value to use for rotation component (after being converted to a quaternion)
	 * @param InTranslation The value to use for the translation component
	 * @param InScale The value to use for the scale component
	 */
	FORCEINLINE FBoneAtom(const FRotator& InRotation, const FVector& InTranslation, FLOAT InScale) 
	{
		FQuat Q(InRotation);
		Rotation = VectorLoadAligned(&Q);
		TranslationScale = VectorMergeVecXYZ_VecW(VectorLoadFloat3(&InTranslation), VectorLoadFloat1(&InScale));
	}

	/**
	 * Copy-constructor
	 *
	 * @param InBoneAtom The source atom from which all components will be copied
	 */
	FORCEINLINE FBoneAtom(const FBoneAtom& InBoneAtom) : 
		Rotation(InBoneAtom.Rotation), 
		TranslationScale(InBoneAtom.TranslationScale)
	{
	}

	/**
	* Constructor for converting a Matrix into a bone atom. InMatrix should not contain any scaling info.
	*/
	FORCEINLINE FBoneAtom(const FMatrix& InMatrix)
		:	Rotation(  )
	{
		FQuat Q(InMatrix);
		Rotation = VectorLoadAligned(&Q);
		TranslationScale = VectorMergeVecXYZ_VecW(VectorLoadAligned(&(InMatrix.M[3][0])), VectorOne());
	}

	FORCEINLINE FBoneAtom(const VectorRegister& InRotation, const VectorRegister& InTranslationScale)
		: Rotation(InRotation)
		, TranslationScale(InTranslationScale)
	{
	}

	/**
	* Does a debugf of the contents of this BoneAtom.
	*/
	void DebugPrint() const;

	FString ToString() const;

	/**
	* Copy another Atom into this one
	*/
	FORCEINLINE FBoneAtom& operator=(const FBoneAtom& Other)
	{
		this->Rotation = Other.Rotation;
		this->TranslationScale = Other.TranslationScale;

		return *this;
	}

	/**
	* Convert this Atom to a transformation matrix.
	*/
	FORCEINLINE FMatrix ToMatrix() const
	{
		FMatrix OutMatrix;
		VectorRegister DiagonalsXYZ;
		VectorRegister Adds;
		VectorRegister Subtracts;

		ToMatrixInternal( DiagonalsXYZ, Adds, Subtracts );
		const VectorRegister DiagonalsXYZ_W0 = VectorSet_W0(DiagonalsXYZ);

#if XBOX || PS3
		// PowerPC with permute instructions

		const VectorRegister AddXY_SubXZ = VectorPermute(Adds, Subtracts, Ax_Ay_Bx_Bz);
		const VectorRegister AddZ_SubY = VectorPermute(Adds, Subtracts, Az_By);

		// OutMatrix.M[0][0] = (1.0f - (yy2 + zz2)) * Scale;    // Diagonal.X
		// OutMatrix.M[0][1] = (xy2 + wz2) * Scale;             // Adds.X
		// OutMatrix.M[0][2] = (xz2 - wy2) * Scale;             // Subtracts.Z
		// OutMatrix.M[0][3] = 0.0f;                            // DiagonalsXYZ_W0.W
		const VectorRegister Row0 = VectorPermute(DiagonalsXYZ_W0, AddXY_SubXZ, Ax_Bx_Bw_Aw); // Diagonal.X, AddXY_SubXZ[0], AddXY_SubXZ[3], Diagonal.W

		// OutMatrix.M[1][0] = (xy2 - wz2) * Scale;             // Subtracts.X
		// OutMatrix.M[1][1] = (1.0f - (xx2 + zz2)) * Scale;    // Diagonal.Y
		// OutMatrix.M[1][2] = (yz2 + wx2) * Scale;             // Adds.Y
		// OutMatrix.M[1][3] = 0.0f;                            // DiagonalsXYZ_W0.W
		const VectorRegister Row1 = VectorPermute(DiagonalsXYZ_W0, AddXY_SubXZ, Bz_Ay_By_Aw); // AddXY_SubXZ[2], Diagonal.Y, AddXY_SubXZ[1], Diagonal.W

		// OutMatrix.M[2][0] = (xz2 + wy2) * Scale;             // Adds.Z
		// OutMatrix.M[2][1] = (yz2 - wx2) * Scale;             // Subtracts.Y
		// OutMatrix.M[2][2] = (1.0f - (xx2 + yy2)) * Scale;    // DiagonalsXYZ_W0.Z
		// OutMatrix.M[2][3] = 0.0f;                            // DiagonalsXYZ_W0.W
		const VectorRegister Row2 = VectorPermute(DiagonalsXYZ_W0, AddZ_SubY, Bx_By_Az_Aw);   // AddZ_SubY[0], AddZ_SubY[1], Diagonal.Z, Diagonal.W

#elif defined(HEADER_UNMATHSSE)
		// Windows with SSE

		// OutMatrix.M[0][0] = (1.0f - (yy2 + zz2)) * Scale;    // Diagonal.X
		// OutMatrix.M[0][1] = (xy2 + wz2) * Scale;             // Adds.X
		// OutMatrix.M[0][2] = (xz2 - wy2) * Scale;             // Subtracts.Z
		// OutMatrix.M[0][3] = 0.0f;                            // DiagonalsXYZ_W0.W
		const VectorRegister AddX_DC_DiagX_DC = _mm_shuffle_ps(Adds, DiagonalsXYZ_W0, SHUFFLEMASK(0, 0, 0, 0));
		const VectorRegister SubZ_DC_DiagW_DC = _mm_shuffle_ps(Subtracts, DiagonalsXYZ_W0, SHUFFLEMASK(2, 0, 3, 0));
		const VectorRegister Row0 = _mm_shuffle_ps(AddX_DC_DiagX_DC, SubZ_DC_DiagW_DC, SHUFFLEMASK(2, 0, 0, 2));

		// OutMatrix.M[1][0] = (xy2 - wz2) * Scale;             // Subtracts.X
		// OutMatrix.M[1][1] = (1.0f - (xx2 + zz2)) * Scale;    // Diagonal.Y
		// OutMatrix.M[1][2] = (yz2 + wx2) * Scale;             // Adds.Y
		// OutMatrix.M[1][3] = 0.0f;                            // DiagonalsXYZ_W0.W
		const VectorRegister SubX_DC_DiagY_DC = _mm_shuffle_ps(Subtracts, DiagonalsXYZ_W0, SHUFFLEMASK(0, 0, 1, 0));
		const VectorRegister AddY_DC_DiagW_DC = _mm_shuffle_ps(Adds, DiagonalsXYZ_W0, SHUFFLEMASK(1, 0, 3, 0));
		const VectorRegister Row1 = _mm_shuffle_ps(SubX_DC_DiagY_DC, AddY_DC_DiagW_DC, SHUFFLEMASK(0, 2, 0, 2));

		// OutMatrix.M[2][0] = (xz2 + wy2) * Scale;             // Adds.Z
		// OutMatrix.M[2][1] = (yz2 - wx2) * Scale;             // Subtracts.Y
		// OutMatrix.M[2][2] = (1.0f - (xx2 + yy2)) * Scale;    // DiagonalsXYZ_W0.Z
		// OutMatrix.M[2][3] = 0.0f;                            // DiagonalsXYZ_W0.W
		const VectorRegister AddZ_DC_SubY_DC = _mm_shuffle_ps(Adds, Subtracts, SHUFFLEMASK(2, 0, 1, 0));
		const VectorRegister Row2 = _mm_shuffle_ps(AddZ_DC_SubY_DC, DiagonalsXYZ_W0, SHUFFLEMASK(0, 2, 2, 3));

#elif NGP
		// OutMatrix.M[0][0] = (1.0f - (yy2 + zz2)) * Scale;    // Diagonal.X
		// OutMatrix.M[0][1] = (xy2 + wz2) * Scale;             // Adds.X
		// OutMatrix.M[0][2] = (xz2 - wy2) * Scale;             // Subtracts.Z
		// OutMatrix.M[0][3] = 0.0f;                            // DiagonalsXYZ_W0.W
		const VectorRegister Row0 = sce_vectormath_xbcw(DiagonalsXYZ_W0, sce_vectormath_xazw(Subtracts, Adds));

		// OutMatrix.M[1][0] = (xy2 - wz2) * Scale;             // Subtracts.X
		// OutMatrix.M[1][1] = (1.0f - (xx2 + zz2)) * Scale;    // Diagonal.Y
		// OutMatrix.M[1][2] = (yz2 + wx2) * Scale;             // Adds.Y
		// OutMatrix.M[1][3] = 0.0f;                            // DiagonalsXYZ_W0.W
		const VectorRegister Row1 = sce_vectormath_zbxd(sce_vectormath_yzab(Adds, Subtracts), DiagonalsXYZ_W0);

		// OutMatrix.M[2][0] = (xz2 + wy2) * Scale;             // Adds.Z
		// OutMatrix.M[2][1] = (yz2 - wx2) * Scale;             // Subtracts.Y
		// OutMatrix.M[2][2] = (1.0f - (xx2 + yy2)) * Scale;    // DiagonalsXYZ_W0.Z
		// OutMatrix.M[2][3] = 0.0f;                            // DiagonalsXYZ_W0.W
		const VectorRegister Row2 = sce_vectormath_xycd(sce_vectormath_zbwa(Adds, Subtracts), DiagonalsXYZ_W0);

#else
#error "There is no implementation of this method for the current platform"
#endif

		VectorStoreAligned(Row0, &(OutMatrix.M[0][0]));
		VectorStoreAligned(Row1, &(OutMatrix.M[1][0]));
		VectorStoreAligned(Row2, &(OutMatrix.M[2][0]));

		// OutMatrix.M[3][0] = Translation.X;
		// OutMatrix.M[3][1] = Translation.Y;
		// OutMatrix.M[3][2] = Translation.Z;
		// OutMatrix.M[3][3] = 1.0f;
		const VectorRegister Row3 = VectorMergeVecXYZ_VecW(TranslationScale, VectorOne());
		VectorStoreAligned(Row3, &(OutMatrix.M[3][0]));

		return OutMatrix;
	}

	/**
	* Convert this Atom to the 3x4 transpose of the transformation matrix.
	*/
	FORCEINLINE void To3x4MatrixTranspose( FLOAT * RESTRICT Out ) const
	{
		VectorRegister DiagonalsXYZ;
		VectorRegister Adds;
		VectorRegister Subtracts;

		ToMatrixInternal( DiagonalsXYZ, Adds, Subtracts );

#if XBOX || PS3 
		// PowerPC with permute instructions
		const VectorRegister DiagXY_TransXY = VectorPermute(DiagonalsXYZ, TranslationScale, Ax_Ay_Bx_By);
		const VectorRegister DiagZZ_TransZZ = VectorPermute(DiagonalsXYZ, TranslationScale, Az_Az_Bz_Bz);
		const VectorRegister AddXZ_SubXY = VectorPermute(Adds, Subtracts, Ax_Az_Bx_By);
		const VectorRegister AddYY_SubZZ = VectorPermute(Adds, Subtracts, Ay_Ay_Bz_Bz);

		// Out[0][0] = (1.0f - (yy2 + zz2)) * Scale;    // DiagonalsXYZ.X
		// Out[0][1] = (xy2 - wz2) * Scale;             // Subtracts.X
		// Out[0][2] = (xz2 + wy2) * Scale;             // Adds.Z
		// Out[0][3] = Translation.X;
		const VectorRegister Row0 = VectorPermute(DiagXY_TransXY, AddXZ_SubXY, Ax_Bz_By_Az);

		// Out[1][0] = (xy2 + wz2) * Scale;             // Adds.X
		// Out[1][1] = (1.0f - (xx2 + zz2)) * Scale;    // DiagonalsXYZ.Y
		// Out[1][2] = (yz2 - wx2) * Scale;             // Subtracts.Y
		// Out[1][3] = Translation.Y;
		const VectorRegister Row1 = VectorPermute(DiagXY_TransXY, AddXZ_SubXY, Bx_Ay_Bw_Aw);

		// Out[2][0] = (xz2 - wy2) * Scale;             // Subtracts.Z
		// Out[2][1] = (yz2 + wx2) * Scale;             // Adds.Y
		// Out[2][2] = (1.0f - (xx2 + yy2)) * Scale;    // DiagonalsXYZ.Z
		// Out[2][3] = Translation.Z;
		const VectorRegister Row2 = VectorPermute(DiagZZ_TransZZ, AddYY_SubZZ, Bz_Bx_Ax_Az);

#elif defined(HEADER_UNMATHSSE)
		// Windows with SSE

		// Out[0][0] = (1.0f - (yy2 + zz2)) * Scale;    // DiagonalsXYZ.X
		// Out[0][1] = (xy2 - wz2) * Scale;             // Subtracts.X
		// Out[0][2] = (xz2 + wy2) * Scale;             // Adds.Z
		// Out[0][3] = Translation.X;
		const VectorRegister DiagXX_SubXX = _mm_shuffle_ps(DiagonalsXYZ, Subtracts, SHUFFLEMASK(0, 0, 0, 0));
		const VectorRegister AddZZ_TransXX = _mm_shuffle_ps(Adds, TranslationScale, SHUFFLEMASK(2, 2, 0, 0));
		const VectorRegister Row0 = _mm_shuffle_ps(DiagXX_SubXX, AddZZ_TransXX, SHUFFLEMASK(0, 2, 0, 2));

		// Out[1][0] = (xy2 + wz2) * Scale;             // Adds.X
		// Out[1][1] = (1.0f - (xx2 + zz2)) * Scale;    // DiagonalsXYZ.Y
		// Out[1][2] = (yz2 - wx2) * Scale;             // Subtracts.Y
		// Out[1][3] = Translation.Y;
		const VectorRegister AddXX_DiagYY = _mm_shuffle_ps(Adds, DiagonalsXYZ, SHUFFLEMASK(0, 0, 1, 1));
		const VectorRegister SubYY_TransYY = _mm_shuffle_ps(Subtracts, TranslationScale, SHUFFLEMASK(1, 1, 1, 1));
		const VectorRegister Row1 = _mm_shuffle_ps(AddXX_DiagYY, SubYY_TransYY, SHUFFLEMASK(0, 2, 0, 2));

		// Out[2][0] = (xz2 - wy2) * Scale;             // Subtracts.Z
		// Out[2][1] = (yz2 + wx2) * Scale;             // Adds.Y
		// Out[2][2] = (1.0f - (xx2 + yy2)) * Scale;    // DiagonalsXYZ.Z
		// Out[2][3] = Translation.Z;
		const VectorRegister SubZZ_AddYY = _mm_shuffle_ps(Subtracts, Adds, SHUFFLEMASK(2, 2, 1, 1));
		const VectorRegister DiagZZ_TransZZ = _mm_shuffle_ps(DiagonalsXYZ, TranslationScale, SHUFFLEMASK(2, 2, 2, 2));
		const VectorRegister Row2 = _mm_shuffle_ps(SubZZ_AddYY, DiagZZ_TransZZ, SHUFFLEMASK(0, 2, 0, 2));

#elif NGP
	
		// Out[0][0] = (1.0f - (yy2 + zz2)) * Scale;    // DiagonalsXYZ.X
		// Out[0][1] = (xy2 - wz2) * Scale;             // Subtracts.X
		// Out[0][2] = (xz2 + wy2) * Scale;             // Adds.Z
		// Out[0][3] = Translation.X;
		const VectorRegister DiagX_SubX = sce_vectormath_xayb( DiagonalsXYZ, Subtracts );
		const VectorRegister AddZ_TransX = sce_vectormath_cxyc( TranslationScale, Adds );
		const VectorRegister Row0 = sce_vectormath_xyab( DiagX_SubX, AddZ_TransX );

		// Out[1][0] = (xy2 + wz2) * Scale;             // Adds.X
		// Out[1][1] = (1.0f - (xx2 + zz2)) * Scale;    // DiagonalsXYZ.Y
		// Out[1][2] = (yz2 - wx2) * Scale;             // Subtracts.Y
		// Out[1][3] = Translation.Y;
		const VectorRegister AddX_DiagY = sce_vectormath_xbzw( Adds, DiagonalsXYZ );
		const VectorRegister SubY_TransY = sce_vectormath_xbzw( sce_vectormath_yyyy( Subtracts ), TranslationScale );
		const VectorRegister Row1 = sce_vectormath_xyab( AddX_DiagY, SubY_TransY );

		// Out[2][0] = (xz2 - wy2) * Scale;             // Subtracts.Z
		// Out[2][1] = (yz2 + wx2) * Scale;             // Adds.Y
		// Out[2][2] = (1.0f - (xx2 + yy2)) * Scale;    // DiagonalsXYZ.Z
		// Out[2][3] = Translation.Z;
		const VectorRegister SubZ_AddY = sce_vectormath_zbwd( Subtracts, Adds );
		const VectorRegister DiagZ_TransZ = sce_vectormath_zcwd( DiagonalsXYZ, TranslationScale );
		const VectorRegister Row2 = sce_vectormath_xyab( SubZ_AddY, DiagZ_TransZ );

#else
#error "There is no implementation of this method for the current platform"
#endif

		VectorStoreAligned(Row0, Out);
		VectorStoreAligned(Row1, Out + 4);
		VectorStoreAligned(Row2, Out + 8);
	}

	/** Set this atom to the weighted blend of the supplied two atoms. */
	FORCEINLINE void Blend(const FBoneAtom& Atom1, const FBoneAtom& Atom2, FLOAT ScalarAlpha)
	{
#if !FINAL_RELEASE && !CONSOLE
		// Check that all bone atoms coming from animation are normalized
		check( Atom1.IsRotationNormalized() );
		check( Atom2.IsRotationNormalized() );
#endif
		const VectorRegister Alpha = VectorLoadFloat1(&ScalarAlpha);

		// Blend translation and scale
		//   Translation = Lerp(Atom1.Translation, Atom2.Translation, Alpha);
		//   Scale = Lerp(Atom1.Scale, Atom2.Scale, Alpha);
		const VectorRegister TranslationScaleDelta = VectorSubtract(Atom2.TranslationScale, Atom1.TranslationScale);
		TranslationScale = VectorMultiplyAdd(Alpha, TranslationScaleDelta, Atom1.TranslationScale);		

		// Blend rotation
		//   Rotation = LerpQuat(Atom1.Rotation, Atom2.Rotation, Alpha);
		//   Rotation.Normalize();
		const VectorRegister BlendedRotation = VectorLerpQuat(Atom1.Rotation, Atom2.Rotation, Alpha);
		Rotation = VectorNormalizeQuaternion(BlendedRotation);
	}

	/** Set this atom to the weighted blend of the supplied two atoms. */
	FORCEINLINE void BlendWith(const FBoneAtom& OtherAtom, FLOAT ScalarAlpha)
	{
#if !FINAL_RELEASE && !CONSOLE
		// Check that all bone atoms coming from animation are normalized
		check( IsRotationNormalized() );
		check( OtherAtom.IsRotationNormalized() );
#endif

		const VectorRegister Alpha = VectorLoadFloat1(&ScalarAlpha);

		// Blend translation and scale
		//   Translation = Lerp(Translation, OtherAtom.Translation, Alpha);
		//   Scale = Lerp(Scale, OtherAtom.Scale, Alpha);
		const VectorRegister TranslationScaleDelta = VectorSubtract(OtherAtom.TranslationScale, TranslationScale);
		TranslationScale = VectorMultiplyAdd(Alpha, TranslationScaleDelta, TranslationScale);		

		// Blend rotation
		//   Rotation = LerpQuat(Rotation, OtherAtom.Rotation, Alpha);
		//   Rotation.Normalize();
		const VectorRegister BlendedRotation = VectorLerpQuat(Rotation, OtherAtom.Rotation, Alpha);
		Rotation = VectorNormalizeQuaternion(BlendedRotation);
	}

	/** 
	* For quaternions, delta angles is done by multiplying the conjugate.
	* Result is normalized.
	*/
	FORCEINLINE FBoneAtom operator-(const FBoneAtom& Atom) const
	{
		return FBoneAtom(
			VectorQuaternionMultiply2(Rotation, MAKE_QUATINV_VECTORREGISTER(Atom.Rotation)), // Rotation * (-Atom.Rotation)
			VectorSubtract(TranslationScale, Atom.TranslationScale)); // Translation - Atom.Translation, Scale - Atom.Scale
	}

	/**
	* Quaternion addition is wrong here. This is just a special case for linear interpolation.
	* Use only within blends!!
	* Rotation part is NOT normalized!!
	*/
	FORCEINLINE FBoneAtom operator+(const FBoneAtom& Atom) const
	{
		return FBoneAtom(VectorAdd(Rotation, Atom.Rotation), VectorAdd(TranslationScale, Atom.TranslationScale));
	}

	FORCEINLINE FBoneAtom& operator+=(const FBoneAtom& Atom)
	{
		Rotation = VectorAdd(Rotation, Atom.Rotation);
		TranslationScale = VectorAdd(TranslationScale, Atom.TranslationScale);

		return *this;
	}

	FORCEINLINE FBoneAtom operator*(const ScalarRegister& Mult) const
	{
		return FBoneAtom(
			VectorMultiply(Rotation, Mult.Value),
			VectorMultiply(TranslationScale, Mult.Value));
	}

	FORCEINLINE FBoneAtom& operator*=(const ScalarRegister& Mult)
	{
		Rotation = VectorMultiply(Rotation, Mult.Value);
		TranslationScale = VectorMultiply(TranslationScale, Mult.Value);

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
	FORCEINLINE FLOAT GetMaximumAxisScale() const { return GetScale(); }
	FORCEINLINE FBoneAtom Inverse() const;
	FORCEINLINE FBoneAtom InverseSafe() const;
	FORCEINLINE FVector4 TransformFVector4(const FVector4& V) const;
	FORCEINLINE FVector TransformFVector(const FVector& V) const;


	/** Inverts the matrix and then transforms V - correctly handles scaling in this matrix. */
	FORCEINLINE FVector InverseTransformFVector(const FVector &V) const;

	FORCEINLINE FVector TransformNormal(const FVector& V) const;
	FORCEINLINE VectorRegister TransformNormal(const VectorRegister& InputVectorW0) const;


	/** 
	 *	Transform a direction vector by the inverse of this matrix - will not take into account translation part.
	 *	If you want to transform a surface normal (or plane) and correctly account for non-uniform scaling you should use TransformByUsingAdjointT with adjoint of matrix inverse.
	 */
	FORCEINLINE FVector InverseTransformNormal(const FVector &V) const;

	FORCEINLINE FBoneAtom ApplyScale(FLOAT Scale) const;
	FORCEINLINE FVector GetAxis(INT i) const;
	FORCEINLINE void Mirror(BYTE MirrorAxis, BYTE FlipAxis);

	// temp function for easy conversion
	FORCEINLINE FVector GetOrigin() const
	{
		FVector Value;
		VectorStoreFloat3(TranslationScale, &Value);
		return Value;
	}

	// temp function for easy conversion
	FORCEINLINE void SetOrigin(const FVector& Origin)
	{
		TranslationScale = VectorMergeVecXYZ_VecW(VectorLoadFloat3(&Origin), TranslationScale);
	}

	/**
	 * Checks the components for NaN's
	 * @return Returns true if any component (rotation, translation, or scale) is a NAN
	 */
	FORCEINLINE UBOOL ContainsNaN() const
	{
		return VectorContainsNaNOrInfinite(Rotation) || VectorContainsNaNOrInfinite(TranslationScale);
	}

	// Serializer.
	inline friend FArchive& operator<<(FArchive& Ar, FBoneAtom& M)
	{
		//@TODO: This is an unpleasant cast
		Ar << *(FVector4*)(&(M.Rotation));
		Ar << *(FVector4*)(&(M.TranslationScale));

		return Ar;
	}

/*
	// Binary comparison operators.
	UBOOL operator==( const FBoneAtom& Other ) const
	{
		const VectorRegister EqualsMask = VectorAnd(VectorCompareEQ(Rotation, Other.Rotation), VectorCompareEQ(TranslationScale, Other.TranslationScale));

		VectorAnyGreaterThan(

		return Rotation==Other.Rotation && Translation==Other.Translation && Scale==Other.Scale;
	}
	UBOOL operator!=( const FBoneAtom& Other ) const
	{
		return !(*this == Other);
	}

	inline UBOOL Equals(const FBoneAtom& Other, FLOAT Tolerance=KINDA_SMALL_NUMBER) const
	{
		return Rotation.Equals(Other.Rotation, Tolerance) && Translation.Equals(Other.Translation, Tolerance) && Abs(Scale-Other.Scale) < Tolerance;
	}
*/

	static FORCEINLINE void Multiply(FBoneAtom& OutBoneAtom, const FBoneAtom& A, FLOAT Mult);
	static FORCEINLINE void Multiply(FBoneAtom * OutBoneAtom, const FBoneAtom * A, const FBoneAtom * B);

	/**
	 * Sets the components (default scale)
	 *
	 * @param InRotation The new value for the Rotation component
	 * @param InTranslation The new value for the Translation component
	 * Scale is set to 1.0f
	 */
	FORCEINLINE void SetComponents(const FQuat& InRotation, const FVector& InTranslation) 
	{
		Rotation = VectorLoadAligned(&InRotation);
		TranslationScale = VectorLoadFloat3_W1(&InTranslation);
	}

	/**
	 * Sets the components
	 * @param InRotation The new value for the Rotation component
	 * @param InTranslation The new value for the Translation component
	 * @param InScale The new value for the Scale component
	 */
	FORCEINLINE void SetComponents(const FQuat& InRotation, const FVector& InTranslation, FLOAT InScale) 
	{
		Rotation = VectorLoadAligned(&InRotation);
		TranslationScale = VectorMergeVecXYZ_VecW(VectorLoadFloat3(&InTranslation), VectorLoadFloat1(&InScale));
	}

	/**
	 * Sets the components to the identity transform:
	 *   Rotation = (0,0,0,1)
	 *   Translation = (0,0,0)
	 *   Scale = 1
	 */
	FORCEINLINE void SetIdentity()
	{
		Rotation = GlobalVectorConstants::Float0001;
		TranslationScale = GlobalVectorConstants::Float0001;
	}

	/**
	 * Scales the scale component by a new factor
	 * @param ScaleMultiplier The value to multiply scale with
	 */
	FORCEINLINE void MultiplyScale(FLOAT ScalarScaleMultiplier)
	{
		const VectorRegister ScaleMultiplier = VectorLoadFloat1(&ScalarScaleMultiplier);
		TranslationScale = VectorMergeVecXYZ_VecW(TranslationScale, VectorMultiply(TranslationScale, ScaleMultiplier));
	}

	/**
	 * Sets the translation component
	 * @param NewTranslation The new value for the translation component
	 */
	FORCEINLINE void SetTranslation(const FVector& NewTranslation)
	{
		TranslationScale = VectorMergeVecXYZ_VecW(VectorLoadFloat3(&NewTranslation), TranslationScale);
	}

	/**
	 * Sets the translation component
	 * @param NewTranslation The new value for the translation component (only XYZ are used)
	 */
	FORCEINLINE void SetTranslation(const VectorRegister NewTranslation)
	{
		TranslationScale = VectorMergeVecXYZ_VecW(NewTranslation, TranslationScale);
	}

	/**
	 * Concatenates another rotation to this transformation 
	 * @param DeltaRotation The rotation to concatenate in the following fashion: Rotation = Rotation * DeltaRotation
	 */
	FORCEINLINE void ConcatenateRotation(const FQuat& DeltaRotation)
	{
		Rotation = VectorQuaternionMultiply2(Rotation, VectorLoadAligned(&DeltaRotation));
	}

	/**
	 * Concatenates another rotation to this transformation 
	 * @param DeltaRotation The rotation to concatenate in the following fashion: Rotation = Rotation * DeltaRotation
	 */
	FORCEINLINE void ConcatenateRotation(const VectorRegister& DeltaRotation)
	{
		Rotation = VectorQuaternionMultiply2(Rotation, DeltaRotation);
	}

	/**
	 * Adjusts the translation component of this transformation 
	 * @param DeltaTranslation The translation to add in the following fashion: Translation += DeltaTranslation
	 */
	FORCEINLINE void AddToTranslation(const FVector& DeltaTranslation)
	{
		TranslationScale = VectorAdd(TranslationScale, VectorLoadFloat3_W0(&DeltaTranslation));
	}

	/**
	 * Adjusts the translation component of this transformation 
	 * @param DeltaTranslation The translation to add in the following fashion: Translation += DeltaTranslation.XYZ
	 */
	FORCEINLINE void AddToTranslation(const VectorRegister& DeltaTranslation)
	{
		TranslationScale = VectorAdd(TranslationScale, VectorSet_W0(DeltaTranslation));
	}

	/**
	 * Sets the rotation component
	 * @param NewRotation The new value for the rotation component
	 */
	FORCEINLINE void SetRotation(const FQuat& NewRotation)
	{
		Rotation = VectorLoadAligned(&NewRotation);
	}

	/**
	 * Sets the rotation component
	 * @param NewRotation The new value for the rotation component
	 */
	FORCEINLINE void SetRotation(const VectorRegister NewRotation)
	{
		Rotation = NewRotation;
	}

	/**
	 * Sets the scale component
	 * @param NewScale The new value for the scale component (only the W component is used)
	 */
	FORCEINLINE void SetScale(const ScalarRegister& NewScale)
	{
		TranslationScale = VectorMergeVecXYZ_VecW(TranslationScale, NewScale.Value);
	}

	/**
	 * Sets the scale component
	 * @param NewScale The new value for the scale component
	 */
	FORCEINLINE void SetScale(const FLOAT ScalarNewScale)
	{
		const VectorRegister NewScale = VectorLoadFloat1(&ScalarNewScale);
		TranslationScale = VectorMergeVecXYZ_VecW(TranslationScale, NewScale);
	}

	/**
	 * Sets both the translation and scale components at the same time (default scale)
	 * Scale is set to 1.0f
	 * @param NewTranslation The new value for the translation component
	 */
	FORCEINLINE void SetTranslationAndScale(const FVector& NewTranslation)
	{
		TranslationScale = VectorLoadFloat3_W1(&NewTranslation);
	}

	/**
	 * Sets both the translation and scale components at the same time
	 * @param NewTranslation The new value for the translation component
	 * @param NewScale The new value for the scale component
	 */
	FORCEINLINE void SetTranslationAndScale(const FVector& NewTranslation, FLOAT NewScale)
	{
		TranslationScale = VectorMergeVecXYZ_VecW(VectorLoadFloat3(&NewTranslation), VectorLoadFloat1(&NewScale));
	}

	/**
	 * Flips the sign of the rotation quaternion's W component
	 */
	FORCEINLINE void FlipSignOfRotationW()
	{
		Rotation = VectorMultiply(Rotation, GlobalVectorConstants::Float111_Minus1);
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
	FORCEINLINE void AccumulateWithShortestRotation(const FBoneAtom& DeltaAtom, const VectorRegister BlendWeight)
	{
		const VectorRegister BlendedRotation = VectorMultiply(DeltaAtom.Rotation, BlendWeight);
		const VectorRegister BlendedTranslationScale = VectorMultiply(DeltaAtom.TranslationScale, BlendWeight);

		Rotation = VectorAccumulateQuaternionShortestPath(Rotation, BlendedRotation);
		TranslationScale = VectorAdd(TranslationScale, BlendedTranslationScale);
	}

	/**
	 * Accumulates another transform with this one
	 *
	 * Rotation is accumulated additively, in the shortest direction (Rotation = Rotation +/- DeltaAtom.Rotation)
	 * Translation is accumulated additively (Translation += DeltaAtom.Translation)
	 * Scale is accumulated additively (Scale += DeltaAtom.Scale)
	 *
	 * @param DeltaAtom The other transform to accumulate into this one
	 */
	FORCEINLINE void AccumulateWithShortestRotation(const FBoneAtom& DeltaAtom)
	{
		Rotation = VectorAccumulateQuaternionShortestPath(Rotation, DeltaAtom.Rotation);
		TranslationScale = VectorAdd(TranslationScale, DeltaAtom.TranslationScale);
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
		const VectorRegister BlendedRotation = SourceAtom.Rotation;
		const VectorRegister BlendedTranslationScale = SourceAtom.TranslationScale;
		const VectorRegister RotationW = VectorReplicate(BlendedRotation, 3);

		// if( Square(SourceAtom.Rotation.W) < 1.f - DELTA * DELTA )
		if (VectorAnyGreaterThan(GlobalVectorConstants::RotationSignificantThreshold, VectorMultiply(RotationW, RotationW)))
		{
			// Rotation = SourceAtom.Rotation * Rotation;
			Rotation = VectorQuaternionMultiply2(BlendedRotation, Rotation);
		}

		// Translation += SourceAtom.Translation;
		// Scale *= SourceAtom.Scale;
		const VectorRegister NewScale = VectorMultiply(TranslationScale, BlendedTranslationScale);   // scale in W
		const VectorRegister NewTranslate = VectorAdd(TranslationScale, BlendedTranslationScale);    // translate in XYZ
		TranslationScale = VectorMergeVecXYZ_VecW(NewTranslate, NewScale);

#ifdef _DEBUG
		check( IsRotationNormalized() );
#endif
	}

	/**
	 * Accumulates another transform with this one, with a blending weight
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
	FORCEINLINE void Accumulate(const FBoneAtom& Atom, const ScalarRegister& BlendWeight)
	{
		// SourceAtom = Atom * BlendWeight;
		const VectorRegister BlendedRotation = VectorMultiply(Atom.Rotation, BlendWeight.Value);
		const VectorRegister BlendedTranslationScale = VectorMultiply(Atom.TranslationScale, BlendWeight.Value);
		const VectorRegister RotationW = VectorReplicate(BlendedRotation, 3);

		// Translation += SourceAtom.Translation;
		// Scale *= SourceAtom.Scale;
		const VectorRegister NewScale = VectorMultiply(TranslationScale, BlendedTranslationScale);   // scale in W
		const VectorRegister NewTranslate = VectorAdd(TranslationScale, BlendedTranslationScale);    // translate in XYZ
		TranslationScale = VectorMergeVecXYZ_VecW(NewTranslate, NewScale);

		// Add ref pose relative animation to base animation, only if rotation is significant.
		// if( Square(SourceAtom.Rotation.W) < 1.f - DELTA * DELTA )
		if (VectorAnyGreaterThan(GlobalVectorConstants::RotationSignificantThreshold, VectorMultiply(RotationW, RotationW)))
		{
			// Rotation = SourceAtom.Rotation * Rotation;
			Rotation = VectorQuaternionMultiply2(BlendedRotation, Rotation);
		}
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
	FORCEINLINE void LerpTranslationScale(const FBoneAtom& SourceAtom1, const FBoneAtom& SourceAtom2, const ScalarRegister& Alpha)
	{
		const VectorRegister TranslationScaleDelta = VectorSubtract(SourceAtom2.TranslationScale, SourceAtom1.TranslationScale);
		TranslationScale = VectorMultiplyAdd(Alpha.Value, TranslationScaleDelta, SourceAtom1.TranslationScale);
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
		// Add ref pose relative animation to base animation, only if rotation is significant.
		const VectorRegister RotationW = VectorReplicate(SourceAtom.Rotation, 3);
		TranslationScale = VectorAdd(TranslationScale, SourceAtom.TranslationScale);

		// if( Square(SourceAtom.Rotation.W) < 1.f - DELTA * DELTA )
		if (VectorAnyGreaterThan(GlobalVectorConstants::RotationSignificantThreshold, VectorMultiply(RotationW, RotationW)))
		{
			// Rotation = SourceAtom.Rotation * Rotation;
			Rotation = VectorQuaternionMultiply2(SourceAtom.Rotation, Rotation);
		}
	}

	/**
	 * Normalize the rotation component of this transformation
	 */
	FORCEINLINE void NormalizeRotation()
	{
		Rotation = VectorNormalizeQuaternion(Rotation);
	}

	/**
	 * Checks whether the rotation component is normalized or not
	 *
	 * @return True if the rotation component is normalized, and false otherwise.
	 */
	FORCEINLINE UBOOL IsRotationNormalized() const
	{
		const VectorRegister TestValue = VectorAbs(VectorSubtract(VectorOne(), VectorDot4(Rotation, Rotation)));
		return !VectorAnyGreaterThan(TestValue, GlobalVectorConstants::FloatOneHundredth);
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
	 * @param SourceAtom The target transformation (used when BlendWeight = 1)
	 * @param Alpha The blend weight between Identity and SourceAtom
	 */
	FORCEINLINE static void BlendFromIdentityAndAccumulate(FBoneAtom& FinalAtom, FBoneAtom& SourceAtom, const ScalarRegister& Alpha)
	{
		const VectorRegister Const0001 = GlobalVectorConstants::Float0001;
		const VectorRegister ConstZero = VectorZero();
		const VectorRegister ConstNegative0001 = VectorSubtract(ConstZero, Const0001);

		const VectorRegister VOneMinusAlpha = VectorSubtract(VectorOne(), Alpha.Value);

		// Blend translation and scale
		//    BlendedAtom.Translation = Lerp(Zero, SourceAtom.Translation, Alpha);
		//    BlendedAtom.Scale = Lerp(1, SourceAtom.Scale, Alpha);
		const VectorRegister TranslateScaleB = SourceAtom.TranslationScale;
		const VectorRegister t0 = VectorSubtract(TranslateScaleB, Const0001);
		const VectorRegister BlendedTranslateScale = VectorMultiplyAdd(Alpha.Value, t0, Const0001);

		// Apply translation and scale to final atom
		//     FinalAtom.Translation += BlendedAtom.Translation
		//     FinalAtom.Scale *= BlendedAtom.Scale
		const VectorRegister OriginalTS = FinalAtom.TranslationScale;
		const VectorRegister FinalScale = VectorMultiply(OriginalTS, BlendedTranslateScale);   // scale in W
		const VectorRegister FinalTranslate = VectorAdd(OriginalTS, BlendedTranslateScale);    // translate in XYZ
		const VectorRegister FinalTS = VectorMergeVecXYZ_VecW(FinalTranslate, FinalScale);

		// Blend rotation
		//     To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
		//     const FLOAT Bias = (|A.B| >= 0 ? 1 : -1)
		//     BlendedAtom.Rotation = (B * Alpha) + (A * (Bias * (1.f - Alpha)));
		//     BlendedAtom.Rotation.QuaternionNormalize();
		//  Note: A = (0,0,0,1), which simplifies things a lot; only care about sign of B.W now, instead of doing a dot product
		const VectorRegister RotationB = SourceAtom.Rotation;

		const VectorRegister QuatRotationDirMask = VectorCompareGE(RotationB, ConstZero);
		const VectorRegister BiasTimesA = VectorSelect(QuatRotationDirMask, Const0001, ConstNegative0001);
		const VectorRegister RotateBTimesWeight = VectorMultiply(RotationB, Alpha.Value);
		const VectorRegister UnnormalizedRotation = VectorMultiplyAdd(BiasTimesA, VOneMinusAlpha, RotateBTimesWeight);

		// Normalize blended rotation ( result = (Q.Q >= 1e-8) ? (Q / |Q|) : (0,0,0,1) )
		const VectorRegister BlendedRotation = VectorNormalizeSafe(UnnormalizedRotation, Const0001);

		// FinalAtom.Rotation = BlendedAtom.Rotation * FinalAtom.Rotation;
		const VectorRegister FinalRotation = VectorQuaternionMultiply2(BlendedRotation, FinalAtom.Rotation);

		// Store back the final result
		FinalAtom.Rotation = FinalRotation;
		FinalAtom.TranslationScale = FinalTS;

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
		FQuat Value;
		VectorStoreAligned(Rotation, &Value);
		return Value;
	}

	/**
	 * Returns the translation component
	 *
	 * @return The translation component
	 */
	FORCEINLINE FVector GetTranslation() const
	{
		FVector Value;
		VectorStoreFloat3(TranslationScale, &Value);
		return Value;
	}

	/**
	 * Returns the scale component
	 *
	 * @return The scale component
	 */
	FORCEINLINE FLOAT GetScale() const
	{
		FLOAT SScale;

		VectorRegister VScale = VectorReplicate(TranslationScale, 3);
		VectorStoreFloat1(VScale, &SScale);
		return SScale;
	}

	/**
	 * Returns an opaque copy of the rotation component
	 * This method should be used when passing rotation from one FBoneAtom to another
	 *
	 * @return The rotation component
	 */
	FORCEINLINE VectorRegister GetRotationV() const
	{
		return Rotation;
	}

	/**
	 * Returns an opaque copy of the translation component
	 * This method should be used when passing translation from one FBoneAtom to another
	 *
	 * @return The translation component
	 */
	FORCEINLINE VectorRegister GetTranslationV() const
	{
		return TranslationScale;
	}

	/**
	 * Returns an opaque copy of the scale component
	 * This method should be used when passing scale from one FBoneAtom to another
	 *
	 * @return The scale component
	 */
	FORCEINLINE ScalarRegister GetScaleV() const
	{
		return ScalarRegister(VectorReplicate(TranslationScale, 3));
	}

	/**
	 * Sets the Rotation and Scale of this transformation from another atom
	 *
	 * @param SrcBA The atom to copy rotation and scale from
	 */
	FORCEINLINE void CopyRotationPart(const FBoneAtom& SrcBA)
	{
		Rotation = SrcBA.Rotation;
		TranslationScale = VectorMergeVecXYZ_VecW(TranslationScale, SrcBA.TranslationScale);
	}

	/**
	 * Sets the Translation and Scale of this transformation from another atom
	 *
	 * @param SrcBA The atom to copy translation and scale from
	 */
	FORCEINLINE void CopyTranslationAndScale(const FBoneAtom& SrcBA)
	{
		TranslationScale = SrcBA.TranslationScale;
	}

	FORCEINLINE void SetMatrix(const FMatrix & InMatrix)
	{
		*this = FBoneAtom(InMatrix.GetMatrixWithoutScale());
	}

	FORCEINLINE void SetMatrixWithScale(const FMatrix & InMatrix)
	{
		// NOTE - This only gets maximum axis 
		// BoneAtom does not support non uniform scale
		SetComponents(FQuat(InMatrix.GetMatrixWithoutScale()), InMatrix.GetOrigin(), InMatrix.GetMaximumAxisScale());
	}

private:
	FORCEINLINE void ToMatrixInternal( VectorRegister& OutDiagonals, VectorRegister& OutAdds, VectorRegister& OutSubtracts ) const
	{
#if !FINAL_RELEASE && !CONSOLE
		// Make sure Rotation is normalized when we turn it into a matrix.
		check( IsRotationNormalized() );
#endif
		const VectorRegister Scale = VectorReplicate(TranslationScale, 3);
		const VectorRegister RotationTimes2 = VectorAdd(Rotation, Rotation);
		const VectorRegister RotationTimesScaleTimes2 = VectorMultiply(Scale, RotationTimes2);
	
		const VectorRegister RotationSquaredTimes2TimesScale = VectorMultiply(Rotation, RotationTimesScaleTimes2); // 2xx, 2yy, 2zz, 2ww

		// The diagonal terms of the rotation matrix are:
		//   (1 - 2(yy + zz)) * scale
		//   (1 - 2(xx + zz)) * scale
		//   (1 - 2(xx + yy)) * scale
		const VectorRegister yy2_xx2_xx2 = VectorSwizzle(RotationSquaredTimes2TimesScale, 1, 0, 0, 0);
		const VectorRegister zz2_zz2_yy2 = VectorSwizzle(RotationSquaredTimes2TimesScale, 2, 2, 1, 0);
		const VectorRegister DiagonalSum = VectorAdd(yy2_xx2_xx2, zz2_zz2_yy2);
		OutDiagonals = VectorSubtract(Scale, DiagonalSum);

		// Grouping the non-diagonal elements in the rotation block by operations:
		//    ((2xy,2yz,2xz) + (2wz,2wx,2wy)) * scale and
		//    ((2xz,2xy,2yz) - (2wy,2wz,2wx)) * scale
		// Rearranging so the LHS and RHS are in the same order as for +
		// => ((2xy,2yz,2xz) - (2wz,2wx,2wy)) * scale

		// RotBase = (xy,yz,xz) * 2 * scale
		// RotOffset = (wz,wx,wy) * 2 * scale
		const VectorRegister x_y_x = VectorSwizzle(Rotation, 0, 1, 0, 0);
		const VectorRegister y2_z2_z2 = VectorSwizzle(RotationTimesScaleTimes2, 1, 2, 2, 0);
		const VectorRegister RotBase = VectorMultiply(x_y_x, y2_z2_z2);

		const VectorRegister w_w_w = VectorReplicate(Rotation, 3);
		const VectorRegister z2_x2_y2 = VectorSwizzle(RotationTimesScaleTimes2, 2, 0, 1, 0);
		const VectorRegister RotOffset = VectorMultiply(z2_x2_y2, w_w_w);

		// Adds = RotBase + RotOffset
		// Subtracts = RotBase - RotOffset
		OutAdds = VectorAdd(RotBase, RotOffset);
		OutSubtracts = VectorSubtract(RotBase, RotOffset);
	}

	/** The following vector registers represent permutes used in ToMatrix and ToMatrix3x4Transpose. */
#if XBOX || PS3
	static const VectorRegister Ax_Ay_Bx_Bz;
	static const VectorRegister Az_By;
	static const VectorRegister Ax_Bx_Bw_Aw;
	static const VectorRegister Bz_Ay_By_Aw;
	static const VectorRegister Bx_By_Az_Aw;
	static const VectorRegister Ax_Ay_Bx_By;
	static const VectorRegister Az_Az_Bz_Bz;
	static const VectorRegister Ax_Az_Bx_By;
	static const VectorRegister Ay_Ay_Bz_Bz;
	static const VectorRegister Ax_Bz_By_Az;
	static const VectorRegister Bx_Ay_Bw_Aw;
	static const VectorRegister Bz_Bx_Ax_Az;
#endif // #if XBOX || PS3
} GCC_ALIGN(16);

/////////////////////////////////////////////////////////////////////////////
// FBoneAtom implementation
// Mostly keep same name as FMatrix for easy toggling
/////////////////////////////////////////////////////////////////////////////
// Scale Translation
FORCEINLINE void FBoneAtom::ScaleTranslation(const FVector& Scale3D)
{
	TranslationScale = VectorMultiply(TranslationScale, VectorLoadFloat3_W1(&Scale3D));
}

// this function is from matrix, and all it does is to normalize rotation portion
FORCEINLINE void FBoneAtom::RemoveScaling(FLOAT Tolerance/*=SMALL_NUMBER*/)
{
	TranslationScale = VectorSet_W1(TranslationScale);
	NormalizeRotation();
}

// Replacement of InverseSafe of FMatrix
FORCEINLINE FBoneAtom FBoneAtom::InverseSafe() const
{
	if (VectorAnyGreaterThan(VectorAbs(VectorReplicate(TranslationScale, 3)), VectorZero()))
	{
		return Inverse();
	}
	else
	{
		return FBoneAtom::Identity;
	}
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
	const VectorRegister Scale = VectorReplicate(TranslationScale, 3);
	checkSlow(IsRotationNormalized());
	checkSlow(VectorAnyGreaterThan(VectorAbs(Scale), VectorZero()));

	// Invert the scale
	const VectorRegister InvScale = VectorReciprocal(Scale);

	// Invert the rotation
	const VectorRegister InvRotation = MAKE_QUATINV_VECTORREGISTER(Rotation);

	// Mask out scale from the original translation/scale vector
	//@TODO is this needed?
	const VectorRegister OriginalTranslation = VectorSet_W0(TranslationScale);

	// Invert the translation
	const VectorRegister ScaledTranslation = VectorMultiply(InvScale, OriginalTranslation);
	const VectorRegister t1 = VectorQuaternionMultiply2(InvRotation, ScaledTranslation);
	const VectorRegister t2 = VectorQuaternionMultiply2(t1, Rotation);
	const VectorRegister InvTranslation = VectorNegate(t2);

	// Combine inverse translation and inverse scale back in to one vector
	const VectorRegister InvTranslateScale = VectorMergeVecXYZ_VecW(InvTranslation, InvScale);

	return FBoneAtom(InvRotation, InvTranslateScale);
}

/** Returns Multiplied Transform of 2 FBoneAtoms **/
FORCEINLINE void FBoneAtom::Multiply(FBoneAtom* OutBoneAtom, const FBoneAtom* A, const FBoneAtom* B)
{
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

	const VectorRegister QuatA = A->Rotation;
	const VectorRegister QuatB = B->Rotation;

	const VectorRegister TranslateScaleA = A->TranslationScale;
	const VectorRegister TranslateScaleB = B->TranslationScale;
	const VectorRegister ScaleB = VectorReplicate(TranslateScaleB, 3);

	const VectorRegister QuatBInv = MAKE_QUATINV_VECTORREGISTER(QuatB);

	// RotationResult = B.Rotation * A.Rotation
	const VectorRegister RotationResult = VectorQuaternionMultiply2(QuatB, QuatA);

	// TranslateResult = ((B.Rotation * B.Scale * A.Translation) * Inverse(B.Rotation)) + B.Translate
	const VectorRegister ScaleBMasked = VectorSet_W0(ScaleB); //@TODO Is this masking necessary?
	const VectorRegister ScaledTransA = VectorMultiply(ScaleBMasked, TranslateScaleA);
	const VectorRegister Temp = VectorQuaternionMultiply2(QuatB, ScaledTransA);
	const VectorRegister RotatedTranslate = VectorQuaternionMultiply2(Temp, QuatBInv);
	const VectorRegister TranslateResult = VectorAdd(RotatedTranslate, TranslateScaleB);

	// ScaleResult = Scale.B * Scale.A
	const VectorRegister ScaleResult = VectorMultiply(TranslateScaleA, TranslateScaleB);

	OutBoneAtom->Rotation = RotationResult;
	OutBoneAtom->TranslationScale = VectorMergeVecXYZ_VecW(TranslateResult, ScaleResult);
}


/** Returns a copy of this transform scaled by by InScale.
 */
FORCEINLINE FBoneAtom FBoneAtom::ApplyScale(FLOAT ScalarScale) const
{
	VectorRegister InScale = VectorLoadFloat1(&ScalarScale);
	VectorRegister NewScale = VectorMultiply(TranslationScale, InScale);

	return FBoneAtom(Rotation, VectorMergeVecXYZ_VecW(TranslationScale, NewScale));
}

/** Transform FVector4 **/
FORCEINLINE FVector4 FBoneAtom::TransformFVector4(const FVector4& V) const
{
	const VectorRegister InputVector = VectorLoadAligned(&V);

	//Transform using QST is following
	//QST(P) = Q*S*P*-Q + T where Q = quaternion, S = scale, T = translation
	const VectorRegister Scale = VectorReplicate(TranslationScale, 3);
	const VectorRegister InverseRotation = MAKE_QUATINV_VECTORREGISTER(Rotation);	

	//FQuat Transform = Rotation*FQuat(Scale*V.X, Scale*V.Y, Scale*V.Z, 0.f)*Rotation.Inverse();
	const VectorRegister InputVectorW0 = VectorMergeVecXYZ_VecW(InputVector, VectorZero());
	const VectorRegister ScaledVec = VectorMultiply(Scale, InputVectorW0);
	const VectorRegister TempStorage = VectorQuaternionMultiply2(Rotation, ScaledVec);
	const VectorRegister RotatedVec = VectorQuaternionMultiply2(TempStorage, InverseRotation);

	// NewVect.XYZ += Translation * W
	// NewVect.W += 1 * W
	const VectorRegister WWWW = VectorReplicate(InputVector, 3);
	const VectorRegister TranslatedVec = VectorAdd(RotatedVec, VectorMultiply(VectorMergeVecXYZ_VecW(TranslationScale, VectorOne()), WWWW));

	FVector4 NewVectOutput;
	VectorStoreAligned(TranslatedVec, &NewVectOutput);
	return NewVectOutput;
}


FORCEINLINE FVector FBoneAtom::TransformFVector(const FVector& V) const
{
	const VectorRegister InputVectorW0 = VectorLoadFloat3_W0(&V);

	//Transform using QST is following
	//QST(P) = Q*S*P*-Q + T where Q = quaternion, S = scale, T = translation
	const VectorRegister Scale = VectorReplicate(TranslationScale, 3);
	const VectorRegister InverseRotation = MAKE_QUATINV_VECTORREGISTER(Rotation);	

	//FQuat Transform = Rotation*FQuat(Scale*V.X, Scale*V.Y, Scale*V.Z, 0.f)*Rotation.Inverse();
	const VectorRegister ScaledVec = VectorMultiply(Scale, InputVectorW0);
	const VectorRegister TempStorage = VectorQuaternionMultiply2(Rotation, ScaledVec);
	const VectorRegister RotatedVec = VectorQuaternionMultiply2(TempStorage, InverseRotation);

	// NewVect.XYZ += Translation
	const VectorRegister TranslatedVec = VectorAdd(RotatedVec, TranslationScale);

	FVector NewVectOutput;
	VectorStoreFloat3(TranslatedVec, &NewVectOutput);
	return NewVectOutput;
}

/** Inverts the matrix and then transforms V - correctly handles scaling in this matrix. */
FORCEINLINE FVector FBoneAtom::InverseTransformFVector(const FVector &V) const
{
	FBoneAtom InvSelf = this->Inverse();
	return InvSelf.TransformFVector(V);
}

FORCEINLINE FVector FBoneAtom::TransformNormal(const FVector& V) const
{
	const VectorRegister InputVectorW0 = VectorLoadFloat3_W0(&V);

	const VectorRegister RotatedVec = TransformNormal(InputVectorW0);

	FVector NewVectOutput;
	VectorStoreFloat3(RotatedVec, &NewVectOutput);
	return NewVectOutput;
}

// Applies the rotation+scaling portion of the transform to V, but not the translation (V.W should be 0)
FORCEINLINE VectorRegister FBoneAtom::TransformNormal(const VectorRegister& InputVectorW0) const
{
	//Transform using QST is following
	//QST(P) = Q*S*P*-Q + T where Q = quaternion, S = scale, T = translation
	const VectorRegister Scale = VectorReplicate(TranslationScale, 3);
	const VectorRegister InverseRotation = MAKE_QUATINV_VECTORREGISTER(Rotation);	

	//FQuat Transform = Rotation*FQuat(Scale*V.X, Scale*V.Y, Scale*V.Z, 0.f)*Rotation.Inverse();
	const VectorRegister ScaledVec = VectorMultiply(Scale, InputVectorW0);
	const VectorRegister TempStorage = VectorQuaternionMultiply2(Rotation, ScaledVec);
	return VectorQuaternionMultiply2(TempStorage, InverseRotation);
}

/** Faster version of InverseTransformNormal that assumes no scaling. WARNING: Will NOT work correctly if there is scaling in the matrix. */
FORCEINLINE FVector FBoneAtom::InverseTransformNormal(const FVector &V) const
{
	FBoneAtom InvSelf(this->Inverse());
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
	FBoneAtom Output;
	FBoneAtom OtherBoneAtom(VectorLoadAligned(&Other), GlobalVectorConstants::Float0001);
	Multiply(&Output, this, &OtherBoneAtom);
	return Output;
}

FORCEINLINE void FBoneAtom::operator*=(const FQuat& Other)
{
	FBoneAtom OtherBoneAtom(VectorLoadAligned(&Other), GlobalVectorConstants::Float0001);
	Multiply(this, this, &OtherBoneAtom);
}

// x = 0, y = 1, z = 2
FORCEINLINE FVector FBoneAtom::GetAxis(INT i) const
{
	VectorRegister RotatedVec;
	if (i==0)
	{
		RotatedVec = TransformNormal(VectorSwizzle(GlobalVectorConstants::Float0001, 3, 0, 0, 0)); // X axis (1,0,0)
	}
	else if (i==1)
	{
		RotatedVec = TransformNormal(VectorSwizzle(GlobalVectorConstants::Float0001, 0, 3, 0, 0)); // Y axis (0,1,0)
	}
	else
	{
		RotatedVec = TransformNormal(VectorSwizzle(GlobalVectorConstants::Float0001, 0, 0, 3, 0)); // Z axis (0,0,1)
	}

	FVector NewVectOutput;
	VectorStoreFloat3(RotatedVec, &NewVectOutput);
	return NewVectOutput;
}

FORCEINLINE void FBoneAtom::Mirror(BYTE MirrorAxis, BYTE FlipAxis)
{
	// We do convert to Matrix for mirroring. 
	FMatrix M = ToMatrix();
	M.Mirror(MirrorAxis, FlipAxis);

	*this = FBoneAtom(M);
	RemoveScaling();
}
