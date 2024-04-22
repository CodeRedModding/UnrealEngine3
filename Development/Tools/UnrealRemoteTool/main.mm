//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//
//  main.m
//  UnrealRemoteTool
//
//  Created by Josh Adams on 4/18/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <netdb.h>
#include <mach/mach_host.h>
#include <ifaddrs.h>

/** Command line options */
int64_t GOption_TaskMemoryAllocation = 600;
bool GOption_ShowDebugInfo = NO;
int GOption_ServerPort = 8199;

/** This is the number of commands that are allowed to execute at the same time */
int GMaxNumCommands = 8;

/** Calculate how many jobs we can do */
void UpdateMaxNumCommands()
{
	// get free memory
	vm_statistics Stats;
	mach_msg_type_number_t Size = HOST_VM_INFO_COUNT;
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &Size);
	
	vm_size_t PageSize;
	host_page_size(mach_host_self(), &PageSize);
	
	uint64_t FreeMemory = ((Stats.free_count + Stats.inactive_count) * PageSize) / (1024 * 1024);
	
	// calculate how many jobs will run with that amount of memory
	GMaxNumCommands = MAX(1, FreeMemory / GOption_TaskMemoryAllocation);
}

/** These types must match on client and server (note that client code is not included in this application) */
const int64_t TypeString = 0;
const int64_t TypeLong = 1;
const int64_t TypeBlob = 2;


class FClientHandler
{
public:
	/**
	 * Constructor
	 */
	FClientHandler()
	{
		PendingData = nil;
		bHandlingMessage = NO;
	}

	/**
	 * Destructor
	 */
	~FClientHandler()
	{
		PendingData = nil;
		
		// shutdown the socket
		CFSocketInvalidate(Socket);
	}

	/**
	 * Sets the socket for this client
	 */
	void SetSocket(CFSocketRef InSocket, CFRunLoopSourceRef InRunLoopSource)
	{
		Socket = InSocket;
		RunLoopSource = InRunLoopSource;
	}
	
	/**
	 * This will read any object supported type from the network stream and 
	 * return the object as a generic id pointer.
	 *
	 * @param Payload Data coming from stream
	 * @param OffsetIntoPayload [in/out] The offset in the Payload to read from, and updated to point to right after the object
	 *
	 * @param Read the object, or nil if there is not enough data (it's expected there is enough room)
	 */
	id ReadObject(NSData* Payload, int64_t& OffsetIntoPayload)
	{
		// get the buffer
		unsigned char* Bytes = ((unsigned char*)[Payload bytes]) + OffsetIntoPayload;
		
		// we need at least 2 longs to start an object, othewise we need to wait for more data
		if (([Payload length] - OffsetIntoPayload) < sizeof(int64_t) * 2)
		{
			return nil;
		}

		// read the type of data
		int64_t Type = *(int64_t*)Bytes;
		Bytes += sizeof(int64_t);
		OffsetIntoPayload += sizeof(int64_t);
		assert(Type == TypeString || Type == TypeLong || Type == TypeBlob);
		
		// simple Long type
		if (Type == TypeLong)
		{
			// skip over number
			OffsetIntoPayload += sizeof(int64_t);
			return [NSNumber numberWithLongLong:*(int64_t*)Bytes];
		}
		// arbitrary length string or blob
		else if (Type == TypeString || Type == TypeBlob)
		{
			// figure out length of string
			int64_t Length = *(int64_t*)Bytes;
			Bytes += sizeof(int64_t);
			OffsetIntoPayload += sizeof(int64_t);

			// we may not have the entire guy here!
			if (([Payload length] - OffsetIntoPayload) < Length)
			{
				// return nil to indicate not complete
				return nil;
			}
			
			// skip over the data
			OffsetIntoPayload += Length;
			
			// make the string or blob
			if (Type == TypeString)
			{
				return [[NSString alloc] initWithBytes:Bytes length:Length encoding:NSUTF8StringEncoding];
			}
			else 
			{
				return [NSData dataWithBytes:Bytes length:Length];
			}
		}
		
		return nil;
	}


