
REM get the latest files from P4
REM p4 sync //depot/UnrealEngine3/...@currentQABuildInTesting

REM integrate to the stable branch
REM p4 integrate -d -i -t -v -n //depot/UnrealEngine3/...@msewTest //depot/UnrealEngine3_Stable/...
REM p4 integrate -d -i -t  //depot/UnrealEngine3/...@latestUnrealEngine3 //depot/UnrealEngine3_Stable/...


REM this will do everything we need it to do.
REM using the msewTestBranch atm as that has the UTGame private dirs removed from the branch view
p4 integrate -d -Ds -Dt -i -t -v -b UnrealEngine3_Stable -s //depot/UnrealEngine3/...@115119



REM now accept all of "their" changes and submit as we are integrating from theirs to ours and theirs has been QA approved
p4 resolve -at
REM p4 submit

REM now that the stable build has gone through we will delete the OnlyUseQABuildSemaphore.txt
REM del /F \\Build-server\BuildFlags\OnlyUseQABuildSemaphore.txt


pause

