/*=============================================================================
	Color.cpp: Unreal color implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

// Common colors.
const FLinearColor FLinearColor::White(1,1,1);
const FLinearColor FLinearColor::Gray(0.5,0.5,0.5);
const FLinearColor FLinearColor::Black(0,0,0);

// Vector constants used by color math.
#if ENABLE_VECTORINTRINSICS && XBOX
static const VectorRegister ColorPowExponent = { 2.2f, 2.2f, 2.2f, 1.0f };
#endif

/** 
 * Helper used by FColor -> FLinearColor conversion. We don't use a lookup table as unlike pow, multiplication is fast.
 */
static const FLOAT OneOver255 = 1.0f / 255.0f;

//	FColor->FLinearColor conversion.
FLinearColor::FLinearColor(const FColor& C)
{
	R = PowOneOver255Table[C.R];
	G = PowOneOver255Table[C.G];
	B =	PowOneOver255Table[C.B];
	A =	FLOAT(C.A) * OneOver255;
}

FLinearColor::FLinearColor(const FVector& Vector) :
	R(Vector.X),
	G(Vector.Y),
	B(Vector.Z),
	A(1.0f)
{}

FLinearColor::FLinearColor(const FFloat16Color& C)
{
	R = C.R.GetFloat();
	G = C.G.GetFloat();
	B =	C.B.GetFloat();
	A =	C.A.GetFloat();
}

// Convert from float to RGBE as outlined in Gregory Ward's Real Pixels article, Graphics Gems II, page 80.
FColor FLinearColor::ToRGBE() const
{
	const FLOAT	Primary = Max3( R, G, B );
	FColor	Color;

	if( Primary < 1E-32 )
	{
		Color = FColor(0,0,0,0);
	}
	else
	{
		INT	Exponent;
		const FLOAT Scale	= frexp(Primary, &Exponent) / Primary * 255.f;

		Color.R		= Clamp(appTrunc(R * Scale), 0, 255);
		Color.G		= Clamp(appTrunc(G * Scale), 0, 255);
		Color.B		= Clamp(appTrunc(B * Scale), 0, 255);
		Color.A		= Clamp(appTrunc(Exponent),-128,127) + 128;
	}

	return Color;
}


/** Quantizes the linear color and returns the result as a FColor with optional sRGB conversion and quality as goal. */
FColor FLinearColor::ToFColor(const UBOOL bSRGB) const
{
	FLOAT FloatR = Clamp(R, 0.0f, 1.0f);
	FLOAT FloatG = Clamp(G, 0.0f, 1.0f);
	FLOAT FloatB = Clamp(B, 0.0f, 1.0f);
	FLOAT FloatA = Clamp(A, 0.0f, 1.0f);

	if(bSRGB)
	{
		FloatR = appPow(FloatR, 1.0f / 2.2f);
		FloatG = appPow(FloatG, 1.0f / 2.2f);
		FloatB = appPow(FloatB, 1.0f / 2.2f);
	}

	FColor ret;

	ret.A = appFloor(FloatA * 255.999f);
	ret.R = appFloor(FloatR * 255.999f);
	ret.G = appFloor(FloatG * 255.999f);
	ret.B = appFloor(FloatB * 255.999f);

	return ret;
}


FColor FLinearColor::Quantize() const
{
	return FColor(
		(BYTE)Clamp<INT>(appTrunc(R*255.f),0,255),
		(BYTE)Clamp<INT>(appTrunc(G*255.f),0,255),
		(BYTE)Clamp<INT>(appTrunc(B*255.f),0,255),
		(BYTE)Clamp<INT>(appTrunc(A*255.f),0,255)
		);
}

/**
 * Returns a desaturated color, with 0 meaning no desaturation and 1 == full desaturation
 *
 * @param	Desaturation	Desaturation factor in range [0..1]
 * @return	Desaturated color
 */
FLinearColor FLinearColor::Desaturate( FLOAT Desaturation ) const
{
	FLOAT Lum = ComputeLuminance();
	return Lerp( *this, FLinearColor( Lum, Lum, Lum, 0 ), Desaturation );
}

/** Computes the perceptually weighted luminance value of a color. */
FLOAT FLinearColor::ComputeLuminance() const
{
	return R * 0.3f + G * 0.59f + B * 0.11f;
}

// Convert from RGBE to float as outlined in Gregory Ward's Real Pixels article, Graphics Gems II, page 80.
FLinearColor FColor::FromRGBE() const
{
	if( A == 0 )
		return FLinearColor::Black;
	else
	{
#if FLASH
		// ldexp doesn't work in Flash for whatever reason.
		const FLOAT Scale = appPow(2.0f,A - 128) * (1.0f / 255.0f);
#else
		const FLOAT Scale = ldexp( 1 / 255.0, A - 128 );
#endif
		return FLinearColor( R * Scale, G * Scale, B * Scale, 1.0f );
	}
}

/**
 * Converts byte hue-saturation-brightness to floating point red-green-blue.
 */
