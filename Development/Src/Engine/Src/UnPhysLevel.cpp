#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"

#ifdef WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

#ifdef WITH_NOVODEX

#define NxTIMESTEP			(1.f/100.f)
#define NxMAXITERATIONS		(10)
#define NxUSEFIXEDTIMESTEP	(1)

class NovodexDebugRenderer : public NxUserDebugRenderer
{
public:
	NovodexDebugRenderer(ULevel* InLevel):
    Level(InLevel)
	{ }

	virtual void renderData(const NxDebugRenderable &DebugData) const
	{
		int NumLines = DebugData.getNbLines();
		if (NumLines > 0)
		{
			const NxDebugLine* Lines = DebugData.getLines();
			for (int i = 0; i < NumLines; i++)
			{
				NxVec3 nStartPos = Lines->p0;
				NxVec3 nEndPos = Lines->p1;

				FVector uStartPos = N2UPosition(nStartPos);
				FVector uEndPos = N2UPosition(nEndPos);

				check(Level);
				Level->LineBatcher->DrawLine(uStartPos, uEndPos, FColor(Lines->color));
				Lines++;
			}
		}
	}

private:
	ULevel* Level;
};

/** 
 *	Novodex contact reporting object. 
 *	Allows us access to actual contact information for certain groups of objects (see the GNovodexSDK->setActorGroupPairFlags below).
 */
class UnrealUserContactReport : public NxUserContactReport
{
public:
	/** 
	 *	Called after Novodex contact generation and allows us to iterate over contact and extract useful information.
	 *	Currently we use this for wheel/ground contacts.
	 */
	virtual void  onContactNotify(NxContactPair& pair, NxU32 events)
	{
		// One of the actors should be Group 1 (ie a wheel)
		NxActor* nVehicle = NULL;
		INT VehicleIndex = INDEX_NONE;
		if(pair.actors[0]->getGroup() == 1)
		{
			nVehicle = pair.actors[0];
			VehicleIndex = 0;
		}
		else if(pair.actors[1]->getGroup() == 1)
		{
			nVehicle = pair.actors[1];
			VehicleIndex = 1;
		}

		check(nVehicle);

		// Iterate through contact points
		NxContactStreamIterator It(pair.stream);

		while(It.goNextPair())
		{
			NxShape* nShape = It.getShape(VehicleIndex);

			// Only shapes with user data are wheel raycast-shapes. If its not one of those (eg. chassis), we don't care.
			USVehicleWheel* vw = NULL;
			if(nShape->userData)
			{
				vw = (USVehicleWheel*)(nShape->userData);
			}

			while(It.goNextPatch())
			{
				NxVec3 ContactNormal = It.getPatchNormal();

				while(It.goNextPoint())
				{
					if(vw)
					{
						//FLOAT ContactSep = It.getSeparation(); // This always seems to return 0.f - why?
						NxVec3 nContactPos = It.getPoint();
						NxMat34 nShapeTM = nShape->getGlobalPose();

						// Transform contact position into ref frame of capsule line-check shape.
						NxVec3 nLocalContactPos = nShapeTM % nContactPos;

						// Offset of contact point from center of capsule along its axis.
						FLOAT ContactFromCenter = P2UScale * nLocalContactPos.y;

						// If less than a wheel radius, we are touching the ground.
						if(ContactFromCenter < vw->WheelRadius)
						{
							vw->bWheelOnGround = true;
							vw->SuspensionPosition = vw->WheelRadius - ContactFromCenter;
						}

						// JTODO: Get contact force...?
					}
				}
			}	
		}
	}

};

/** 
 *	Novodex notification object for joint breakage.
 */
class UnrealUserNotify : public NxUserNotify
{
public:
	/** Called by Novodex when a constraint is broken. From here we call SeqAct_ConstraintBroken events. */
	virtual bool onJointBreak(NxReal breakingForce, NxJoint& brokenJoint)
	{
		// NxJoints should have their RB_ConstraintInstance as userData.
		if(brokenJoint.userData)
		{
			URB_ConstraintInstance* Inst = (URB_ConstraintInstance*)(brokenJoint.userData);
			if(Inst->Owner)
			{
				// See if the owner supports the 'constraint broken' event. If so, fire it.
				USeqEvent_ConstraintBroken *BreakEvent = Cast<USeqEvent_ConstraintBroken>(Inst->Owner->GetEventOfClass(USeqEvent_ConstraintBroken::StaticClass()));
				if(BreakEvent)
				{
					BreakEvent->CheckActivate(Inst->Owner, Inst->Owner);
				}
			}
		}

		// We still hold references to this joint, so do not want it released, so we return false here.
		return false;
	}
};

