/*=============================================================================
	This code taken with minimal modifications from PCCompat2007.6.29.4.
=============================================================================*/

#include "D3D9DrvPrivate.h"

#if !_WIN64	//@todo win64

#include "MinWindows.h"
#include "Powrprof.h"
#include "hardwareid.h"

// PCCOMPAT CHANGE -  Add lib linkage to powrprof
#pragma comment(lib, "Powrprof.lib")

void HardwareID::DoDeviceDetection()
{
	cpuid();
	NumProcessorsPerCPU = LogicalProcessorsPerPackage();
	CoresPerProcessor = GetCoresPerPackage();
	bHyperThreading = HTSupported();
	HTEnabled = IsHTEnabled();
	ProcessorState();
	PowerState();
	L1CacheSize = GetCacheSize(1);
	L2CacheSize = GetCacheSize(2);
}
bool HardwareID::IsHTEnabled()
{
    DWORD dwMax = 0;
	DWORD dwHT=0, dwHT2 = 0, dwHTf = 0;
	bool bHT = false;
    if (!IsCPUID()) {
        return false;
    }

	int CPUInfo[4];
	__cpuid(CPUInfo, 0);
	dwMax = CPUInfo[0];

	__cpuid(CPUInfo, 1);
	dwHT = CPUInfo[1];
	dwHTf = CPUInfo[3];

	// to determine if HT is enabled must be able to execute cpuid function 4
	if (dwMax >= 4)
	{
		__cpuid(CPUInfo, 4);
		dwHT2 = CPUInfo[0];
		dwHT >>=16;
		dwHT2 >>=26;
		dwHT &= 0xFF;
		dwHT2 &= 0x3F;
		bHT = ((dwHT/(dwHT2+1)) > 1) ? true : false;
	}
	else
	{
		// If we cannot do cpuid 4, we'll assume that hyperthreading is enabled if it's supported
		bHT = _CPU_FEATURE_HYPERTHREADING & dwHTf ? true : false;
	}
	return bHT;
}