FLinearColor FLinearColor::FGetHSV( BYTE H, BYTE S, BYTE V )
{
	FLOAT Brightness = V * 1.4f / 255.f;
	Brightness *= 0.7f/(0.01f + appSqrt(Brightness));
	Brightness  = Clamp(Brightness,0.f,1.f);
	const FVector Hue = (H<86) ? FVector((85-H)/85.f,(H-0)/85.f,0) : (H<171) ? FVector(0,(170-H)/85.f,(H-85)/85.f) : FVector((H-170)/85.f,0,(255-H)/84.f);
	const FVector ColorVector = (Hue + S/255.f * (FVector(1,1,1) - Hue)) * Brightness;
	return FLinearColor(ColorVector.X,ColorVector.Y,ColorVector.Z,1);
}


/** Converts a linear space RGB color to an HSV color */
FLinearColor FLinearColor::LinearRGBToHSV() const
{
	const FLOAT RGBMin = Min3(R, G, B);
	const FLOAT RGBMax = Max3(R, G, B);
	const FLOAT RGBRange = RGBMax - RGBMin;

	const FLOAT Hue = (RGBMax == RGBMin ? 0.0f :
					   RGBMax == R    ? fmod((((G - B) / RGBRange) * 60.0f) + 360.0f, 360.0f) :
					   RGBMax == G    ?      (((B - R) / RGBRange) * 60.0f) + 120.0f :
					   RGBMax == B    ?      (((R - G) / RGBRange) * 60.0f) + 240.0f :
					   0.0f);
	
	const FLOAT Saturation = (RGBMax == 0.0f ? 0.0f : RGBRange / RGBMax);
	const FLOAT Value = RGBMax;

	// In the new color, R = H, G = S, B = V, A = 1.0
	return FLinearColor(Hue, Saturation, Value);
}



/** Converts an HSV color to a linear space RGB color */
FLinearColor FLinearColor::HSVToLinearRGB() const
{
	// In this color, R = H, G = S, B = V
	const FLOAT Hue = R;
	const FLOAT Saturation = G;
	const FLOAT Value = B;

	const FLOAT HDiv60 = Hue / 60.0f;
	const FLOAT HDiv60_Floor = floorf(HDiv60);
	const FLOAT HDiv60_Fraction = HDiv60 - HDiv60_Floor;

	const FLOAT RGBValues[4] = {
		Value,
		Value * (1.0f - Saturation),
		Value * (1.0f - (HDiv60_Fraction * Saturation)),
		Value * (1.0f - ((1.0f - HDiv60_Fraction) * Saturation)),
	};
	const UINT RGBSwizzle[6][3] = {
		{0, 3, 1},
		{2, 0, 1},
		{1, 0, 3},
		{1, 2, 0},
		{3, 1, 0},
		{0, 1, 2},
	};
	const UINT SwizzleIndex = ((UINT)HDiv60_Floor) % 6;

	return FLinearColor(RGBValues[RGBSwizzle[SwizzleIndex][0]],
						RGBValues[RGBSwizzle[SwizzleIndex][1]],
						RGBValues[RGBSwizzle[SwizzleIndex][2]]);
}


/**
 * Makes a random but quite nice color.
 */
FColor FColor::MakeRandomColor()
{
	const BYTE Hue = (BYTE)( appFrand()*255.f );
	return FColor( FLinearColor::FGetHSV(Hue, 0, 255) );
}

FColor FColor::MakeRedToGreenColorFromScalar(FLOAT Scalar)
{
	INT R,G,B;
	R=G=B=0;

	FLOAT RedSclr = Clamp<FLOAT>((1.0f - Scalar)/0.5f,0.f,1.f);
	FLOAT GreenSclr = Clamp<FLOAT>((Scalar/0.5f),0.f,1.f);
	R = appTrunc(255 * RedSclr);
	G = appTrunc(255 * GreenSclr);
	return FColor(R,G,B);
}

void ComputeAndFixedColorAndIntensity(const FLinearColor& InLinearColor,FColor& OutColor,FLOAT& OutIntensity)
{
	FLOAT MaxComponent = Max(DELTA,Max(InLinearColor.R,Max(InLinearColor.G,InLinearColor.B)));
	OutColor = InLinearColor / MaxComponent;
	OutIntensity = MaxComponent;
}



/**
 * Pow table for fast FColor -> FLinearColor conversion.
 *
 * appPow( i / 255.f, 2.2f )
 */