	/**
	 * Process data coming over the network stream
	 */
	bool HandleData(CFDataRef DataRef)
	{
		bHandlingMessage = YES;
		
		// make a nice Obj-C object for the data
		NSData* IncomingData = (__bridge NSData*)DataRef;
		
		// this is the data that we will actually process
		NSData* Data = IncomingData;
		
		// if there was any previous data, join with it.
		if (PendingData != nil)
		{
			[PendingData appendData:IncomingData];
			Data = PendingData;
		}

		// figure out how big the ENTIRE mesage is (direct size loolup, not using an object)
		int64_t FullMessageSize = 0;
		if ([Data length] >= sizeof(int64_t))
		{
			memcpy(&FullMessageSize, [Data bytes], sizeof(int64_t));
		}
		
		// is there enough room to read the entire message? (and did we have enough to get the size)
		if (FullMessageSize == 0 || [Data length] < FullMessageSize)
		{
			// if not, and we don't already have pending data ready, create a pending data block from
			// the available non-mutable data
			if (PendingData == nil)
			{
				PendingData = [NSMutableData dataWithData:IncomingData];
			}
			
			// we don't continue until we have the ENTIRE message
			bHandlingMessage = NO;
			return false;
		}
		
		// at this point, we no longer need any pending data
		PendingData = nil;

		// this count moves through the data, so start after the length bytes
		int64_t OffsetIntoData = sizeof(int64_t);
		
		// now we know we have the whole message, let's get how many pairs there are
		NSNumber* Num = ReadObject(Data, OffsetIntoData);
		if (Num == nil)
		{
			NSLog(@"Received an ill formed message (failed to get num pairs), throwing the whole thing away");
			bHandlingMessage = NO;
			return false;
		}
		int64_t NumPairs = [Num longLongValue];
		
		// a message is a set of key/value pairs; this holds them
		NSMutableDictionary* Message = [NSMutableDictionary dictionaryWithCapacity:NumPairs];

		// we continue until data runs out (or we hit the number of pairs needed). it's an error if BOTH don't 
		// hit at the same time!
		while (OffsetIntoData < [Data length] && [Message count] < NumPairs)
		{
			// get a key 
			NSString* Key = ReadObject(Data, OffsetIntoData);
			
			// make sure enough data was available
			if (Key != nil)
			{
				// read a value (the entire thing should be there)
				id Value = ReadObject(Data, OffsetIntoData);

				// make sure enough data was available
				if (Value != nil)
				{
					[Message setObject:Value forKey:Key];
				}
			}
		}
		
		// make sure our assumptions about sizes were met
		if (OffsetIntoData < [Data length] || [Message count] < NumPairs)
		{
			NSLog(@"Received an ill formed message, throwing the whole thing away");
			bHandlingMessage = NO;
			return false;
		}
		
		// we are done getting the message, now execute it!
		ExecuteMessage(Message);
			
		// we are done, clean up!
		[Message removeAllObjects];
			
		bHandlingMessage = NO;
		return true;
	}
	