UnrealUserContactReport ContactReportObject;
UnrealUserNotify		UserNotifyObject;


// BSP triangle gathering.
struct FBSPTriIndices
{
	INT v0, v1, v2;
};

static void GatherBspTrisRecursive( UModel* model, INT nodeIndex, TArray<FBSPTriIndices>& tris, TArray<NxMaterialIndex>& materials )
{
	while(nodeIndex != INDEX_NONE)
	{
		FBspNode* curBspNode   = &model->Nodes( nodeIndex );

		INT planeNodeIndex = nodeIndex;
		while(planeNodeIndex != INDEX_NONE)
		{
			FBspNode* curPlaneNode = &model->Nodes( planeNodeIndex );
			FBspSurf* curSurf = &model->Surfs( curPlaneNode->iSurf );

			UClass* PhysMatClass = NULL;
			if(curSurf->Material && curSurf->Material->GetMaterial() && curSurf->Material->GetMaterial()->PhysicalMaterial)
			{
				PhysMatClass = curSurf->Material->GetMaterial()->PhysicalMaterial;
			}
			else
			{
				PhysMatClass = UPhysicalMaterial::StaticClass();
			}
			check( PhysMatClass );
			check( PhysMatClass->IsChildOf(UPhysicalMaterial::StaticClass()) );
			NxMaterialIndex MatIndex = GEngine->FindMaterialIndex( PhysMatClass );

			int vertexOffset = curPlaneNode->iVertPool;

			if( (curPlaneNode->NumVertices > 0) && !(curSurf->PolyFlags & PF_NotSolid)) /* If there are any triangles to add. */
			{
				for( int i = 2; i < curPlaneNode->NumVertices; i++ )
				{
					// Verts, indices added as a fan from v0
					FBSPTriIndices ti;
					ti.v0 = model->Verts( vertexOffset ).pVertex;
					ti.v1 = model->Verts( vertexOffset + i - 1 ).pVertex;
					ti.v2 = model->Verts( vertexOffset + i ).pVertex;

					tris.AddItem( ti );
					materials.AddItem( MatIndex );
				}
			}

			planeNodeIndex = curPlaneNode->iPlane;
		}

		// recurse back and iterate to front.
		if( curBspNode->iBack != INDEX_NONE )
		{
			GatherBspTrisRecursive(model, curBspNode->iBack, tris, materials );
		}

		nodeIndex = curBspNode->iFront;
	}
}
#endif // WITH_NOVODEX

