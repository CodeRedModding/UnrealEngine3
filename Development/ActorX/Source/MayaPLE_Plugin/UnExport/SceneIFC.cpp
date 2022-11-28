/************************************************************************** 

    SceneIFC.cpp	- Scene Interface Class for the ActorX exporter.

	Copyright (c) 1999,2000 Epic MegaGames, Inc. All Rights Reserved.

    Note:this file ONLY contains those parts of SceneIFC that are parent-application independent.
    All Max/Maya specific code and parts of SceneIFC are to be found in MaxInterface.cpp / MayaInterface.cpp.

	TODO: plenty left to clean up, reorganize and make more platform-indepentent.


	Parent-platform independent SceneIFC functions:
	- LogAnimInfo
	- LogSkinInfo
	- WriteScriptFile
	- FixRootMotion

***************************************************************************/

//
// Max includes
//
#include "precompiled.h"
#include "SceneIFC.h"
#include "unExportCmd.h"

int	SceneIFC::LogAnimInfo(VActor *Thing, char* AnimName)
{	
	if (!!HaveLogPath()) {
		return 1;
	}

	char LogFileName[MAX_PATH]; // = _T("\\X_AnimInfo_")+ _T(AnimName) +_T(".LOG");
	sprintf(LogFileName,("\\X_AnimInfo_%s%s"),AnimName,".LOG");	
	DLog.Open( m_logpath, LogFileName, DOLOGFILE );
	
   	DLog.Logf("\n\n Unreal skeletal exporter - Animation information for [%s.psa] \n\n",AnimName );	
	// DLog.Logf(" Frames: %i\n", Thing->BetaNumFrames);
	// DLog.Logf(" Time Total: %f\n seconds", Thing->FrameTimeInfo * 0.001 );
	DLog.Logf(" Bones: %i\n", Thing->RefSkeletonBones.Num() );	
	DLog.Logf(" \n\n");

	for( INT i=0; i< Thing->OutAnims.Num(); i++)
	{
		DLog.Logf(" * Animation sequence %s %4i  Tracktime: %6f rate: %6f \n", Thing->OutAnims[i].AnimInfo.Name, i, (FLOAT)Thing->OutAnims[i].AnimInfo.TrackTime, (FLOAT)Thing->OutAnims[i].AnimInfo.AnimRate);
		DLog.Logf("	  First raw frame  %i  total raw frames %i  group: [%s]\n\n", Thing->OutAnims[i].AnimInfo.FirstRawFrame, Thing->OutAnims[i].AnimInfo.NumRawFrames,Thing->OutAnims[i].AnimInfo.Group);
	}
	DLog.Close();

	return 1;
}