	/**
	 * Retrieves information about a file
	 *
	 * @param Filename Path to the file
	 * @param ClientNow Time on the other side, used to adjust for time differences on the client and server
	 * @param FileSize [out] The length of the file
	 * @param ModificationTime [out] The timestamp of the file 
	 * @param Error [out] Returns any errors getting the info
	 */
	void GetFileInfo(NSString* Filename, int64_t ClientNow, int64_t& FileSize, int64_t& ModificationTime, NSError** Error)
	{
		// default to file doesn't exist
		FileSize = -1;
		ModificationTime = 0;

		// does the file even exist?
		if ([[NSFileManager defaultManager] fileExistsAtPath:Filename])
		{
			// if so, look up info
			NSDictionary* Attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:Filename error:Error];
			if (*Error == nil)
			{
				// calculate difference in machine times
				NSDate* Now = [NSDate dateWithTimeIntervalSinceNow:0];
				int64_t NowSeconds = (int64_t)[Now timeIntervalSinceReferenceDate];
				int64_t MachineDiff = ClientNow - NowSeconds;
				
				// get the file's mod time
				NSDate* ModTime = [Attributes fileModificationDate];
				
				// turn time into seconds since 2001, chopping off any sub-second time
				ModificationTime = (int64_t)[ModTime timeIntervalSinceReferenceDate];
				
				// now add in the machine difference to put our time into client time
				ModificationTime += MachineDiff;
				
				// pull out the size of the file
				FileSize = [Attributes fileSize]; 
			}
		}	
	}

	/**
	 * Once a message has completely come over the wire, this function will process it
	 */
	void ExecuteMessage(NSDictionary* Message)
	{
		// tracking a bug with this
		CFSocketRef SocketAtStart = Socket;
		
		// all messages must have a command name
		NSString* CommandName = [Message objectForKey:@"CommandName"];  
		assert(CommandName != nil);

		NSString* ShortCommandName = [CommandName lastPathComponent];
		NSLog(@"Executing a command %@", ShortCommandName);
		
		// most have CommandArgs, so just get it here
		NSString* CommandArgs = [Message objectForKey:@"CommandArgs"];
		
		if (GOption_ShowDebugInfo)
		{
			NSLog(@"Command = %@ %@", CommandName, CommandArgs);
		}
		
		static int CurrentNumCommands = 0;

		// make space for the result to send back
		NSDictionary* Result = [[NSMutableDictionary alloc] initWithCapacity:0];
		int64_t ExitCode = 0;
		
		// allow for generic errors - if this is non-null at the end, the CommandOutput will be set 
		// to the error, and the ExitCode will be -1
		NSError* Error = nil;

		// handle special case commands
		if ([CommandName isEqualToString:@"rpc:dummy"])
		{
			// nothing to do
		}
		else if ([CommandName isEqualToString:@"rpc:command_slots_available"])
		{
			// add one to account for this task taking up a slot that won't be taken once this completes
			NSString* Output = [NSString stringWithFormat:@"%d", GMaxNumCommands - CurrentNumCommands + 1];
			[Result setValue:Output forKey:@"CommandOutput"];
		}
		else if ([CommandName isEqualToString:@"rpc:makedirectory"])
		{
			// safely create a directory
			BOOL bIsDirectory;
			if ([[NSFileManager defaultManager] fileExistsAtPath:CommandArgs isDirectory:&bIsDirectory])
			{
				if (!bIsDirectory)
				{
					[Result setValue:[NSString stringWithFormat:@"Can't create directory %@ on top of file", CommandArgs] forKey:@"CommandOutput"];
					ExitCode = -1;
				}
			}
			else 
			{
				// actually make the directory if it wasn't there before
				[[NSFileManager defaultManager] createDirectoryAtPath:CommandArgs withIntermediateDirectories:YES attributes:nil error:&Error];
				
				if (Error == nil)
				{
					[Result setValue:[NSString stringWithFormat:@"Created %@", CommandArgs] forKey:@"CommandOutput"];
				}
			}

		}
		else if ([CommandName isEqualToString:@"rpc:removedirectory"])
		{
			[[NSFileManager defaultManager] removeItemAtPath:CommandArgs error:&Error];
			if (Error == nil)
			{
				[Result setValue:[NSString stringWithFormat:@"Removed %@", CommandArgs] forKey:@"CommandOutput"];
			}
		}
		else if ([CommandName isEqualToString:@"rpc:getfileinfo"])
		{
			NSNumber* ClientNow = [Message objectForKey:@"ClientNow"];
			int64_t ClientNowSeconds = [ClientNow longLongValue];
			
			int64_t FileSize, ModificationTime;
			GetFileInfo(CommandArgs, ClientNowSeconds, FileSize, ModificationTime, &Error);

			// pass back results
			[Result setValue:[NSNumber numberWithLongLong:ModificationTime] forKey:@"ModificationTime"];
			[Result setValue:[NSNumber numberWithLongLong:FileSize] forKey:@"Length"];
		}
		else if ([CommandName isEqualToString:@"rpc:batchfileinfo"])
		{
			NSString* FileList = [Message objectForKey:@"Files"];
			NSNumber* ClientNow = [Message objectForKey:@"ClientNow"];
			int64_t ClientNowSeconds = [ClientNow longLongValue];
			
			int64_t FileSize, ModificationTime;
	
			// break up the DestNames into an array of filenames
			NSArray* Filenames = [FileList componentsSeparatedByString:@"\n"];
			
			// make an array to hold results
			int NumPairs = [Filenames count];
			int64_t* Pairs = new int64_t[NumPairs * 2];
			int FileIndex = 0;
			for (NSString* Filename in Filenames)
			{
				// handle the special case of empty filename at the end
				if ([Filename length] == 0 && FileIndex == NumPairs - 1)
				{
					NumPairs--;
					break;
				}
				
				// get the info for a single file
				GetFileInfo(Filename, ClientNowSeconds, FileSize, ModificationTime, &Error);
				Pairs[FileIndex * 2 + 0] = FileSize;
				Pairs[FileIndex * 2 + 1] = ModificationTime;
				
				// move on to the next one
				FileIndex++;
			}
			
			// pass back results
			[Result setValue:[NSData dataWithBytes:Pairs length:sizeof(int64_t) * 2 * NumPairs] forKey:@"FileInfo"];

			delete [] Pairs;
		}
		else if ([CommandName isEqualToString:@"rpc:upload"])
		{
			NSString* Filename = CommandArgs;
			NSData* Contents = [Message objectForKey:@"Contents"];
			NSNumber* bAppend = [Message objectForKey:@"bAppend"];
			NSNumber* SrcFiletime = [Message objectForKey:@"SrcFiletime"];
			
			// make sure the directory exists
			NSString* Directory = [Filename stringByDeletingLastPathComponent];
			[[NSFileManager defaultManager] createDirectoryAtPath:Directory withIntermediateDirectories:YES attributes:nil error:&Error];
			
			if (Error == nil)
			{
				// do we want to append to the file?
				if ([bAppend longLongValue] > 0)
				{
					// open a file for appending, and move to the end to append
					NSFileHandle* File = [NSFileHandle fileHandleForUpdatingAtPath:Filename];
					[File seekToEndOfFile];

					// actually write the contents of the file
					[File writeData:Contents];
					
					// peace out!
					[File closeFile];
				}
				else
				{
					// open a file for writing, destroying what's there
					[[NSFileManager defaultManager] createFileAtPath:Filename contents:Contents attributes:nil];
				}

				
				// now set the modification time to the source time
				NSDate* ClientModTime = [NSDate dateWithTimeIntervalSinceReferenceDate:[SrcFiletime longLongValue]];
				NSDictionary* Attributes = [NSDictionary dictionaryWithObjectsAndKeys:ClientModTime, NSFileModificationDate, nil];
				[[NSFileManager defaultManager] setAttributes:Attributes ofItemAtPath:Filename error:&Error];
				
				if (Error == nil)
				{
					[Result setValue:[NSString stringWithFormat:@"Uploaded %@...", Filename] forKey:@"CommandOutput"];
				}
			}
		}
		else if ([CommandName isEqualToString:@"rpc:download"])
		{
			NSString* Filename = CommandArgs;
			NSNumber* MaxChunkSize = [Message objectForKey:@"MaxChunkSize"];
			NSNumber* StartNum = [Message objectForKey:@"Start"];
			int64_t Start = [StartNum longLongValue];
			
			// does the file even exist?
			if ([[NSFileManager defaultManager] fileExistsAtPath:CommandArgs])
			{
				// if so, look up info
				NSDictionary* Attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:CommandArgs error:&Error];

				// open a file for reading, and seek to read start spot
				NSFileHandle* File = [NSFileHandle fileHandleForReadingAtPath:Filename];
				[File seekToFileOffset:Start];

				int64_t FileSize = [Attributes fileSize];
				// read up to the end of the file, or the max chunk size
				int64_t AmountToRead = MIN(FileSize - Start, [MaxChunkSize longLongValue]);
				NSData* Contents = [File readDataOfLength:AmountToRead];

				// peace out!
				[File closeFile];

				// tell the other end to keep asking for more
				if (Start + AmountToRead < FileSize)
				{
					[Result setValue:@"1" forKey:@"NeedsToContinue"];
				}
				[Result setValue:Contents forKey:@"Contents"];
			}
		}
		else if ([CommandName isEqualToString:@"rpc:getnewerfiles"])
		{
			NSString* DestNames = [Message objectForKey:@"DestNames"];
			NSData* SourceTimes = [Message objectForKey:@"SourceTimes"];
			
			// break up the DestNames into an array of filenames
			NSArray* Filenames = [DestNames componentsSeparatedByString:@"\n"];

			// this the list of file indices that are newer
			int64_t* NewerFileIndices = new int64_t[[Filenames count]];
			
			int FileIndex = 0;
			int NewerFileIndex = 0;
			for (NSString* Filename in Filenames)
			{
				if (Filename != nil && [Filename length] > 0)
				{
					// get the local filetime
					NSDictionary* Attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:Filename error:&Error];
					
					// if there was an error getting timestamp, assume if needs to be copied
					bool bNeedsToCopy = NO;
					if (Error != nil)
					{
						bNeedsToCopy = YES;
						Error = nil;
					}
					else 
					{
						int64_t LocalTime = (int64_t)[[Attributes fileModificationDate] timeIntervalSinceReferenceDate];
						
						// pull source file time out of the array of times
						int64_t SourceTime;
						memcpy(&SourceTime, (unsigned char*)[SourceTimes bytes] + FileIndex * sizeof(int64_t), sizeof(int64_t));
						
						if (SourceTime > LocalTime)
						{
							bNeedsToCopy = YES;
						}
					}
					
					// any files that are newer on 
					if (bNeedsToCopy)
					{
						NewerFileIndices[NewerFileIndex++] = FileIndex;
					}
				}
				
				FileIndex++;
			}
			
			// pass back the list of integers as a data blob
			[Result setValue:[NSData dataWithBytes:NewerFileIndices length:NewerFileIndex * sizeof(int64_t)] forKey:@"NewerFiles"];
			
			delete [] NewerFileIndices;
		}
		// otherwise execute a commandline command
		else 
		{
			// make sure we can execute (we only allow a certain number of operations at the same time)
			bool bCanExecute = NO;
			while (!bCanExecute)
			{
				int NewNumCommands = OSAtomicIncrement32(&CurrentNumCommands);
				if (NewNumCommands > GMaxNumCommands)
				{
					OSAtomicDecrement32(&CurrentNumCommands);
					[NSThread sleepForTimeInterval:0.3f];
				}
				else 
				{
					bCanExecute = YES;
				}
			}		
			
			NSLog(@"Executing commandline action %@, %d / %d slots taken", ShortCommandName, CurrentNumCommands, GMaxNumCommands);

			// handle working directory
			NSString* WorkingDir = [Message objectForKey:@"WorkingDirectory"];
			
			@try 
			{
				// if an output file is specified, make sure the directory exists
				NSString* OutputFile = [Message objectForKey:@"OutputFile"];
				if (OutputFile != nil)
				{
					NSString* OutputDirectory = [OutputFile stringByDeletingLastPathComponent];
					[[NSFileManager defaultManager] createDirectoryAtPath:OutputDirectory withIntermediateDirectories:YES attributes:nil error:&Error];
				}
				
				// create task object
				NSTask* Task = [[NSTask alloc] init];
				
				// by going through bash, we can treat the entire commandline as a single unit
				[Task setLaunchPath:@"/bin/bash"];
				
				// arguments are the -c token followed by full commandline
				NSString* BashCommand = [NSString stringWithFormat:@"%@ %@", CommandName, CommandArgs];
				[Task setArguments:[NSArray arrayWithObjects:@"-c", BashCommand, nil]];
				
				// it must exist or we get an exception
				[[NSFileManager defaultManager] createDirectoryAtPath:WorkingDir withIntermediateDirectories:YES attributes:nil error:&Error];
				[Task setCurrentDirectoryPath:WorkingDir];
				
				
				// set up in/out pipes
				[Task setStandardInput:[NSPipe pipe]];
				NSPipe* StdOutPipe = [NSPipe pipe];
				[Task setStandardOutput:StdOutPipe];
				NSPipe* StdErrPipe = [NSPipe pipe];
				[Task setStandardError:StdErrPipe];

				// stdout/err file wrappers
				NSFileHandle* StdOutFile = [StdOutPipe fileHandleForReading];
				NSFileHandle* StdErrFile = [StdErrPipe fileHandleForReading];

				NSMutableData* OutputData = [NSMutableData dataWithLength:0];
				
				StdOutFile.readabilityHandler = ^(NSFileHandle* File)
				{
					@synchronized(OutputData)
					{
						[OutputData appendData:[File availableData]];
					}
				};
				StdErrFile.readabilityHandler = StdOutFile.readabilityHandler;
				
				// execute the task!
				[Task launch];
				[Task waitUntilExit];
				
				// read in whatever is left over
				[OutputData appendData:[StdOutFile readDataToEndOfFile]];
				[OutputData appendData:[StdErrFile readDataToEndOfFile]];

				// get return code
				ExitCode = [Task terminationStatus];

				// convert to string to pass back
				NSString* OutputString = [[NSString alloc] initWithData:OutputData encoding:NSUTF8StringEncoding];
		
				if (GOption_ShowDebugInfo && [OutputString length] > 0)
				{
					NSLog(@"Command returned\n%@", OutputString);
				}
				
				// return output and exit code
				[Result setValue:OutputString forKey:@"CommandOutput"];
			}
			@catch (NSException* Exception)
			{
				NSString* ErrorString = [NSString stringWithFormat:@"Received an exception while handling command:\nDirectory: %@\nCommand: %@\nArgs:%@\nException:%@",
					  WorkingDir, CommandName, CommandArgs, Exception];

				NSLog(@"%@", ErrorString);						 
				
				// return output and exit code
				[Result setValue:ErrorString forKey:@"CommandOutput"];
				ExitCode = -1;
			}

			// we are no longer executing, update num current commands
			int NewNumCommands = OSAtomicDecrement32(&CurrentNumCommands);
			if (NewNumCommands == 0)
			{
				UpdateMaxNumCommands();
			}

			NSLog(@"Done commandline %@, %d / %d slots taken", ShortCommandName, NewNumCommands, GMaxNumCommands);
		}

		if (Error != nil)
		{
			NSString* OutputFile = OutputFile = [Message objectForKey:@"OutputFile"];
			NSString* ErrorString = [NSString stringWithFormat:@"Received an file error while handling command:\nCommand: %@\nArgs:%@OutputFile:%@\nError:%@",
									 CommandName, CommandArgs, OutputFile ? OutputFile : @"None", [Error description]];
			NSLog(@"%@", ErrorString);						 
			
			[Result setValue:ErrorString forKey:@"CommandOutput"];
			ExitCode = -1;
		}
		
		// send the results back
		[Result setValue:[NSNumber numberWithLongLong:ExitCode] forKey:@"ExitCode"];
		if (Socket != SocketAtStart)
		{
			NSLog(@"========================== SOCKET HAS CHANGED! GOing to use older value to reply ===============================");
			Socket = SocketAtStart;
		}
		SendReply(Socket, Result);

		NSLog(@"Done command %@", ShortCommandName);
	}

	/**
	 * Sends the given long over the network
	 */
	void SendLong(NSMutableData* Data, int64_t Value)
	{
		// build up a long object
		int64_t LocalData[2];
		LocalData[0] = TypeLong;
		LocalData[1] = Value;

		[Data appendBytes:LocalData length:sizeof(LocalData)];
	}
	 
	/**
	 * Sends the given string object over the network
	 */
	void SendString(NSMutableData* Data, NSString* Value)
	{
		// convert to UTF8
		NSData* StringData = [Value dataUsingEncoding:NSUTF8StringEncoding];
		int64_t Length = [StringData length];

		// build up a long object
		int64_t LocalData[2];
		LocalData[0] = TypeString;
		LocalData[1] = Length;

		[Data appendBytes:LocalData length:sizeof(LocalData)];

		// and append the string
		[Data appendData:StringData];
	}
	 
	/**
	 * Sends the given binary blob object over the network
	 */
	void SendBlob(NSMutableData* Data, NSData* Value)
	{
		// build up a long object
		int64_t LocalData[2];
		LocalData[0] = TypeBlob;
		LocalData[1] = [Value length];
		
		[Data appendBytes:LocalData length:sizeof(LocalData)];
		
		// and append the blob
		[Data appendData:Value];
	}

	void SendReply(CFSocketRef Socket, NSDictionary* Reply)
	{
		int64_t FullMessageLength = 0;

		// make a buffer to jam data into
		NSMutableData* MessageData = [NSMutableData dataWithCapacity:sizeof(int64_t)];
		
		// leave space for the size
		[MessageData appendBytes:&FullMessageLength length:sizeof(int64_t)];
		
		// send how long the dictionary is
		int64_t NumPairs = [Reply count];
		SendLong(MessageData, NumPairs);
		
		[Reply enumerateKeysAndObjectsUsingBlock:^(NSString* Key, id Value, BOOL* bStop)
		{
			// send the key
			SendString(MessageData, Key);
			
			// send the value
			if ([Value isKindOfClass:[NSString class]])
			{
				SendString(MessageData, Value);
			}
			else if ([Value isKindOfClass:[NSData class]])
			{
				SendBlob(MessageData, Value);
			}
			else
			{
				NSNumber* Number = Value;
				SendLong(MessageData, [Number longLongValue]);
			}
		}];
		
		// put the length into the first bytes
		FullMessageLength = [MessageData length];
		memcpy((void*)[MessageData bytes], &FullMessageLength, sizeof(int64_t));

		// now send over the wire
	   CFSocketSendData(Socket, NULL, (__bridge CFDataRef)MessageData, 0);
	}

	
	/** The socket this handler is dealing with */
	CFSocketRef Socket;
	CFRunLoopSourceRef RunLoopSource;
	
	BOOL bHandlingMessage;