void ULevel::InitLevelRBPhys()
{
#ifdef WITH_NOVODEX
	// JTODO: How do we handle different gravities in different parts of the level? Or changing gravity on the fly?
	FVector Gravity = GetLevelInfo()->GetDefaultPhysicsVolume()->Gravity;
	NxVec3 nGravity = U2NPosition(Gravity);

	NxSceneDesc SceneDesc;
	SceneDesc.gravity = nGravity;
	SceneDesc.broadPhase = NX_BROADPHASE_COHERENT;
	SceneDesc.collisionDetection = true;
	SceneDesc.maxTimestep = NxTIMESTEP;
	SceneDesc.maxIter = NxMAXITERATIONS;
#if NxUSEFIXEDTIMESTEP
	SceneDesc.timeStepMethod = NX_TIMESTEP_FIXED;
#else
	SceneDesc.timeStepMethod = NX_TIMESTEP_VARIABLE;
#endif

	SceneDesc.userContactReport = &ContactReportObject; // See the GNovodexSDK->setActorGroupPairFlags call below for which object will cause notification.
	SceneDesc.userNotify = &UserNotifyObject;

	DebugRenderer = new NovodexDebugRenderer(this);

	NovodexScene = GNovodexSDK->createScene(SceneDesc);
	check(NovodexScene);

	// Now create the actor representing the level BSP. 
	// Do nothing if no BSP nodes.
	if(Model->Nodes.Num() > 0)
	{
		// Generate a vertex buffer for all BSP verts at physics scale.
		TArray<FVector> PhysScaleTriVerts;
		PhysScaleTriVerts.Add( Model->Points.Num() );
		for(INT i=0; i<Model->Points.Num(); i++)
		{
			PhysScaleTriVerts(i) = Model->Points(i) * U2PScale;
		}

		// Harvest an overall index buffer for the level BSP.
		TArray<FBSPTriIndices> TriInidices;
		TArray<NxMaterialIndex> MaterialIndices;
		GatherBspTrisRecursive( Model, 0, TriInidices, MaterialIndices );
		check(TriInidices.Num() == MaterialIndices.Num());

		// Then create Novodex descriptor
		NxTriangleMeshDesc LevelBSPDesc;

		LevelBSPDesc.numVertices = PhysScaleTriVerts.Num();
		LevelBSPDesc.pointStrideBytes = sizeof(FVector);
		LevelBSPDesc.points = PhysScaleTriVerts.GetData();

		LevelBSPDesc.numTriangles = TriInidices.Num();
		LevelBSPDesc.triangleStrideBytes = sizeof(FBSPTriIndices);
		LevelBSPDesc.triangles = TriInidices.GetData();

		LevelBSPDesc.materialIndexStride = sizeof(NxMaterialIndex);
		LevelBSPDesc.materialIndices = MaterialIndices.GetData();

		LevelBSPDesc.flags = 0;

		// Create Novodex mesh from that info.
		NxTriangleMesh* LevelBSPTriMesh = GNovodexSDK->createTriangleMesh(LevelBSPDesc);

		NxTriangleMeshShapeDesc LevelBSPShapeDesc;
		LevelBSPShapeDesc.meshData = LevelBSPTriMesh;
		LevelBSPShapeDesc.meshFlags = 0;
		LevelBSPShapeDesc.materialIndex = GEngine->FindMaterialIndex( UPhysicalMaterial::StaticClass() );

		// Create actor description and instance it.
		NxActorDesc LevelBSPActorDesc;
		LevelBSPActorDesc.shapes.pushBack(&LevelBSPShapeDesc);

		LevelBSPActor = NovodexScene->createActor(LevelBSPActorDesc);
	}
#endif // WITH_NOVODEX
}

void ULevel::TermLevelRBPhys()
{
#ifdef WITH_NOVODEX
	if (NovodexScene)
	{
		// First, free the level BSP actor.
		if(LevelBSPActor)
		{
			NovodexScene->releaseActor(*LevelBSPActor);
			LevelBSPActor = NULL;
		}

		// Then release the scene itself.
		GNovodexSDK->releaseScene(*NovodexScene);
		NovodexScene = NULL;
	}

	if (DebugRenderer)
	{
		delete DebugRenderer;
		DebugRenderer = NULL;
	}
#endif // WITH_NOVODEX
}

void ULevel::TickLevelRBPhys(FLOAT DeltaSeconds)
{
#ifdef WITH_NOVODEX
	if (!NovodexScene)
		return;
	
	// Update wind forces for all primitive components with nonzero wind response.
	for (UINT Index = 0; Index < UINT(PhysPrimComponents.Num()); Index++)
	{
		PhysPrimComponents(Index)->UpdateWindForces();
	}

	FVector DefaultGravity = GetLevelInfo()->GetDefaultPhysicsVolume()->Gravity;
	NovodexScene->setGravity( U2NPosition(DefaultGravity) );

#if 0
	INT	NumSubSteps = appCeil(DeltaSeconds / NxTIMESTEP);
	NumSubSteps = Clamp<INT>(NumSubSteps, 1, NxMAXITERATIONS);

	FLOAT TimestepSize = DeltaSeconds/NumSubSteps;

	debugf( TEXT("%f %d"), TimestepSize, NumSubSteps );
	NovodexScene->setTiming( TimestepSize, NumSubSteps );
#endif

#if 0
	NovodexScene->setTiming( DeltaSeconds, 1 );
#endif

	BEGINCYCLECOUNTER(GPhysicsStats.RBDynamicsTime);
	NovodexScene->simulate(DeltaSeconds);
	NovodexScene->fetchResults(NX_RIGID_BODY_FINISHED, true);
	ENDCYCLECOUNTER;

	if (DebugRenderer)
	{
		NovodexScene->visualize();
		GNovodexSDK->visualize(*DebugRenderer);
	}
#endif // WITH_NOVODEX
}

