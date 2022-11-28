#include "WarfareGame.h"

IMPLEMENT_CLASS(AWarAIController)

void AWarAIController::execAILog(FFrame &Stack,RESULT_DECL)
{
	P_GET_STR(logTxt);
	P_GET_NAME(logCategory);
	P_FINISH;
	// check to see if we need to create the log file
	if (AILogFile == NULL)
	{
		AILogFile = (AFileLog*)GetLevel()->SpawnActor(AFileLog::StaticClass(),NAME_None,Location,Rotation,NULL,1,0,this);
		check(AILogFile != NULL && "Failed to create AI log file");
		FString fileName(GetName());
		FString extension(TEXT(".ailog"));
		AILogFile->OpenLog(fileName,extension);
	}

	logTxt = FString::Printf(TEXT("[%2.3f] [%s] %s: %s"),Level->TimeSeconds,GetStateFrame()!=NULL && GetStateFrame()->StateNode!=NULL?GetStateFrame()->StateNode->GetName():TEXT("NONE"),*logCategory,*logTxt);
	AILogFile->Logf(logTxt);
}