private:	
			
	/** If we are in the middle of a large value, this is the pending data */
	NSMutableData* PendingData;
};



void ClientCallback(CFSocketRef Socket, CFSocketCallBackType CallbackType, CFDataRef Address, const void* Data, void* Info)
{
	@autoreleasepool
	{
		CFDataRef CFData = (CFDataRef)Data;
		
		FClientHandler* Wrapper = (FClientHandler*)Info;
		
		// if we get a packet while handling a message (????) dump something
		if (Wrapper->bHandlingMessage)
		{
			NSLog(@"====================================== RECEIVED DATA WHILE PROCESSING MESSAGE! ====================================");
			while (Wrapper->bHandlingMessage)
			{
				[NSThread sleepForTimeInterval:0.1];
			}
		}
		
		// check for other end closing the socket, we don't close from this end
		if (CFDataGetLength(CFData) == 0)
		{
	//		NSLog(@"Client has left!");
			
			// clean up helper object, and its socket
			delete Wrapper;
			
			CFRunLoopStop(CFRunLoopGetCurrent());
		}
		else 
		{
			// process the data into a message
			CFRunLoopRemoveSource(CFRunLoopGetCurrent(), Wrapper->RunLoopSource, kCFRunLoopDefaultMode);
			Wrapper->HandleData(CFData);
			CFRunLoopAddSource(CFRunLoopGetCurrent(), Wrapper->RunLoopSource, kCFRunLoopDefaultMode);
		}
	}
}

