
#include "Engine.h"

#define IS_STANDALONE_APPLICATION 0

#import "IPhoneHome.h"
#import <Foundation/NSURL.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <UIKit/UIDevice.h>
#import <UIKit/UIKit.h>
#import "IPhoneAsyncTask.h"
#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <TargetConditionals.h>

#include <mach/mach.h>
#include <mach/mach_time.h>
#include "IPhoneObjCWrapper.h"

#include <time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdlib.h>

extern uint64_t GStartupFreeMem;
extern uint64_t GStartupUsedMem;

static Boolean doFlagsIndicateWiFi(SCNetworkReachabilityFlags flags);

// Increment if you make non-compatible changes to the phone home xml (not including the game specific block)
static const int GPhoneHomeXMLVer = 4;

// URL that phone home stats are sent to.
// Internally, we send it to "et.epicgames.com". If you set up your own service, define it's URL here.
#if !defined(PHONE_HOME_URL)
#define PHONE_HOME_URL L"tempuri.org"
#endif


static Boolean GIsPhoneHomeInFlight = FALSE;

extern int main(int argc, char *argv[]);

/**
 * Creates a string of hex values for the given array
 * The input 0xAB 0xCD 0xEF would return "abcdef"
 *
 * @param BytesToConvert Data to convert to a string
 * @param Length Number of bytes in the BytesToConvert array
 *
 * @return A string representing hex values as a string
 */
NSString* CArrayToHexNSString(BYTE* BytesToConvert, INT Length)
{	
	// allocate space for 2 characters per byte
	ANSICHAR* StringBuffer = (ANSICHAR*)appMalloc(sizeof(ANSICHAR) * (Length * 2 + 1));
	StringBuffer[Length * 2] = '\0';

	// convert into StringBuffer
	static const ANSICHAR ALPHA_CHARS[] = "0123456789abcdef";
	for (INT ByteIndex = 0; ByteIndex < Length; ByteIndex++)
	{
		StringBuffer[2 * ByteIndex + 0] = ALPHA_CHARS[(BytesToConvert[ByteIndex] >> 4) & 0x0F];
		StringBuffer[2 * ByteIndex + 1] = ALPHA_CHARS[BytesToConvert[ByteIndex] & 0x0F];
	}

	// make an NSString from the buffer (autoreleased)
	NSString* DataString = [NSString stringWithCString:StringBuffer encoding:NSASCIIStringEncoding];
	
	// clean up 
	appFree(StringBuffer);
	return DataString;
}

/** Checks for the encryption info command in the MACH-O header.
    Returns 0 if the segment is found and mode is non-zero
	Returns 1 if the segment is found and mode is zero (no encryption; either a dev version or a pirated version)
	Returns 2 if the segment wasn't found (something went wrong)
  */
int CheckHeader()
{
	mach_header* ExecutableHeader;
	Dl_info Info;

	if ((dladdr((const void*)main, &Info) == 0) || (Info.dli_fbase == NULL))
	{
		// Problem finding entry point
	}
	else
	{
		ExecutableHeader = (mach_header*)Info.dli_fbase;

		// Run through the load commands looking for an encryption info command
		uint8_t* CommandPtr = (uint8_t*)(ExecutableHeader + 1);
		for (int i = 0; i < ExecutableHeader->ncmds; ++i)
		{
			// Check for an encryption info command
			load_command* LC = (struct load_command*)CommandPtr;
			if (LC->cmd == LC_ENCRYPTION_INFO)
			{
				// If the encryption mode is 0, it means no encryption.  This isn't expected from an app store app
				// (it is 0 during development, but we catch that via other means).
				encryption_info_command* EIC = (encryption_info_command*)LC;
				return (EIC->cryptid == 0) ? 1 : 0;
			}

			// Advance to the next load command
			CommandPtr += LC->cmdsize;
		}
	}

	// Unable to find the load command
	return 2;
}

