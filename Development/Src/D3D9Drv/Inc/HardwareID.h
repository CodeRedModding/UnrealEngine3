/*=============================================================================
This code taken with minimal modifications from PCCompat2007.6.29.4.
=============================================================================*/

#pragma once

#if !_WIN64	//@todo win64

#define NUM_LOGICAL_BITS   0x00FF0000 //EBX[23:16] indicate number of
// These are the bit flags that get set on calling cpuid
// with register eax set to 1
#define _MMX_FEATURE_BIT        0x00800000
#define _SSE_FEATURE_BIT        0x02000000
#define _SSE2_FEATURE_BIT       0x04000000

// This bit is set when cpuid is called with
// register set to 80000001h (only applicable to AMD)
#define _3DNOW_FEATURE_BIT      0x80000000

#define INITIAL_APIC_ID_BITS 0xFF000000 // EBX[31:24] unique APIC ID

#define HT_BIT 0x10000000 // EDX[28] - Bit 28 set indicates

// Hyper-Threading Technology is supported

// in hardware.

#define FAMILY_ID    0x0f00 // EAX[11:8] - Bit 11 thru 8 contains family

// processor id

#define EXT_FAMILY_ID 0x0f00000 // EAX[23:20] - Bit 23 thru 20 contains    

// extended family processor id

#define PENTIUM4_ID    0x0f00 // Pentium 4 family processor id      

#define _CPU_FEATURE_MMX    0x0001
#define _CPU_FEATURE_SSE    0x0002
#define _CPU_FEATURE_SSE2   0x0004
#define _CPU_FEATURE_3DNOW  0x0008
#define _CPU_FEATURE_IA64	0x40000000
#define _CPU_FEATURE_HYPERTHREADING 0x10000000

#define _MAX_VNAME_LEN  13
#define _MAX_MNAME_LEN  30

#ifndef PROCESSOR_POWER_INFORMATION
typedef struct _PROCESSOR_POWER_INFORMATION {  
	ULONG Number;
	ULONG MaxMhz;
	ULONG CurrentMhz;
	ULONG MhzLimit;
	ULONG MaxIdleState;
	ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;
#endif

typedef struct cpuid_args_s {
	DWORD eax;
	DWORD ebx;
	DWORD ecx;
	DWORD edx;
} CPUID_ARGS;

class HardwareID {
private:
    TCHAR v_name[_MAX_VNAME_LEN];        // vendor name
    TCHAR model_name[_MAX_MNAME_LEN];    // name of model
	TCHAR manufacturer[_MAX_MNAME_LEN];  // name of manufacturer
                                        // e.g. Intel Pentium-Pro
	TCHAR CPUFeature[_MAX_MNAME_LEN];	// SSE, MMX, SSE2, 3DNOW
    int family;                         // family of the processor
                                        // e.g. 6 = Pentium-Pro architecture
    int model;                          // model of processor
                                        // e.g. 1 = Pentium-Pro for family = 6
    int stepping;                       // processor revision number
    int feature;                        // processor feature
                                        // (same as return value from _cpuid)
    int os_support;                     // does OS Support the feature?
    int checks;                         // mask of checked bits in feature
                                        // and os_support fields
	int NumLogicalProcessors;
	int NumProcessorsPerCPU;
	bool bHyperThreading;
	ULONG numProcessors;
	ULONG MaxSpeed;
	ULONG CurrentSpeed;
	int	CoresPerProcessor;
	int BatteryRemaining;
	bool bOnBattery;
	ULONG cpumask;
	ULONG cpuStandardInfo;
	int L1CacheSize;
	int L2CacheSize;
	bool bIA64;
	bool bAmd;
	bool HTEnabled;

private:
	int IsCPUID();
	int _os_support(int);
	void map_mname(const TCHAR *, int buffsize);
	template <int size> void map_mname(TCHAR (&vname)[size]);
	int cpuid ();
	int LogicalProcessorsPerPackage(void);
	unsigned char GetAPIC_ID (void);
	bool HTSupported(void);
	void ProcessorState();
	int GetCoresPerPackage();
	void cpuid32(CPUID_ARGS* p);
	int CoresPerPackage();
	void PowerState();
	int GetCacheSize(int);
	void SetAMD(unsigned int);
	bool IsHTEnabled();
public:
	HardwareID() {
    ZeroMemory((void *)v_name,sizeof(v_name));        // vendor name
    ZeroMemory((void *)model_name,sizeof(model_name));    // name of model
    family = 0;
    model = 0;
    stepping = 0;
    feature = 0;
    os_support = 0;
    checks = 0;
	NumLogicalProcessors = 1;
	NumProcessorsPerCPU = 1;
	bHyperThreading = false;
	numProcessors = 1;
	MaxSpeed = 0;
	CurrentSpeed = 0;
	CoresPerProcessor = 1;
	BatteryRemaining = -1;
	bOnBattery = false;
	bIA64 = false;
	}
	~HardwareID() {}
	void DoDeviceDetection();
	bool IsHyperThreaded() { return HTEnabled;}
	int GetNumProcessorsPerCPU() { return NumProcessorsPerCPU;}
	int GetNumLogicalProcessors() {return NumLogicalProcessors; }
	int GetNumPhysicalProcessors() { return numProcessors; }
	int GetMaxSpeed() { return MaxSpeed; }
	int GetCurrentSpeed() { return CurrentSpeed; }
	int GetCoresPerProcessor() { return CoresPerProcessor; }
	UINT GetCpuStandardInfo();
	bool IsOnBattery() { return bOnBattery; }
	int BatteryLevel() { return BatteryRemaining; }
	TCHAR *GetManufacturer() { return manufacturer; }
	int GetCPUMask();
	TCHAR *GetCPUName() { return model_name;}
	TCHAR *GetCPUFeatures() { return CPUFeature; }
	int GetL1CacheSize() { return L1CacheSize; }
	int GetL2CacheSize() { return L2CacheSize; }
	// PCCOMPAT CHANGE - Replace String table with hard-coded string
	TCHAR *GetArchitecture() { return (bIA64) ? (TCHAR *)L"x64" : (TCHAR *)L"x86"; }
};

#endif