@interface ThreadHelper : NSObject
{
	
}

- (void)ThreadFunc:(NSData*)Data;

@end

@implementation ThreadHelper

- (void)ThreadFunc:(NSData*)Data
{
	FClientHandler* Handler = new FClientHandler;
	
	// keep a dictionary during the life of the socket
	CFSocketContext Context;
	Context.info = Handler;
	Context.copyDescription = NULL;
	Context.retain = NULL;
	Context.release = NULL;
	Context.version = 0;
	
	// wrap the new socket in a CFSocket object
	CFSocketRef ClientSocket = CFSocketCreateWithNative(NULL, *((CFSocketNativeHandle*)[Data bytes]), kCFSocketDataCallBack, ClientCallback, &Context);


	// disable SIGPIPE
	unsigned int Yes = 1;
	setsockopt(CFSocketGetNative(ClientSocket), SOL_SOCKET, SO_NOSIGPIPE, &Yes, sizeof(Yes));
	
	// start the background processing of the socket
	CFRunLoopSourceRef ClientSource = CFSocketCreateRunLoopSource(NULL, ClientSocket, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), ClientSource, kCFRunLoopDefaultMode);

	Handler->SetSocket(ClientSocket, ClientSource);

	CFRunLoopRun();
	
	CFRelease(ClientSocket);
	CFRelease(ClientSource);
}