// This function attempts to check for a common process that can only be
// running on a jailbroken phone.  It relies on the user running an application
// (SbSettings) that isn't installed by default when jailbreaking, so it's not
// a very reliable check, but it can be an extra nugget of information
// @return A count of bad processes found, or -1 if the check is unable to complete
inline int CheckJailbreakBySysctl()
{
	int BadProcessCount = 0;

	int Name[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
	unsigned int NameLength = 4;

	const int NumBadProcesses = 1;
	const char* BadProcesses[NumBadProcesses] = {"sbsettingsd"};

	// Get the size of the report
	size_t RequiredSizeInBytes;
	sysctl(Name, NameLength, NULL, &RequiredSizeInBytes, NULL, 0);

	size_t NumProcs = RequiredSizeInBytes / sizeof(kinfo_proc);
	if (NumProcs > 0)
	{
		// Allocate space for the result
		kinfo_proc* ProcessList = new kinfo_proc[NumProcs];

		// Get the data
		size_t ProcessListSizeInBytes = NumProcs * sizeof(kinfo_proc);
		UBOOL bSuccess = sysctl(Name, NameLength, ProcessList, &ProcessListSizeInBytes, NULL, 0) == 0;

		NumProcs = ProcessListSizeInBytes / sizeof(kinfo_proc);

		// Run thru the process list, looking for any of the jailbreak-only ones
		if (bSuccess && (NumProcs > 0))
		{
			for (INT ProcessIndex = 0; ProcessIndex < NumProcs; ++ProcessIndex)
			{
				char* ProcessName = ProcessList[ProcessIndex].kp_proc.p_comm;

				if (ProcessName != NULL)
				{
					for (INT BadProcessTestIndex = 0; BadProcessTestIndex < NumBadProcesses; ++BadProcessTestIndex)
					{
						if (strncmp(ProcessName, BadProcesses[BadProcessTestIndex], MAXCOMLEN) == 0)
						{
							++BadProcessCount;
						}
					}
				}
			}
		}
		else
		{
			BadProcessCount = -1;
		}

		delete [] ProcessList;
	}

	return BadProcessCount;
}

// Check to see if the phone is jailbroken by trying to look outside of our sandbox,
// for files that would only be present on a jailbroken phone.  This doesn't actually
// open the files, only checks for their existence, so we're not really trying to
// leave the sandbox.
// @return A count of bad files found
int CheckJailbreakBySandbox()
{
	int BadFileCount = 0;

	const int NumBadFiles = 2;
	const char* BadFiles[NumBadFiles] =
	{
		"/private/var/lib/apt",
		"/Applications/Cydia.app"
	};
	
	// Try to find any of the file paths that shouldn't exist/be accessible on a non-jailbroken phone
	for (INT BadFileIndex = 0; BadFileIndex < NumBadFiles; ++BadFileIndex)
	{
		NSString* TestPath = [NSString stringWithUTF8String: BadFiles[BadFileIndex]];

		Boolean bFileExists = [[NSFileManager defaultManager] fileExistsAtPath:TestPath];
		if (bFileExists == YES)
		{
			++BadFileCount;
		}
	}

	return BadFileCount;
}

/**
  This function scans the root directory and hashes all filenames / directory names found.
  It is used as part of the piracy checks, and has nothing to do with DLC.
  It is only named that because symbols in MM files aren't always stripped

  @return A string containing the hex representation of the SHA-1 hash of the sorted list of filenames in the main app directory (peer to Info.plist)
*/
NSString* ScanForDLC()
{
	// use the API to retrieve where the application is stored
	NSString* SearchDir = [[NSBundle mainBundle] resourcePath];
	
	// Get the list of files in the main directory of the application bundle and sort them
	NSArray* DirectoryContents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:SearchDir error:nil];
	NSArray* SortedContents = [DirectoryContents sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

	// Run through the list of files, hashing each filename / directory name
	FSHA1 ShaHasher;
	for (NSString* Filename in SortedContents)
	{
		NSString* LastPartOfFilename = [Filename lastPathComponent];
		int NumChars = [LastPartOfFilename length];

		char AssertWCharSizeIs4[(sizeof(wchar_t) == 4) ? 0 : -1];
		(void)AssertWCharSizeIs4;

		const void* FilenameChars = [LastPartOfFilename cStringUsingEncoding:NSUTF32StringEncoding];

		// Hash this filename
		ShaHasher.Update((const BYTE *)(FilenameChars), NumChars * sizeof(wchar_t));
	}

	//@TODO: Open the plist and hash it's contents (as a replacement for the SigningIdenitity check, which is pretty useless anyways)

	// Add any additional anti-tamper file detections here (take no action on them, just check and modify the hash)

	// Finalize the hash and convert it to a string
	BYTE FilenameListHash[20];
	ShaHasher.Final();
	ShaHasher.GetHash(FilenameListHash);

	NSString* HashOfRootFiles = CArrayToHexNSString(FilenameListHash, 20);
	return HashOfRootFiles;
}

@implementation IPhoneHome

- (void)didSucceed
{
	// if, somehow, this gets called after we got our data, ignore it
	if (Completed)
	{
		return;
	}
	Completed = YES;

	// reset our "since last upload" stats
	IPhoneSaveUserSettingU64("IPhoneHome::NumSurveyFailsSinceLast", 0);

	// record last upload time
	IPhoneSaveUserSettingU64("IPhoneHome::LastSuccess", (uint64_t)time(NULL));

	// reset our "since last submission stats"
	IPhoneSaveUserSettingU64("IPhoneHome::NumInvocations", 0);
	IPhoneSaveUserSettingU64("IPhoneHome::NumCrashes", 0);
	IPhoneSaveUserSettingU64("IPhoneHome::NumMemoryWarnings", 0);
	IPhoneSaveUserSettingU64("IPhoneHome::AppPlaytimeSecs", 0);

	debugf(TEXT("PhoneHome succeeded!"));

	GIsPhoneHomeInFlight = FALSE;
}

- (void)didFail
{
	// if, somehow, this gets called after we got our data, ignore it
	if (Completed)
	{
		return;
	}
	Completed = YES;

	// increment failure count
	IPhoneIncrementUserSettingU64("IPhoneHome::TotalSurveyFails");
	IPhoneIncrementUserSettingU64("IPhoneHome::NumSurveyFailsSinceLast");

	// record last failure time so we don't retry to soon
	IPhoneSaveUserSettingU64("IPhoneHome::LastFailure", (uint64_t)time(NULL));

	debugf(TEXT("PhoneHome succeeded!"));

	GIsPhoneHomeInFlight = FALSE;
}

- (void)doSendPayload
{
	// check for an encryption segment with encryption mode = decrypted, or no encryption segment at all
	int DevResult = CheckHeader(); // 0 is valid, 1 is pirated, 2 is missing / unknown LC

	// combine the payloads
	NSMutableString* Payload = [NSMutableString stringWithString:@"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"];
	[Payload appendFormat:@"<phone-home ver=\"%d\" dev=\"%d\">\n", GPhoneHomeXMLVer, DevResult];
	[Payload appendString:AppPayload];
	[Payload appendString:GamePayload];

	// end timing and record
	mach_timebase_info_data_t TimeBase;
	mach_timebase_info(&TimeBase);
	double dts = 1e-9 * (double)(TimeBase.numer * (mach_absolute_time() - CollectStart)) / (double)TimeBase.denom;
	[Payload appendFormat:@"<meta collect-time-secs=\"%.3lf\"/>\n", dts];

	// terminate and convert to data
	[Payload appendString:@"\n</phone-home>"];
	NSData* Data = [Payload dataUsingEncoding:NSUTF8StringEncoding];

	extern FString IPhoneGetMD5HashedMacAddress();

	// convert it to a URL (with OS4 weak reference workaround to the class)
	NSString* UrlStr = [NSString stringWithFormat:@"http://%s/PhoneHome-1?uid=%s&game=%@%s", 
		TCHAR_TO_ANSI(PHONE_HOME_URL), 
		TCHAR_TO_ANSI(*IPhoneGetMD5HashedMacAddress()), 
		[GameName stringByAddingPercentEscapesUsingEncoding:NSASCIIStringEncoding], 
		WiFiConn ? "" : "&wwan=1"];
	Class NSURLClass = NSClassFromString(@"NSURL");
	NSURL* Url = [NSURLClass URLWithString:UrlStr];

	// encode Payload and create a POST HTTP request to send it
	NSMutableURLRequest * Request = [NSMutableURLRequest requestWithURL:Url];
	[Request setHTTPMethod:@"POST"];
	[Request setHTTPBody:Data];
	[Request setValue:@"application/xml" forHTTPHeaderField:@"Content-Type"];
	[Request setValue:[NSString stringWithFormat:@"%llu", [Data length]] forHTTPHeaderField:@"Content-Length"];

	// assume it's a failure and record the time now so we don't retry to soon (but don't increment failure count)
	// it's possible we'll quit before the delegate tells us we've sent but it will still go through
	IPhoneSaveUserSettingU64("IPhoneHome::LastFailure", (uint64_t)time(NULL));
	
	// send the request asynchronously
	NSURLConnection* Conn = [NSURLConnection connectionWithRequest:Request delegate:self];
	if (Conn == nil)
	{
		// record that we failed
		[self didFail];
		
		// autorelease ourselves
		[self autorelease];
		return;
	}
}

/**
 * This method is called in the game thread, it is safe to invoke UPhoneHome from here. 
 * We schedule ourselves back into the app thread when the payload gathering is complete.
 */
- (void)collectStats
{
	GameName = [[[NSBundle mainBundle] infoDictionary] objectForKey: @"CFBundleIdentifier"];
	if (GameName == nil)
	{
		GameName = @"unknown";
	}

	NSString* PackagingMode = [[[NSBundle mainBundle] infoDictionary] objectForKey: @"EpicPackagingMode"];
	if (PackagingMode == nil)
	{
		PackagingMode = @"unknown";
	}

	NSString* AppVersionString = [[[NSBundle mainBundle] infoDictionary] objectForKey: @"EpicAppVersion"];
	if (AppVersionString == nil)
	{
		AppVersionString = @"unknown";
	}

	// Do jailbreak checks
	const int NumJailbrokenFilesFound = CheckJailbreakBySandbox();
	const int NumJailbrokenProcessesFound = CheckJailbreakBySysctl();

	// Do another piracy check (file hash)
	NSString* DLCHash = ScanForDLC();

	INT EngineVersion = GEngineVersion;

	// report the game name and engine version
	[GamePayload appendFormat:@"<game name=\"%@\" engver=\"%d\" pkgmode=\"%@\" appver=\"%@\" dlc=\"%@\" cf=\"%d\" cp=\"%d\">\n", 
		[[GameName stringByReplacingOccurrencesOfString: @"&" withString: @"&amp;"] 
			stringByReplacingOccurrencesOfString: @"\"" withString: @"&quot;"], 
		EngineVersion,
		[[PackagingMode stringByReplacingOccurrencesOfString: @"&" withString: @"&amp;"] 
			stringByReplacingOccurrencesOfString: @"\"" withString: @"&quot;"],
		[[AppVersionString stringByReplacingOccurrencesOfString: @"&" withString: @"&amp;"] 
			stringByReplacingOccurrencesOfString: @"\"" withString: @"&quot;"],
		DLCHash,
		NumJailbrokenFilesFound,
		NumJailbrokenProcessesFound
		];

	// implement game-specifc stats here (UPhoneHome)

	// close the game tag
	[GamePayload appendString:@"</game>\n"];

	// and, we're done!
	[self doSendPayload];
}

- (void)reachabilityDone:(id)bHasWifiIfNotNil
{	
	// did we find wifi?
	WiFiConn = bHasWifiIfNotNil != nil;

	// add stats about upload connectivity
	uint64_t TotalSurveys = IPhoneLoadUserSettingU64("IPhoneHome::TotalSurveys");
	uint64_t TotalSurveyFails = IPhoneLoadUserSettingU64("IPhoneHome::TotalSurveyFails");
	uint64_t TotalSurveyFailsSinceLast = IPhoneLoadUserSettingU64("IPhoneHome::NumSurveyFailsSinceLast");
	[AppPayload appendFormat:@"<upload total-attempts=\"%llu\" total-failures=\"%llu\" recent-failures=\"%llu\"/>\n", TotalSurveys, TotalSurveyFails, TotalSurveyFailsSinceLast];

	// get the device hardware type
	size_t hwtype_size;
	sysctlbyname("hw.machine", NULL, &hwtype_size, NULL, 0);
	char* hwtype = (char*)malloc(hwtype_size+1);
	hwtype[hwtype_size] = '\0';
	sysctlbyname("hw.machine", hwtype, &hwtype_size, NULL, 0);

	// get the UEID (UnrealEngine ID, MD5 hash of string based on MAC address)
	extern FString IPhoneGetSHA1HashedUDID();
	extern FString IPhoneGetMD5HashedUDID();

	// get other device info (os, device type, etc)
	UIDevice* dev = [UIDevice currentDevice];
	[AppPayload appendFormat:@"<device type=\"%@\" model=\"%s\" os-ver=\"%@\" md5=\"%s\" sha1=\"%s\"/>\n", 
		[dev model], 
		hwtype, 
		[dev systemVersion],
		TCHAR_TO_ANSI(*IPhoneGetMD5HashedUDID()),
		TCHAR_TO_ANSI(*IPhoneGetSHA1HashedUDID())];

	free(hwtype); // release hardware string

	// get locale (language settings) info
	NSLocale* loc = [NSLocale currentLocale];
	NSString* vc = [loc objectForKey:NSLocaleVariantCode];
	[AppPayload appendFormat:@"<locale country=\"%@\" language=\"%@\" variant=\"%@\"/>\n", [loc objectForKey:NSLocaleCountryCode], [loc objectForKey:NSLocaleLanguageCode], vc == NULL ? @"" : vc];

	// Screen dimensions
	UIScreen* mainScreenPtr =  [UIScreen mainScreen];
	[AppPayload appendFormat:@"<screen scale=\"%.2f\" width=\"%.2f\" height=\"%.2f\"/>\n", mainScreenPtr.scale, mainScreenPtr.bounds.size.width, mainScreenPtr.bounds.size.height];

	// System info
	NSProcessInfo* processInfoPtr = [NSProcessInfo processInfo];
	[AppPayload appendFormat:@"<processInfo processors=\"%d\" uptime=\"%llf\" physmem=\"%llu\"/>\n", [processInfoPtr processorCount], [processInfoPtr systemUptime], [processInfoPtr physicalMemory]];


	// report the memory we had to work with when we started
	[AppPayload appendFormat:@"<startmem free-bytes=\"%llu\" used-bytes=\"%llu\"/>\n", GStartupFreeMem, GStartupUsedMem];

	// report application stats
	[AppPayload appendFormat:@"<app invokes=\"%llu\" crashes=\"%llu\" memwarns=\"%llu\" playtime-secs=\"%llu\"/>\n", 
		IPhoneLoadUserSettingU64("IPhoneHome::NumInvocations"),
		IPhoneLoadUserSettingU64("IPhoneHome::NumCrashes"),
		IPhoneLoadUserSettingU64("IPhoneHome::NumMemoryWarnings"),
		IPhoneLoadUserSettingU64("IPhoneHome::AppPlaytimeSecs")];

	// gather stats (then send everything)
	[self collectStats];
}

/**
 * Callback when reachability has been determined (could be 30 seconds later if 
 * a WiFi access point has no Internet connection)
 */
void NetworkReachabilityCallBack(SCNetworkReachabilityRef Target, SCNetworkReachabilityFlags Flags, void* UserData)
{
	// are we on wifi? (the result of the reachability check)
	BOOL bHasWifi = doFlagsIndicateWiFi(Flags);

	// run the rest on main thread
	IPhoneHome* PhoneHome = (IPhoneHome*)UserData;
	// any non-nil value will imply wifi is on
	[PhoneHome performSelectorOnMainThread:@selector(reachabilityDone:) 
								withObject:(bHasWifi ? PhoneHome : nil)
							 waitUntilDone:NO];
}

- (void)collectPayload
{
	// begin timing
	CollectStart = mach_absolute_time(); 

	// increment total surveys attempted.
	IPhoneIncrementUserSettingU64("IPhoneHome::TotalSurveys");

	// kick off check for WiFi connection
	WiFiConn = NO;

	// create the reachability object
	if (Reachability == NULL)
	{
		SCNetworkReachabilityRef Ref = SCNetworkReachabilityCreateWithName(NULL, TCHAR_TO_ANSI(PHONE_HOME_URL));	
	}

	if (Reachability != NULL)
	{
		// set up async reachability
		SCNetworkReachabilityContext Context = {0, self, NULL, NULL, NULL};
		SCNetworkReachabilitySetCallback(Reachability, NetworkReachabilityCallBack, &Context);
		SCNetworkReachabilitySetDispatchQueue(Reachability, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
	}
	else
	{
		// if reachability failed, just call the completion function directly
		[self reachabilityDone:nil];
	}
}

- (id)init
{
	if (self = [super init])
	{
		Completed = NO;
		AppPayload = [[NSMutableString alloc] init];
		GamePayload = [[NSMutableString alloc] init];
		ReplyBuffer = [[NSMutableData alloc] init];
		GameName = nil;
		Reachability = NULL;
	}
	
	return self;
}

+ (Boolean)shouldPhoneHome
{
#if !IS_STANDALONE_APPLICATION
	// check the command line for overrides
	if (ParseParam(appCmdLine(), TEXT("nophonehome")))
	{
		return NO;
	}
	if (ParseParam(appCmdLine(), TEXT("forcephonehome")))
	{
		return YES;
	}
#endif

	// get last success and last failure times
	time_t Now = time(NULL);
	time_t LastSuccess = (time_t)IPhoneLoadUserSettingU64("IPhoneHome::LastSuccess");
	time_t LastFailure = (time_t)IPhoneLoadUserSettingU64("IPhoneHome::LastFailure");

	// make sure at least 1 day has passed since last_success
	if (LastSuccess != 0 && Now - LastSuccess < 60*60*24)
	{
		return NO;
	}

	// make sure at least 5 minutes have passed since last_failure
	if (LastFailure != 0 && Now - LastFailure < 60*5)
	{
		return NO;
	}
	
	return YES;
}

+ (void)queueRequest
{
	// if there's one in flight, never phone home
	// NOTE: this isn't intended to be MT-safe as queueRequest makes no such guarantee either
	if (GIsPhoneHomeInFlight)
	{
		return;
	}

	// enforce throttling
	if (![IPhoneHome shouldPhoneHome])
	{
		return;
	}

	// mark that a phone home is in flight because we may get called again while async callbacks are processing
	GIsPhoneHomeInFlight = TRUE;

	// NOTE: intentionally keeping the refcount at 1 here since IPhoneHome releases itself
	// if you need to keep a copy, please still call retain as usual.
	IPhoneHome* Iph = [[IPhoneHome alloc] init];

	// send the payload
	// NOTE: this completes in several asynchronous callback steps
	[Iph collectPayload];
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
	// accumulate the response data
	[ReplyBuffer appendData:data];
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSHTTPURLResponse *)response
{
	if ([response statusCode] == 200)
	{
		// record that we succeeded
		[self didSucceed];
	}

	// reset the ReplyBuffer (in case we get redirects)
	[ReplyBuffer setLength:0];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
	// record that we failed
	// IF we have already reported success, this will be ignored
	[self didFail];
	
	// release ourselves
	[self autorelease];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	// record that we failed
	[self didFail];
	
	// release ourselves
	[self autorelease];
}

// This code is paraphrased from the Apple Reachability sample 
// http://developer.apple.com/iphone/library/samplecode/Reachability/Introduction/Intro.html
Boolean doFlagsIndicateWiFi(SCNetworkReachabilityFlags flags)
{   
	if ((flags & kSCNetworkReachabilityFlagsReachable) == 0)
	{
		// if target host is not reachable
		return NO;
	}

	Boolean isWiFi = NO;
    if ((flags & kSCNetworkReachabilityFlagsConnectionRequired) == 0)
    {
        // if target host is reachable and no connection is required
        //  then we'll assume (for now) that you are on Wi-Fi
		isWiFi = YES;
    }
    
    if ((((flags & kSCNetworkReachabilityFlagsConnectionOnDemand ) != 0) ||
        (flags & kSCNetworkReachabilityFlagsConnectionOnTraffic) != 0))
    {
        // ... and the connection is on-demand (or on-traffic) if the
        //     calling application is using the CFSocketStream or higher APIs

        if ((flags & kSCNetworkReachabilityFlagsInterventionRequired) == 0)
        {
            // ... and no [user] intervention is needed
			isWiFi = YES;
        }
    }
    
    if ((flags & kSCNetworkReachabilityFlagsIsWWAN) == kSCNetworkReachabilityFlagsIsWWAN)
    {
        // ... but WWAN connections are OK if the calling application
        //     is using the CFNetwork (CFSocketStream?) APIs.
        isWiFi = NO;
    }
    return isWiFi;
}

- (void)dealloc
{	
	[ReplyBuffer release];
	[AppPayload release];
	[GamePayload release];
	if (Reachability)
	{
		CFRelease(Reachability);
	}
	
	[super dealloc];
}

@end

/**
 * Creates a string of hex values for the given data blob, potentially
 * using the SHA1 hash of the data as the string. 
 * The input 0xAB 0xCD 0xEF would return "abcdef"
 *
 * @param InData Data to convert to a string
 * @param bUseDataHash If TRUE, the returned string will be the 20 byte hash as a string
 *
 * @return A string representing hex values as a string
 */
NSString* DataToString(NSData* InData, BOOL bUseDataHash)
{
	BYTE* BytesToConvert;
	INT Length;

	// hash the data to a stack buffer if needed
	BYTE DataHash[20];
	if (bUseDataHash)
	{
		FSHA1::HashBuffer([InData bytes], [InData length], DataHash);
		BytesToConvert = DataHash;
		Length = 20;
	}
	// otherwise, use the bytes in the InData
	else
	{
		BytesToConvert = (BYTE*)[InData bytes];
		Length = [InData length];
	}

	return CArrayToHexNSString(BytesToConvert, Length);
}