void ALevelInfo::execSetLevelRBGravity(FFrame &Stack, RESULT_DECL)
{
	P_GET_VECTOR(NewGrav);
	P_FINISH;

#ifdef WITH_NOVODEX
	if(XLevel->NovodexScene)
	{
		NxVec3 nGravity = U2NPosition(NewGrav);
		XLevel->NovodexScene->setGravity(nGravity);
	}
#endif // WITH_NOVODEX
}

//// EXEC

static INT NxDumpIndex = -1;
UBOOL ExecRBCommands(const TCHAR* Cmd, FOutputDevice* Ar, ULevel* level)
{
#ifdef WITH_NOVODEX
	if( ParseCommand(&Cmd, TEXT("NXDUMP")) )
	{
		TCHAR File[64];
		TCHAR* FileNameBase = TEXT("UE3_NxCoreDump_");

		if( NxDumpIndex == -1 )
		{
			for( INT TestDumpIndex=0; TestDumpIndex<65536; TestDumpIndex++ )
			{
				appSprintf( File, TEXT("%s%03i.psc"), FileNameBase, TestDumpIndex );
				if( GFileManager->FileSize(File) < 0 )
				{
					NxDumpIndex = TestDumpIndex;
					break;
				}
			}
		}

		appSprintf( File, TEXT("%s%03i"), FileNameBase, NxDumpIndex++ );

		GNovodexSDK->coreDump( TCHAR_TO_ANSI(File), true );
	}
	else if( ParseCommand(&Cmd, TEXT("NXVIS")) )
	{
		struct { const TCHAR* Name; NxParameter Flag; FLOAT Size; } Flags[] =
		{
			// Axes
			{ TEXT("WORLDAXES"),			NX_VISUALIZE_WORLD_AXES,		1.f },
			{ TEXT("BODYAXES"),				NX_VISUALIZE_BODY_AXES,			1.f },

			// Contacts
			{ TEXT("CONTACTPOINT"),			NX_VISUALIZE_CONTACT_POINT,		1.f },
			{ TEXT("CONTACTNORMAL"),		NX_VISUALIZE_CONTACT_NORMAL,	1.f },
			{ TEXT("CONTACTERROR"),			NX_VISUALIZE_CONTACT_ERROR,		100.f },
			{ TEXT("CONTACTFORCE"),			NX_VISUALIZE_CONTACT_FORCE,		1.f },

			// Joints
			{ TEXT("JOINTLIMITS"),			NX_VISUALIZE_JOINT_LIMITS,		1.f },
			{ TEXT("JOINTLOCALAXES"),		NX_VISUALIZE_JOINT_LOCAL_AXES,	1.f },
			{ TEXT("JOINTWORLDAXES"),		NX_VISUALIZE_JOINT_WORLD_AXES,	1.f },
			{ TEXT("JOINTERROR"),			NX_VISUALIZE_JOINT_ERROR,		1.f },

			// Collision
			{ TEXT("COLLISIONAABBS"),		NX_VISUALIZE_COLLISION_AABBS,	1.f },
			{ TEXT("COLLISIONSHAPES"),		NX_VISUALIZE_COLLISION_SHAPES,	1.f },
			{ TEXT("COLLISIONAXES"),		NX_VISUALIZE_COLLISION_AXES,	1.f },
		};

		UBOOL bDebuggingActive = false;
		UBOOL bFoundFlag = false;
		for (int i = 0; i < ARRAY_COUNT(Flags); i++)
		{
			if (ParseCommand(&Cmd, Flags[i].Name))
			{
				NxReal Result = GNovodexSDK->getParameter(Flags[i].Flag);
				if (Result == 0.0f)
				{
					GNovodexSDK->setParameter(Flags[i].Flag, Flags[i].Size);
					Ar->Logf(TEXT("Flag set."));
				}
				else
				{
					GNovodexSDK->setParameter(Flags[i].Flag, 0.0f);
					Ar->Logf(TEXT("Flag un-set."));
				}

				bFoundFlag = true;
			}

			// See if any flags are true
			NxReal Result = GNovodexSDK->getParameter(Flags[i].Flag);
			if(Result > 0.f)
			{
				bDebuggingActive = true;
			}
		}

		// If no debugging going on - disable it using NX_VISUALIZATION_SCALE
		if(bDebuggingActive)
		{
			GNovodexSDK->setParameter(NX_VISUALIZATION_SCALE, 1.0f);
		}
		else
		{
			GNovodexSDK->setParameter(NX_VISUALIZATION_SCALE, 0.0f);
		}

		if(!bFoundFlag)
		{
			Ar->Logf(TEXT("Unknown Novodex visualization flag specified."));
		}

		return 1;
	}
#endif // WITH_NOVODEX

	return 0;
}