// Log skin data including materials to file.
int	SceneIFC::LogSkinInfo( VActor *Thing, const char* SkinName ) // char* ModelName )
{
	if( !HaveLogPath() )
	{
		return 1; // Prevent logs getting lost in root.
	}

	char LogFileName[MAX_PATH];
	sprintf(LogFileName,"X_ModelInfo_%s%s",SkinName,(".LOG"));
	
	DLog.Open( m_logpath, LogFileName, DOLOGFILE );

	if( DLog.Error() ) return 0;

   	DLog.Logf("\n\n Unreal skeletal exporter - Model information for [%s.psk] \n\n",SkinName );	   	

	DLog.Logf(" Skin faces: %6i\n",Thing->SkinData.Faces.Num());
	DLog.Logf(" Skin vertices: %6i\n",Thing->SkinData.Points.Num());
	DLog.Logf(" Skin wedges (vertices with unique U,V): %6i\n",Thing->SkinData.Wedges.Num());
	DLog.Logf(" Total bone-to-vertex linkups: %6i\n",Thing->SkinData.RawWeights.Num());
	DLog.Logf(" Reference bones: %6i\n",Thing->RefSkeletonBones.Num());
	DLog.Logf(" Unique materials: %6i\n",Thing->SkinData.Materials.Num());
	DLog.Logf("\n = materials =\n");	

#pragma message("TODO:DebugBox")
	//DebugBox("Start printing skins");
	
	for( INT i=0; i < Thing->SkinData.Materials.Num(); i++)
	{
		//DebugBox("Start printing skin [%i]",i);

		char MaterialName[256];
		_tcscpy(MaterialName,CleanString( Thing->SkinData.Materials[i].MaterialName ));

		//DebugBox("Start printing bitmap");

		// Find bitmap:		
		char BitmapName[MAX_PATH];
		char BitmapPath[MAX_PATH];

		if( (Thing->SkinData.RawMaterials.Num()>i) && Thing->SkinData.RawMaterials[i] )
		{
			_tcscpy( BitmapName,Thing->SkinData.RawBMPaths[i].RawBitmapName );  
			_tcscpy( BitmapPath,Thing->SkinData.RawBMPaths[i].RawBitmapPath );  
		}
		else
		{
			sprintf(BitmapName,"[Quadtexed- or internal material]");
			BitmapPath[MAX_PATH];
		}

		//DebugBox("Retrieved bitmapname");
		// Log.
		DLog.Logf(" * Index: [%2i]  name: %s  \n",i, MaterialName);
									 if( BitmapName[0] ) DLog.Logf("   Original bitmap: %s  Path: %s\n ",BitmapName,BitmapPath);
			                         DLog.Logf("   - Skin Index: %2i\n",Thing->SkinData.Materials[i].TextureIndex );
									 //DLog.Logf("   - LOD Bias:   %2i\n",Thing->SkinData.Materials[i].LodBias );
									 //DLog.Logf("   - LOD Style:  %2i\n",Thing->SkinData.Materials[i].LodStyle );

		//DebugBox("End printing bitmap");

		DWORD Flags = Thing->SkinData.Materials[i].PolyFlags;
		DLog.Logf("   - render mode flags:\n");
		if( Flags & MTT_NormalTwoSided ) DLog.Logf("		- Twosided\n");
		if( Flags & MTT_Unlit          ) DLog.Logf("		- Unlit\n");
		if( Flags & MTT_Translucent    ) DLog.Logf("		- Translucent\n");
		if( (Flags & MTT_Masked) == MTT_Masked ) 
										 DLog.Logf("		- Masked\n");
		if( Flags & MTT_Modulate       ) DLog.Logf("		- Modulated\n");
		if( Flags & MTT_Placeholder    ) DLog.Logf("		- Invisible\n");
		if( Flags & MTT_Alpha          ) DLog.Logf("		- Per-pixel alpha.\n");
		//if( Flags & MTT_Flat           ) DLog.Logf("		- Flat\n");
		if( Flags & MTT_Environment    ) DLog.Logf("		- Environment mapped\n");
		if( Flags & MTT_NoSmooth       ) DLog.Logf("		- Nosmooth\n");
	}



	// Print out bones list
	DLog.Logf(" \n\n");
	DLog.Logf(" BONE LIST        [%i] total bones in reference skeleton. \n",Thing->RefSkeletonBones.Num());

	    DLog.Logf("  number     name                           (parent) \n");

	for( INT b=0; b < Thing->RefSkeletonBones.Num(); b++)
	{
		char BoneName[65];
		_tcscpy( BoneName, Thing->RefSkeletonBones[b].Name );
		DLog.Logf("  [%3i ]    [%s]      (%3i ) \n",b,BoneName, Thing->RefSkeletonBones[b].ParentIndex );
	}
	
	DLog.Logf(" \n\n");
	DLog.Close();
	return 1;
}