@end

ThreadHelper* GThreadHelper;

void ServerCallback(CFSocketRef Socket, CFSocketCallBackType CallbackType, CFDataRef Address, const void* Data, void* Info)
{
//	NSLog(@"Accepted a client, spawning a thread!");	

	NSData* Param = [NSData dataWithBytes:Data length:sizeof(CFSocketNativeHandle)];
	[NSThread detachNewThreadSelector:@selector(ThreadFunc:) toTarget:GThreadHelper withObject:Param];
}


int main(int argc, const char * argv[])
{
	@autoreleasepool 
	{
		GThreadHelper = [[ThreadHelper alloc] init];

		// process command line options
		for (int ArgIndex = 1; ArgIndex < argc; ArgIndex++)
		{
			const char* Arg = argv[ArgIndex];
			
			if (strcasecmp(Arg, "-h") == 0 || strcasecmp(Arg, "-?") == 0 || strcasecmp(Arg, "-help") == 0 || strcasecmp(Arg, "--help") == 0)
			{
				printf("\nUsage: %s [-h] [-port=<ServerPort>] [-taskmem=<TaskMemInMB>] [-v]\n\n", argv[0]);
				return 0;
			}
			else if (strncasecmp(Arg, "-port=", 6) == 0)
			{
				int NewPort = atoi(Arg + 6);
				if (NewPort > 0)
				{
					GOption_ServerPort = NewPort;
				}
			}
			else if (strncasecmp(Arg, "-taskmem=", 9) == 0)
			{
				int TaskMem = atoi(Arg + 9);
				if (TaskMem > 0)
				{
					GOption_TaskMemoryAllocation = TaskMem;
				}
			}
			else if (strcasecmp(Arg, "-v") == 0)
			{
				GOption_ShowDebugInfo = YES;
			}
		}
    
		UpdateMaxNumCommands();

		// create single server socket, it's callbacks will handle client connections
		CFSocketRef ServerSocket = CFSocketCreate(NULL, 0, SOCK_STREAM, 0, kCFSocketAcceptCallBack, ServerCallback, NULL);
		unsigned int Yes = 1;
		setsockopt(CFSocketGetNative(ServerSocket), SOL_SOCKET, SO_REUSEADDR, &Yes, sizeof(Yes));
		setsockopt(CFSocketGetNative(ServerSocket), SOL_SOCKET, SO_REUSEPORT, &Yes, sizeof(Yes));

		// add to main thread's run loop
		CFRunLoopSourceRef ServerSource = CFSocketCreateRunLoopSource(NULL, ServerSocket, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), ServerSource, kCFRunLoopDefaultMode);
		
		// set as server
		sockaddr_in Addr;
		Addr.sin_family = AF_INET;
		Addr.sin_port = htons(GOption_ServerPort);
		Addr.sin_addr.s_addr = htonl(INADDR_ANY);
		NSData* Data = [NSData dataWithBytes:&Addr length:sizeof(Addr)];
		CFSocketSetAddress(ServerSocket, (__bridge CFDataRef)Data);

		NSLog(@"UnrealRemoteTool is now running on port %d, allowing %d jobs at once", GOption_ServerPort, GMaxNumCommands);

		// socket will stop this run loop when the socket closes
		CFRunLoopRun();
		
		CFRelease(ServerSocket);
		CFRelease(ServerSource);

	}
    return 0;
}