//////// GAME-LEVEL RIGID BODY PHYSICS STUFF ///////


void InitGameRBPhys()
{
#ifdef WITH_NOVODEX
	class UnrealAllocator : public NxUserAllocator
	{
	public:
		virtual void* mallocDEBUG(size_t Size, const char* Filename, int Line)
		{
			return appMalloc(Size);
		}

		virtual void* malloc(size_t Size)
		{
			return appMalloc(Size);
		}

		virtual void* realloc(void* Memory, size_t Size)
		{
			return appRealloc(Memory, Size);
		}

		virtual void free(void* Memory)
		{
			appFree(Memory);
		}
	};

	class UnrealOutputStream : public NxUserOutputStream
	{
	public:
		virtual void reportError(NxErrorCode ErrorCode, const char* Message, const char* Filename, int Line)
		{
			debugf(TEXT("[Novodex] Error (%d) in file %s, line %d: %s"), ErrorCode, ANSI_TO_TCHAR(Filename), Line, ANSI_TO_TCHAR(Message));
		}

		virtual NxAssertResponse reportAssertViolation(const char* Message, const char* Filename, int Line)
		{
			debugf(TEXT("[Novodex] Assert in file %s, line %d: %s"), ANSI_TO_TCHAR(Filename), Line, ANSI_TO_TCHAR(Message));
			return NX_AR_BREAKPOINT;
		}

		virtual void print(const char* Message)
		{
			debugf(TEXT("[Novodex] %s"), ANSI_TO_TCHAR(Message));
		}
	};

	GNovodexSDK = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION, new UnrealAllocator(), new UnrealOutputStream());
	check(GNovodexSDK);

	GNovodexSDK->setParameter(NX_MIN_SEPARATION_FOR_PENALTY, -0.005f);
	GNovodexSDK->setParameter(NX_ADAPTIVE_FORCE, 0);

	// Actor group '1' is vehicles. We want a callback when they hit other stuff in the world.
	GNovodexSDK->setActorGroupPairFlags(0, 1, NX_NOTIFY_ON_TOUCH);
#endif // WITH_NOVODEX
}

void DestroyGameRBPhys()
{
#ifdef WITH_NOVODEX
	if( GNovodexSDK )
		GNovodexSDK->release();
	GNovodexSDK = NULL;
#endif
}


/////////////////  /////////////////

UINT UEngine::FindMaterialIndex(UClass* PhysMatClass)
{
#ifdef WITH_NOVODEX
	// Return Novodex default material if not a physical material. Shouldn't really happen!
	if( !PhysMatClass->IsChildOf(UPhysicalMaterial::StaticClass()) )
	{
		return 0;
	}

	// Find the name of the PhysicalMaterial
	FName PhysMatName = PhysMatClass->GetFName();

	// If map is not already allocated, do it now.
	if(!MaterialMap)
	{
		MaterialMap = new TMap<FName, UINT>();
	}

	// Search for name in map.
	UINT* MatIndexPtr = MaterialMap->Find(PhysMatName);

	// If present, just return it.
	if(MatIndexPtr)
	{
		return *MatIndexPtr;
	}
	// If not, add to mapping.
	else
	{
		UPhysicalMaterial* PhysMat = (UPhysicalMaterial*)PhysMatClass->GetDefaultObject();

		NxMaterial Material;

		Material.dynamicFriction = PhysMat->Friction;
		Material.staticFriction = PhysMat->Friction;
		Material.frictionCombineMode = NX_CM_MULTIPLY;

		Material.restitution = PhysMat->Restitution;
		Material.restitutionCombineMode = NX_CM_MAX;

		NxMaterialIndex NewMatIndex = GNovodexSDK->addMaterial(Material);

		MaterialMap->Set( PhysMatName, NewMatIndex );

		return NewMatIndex;
	}
#else
	return 0;
#endif
}