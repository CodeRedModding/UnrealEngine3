REM obj is the username which can access all of the DB's that the proxy is going to send data to

sc create UnrealDatabaseProxy binPath= "\"C:\Users\keith.newton\Documents\Visual Studio 2008\Projects\UnrealDatabaseProxy\UnrealDatabaseProxy\bin\Debug\UnrealDatabaseProxy.exe\"" type= own start= demand obj= teamonline@epicgames.net password= <passwd> DisplayName= UnrealDatabaseProxy