int HardwareID::GetCPUMask()
{ 
	ULONG cpu = 0;
	for(int x=0;x<NumLogicalProcessors;x++)
	{
		if(x%2 != 0 && HTEnabled)
			cpu |= 1<<x;
	}
	return cpu;
}
int HardwareID::IsCPUID()
{
    __try {
		int CPUInfo[4];
		__cpuid(CPUInfo, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 1;
}


/***
* int _os_support(int feature)
*   - Checks if OS Supports the capablity or not
*
* Entry:
*   feature: the feature we want to check if OS supports it.
*
* Exit:
*   Returns 1 if OS support exist and 0 when OS doesn't support it.
*
****************************************************************/

int HardwareID::_os_support(int feature)
{
    __try {
        switch (feature) {
        case _CPU_FEATURE_SSE:
            __asm {
                xorps xmm0, xmm0        // executing SSE instruction
            }
            break;
        case _CPU_FEATURE_SSE2:
            __asm {
                xorpd xmm0, xmm0        // executing SSE2 instruction
            }
            break;
#if !__INTEL_COMPILER
        case _CPU_FEATURE_3DNOW:
            __asm {
                pfrcp mm0, mm0          // executing 3DNow! instruction
                emms
            }
            break;
#endif
        case _CPU_FEATURE_MMX:
            __asm {
                pxor mm0, mm0           // executing MMX instruction
                emms
            }
            break;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (_exception_code() == STATUS_ILLEGAL_INSTRUCTION) {
            return 0;
        }
        return 0;
    }
    return 1;
}


/***
*
* void map_mname(int, int, const char *, char *)
*   - Maps family and model to processor name
*
****************************************************/

template <int size>  void HardwareID::map_mname(TCHAR (&vname)[size])
{
	map_mname(vname, size);
}


void HardwareID::map_mname(const TCHAR *v_name, int /*buffersize*/)
{
    // Default to name not known
    model_name[0] = '\0';

    if (!_tcsncmp(L"AuthenticAMD", v_name, 12)) {
        _tcscpy_s (model_name, L"Unknown AMD CPU");
		_tcscpy_s(manufacturer,L"AMD");
        switch (family) { // extract family code
        case 4: // Am486/AM5x86
            _tcscpy_s(model_name, L"AMD Am486");
            break;

        case 5: // K6
            switch (model) { // extract model code
            case 0:
            case 1:
            case 2:
            case 3:
                _tcscpy_s (model_name, L"AMD K5");
                break;
            case 6:
            case 7:
                _tcscpy_s (model_name, L"AMD K6");
                break;
            case 8:
                _tcscpy_s (model_name, L"AMD K6-2");
                break;
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
                _tcscpy_s (model_name, L"AMD K6-3");
                break;
            }
            break;

        case 6: // Athlon
            // No model numbers are currently defined
            _tcscpy_s (model_name, L"AMD Athlon");
            break;
		default:
            _tcscpy_s (model_name, L"Unknown AMD CPU");
            break;
        }
    }
    else if (!_tcsncmp(L"GenuineIntel", v_name, 12)) {
		_tcscpy_s(manufacturer,L"Intel");
        _tcscpy_s (model_name, L"Unknown INTEL CPU");
        switch (family) { // extract family code
        case 4:
            switch (model) { // extract model code
            case 0:
            case 1:
                _tcscpy_s (model_name, L"INTEL 486DX");
                break;
            case 2:
                _tcscpy_s (model_name, L"INTEL 486SX");
                break;
            case 3:
                _tcscpy_s (model_name, L"INTEL 486DX2");
                break;
            case 4:
                _tcscpy_s (model_name, L"INTEL 486SL");
                break;
            case 5:
                _tcscpy_s (model_name, L"INTEL 486SX2");
                break;
            case 7:
                _tcscpy_s (model_name, L"INTEL 486DX2E");
                break;
            case 8:
                _tcscpy_s (model_name, L"INTEL 486DX4");
                break;
            }
            break;

        case 5:
            switch (model) { // extract model code
            case 1:
            case 2:
            case 3:
                _tcscpy_s (model_name, L"INTEL Pentium");
                break;
            case 4:
                _tcscpy_s (model_name, L"INTEL Pentium-MMX");
                break;
            }
            break;

        case 6:
            switch (model) { // extract model code
            case 1:
                _tcscpy_s (model_name, L"INTEL Pentium-Pro");
                break;
            case 3:
            case 5:
                _tcscpy_s (model_name, L"INTEL Pentium-II");
                break;  // actual differentiation depends on cache settings
            case 6:
                _tcscpy_s (model_name, L"INTEL Celeron");
                break;
            case 7:
            case 8:
            case 10:
                _tcscpy_s (model_name, L"INTEL Pentium-III");
                break;  // actual differentiation depends on cache settings
			case 9:
			case 13:
                _tcscpy_s (model_name, L"INTEL Pentium-M");
                break;  // actual differentiation depends on cache settings
            }
            break;

        case 15 | (0x00 << 4): // family 15, extended family 0x00
            switch (model) {
            case 0:
			case 1:
			case 2:
                _tcscpy_s (model_name, L"INTEL Pentium-4");
				break;
			case 3:
			case 4:
				{
					if(GetCoresPerPackage() > 1)
						_tcscpy_s (model_name, L"INTEL Pentium-D");
					else
			            _tcscpy_s (model_name, L"INTEL Pentium-Extreme Edition");
				}
                break;
			default:
	            _tcscpy_s (model_name, L"Unknown INTEL CPU");
				break;
            }
            break;
        }
    }
    else if (!_tcsncmp(L"CyrixInstead", v_name, 12)) {
        _tcscpy_s (model_name, L"Cyrix");
		_tcscpy_s (manufacturer,L"Cyrix");

    }
    else if (!_tcsncmp(L"CentaurHauls", v_name, 12)) {
		_tcscpy_s (manufacturer,L"Centaur");
        _tcscpy_s (model_name, L"Centaur");
    }

    if (!model_name[0]) {
		_tcscpy_s(manufacturer,L"Unknown");
        _tcscpy_s(model_name, L"Unknown");
    }
}


/***
*
* int _cpuid (_p_info *pinfo)
*
* Entry:
*
*   pinfo: pointer to _p_info.
*
* Exit:
*
*   Returns int with capablity bit set even if pinfo = NULL
*
/****************************************************/


int HardwareID::cpuid ()
{
    DWORD dwStandard = 0;
    DWORD dwFeature = 0;
    DWORD dwMax = 0;
    DWORD dwExt = 0;
	bIA64 = false;
	ZeroMemory((void *)CPUFeature,sizeof(CPUFeature));
    int feature = 0;
    int os_support = 0;
	bAmd = false;
	unsigned int AmdModel=0,AmdBID=0;

    union {
        char cBuf[12+1];
        struct {
            DWORD dw0;
            DWORD dw1;
            DWORD dw2;
        } s;
    } Ident;

    if (!IsCPUID()) {
        return 0;
    }

    _asm {
        push ebx
        push ecx
        push edx

        // get the vendor string
        xor eax, eax
        cpuid
        mov dwMax, eax
        mov Ident.s.dw0, ebx
        mov Ident.s.dw1, edx
        mov Ident.s.dw2, ecx

        // get the Standard bits
        mov eax, 1
        cpuid
        mov dwStandard, eax
        mov dwFeature, edx
		mov AmdModel,ebx

        // get AMD-specials
        mov eax, 80000000h
        cpuid
        cmp eax, 80000000h
        jc notamd
        mov eax, 80000001h
        cpuid
		mov AmdBID,ebx
        mov dwExt, edx

notamd:
        pop ecx
        pop ebx
        pop edx
    }

    cpuStandardInfo = dwStandard;

    if (dwFeature & _MMX_FEATURE_BIT) {
        feature |= _CPU_FEATURE_MMX;
        if (_os_support(_CPU_FEATURE_MMX))
		{
            os_support |= _CPU_FEATURE_MMX;
			if(CPUFeature[0] != L'\0')
				_tcscat_s(CPUFeature,L";");
			// PCCOMPAT CHANGE - Replace String table 
			_tcscat_s(CPUFeature,L"MMX");
		}

    }
    if (dwExt & _3DNOW_FEATURE_BIT) {
        feature |= _CPU_FEATURE_3DNOW;
        if (_os_support(_CPU_FEATURE_3DNOW))
		{
            os_support |= _CPU_FEATURE_3DNOW;
			if(CPUFeature[0] != L'\0')
				_tcscat_s(CPUFeature,L";");
			// PCCOMPAT CHANGE - Replace String table 
			_tcscat_s(CPUFeature,L"3DNow");
		}
	}
    if (dwFeature & _SSE_FEATURE_BIT) {
        feature |= _CPU_FEATURE_SSE;
        if (_os_support(_CPU_FEATURE_SSE))
		{
            os_support |= _CPU_FEATURE_SSE;
			if(CPUFeature[0] != L'\0')
				_tcscat_s(CPUFeature,L";");
			// PCCOMPAT CHANGE - Replace String table 
			_tcscat_s(CPUFeature,L"SSE");
		}

    }
    if (dwFeature & _SSE2_FEATURE_BIT) {
        feature |= _CPU_FEATURE_SSE2;
        if (_os_support(_CPU_FEATURE_SSE2))
		{
            os_support |= _CPU_FEATURE_SSE2;
			if(CPUFeature[0] != L'\0')
				_tcscat_s(CPUFeature,L";");
			// PCCOMPAT CHANGE - Replace String table 
			_tcscat_s(CPUFeature,L"SSE2");
		}
    }
    Ident.cBuf[12] = 0;
    _tcscpy_s(v_name, ANSI_TO_TCHAR(Ident.cBuf));
	if(!_tcscmp(v_name,L"GenuineIntel"))
		bAmd = false;
	else if(!_tcscmp(v_name,L"AuthenticAMD"))
		bAmd = true;

	if(dwFeature & _CPU_FEATURE_IA64)
		bIA64 = true;
	if(bAmd)
	{
		AmdModel &= 0xFF;
		if(AmdModel == 0)
			AmdModel = (AmdBID >> 6) & 0x3F;
		else
			AmdModel = (AmdModel >>3);

       family = (dwStandard >> 8) & 0xF; // retrieve family
        if (family == 15)
		{               // retrieve extended family
            family |= (dwStandard >> 20) & 0xF;
			bIA64 = (family == 0x6) ? true : false;
        }
        model = ((dwStandard >> 4) & 0xF) | (((dwStandard >> 16) & 0xF)<<4);  // retrieve model
        stepping = (dwStandard) & 0xF;    // retrieve stepping
        Ident.cBuf[12] = 0;
		_tcscpy_s(v_name, ANSI_TO_TCHAR(Ident.cBuf));
		SetAMD(AmdModel);
	}
	else
	{
        family = (dwStandard >> 8) & 0xF; // retrieve family
        if (family == 15) {               // retrieve extended family
            family |= (dwStandard >> 16) & 0xFF0;
        }
        model = (dwStandard >> 4) & 0xF;  // retrieve model
        if (model == 15) {                // retrieve extended model
            model |= (dwStandard >> 12) & 0xF;
        }
        stepping = (dwStandard) & 0xF;    // retrieve stepping
        Ident.cBuf[12] = 0;
		_tcscpy_s(v_name, ANSI_TO_TCHAR(Ident.cBuf));
	
        map_mname(v_name);
	}
	
	checks = _CPU_FEATURE_MMX |
             _CPU_FEATURE_SSE |
             _CPU_FEATURE_SSE2 |
             _CPU_FEATURE_3DNOW;
    return feature;
}
void HardwareID::SetAMD(unsigned AmdModel)
{
	_tcscpy_s(manufacturer,L"AMD");
	switch (AmdModel)
	{
	case 0x00:
		if(family != 0)
		{
			map_mname(v_name);
			return;
		}
		else
			_tcscpy_s (model_name, L"AMD Engineering Sample");
		break;
	case 0x05:
        _tcscpy_s (model_name, L"AMD Athlon 64 X2 Dual Core");
		bIA64 = true;
		break;
	case 0x01:
	case 0x04:
	case 0x18:
	case 0x24:
        _tcscpy_s(model_name, L"AMD Athlon 64");
		bIA64 = true;
		break;
	case 0x08:
	case 0x09:
        _tcscpy_s (model_name, L"Mobile AMD Athlon 64");
		bIA64 = true;
		break;
	case 0x0a:
	case 0x0b:
        _tcscpy_s (model_name, L"AMD Turion 64 Mobile");
		bIA64 = true;
		break;
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
        _tcscpy_s (model_name, L"AMD Opteron");
		bIA64 = true;
		break;
	case 0x1d:
	case 0x1e:
        _tcscpy_s (model_name, L"Mobile AMD Athlon XP-M");
		break;
	case 0x20:
        _tcscpy_s (model_name, L"AMD Athlon XP");
		break;
	case 0x21:
		_tcscpy_s (model_name, L"Mobile AMD Sempron");
		break;
	case 0x23:
        _tcscpy_s (model_name, L"AMD Athlon 64 X2 Dual Core");
		bIA64 = true;
		break;
	case 0x22:
	case 0x26:
        _tcscpy_s (model_name, L"AMD Sempron");
		if(AmdModel == 0x26)
			bIA64 = true;
		break;
	case 0x29:
	case 0x2A:
	case 0x2b:
	case 0x2c:
	case 0x2d:
	case 0x2e:
	case 0x2f:
	case 0x30:
	case 0x31:
	case 0x32:
	case 0x33:
	case 0x34:
	case 0x35:
	case 0x36:
	case 0x37:
	case 0x38:
	case 0x39:
	case 0x3a:
        _tcscpy_s (model_name, L"Dual-Core AMD Opteron");
		bIA64 = true;
		break;
	default:
        _tcscpy_s (model_name, L"AMD model unknown");
		break;
	}
}

// Returns the number of logical processors per physical processors.

int HardwareID::LogicalProcessorsPerPackage(void)
{
	unsigned int reg_ebx = 0;
	int HT_Enabled = 0;
	if(!HTSupported()) return(unsigned char) 1;
	__asm {
		mov    eax, 1 // call cpuid with eax = 1
		cpuid
		mov    reg_ebx, ebx	// Hasinfo on number of logical processors
	}
	int Logical_Per_Package = ((reg_ebx& NUM_LOGICAL_BITS) >> 16);
	if (Logical_Per_Package > 1)
		{
			HANDLE hCurrentProcessHandle;
			DWORD dwProcessAffinity;
			DWORD dwSystemAffinity;
			DWORD dwAffinityMask;

			// Physical processor ID and Logical processor IDs are derived
			// from the APIC ID. We'll calculate the appropriate shift
			// and mask values knowing the number of logical processors
			// per physical processor package.
			unsigned char i = 1;
			unsigned char PHY_ID_MASK = 0xFF;
			unsigned char PHY_ID_SHIFT = 0;
			while (i < Logical_Per_Package)
			{
				i *= 2;
				PHY_ID_MASK <<= 1;
				PHY_ID_SHIFT++;
			}

			// The OS may limit the processors that
			// this process may run on.

			hCurrentProcessHandle = GetCurrentProcess();
			GetProcessAffinityMask(hCurrentProcessHandle,&dwProcessAffinity,&dwSystemAffinity);

			// If our available process affinity mask does not equal the
			// available system affinity mask, then we may not be able to
			// determine if Hyper-Threading Technology is enabled.
			
			dwAffinityMask = 1;

			while (dwAffinityMask != 0 && dwAffinityMask <=dwProcessAffinity)
			{
				// Check to make sure we can utilize this processor first.
				if (dwAffinityMask & dwProcessAffinity)
				{
					if (SetProcessAffinityMask(hCurrentProcessHandle,dwAffinityMask))
					{
						unsigned char APIC_ID;
						unsigned char LOG_ID;
						unsigned char PHY_ID;
						Sleep(0); // We may not be running on the CPU
						// that we just set the affinity to.
						// Sleep gives the OS a chance to
						// switch us to the desired CPU.

						APIC_ID = GetAPIC_ID();
						LOG_ID = APIC_ID & ~PHY_ID_MASK;
						PHY_ID = APIC_ID >> PHY_ID_SHIFT;
						if (LOG_ID != 0) HT_Enabled = 1;
					}
					else
					{
						// This shouldn't happen since we check to make
						// sure we can utilize this processor before
						// trying to set affinity mask.
						return -1;
					}
				}
				dwAffinityMask= dwAffinityMask << 1;
			}
			// Don't forget to reset the processor affinity if you use
			// this code in an application.
			SetProcessAffinityMask(hCurrentProcessHandle,dwProcessAffinity);
			if (HT_Enabled)
				return Logical_Per_Package;
			else return 1;

		}
		else
			return 1;
}
unsigned char HardwareID::GetAPIC_ID (void)
{
    unsigned int reg_ebx = 0;
    if (!HTSupported()) return (unsigned char) -1;
    __asm {
        mov  eax, 1          // call cpuid with eax = 1
        cpuid
        mov  reg_ebx, ebx   // Has APIC ID info
    }
    return (unsigned char) ((reg_ebx & INITIAL_APIC_ID_BITS) >> 24);
}
// Returns non-zero if Hyper-Threading Technology is supported on       
// the processors and zero if not. This does not mean that
// Hyper-Threading Technology is necessarily enabled.

bool HardwareID::HTSupported(void)

{
	unsigned int reg_eax = 0;
	unsigned int reg_edx = 0;
	unsigned int vendor_id[3] = {0, 0, 0};

	__try {	   // verify cpuid instruction is supported
		__asm {

			xor    eax,eax	// call cpuid with eax = 0
			cpuid	 // get vendor id string
			mov   vendor_id, ebx
			mov   vendor_id + 4, edx
			mov   vendor_id + 8, ecx
			mov   eax,1	 // call cpuid with eax = 1
			cpuid
			mov   reg_eax, eax // eax contains cpu family type info
			mov   reg_edx, edx // edx has info whether
			// Hyper-Threading Technology is available
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER )
	{
		return false; // CPUID is not supported and so
				// Hyper-Threading Technology is not supported
	}

	// Check to see if this is a Pentium 4 or later processor
	if(((reg_eax & FAMILY_ID) == PENTIUM4_ID) || (reg_eax & EXT_FAMILY_ID))
		if(vendor_id[0] =='uneG')
			if(vendor_id[1] == 'Ieni')
				if(vendor_id[2] == 'letn')
					return !!(reg_edx & HT_BIT); // Genuine Intel Processor
	// with Hyper-Threading
	// Technology
				return false;	// If we get here, must not be genuine Intel processor.
}

void HardwareID::ProcessorState()
{
	SYSTEM_INFO sys;
	ZeroMemory((void *)&sys,sizeof(sys));
	GetSystemInfo(&sys);
	if(CoresPerProcessor >= 2)  
		numProcessors = CoresPerProcessor;
	else if(bHyperThreading && HTEnabled)
	{
		numProcessors = sys.dwNumberOfProcessors/2/CoresPerProcessor;
	}
	else
		numProcessors = CoresPerProcessor;
		
	NumLogicalProcessors = sys.dwNumberOfProcessors;
	if (!sys.dwNumberOfProcessors)
		return;
	PROCESSOR_POWER_INFORMATION *Info = new PROCESSOR_POWER_INFORMATION[sys.dwNumberOfProcessors];
	if(Info == NULL)
		return;
	ZeroMemory((void *)Info,sizeof(PROCESSOR_POWER_INFORMATION)*sys.dwNumberOfProcessors);

	LONG nt = CallNtPowerInformation(ProcessorInformation,NULL,0,(PVOID) Info,sizeof(PROCESSOR_POWER_INFORMATION)*sys.dwNumberOfProcessors);
	if(nt ==  0)
	{
		MaxSpeed = Info[0].MaxMhz;
		CurrentSpeed = Info[0].CurrentMhz;
	}
	delete [] Info;
}
void HardwareID::PowerState()
{
	PROCESSOR_POWER_POLICY Info;
	SYSTEM_BATTERY_STATE Bat;
	ZeroMemory((void *)&Info,sizeof(Info));
	ZeroMemory((void *)&Bat,sizeof(Bat));
	LONG nt = CallNtPowerInformation(ProcessorPowerPolicyCurrent,NULL,0,(PVOID)&Info,sizeof(Info));
	nt = CallNtPowerInformation(SystemBatteryState,NULL,0,(PVOID)&Bat,sizeof(Bat));
	bOnBattery = !Bat.AcOnLine;
	BatteryRemaining = (Bat.MaxCapacity == 0) ? -1 : (int)((float)Bat.RemainingCapacity/(float)Bat.MaxCapacity * 100.0);
}

void HardwareID::cpuid32(CPUID_ARGS* p) {
	__asm {
		mov	edi, p
		mov eax, [edi].eax
		mov ecx, [edi].ecx // for functions such as eax=4
		cpuid
		mov [edi].eax, eax
		mov [edi].ebx, ebx
		mov [edi].ecx, ecx
		mov [edi].edx, edx
	}
}

// Assumptions prior to calling:
// - CPUID instruction is available
// - We have already used CPUID to verify that this in an Intel® processor
int HardwareID::GetCoresPerPackage()
{
	// Is explicit cache info available?
	int nCaches=0;
	int coresPerPackage=1; // Assume 1 core per package if info not available 
	DWORD t;
	int cacheIndex;
	CPUID_ARGS ca;
	CPUID_ARGS *p;
	p = &ca;

	ca.eax = 0;
	cpuid32(&ca);
	if(bAmd){
		ca.eax = 0x80000008;
		cpuid32(&ca);
		coresPerPackage = ca.ecx+1;
		return coresPerPackage;
	}
	else{
		t = ca.eax;
		if ((t > 3) && (t < 0x80000000)) { 
			for (cacheIndex=0; ; cacheIndex++) {
				ca.eax = 4;
				ca.ecx = cacheIndex;
				cpuid32(&ca);
				t = ca.eax;
				if ((t & 0x1F) == 0)
					break;
				nCaches++;
			}
		}
	
		if (nCaches > 0) {
			ca.eax = 4;
			ca.ecx = 0; // first explicit cache
			cpuid32(&ca);
			coresPerPackage = ((ca.eax >> 26) & 0x3F) + 1; // 31:26
		}
		return coresPerPackage;
	}
}


 
int HardwareID::GetCacheSize(int level)
{
	CPUID_ARGS ca;
	ZeroMemory((void *)&ca,sizeof(ca));
	ca.eax = 0;
	cpuid32(&ca);
	DWORD t = ca.eax;
	if(bAmd){
		if(level == 1)
		{
			ca.eax = 0x80000005;
			cpuid32(&ca);
			return (ca.ecx >> 24) & 0xFF;
		}
		else
		{
			ca.eax = 0x80000006;
			cpuid32(&ca);
			return (ca.ecx>>16) & 0xffff;
		}
	}
	else
	{

	if (((t > 3) && (t < 0x80000000)) && level ==2)
	{
//		if(!wcscmp(manufacturer,L"AMD"))
//			ca.eax = 0x80000005;
//		else
			ca.eax = 4;
		ca.ecx = 1;
		cpuid32(&ca);
		DWORD L,P,W,S;

		DWORD CacheLevel = (ca.eax >> 5) & 0x7;

		L = (ca.ebx & 0xFFF) + 1;			// 11:0
		P = ((ca.ebx >> 12) & 0x3FF) + 1;	// 21:12
		W = ((ca.ebx >> 22) & 0x3FF) + 1;	// 31:22
		S = ca.ecx + 1;


		DWORD SizeKB = L*P*W*S / 1024;
		if(CacheLevel != static_cast<DWORD>(level))
			return -1;
		return (int)SizeKB;
	}
	else
	{
		ca.eax = 2;
		cpuid32(&ca);
		if((ca.eax & 0x000000FF) != 1)
		{
			int numCalls = ca.eax & 0x000000FF;
			for(int x = 1;x<numCalls;x++)
			{
				ca.eax = 2;
				cpuid32(&ca);
				if((ca.eax&0x80000000) == 0)
					break;
			}
		}
		BYTE tlbs[4] = {0,0,0,0};
		for(int registers=0;registers<4;registers++)
		{
			DWORD iregister = 0;
			switch(registers)
			{
				case 0:
					iregister = ca.eax&0xFFFFFF00;
					break;
				case 1:
					iregister = ca.ebx;
					break;
				case 2:
					iregister = ca.ecx;
					break;
				case 3:
					iregister = ca.edx;
					break;
			}
			if((iregister&0x80000000) == 0 && iregister != 0)
				{
					tlbs[0] = (BYTE)(iregister & 0x000000FF);
					tlbs[1] = (BYTE)((iregister & 0x0000FF00)>>8);
					tlbs[2] = (BYTE)((iregister & 0x00FF0000)>>16);
					tlbs[3] = (BYTE)((iregister & 0xFF000000) >>24);
					for(int index=0;index<4;index++)
						if(level == 1)
						{
							switch(tlbs[index])
							{
								case 0x06: 
								case 0x0A:
								case 0x66:
									return 8;
								case 0x08:
								case 0x0c:
								case 0x60:
								case 0x67:
									return 16;
								case 0x2C:
								case 0x68:
								return 32;
							}
						}
						else if (level == 2)
						{
							switch(tlbs[index])
							{
								case 0x39: 
								case 0x3b:
								case 0x41:
								case 0x79:
									return 128;
								case 0x3a:
									return 192;
								case 0x3C:
								case 0x42:
								case 0x7a:
									return 256;
								case 0x3d:
									return 384;
								case 0x3e:
								case 0x43:
								case 0x7b:
								case 0x7f:
								case 0x83:
								case 0x86:
									return 512;
								case 0x44:
								case 0x78:
								case 0x7c:
								case 0x84:
								case 0x87:
									return 1024;
								case 0x45:
								case 0x7d:
								case 0x85:
									return 2048;
							}
						}
					}
				}
			}
		}
	return -1;
}

UINT HardwareID::GetCpuStandardInfo()
{
    return cpuStandardInfo;
}

#endif