FLOAT FLinearColor::PowOneOver255Table[256] = 
{
	0,5.07705190066176E-06,2.33280046660989E-05,5.69217657121931E-05,0.000107187362341244,0.000175123977503027,0.000261543754548491,0.000367136269815943,0.000492503787191433,
	0.000638182842167022,0.000804658499513058,0.000992374304074325,0.0012017395224384,0.00143313458967186,0.00168691531678928,0.00196341621339647,0.00226295316070643,
	0.00258582559623417,0.00293231832393836,0.00330270303200364,0.00369723957890013,0.00411617709328275,0.00455975492252602,0.00502820345685554,0.00552174485023966,
	0.00604059365484981,0.00658495738258168,0.00715503700457303,0.00775102739766061,0.00837311774514858,0.00902149189801213,0.00969632870165823,0.0103978022925553,
	0.0111260823683832,0.0118813344348137,0.0126637200315821,0.0134733969401426,0.0143105193748841,0.0151752381596252,0.0160677008908869,0.01698805208925,0.0179364333399502,
	0.0189129834237215,0.0199178384387857,0.0209511319147811,0.0220129949193365,0.0231035561579214,0.0242229420675342,0.0253712769047346,0.0265486828284729,0.027755279978126,
	0.0289911865471078,0.0302565188523887,0.0315513914002264,0.0328759169483838,0.034230206565082,0.0356143696849188,0.0370285141619602,0.0384727463201946,0.0399471710015256,
	0.0414518916114625,0.0429870101626571,0.0445526273164214,0.0461488424223509,0.0477757535561706,0.049433457555908,0.0511220500564934,0.052841625522879,0.0545922772817603,
	0.0563740975519798,0.0581871774736854,0.0600316071363132,0.0619074756054558,0.0638148709486772,0.0657538802603301,0.0677245896854243,0.0697270844425988,0.0717614488462391,
	0.0738277663277846,0.0759261194562648,0.0780565899581019,0.080219258736215,0.0824142058884592,0.0846415107254295,0.0869012517876603,0.0891935068622478,0.0915183529989195,
	0.0938758665255778,0.0962661230633397,0.0986891975410945,0.1011451642096,0.103634096655137,0.106156067812744,0.108711149979039,0.11129941482466,0.113920933406333,
	0.116575776178572,0.119264013005047,0.121985713169619,0.124740945387051,0.127529777813422,0.130352278056244,0.1332085131843,0.136098549737202,0.139022453734703,
	0.141980290685736,0.144972125597231,0.147998022982685,0.151058046870511,0.154152260812165,0.157280727890073,0.160443510725344,0.16364067148529,0.166872271890766,
	0.170138373223312,0.173439036332135,0.176774321640903,0.18014428915439,0.183548998464951,0.186988508758844,0.190462878822409,0.193972167048093,0.19751643144034,
	0.201095729621346,0.204710118836677,0.208359655960767,0.212044397502288,0.215764399609395,0.219519718074868,0.223310408341127,0.227136525505149,0.230998124323267,
	0.23489525921588,0.238827984272048,0.242796353254002,0.24680041960155,0.2508402364364,0.254915856566385,0.259027332489606,0.263174716398492,0.267358060183772,
	0.271577415438375,0.275832833461245,0.280124365261085,0.284452061560024,0.288815972797219,0.293216149132375,0.297652640449211,0.302125496358853,0.306634766203158,
	0.311180499057984,0.315762743736397,0.32038154879181,0.325036962521076,0.329729032967515,0.334457807923889,0.339223334935327,0.344025661302187,0.348864834082879,
	0.353740900096629,0.358653905926199,0.363603897920553,0.368590922197487,0.373615024646202,0.37867625092984,0.383774646487975,0.388910256539059,0.394083126082829,
	0.399293299902674,0.404540822567962,0.409825738436323,0.415148091655907,0.420507926167587,0.425905285707146,0.43134021380741,0.436812753800359,0.442322948819202,
	0.44787084180041,0.453456475485731,0.45907989242416,0.46474113497389,0.470440245304218,0.47617726539744,0.481952237050698,0.487765201877811,0.493616201311074,
	0.49950527660303,0.505432468828216,0.511397818884879,0.517401367496673,0.523443155214325,0.529523222417277,0.535641609315311,0.541798355950137,0.547993502196972,
	0.554227087766085,0.560499152204328,0.566809734896638,0.573158875067523,0.579546611782525,0.585972983949661,0.592438030320847,0.598941789493296,0.605484299910907,
	0.612065599865624,0.61868572749878,0.625344720802427,0.632042617620641,0.638779455650817,0.645555272444934,0.652370105410821,0.659223991813387,0.666116968775851,
	0.673049073280942,0.680020342172095,0.687030812154625,0.694080519796882,0.701169501531402,0.708297793656032,0.715465432335048,0.722672453600255,0.729918893352071,
	0.737204787360605,0.744530171266715,0.751895080583051,0.759299550695091,0.766743616862161,0.774227314218442,0.781750677773962,0.789313742415586,0.796916542907978,
	0.804559113894567,0.81224148989849,0.819963705323528,0.827725794455034,0.835527791460841,0.843369730392169,0.851251645184515,0.859173569658532,0.867135537520905,
	0.875137582365205,0.883179737672745,0.891262036813419,0.899384513046529,0.907547199521614,0.915750129279253,0.923993335251873,0.932276850264543,0.940600707035753,
	0.948964938178195,0.957369576199527,0.96581465350313,0.974300202388861,0.982826255053791,0.99139284359294,1
};