//
// Output a template .uc file.....
//
//int	SceneIFC::WriteScriptFile( VActor *Thing, char* ScriptName, char* BaseName, char* SkinFileName, char* AnimFileName ) 
int	SceneIFC::WriteScriptFile( VActor *Thing, char* ScriptName, char* BaseName, char* SkinFileName, char* AnimFileName ) 
{		
	if( !HaveLogPath() )
	{
		return 1; // Prevent logs getting lost in root. 
	}
	
	char LogFileName[MAX_PATH];
	sprintf(LogFileName,"%s%s",ScriptName,".uc");

	DLog.Open(m_logpath,LogFileName,1); 
	if( DLog.Error() ) return 0;

	//PopupBox("Assigning script names...");
	char MeshName[200];
	char AnimName[200];
	sprintf(MeshName,"%s%s",ScriptName,"Mesh");
	sprintf(AnimName,"%s%s",ScriptName,"Anims");

	//PopupBox(" Writing header lines.");

	DLog.Logf("//===============================================================================\n");
   	DLog.Logf("//  [%s] \n",ScriptName );	   	
	DLog.Logf("//===============================================================================\n\n");

	//PopupBox(" Writing import #execs.");

	DLog.Logf("class %s extends %s;\n",ScriptName,BaseName );

	DLog.Logf("#exec MESH  MODELIMPORT MESH=%s MODELFILE=models\\%s.PSK LODSTYLE=10\n", MeshName, SkinFileName );
	DLog.Logf("#exec MESH  ORIGIN MESH=%s X=0 Y=0 Z=0 YAW=0 PITCH=0 ROLL=0\n", MeshName );
	DLog.Logf("#exec ANIM  IMPORT ANIM=%s ANIMFILE=models\\%s.PSA COMPRESS=1 MAXKEYS=999999", AnimName, AnimFileName );
	if (!DoExplicitSequences) DLog.Logf(" IMPORTSEQS=1" );
	DLog.Logf("\n");
	DLog.Logf("#exec MESHMAP   SCALE MESHMAP=%s X=1.0 Y=1.0 Z=1.0\n", MeshName );
	DLog.Logf("#exec MESH  DEFAULTANIM MESH=%s ANIM=%s\n",MeshName,AnimName );
	
	// Explicit sequences?
	if( DoExplicitSequences )
	{
		// PopupBox(" Writing explicit sequence info.");
		DLog.Logf("\n// Animation sequences. These can replace or override the implicit (exporter-defined) sequences.\n");
		for( INT i=0; i< Thing->OutAnims.Num(); i++)
		{
			// Prepare strings from implicit sequence info; default strings when none present.
			DLog.Logf("#EXEC ANIM  SEQUENCE ANIM=%s SEQ=%s STARTFRAME=%i NUMFRAMES=%i RATE=%.4f COMPRESS=%.2f GROUP=%s \n",
				AnimName, 
				Thing->OutAnims[i].AnimInfo.Name, 
				Thing->OutAnims[i].AnimInfo.FirstRawFrame, 
				Thing->OutAnims[i].AnimInfo.NumRawFrames,
				(FLOAT)Thing->OutAnims[i].AnimInfo.AnimRate,
				(FLOAT)Thing->OutAnims[i].AnimInfo.KeyReduction,
				Thing->OutAnims[i].AnimInfo.Group );
		}
	}

	// PopupBox(" Writing Digest #exec.");
	// Digest
	DLog.Logf("\n");
	DLog.Logf("// Digest and compress the animation data. Must come after the sequence declarations.\n");
	DLog.Logf("// 'VERBOSE' gives more debugging info in UCC.log \n");
	DLog.Logf("#exec ANIM DIGEST ANIM=%s %s VERBOSE", AnimName, DoExplicitSequences ? "" : "USERAWINFO" );
	DLog.Logf("\n\n");

	//
	// TEXTURES/SETTEXTURES. 
	//

	//
	// - Determine unique textures - these are linked up with a number of (max) materials which may be larger.	
	// - extract the main names from textures and append PCX names
	//
	// - SkinIndex: what's it do exactly: force right index into the textures, for a certain material.
	//   Not to be confused with a material index.
	//

	// Determine unique textures, update pointers to them.
	// Order determined by skin indices; let these rearrange the textures if at all possible.
	TArray<VMaterial> UniqueMaterials;

	//To split off the extension
	//SplitFilename(char*(bmt->GetMapName()),&BmpPath, &BmpFile, &strExt);
	//PopupBox(" Writing #exec texture import lines.");

	{for( INT i=0; i < Thing->SkinData.Materials.Num(); i++)
	{				
		char BitmapName[MAX_PATH];
		char BitmapPath[MAX_PATH];
		_tcscpy(BitmapName,Thing->SkinData.RawBMPaths[i].RawBitmapName);
		_tcscpy(BitmapPath,Thing->SkinData.RawBMPaths[i].RawBitmapPath);

		if( DoTexPCX ) // force PCX texture extension
		{
			/*
			char BFullname[MAX_PATH];
			char BPath[MAX_PATH];
			char BFile[MAX_PATH];
			char BExt[MAX_PATH];
			_tcscpy(BFullname,BitmapName);
			SplitFilename( BFullname,&BPath,&BFile,&BExt);
			sprintf(BitmapName,"%s%s",BFile,".pcx");
			*/
			// Less elegant...
			int j = strlen(BitmapName);
			if( (j>4) && (BitmapName[j-4] == char(".")))
			{								
				BitmapName[j-3] = 112;//char("p"); // 112
				BitmapName[j-2] =  99;//char("c"); //  99
				BitmapName[j-1] = 120;//char("x"); // 120
			}			
		}

		char NumStr[10];
		char TexName[MAX_PATH];
		_itoa(i,NumStr,10);
		sprintf(TexName,"%s%s%s",ScriptName,"Tex",NumStr);
			
		DLog.Logf("#EXEC TEXTURE IMPORT NAME=%s  FILE=TEXTURES\\%s  GROUP=Skins\n",TexName,BitmapName);
	}}
	DLog.Logf("\n");

	{for( INT i=0; i < Thing->SkinData.Materials.Num(); i++)
	{
		char NumStr[10];
		_itoa(i,NumStr,10);
		// Override default skin number with TextureIndex:
		char TexName[100];
		sprintf(TexName,"%s%s%s",ScriptName,"Tex",NumStr);
		DLog.Logf("#EXEC MESHMAP SETTEXTURE MESHMAP=%s NUM=%i TEXTURE=%s\n",MeshName,i,TexName);
	}}
	DLog.Logf("\n");

	// Full material list for debugging purposes.
	{for( INT i=0; i < Thing->SkinData.Materials.Num(); i++)
	{
		char BitmapName[MAX_PATH];
		char BitmapPath[MAX_PATH];
		_tcscpy(BitmapName,Thing->SkinData.RawBMPaths[i].RawBitmapName);
		_tcscpy(BitmapPath,Thing->SkinData.RawBMPaths[i].RawBitmapPath);		
		DLog.Logf("// Original material [%i] is [%s] SkinIndex: %i Bitmap: %s  Path: %s \n",i,Thing->SkinData.Materials[i].MaterialName,Thing->SkinData.Materials[i].TextureIndex,BitmapName,BitmapPath );
		
	}}
	DLog.Logf("\n\n");

	// defaultproperties
	DLog.Logf("defaultproperties\n{\n");
	DLog.Logf("    Mesh=%s\n",MeshName);
	DLog.Logf("    DrawType=DT_Mesh\n");
	DLog.Logf("    bStatic=False\n");
	DLog.Logf("}\n\n");

	DLog.Close();
	return 1;
}


//
// Extract root motion... to be called AFTER DigestAnim has been called.
// 'Default' =  make end and start line up linearly;
// 'Locked' =  set it all to the exact position of frame 0.
//
// 'Rotation' => should be used to S-lerp the rotation so that that matches up automatically also ->
// do this later, part of an automatic kind of 'welding' of looping anims...>> 3dsmax should probably
// be used for this though.
//

void SceneIFC::FixRootMotion(VActor *Thing) //, BOOL Fix, BOOL Lock )
{
	if( ! DoFixRoot ) 
		return;
	
	// Regular linear motion fixup
	if( ! DoLockRoot )
	{
		FVector StartPos = Thing->BetaKeys[0].Position;
		FVector EndPos   = Thing->BetaKeys[(OurTotalFrames-1)*OurBoneTotal].Position;

		for(int n=0; n<OurTotalFrames; n++)
		{
			float Alpha = ((float)n)/(float(OurTotalFrames-1));
			Thing->BetaKeys[n*OurBoneTotal].Position += Alpha*(StartPos - EndPos); // aligns end position onto start.
		}
	}

	// Motion LOCK
	if( DoLockRoot )
	{
		FVector StartPos = Thing->BetaKeys[0].Position;
		FVector EndPos   = Thing->BetaKeys[(OurTotalFrames-1)*OurBoneTotal].Position;

		for(int n=0; n<OurTotalFrames; n++)
		{
			//float Alpha = ((float)n)/(float(OurTotalFrames-1));
			Thing->BetaKeys[n*OurBoneTotal].Position = StartPos; //Locked
		}
	}
}



//
// Animation range parser - based on code by Steve Sinclair, Digital Extremes
//
void SceneIFC::ParseFrameRange(const char *pString,INT startFrame,INT endFrame )
{
	char*	pSeek;
	char*	pPos;
	int		lastPos = 0;
	int		curPos = 0;
	UBOOL	rangeStart=false;
	int		start;
	int		val;
	char    LocalString[600];

	FrameList.StartFrame = startFrame;
	FrameList.EndFrame   = endFrame;

	_tcscpy(LocalString, pString);

	FrameList.Empty();

	pPos = pSeek = LocalString;
	
	if (!LocalString || LocalString[0]==0) // export it all if nothing was specified.
	{
		for(INT i=startFrame; i<=endFrame; i++)
		{
			FrameList.AddFrame(i);
		}
		return;
	}

	while (1)
	{
		if (*pSeek=='-')
		{
			*pSeek=0; // terminate this position for atoi
			pSeek++;
			// Eliminate multiple -'s
			while(*pSeek=='-')
			{
				*pSeek=0;
				pSeek++;
			}
			start = atoi( pPos );							
			pPos = pSeek;			
			rangeStart=true;
		}
		
		if (*pSeek==',')
		{
			*pSeek=0; // terminate this position for atoi
			pSeek++;			
			val = atoi( pPos );			
			pPos = pSeek;			
			// Single frame or end of range?
			if( rangeStart )
			{
				// Allow reversed ranges.
				if( val<start)
					for( INT i=start; i>=val; i--)
					{
						FrameList.AddFrame(i);								
					}
				else
					for( INT i=start; i<=val; i++)
					{
						FrameList.AddFrame(i);												
					}
			}
			else
			{
				FrameList.AddFrame(val); 
			}

			rangeStart=false;
		}
		
		if (*pSeek==0) // Real end of string.
		{
			if (pSeek!=pPos)
			{
				val = atoi ( pPos );
				if( rangeStart )
				{	
					// Allow reversed ranges.
					if( val<start)
						for( INT i=start; i>=val; i--)
						{
							FrameList.AddFrame(i);								
						}
					else
						for( INT i=start; i<=val; i++)
						{
							FrameList.AddFrame(i);												
						}
				}
				else
				{
					FrameList.AddFrame(val); 
				}
			}
			break;
		}

		pSeek++;
	}
}
