/*
 * Some extra defines, types and functions that GNU-WIN32 doesn't yet
 * have (B18/B19).
 */

#ifndef _WX_EXTRAH_
#define _WX_EXTRAH_

#ifndef __CYGWIN10__

#define ENDSESSION_LOGOFF    0x80000000

/* WM_NCHITTEST message */
#ifndef HTMINBUTTON
#define HTMINBUTTON	(8)
#define HTMAXBUTTON	(9)
#endif

#ifndef TMPF_VECTOR
#define TMPF_VECTOR 0x02
#define TMPF_TRUETYPE 0x04
#endif

#ifndef LVS_TYPEMASK
#define LVS_TYPEMASK 0x0003
#endif

typedef char *NPSTR;

typedef BITMAPINFO *LPBITMAPINFO;
typedef BITMAPINFOHEADER *LPBITMAPINFOHEADER;
typedef BITMAPCOREHEADER *LPBITMAPCOREHEADER;

#ifndef TB_REPLACEBITMAP
#define TB_REPLACEBITMAP        (WM_USER + 46)
#endif

typedef struct {
        HINSTANCE       hInstOld;
        UINT            nIDOld;
        HINSTANCE       hInstNew;
        UINT            nIDNew;
        int             nButtons;
} TBREPLACEBITMAP, *LPTBREPLACEBITMAP;

#define far

// This corrects a bug in the GnuWin32 defines.h: a missing bracket

#undef LPSTR_TEXTCALLBACKA
#undef LPSTR_TEXTCALLBACK

#define LPSTR_TEXTCALLBACKA	((LPSTR)-1L)

#ifdef UNICODE
#define LPSTR_TEXTCALLBACK LPSTR_TEXTCALLBACKW
#else
#define LPSTR_TEXTCALLBACK LPSTR_TEXTCALLBACKA
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

HDDEDATA WINAPI DdeClientTransaction(LPBYTE, DWORD, HCONV, HSZ, UINT, UINT, DWORD, LPDWORD);
HDDEDATA WINAPI DdeCreateDataHandle(DWORD, LPBYTE, DWORD, DWORD, HSZ, UINT, UINT);

#ifdef __cplusplus
}
#endif /* __cplusplus */

////
//// Tray notification definitions
////

typedef struct _NOTIFYICONDATAA {
        DWORD cbSize;
        HWND hWnd;
        UINT uID;
        UINT uFlags;
        UINT uCallbackMessage;
        HICON hIcon;
        CHAR   szTip[64];
} NOTIFYICONDATAA, *PNOTIFYICONDATAA;
typedef struct _NOTIFYICONDATAW {
        DWORD cbSize;
        HWND hWnd;
        UINT uID;
        UINT uFlags;
        UINT uCallbackMessage;
        HICON hIcon;
        WCHAR  szTip[64];
} NOTIFYICONDATAW, *PNOTIFYICONDATAW;

#ifdef UNICODE
typedef NOTIFYICONDATAW NOTIFYICONDATA;
typedef PNOTIFYICONDATAW PNOTIFYICONDATA;
#else
typedef NOTIFYICONDATAA NOTIFYICONDATA;
typedef PNOTIFYICONDATAA PNOTIFYICONDATA;
#endif // UNICODE

#ifndef NIM_ADD
   #define NIM_ADD         0x00000000
   #define NIM_MODIFY      0x00000001
   #define NIM_DELETE      0x00000002

   #define NIF_MESSAGE     0x00000001
   #define NIF_ICON        0x00000002
   #define NIF_TIP         0x00000004
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BOOL WINAPI Shell_NotifyIconA(DWORD dwMessage, PNOTIFYICONDATAA lpData);
BOOL WINAPI Shell_NotifyIconW(DWORD dwMessage, PNOTIFYICONDATAW lpData);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef UNICODE
#define Shell_NotifyIcon  Shell_NotifyIconW
#else
#define Shell_NotifyIcon  Shell_NotifyIconA
#endif // !UNICODE

////
//// End Tray Notification Icons
////

////
//// From mmsystem.h
////

/* general data types */

typedef UINT        MMVERSION;  /* major (high byte), minor (low byte) */
typedef UINT        MMRESULT;   /* error return code, 0 means no error */
#define _MMRESULT_

#ifndef LPUINT
typedef UINT FAR   *LPUINT;
#endif

#define MAXPNAMELEN      32     /* max product name length (including NULL) */
#define MAXERRORLENGTH   256    /* max error text length (including NULL) */
#define MAX_JOYSTICKOEMVXDNAME 260 /* max oem vxd name length (including NULL) */

/****************************************************************************

		    Multimedia Extensions Window Messages

****************************************************************************/

#define MM_JOY1MOVE         0x3A0           /* joystick */
#define MM_JOY2MOVE         0x3A1
#define MM_JOY1ZMOVE        0x3A2
#define MM_JOY2ZMOVE        0x3A3
#define MM_JOY1BUTTONDOWN   0x3B5
#define MM_JOY2BUTTONDOWN   0x3B6
#define MM_JOY1BUTTONUP     0x3B7
#define MM_JOY2BUTTONUP     0x3B8

#define MM_MCINOTIFY        0x3B9           /* MCI */

#define MM_WOM_OPEN         0x3BB           /* waveform output */
#define MM_WOM_CLOSE        0x3BC
#define MM_WOM_DONE         0x3BD

#define MM_WIM_OPEN         0x3BE           /* waveform input */
#define MM_WIM_CLOSE        0x3BF
#define MM_WIM_DATA         0x3C0

#define MM_MIM_OPEN         0x3C1           /* MIDI input */
#define MM_MIM_CLOSE        0x3C2
#define MM_MIM_DATA         0x3C3
#define MM_MIM_LONGDATA     0x3C4
#define MM_MIM_ERROR        0x3C5
#define MM_MIM_LONGERROR    0x3C6

#define MM_MOM_OPEN         0x3C7           /* MIDI output */
#define MM_MOM_CLOSE        0x3C8
#define MM_MOM_DONE         0x3C9

/* these are also in msvideo.h */
#ifndef MM_DRVM_OPEN
 #define MM_DRVM_OPEN       0x3D0           /* installable drivers */
 #define MM_DRVM_CLOSE      0x3D1
 #define MM_DRVM_DATA       0x3D2
 #define MM_DRVM_ERROR      0x3D3
#endif

/* these are used by msacm.h */
#define MM_STREAM_OPEN	    0x3D4
#define MM_STREAM_CLOSE	    0x3D5
#define MM_STREAM_DONE	    0x3D6
#define MM_STREAM_ERROR	    0x3D7

#if(WINVER >= 0x0400)
#define MM_MOM_POSITIONCB   0x3CA           /* Callback for MEVT_POSITIONCB */

#ifndef MM_MCISIGNAL
 #define MM_MCISIGNAL        0x3CB
#endif

#define MM_MIM_MOREDATA      0x3CC          /* MIM_DONE w/ pending events */

#endif /* WINVER >= 0x0400 */
#define MM_MIXM_LINE_CHANGE     0x3D0       /* mixer line change notify */
#define MM_MIXM_CONTROL_CHANGE  0x3D1       /* mixer control change notify */

/****************************************************************************

		String resource number bases (internal use)

****************************************************************************/

#define MMSYSERR_BASE          0
#define WAVERR_BASE            32
#define MIDIERR_BASE           64
#define TIMERR_BASE            96
#define JOYERR_BASE            160
#define MCIERR_BASE            256
#define MIXERR_BASE            1024

#define MCI_STRING_OFFSET      512
#define MCI_VD_OFFSET          1024
#define MCI_CD_OFFSET          1088
#define MCI_WAVE_OFFSET        1152
#define MCI_SEQ_OFFSET         1216

/****************************************************************************

			General error return values

****************************************************************************/

/* general error return values */
#define MMSYSERR_NOERROR      0                    /* no error */
#define MMSYSERR_ERROR        (MMSYSERR_BASE + 1)  /* unspecified error */
#define MMSYSERR_BADDEVICEID  (MMSYSERR_BASE + 2)  /* device ID out of range */
#define MMSYSERR_NOTENABLED   (MMSYSERR_BASE + 3)  /* driver failed enable */
#define MMSYSERR_ALLOCATED    (MMSYSERR_BASE + 4)  /* device already allocated */
#define MMSYSERR_INVALHANDLE  (MMSYSERR_BASE + 5)  /* device handle is invalid */
#define MMSYSERR_NODRIVER     (MMSYSERR_BASE + 6)  /* no device driver present */
#define MMSYSERR_NOMEM        (MMSYSERR_BASE + 7)  /* memory allocation error */
#define MMSYSERR_NOTSUPPORTED (MMSYSERR_BASE + 8)  /* function isn't supported */
#define MMSYSERR_BADERRNUM    (MMSYSERR_BASE + 9)  /* error value out of range */
#define MMSYSERR_INVALFLAG    (MMSYSERR_BASE + 10) /* invalid flag passed */
#define MMSYSERR_INVALPARAM   (MMSYSERR_BASE + 11) /* invalid parameter passed */
#define MMSYSERR_HANDLEBUSY   (MMSYSERR_BASE + 12) /* handle being used */
						   /* simultaneously on another */
						   /* thread (eg callback) */
#define MMSYSERR_INVALIDALIAS (MMSYSERR_BASE + 13) /* specified alias not found */
#define MMSYSERR_BADDB        (MMSYSERR_BASE + 14) /* bad registry database */
#define MMSYSERR_KEYNOTFOUND  (MMSYSERR_BASE + 15) /* registry key not found */
#define MMSYSERR_READERROR    (MMSYSERR_BASE + 16) /* registry read error */
#define MMSYSERR_WRITEERROR   (MMSYSERR_BASE + 17) /* registry write error */
#define MMSYSERR_DELETEERROR  (MMSYSERR_BASE + 18) /* registry delete error */
#define MMSYSERR_VALNOTFOUND  (MMSYSERR_BASE + 19) /* registry value not found */
#define MMSYSERR_NODRIVERCB   (MMSYSERR_BASE + 20) /* driver does not call DriverCallback */
#define MMSYSERR_LASTERROR    (MMSYSERR_BASE + 20) /* last error in range */

/****************************************************************************

			    Joystick support

****************************************************************************/

/* joystick error return values */
#define JOYERR_NOERROR        (0)                  /* no error */
#define JOYERR_PARMS          (JOYERR_BASE+5)      /* bad parameters */
#define JOYERR_NOCANDO        (JOYERR_BASE+6)      /* request not completed */
#define JOYERR_UNPLUGGED      (JOYERR_BASE+7)      /* joystick is unplugged */

/* constants used with JOYINFO and JOYINFOEX structures and MM_JOY* messages */
#define JOY_BUTTON1         0x0001
#define JOY_BUTTON2         0x0002
#define JOY_BUTTON3         0x0004
#define JOY_BUTTON4         0x0008
#define JOY_BUTTON1CHG      0x0100
#define JOY_BUTTON2CHG      0x0200
#define JOY_BUTTON3CHG      0x0400
#define JOY_BUTTON4CHG      0x0800

/* constants used with JOYINFOEX */
#define JOY_BUTTON5         0x00000010l
#define JOY_BUTTON6         0x00000020l
#define JOY_BUTTON7         0x00000040l
#define JOY_BUTTON8         0x00000080l
#define JOY_BUTTON9         0x00000100l
#define JOY_BUTTON10        0x00000200l
#define JOY_BUTTON11        0x00000400l
#define JOY_BUTTON12        0x00000800l
#define JOY_BUTTON13        0x00001000l
#define JOY_BUTTON14        0x00002000l
#define JOY_BUTTON15        0x00004000l
#define JOY_BUTTON16        0x00008000l
#define JOY_BUTTON17        0x00010000l
#define JOY_BUTTON18        0x00020000l
#define JOY_BUTTON19        0x00040000l
#define JOY_BUTTON20        0x00080000l
#define JOY_BUTTON21        0x00100000l
#define JOY_BUTTON22        0x00200000l
#define JOY_BUTTON23        0x00400000l
#define JOY_BUTTON24        0x00800000l
#define JOY_BUTTON25        0x01000000l
#define JOY_BUTTON26        0x02000000l
#define JOY_BUTTON27        0x04000000l
#define JOY_BUTTON28        0x08000000l
#define JOY_BUTTON29        0x10000000l
#define JOY_BUTTON30        0x20000000l
#define JOY_BUTTON31        0x40000000l
#define JOY_BUTTON32        0x80000000l

/* constants used with JOYINFOEX structure */
#define JOY_POVCENTERED		(WORD) -1
#define JOY_POVFORWARD		0
#define JOY_POVRIGHT		9000
#define JOY_POVBACKWARD		18000
#define JOY_POVLEFT		27000

#define JOY_RETURNX		0x00000001l
#define JOY_RETURNY		0x00000002l
#define JOY_RETURNZ		0x00000004l
#define JOY_RETURNR		0x00000008l
#define JOY_RETURNU		0x00000010l	/* axis 5 */
#define JOY_RETURNV		0x00000020l	/* axis 6 */
#define JOY_RETURNPOV		0x00000040l
#define JOY_RETURNBUTTONS	0x00000080l
#define JOY_RETURNRAWDATA	0x00000100l
#define JOY_RETURNPOVCTS	0x00000200l
#define JOY_RETURNCENTERED	0x00000400l
#define JOY_USEDEADZONE		0x00000800l
#define JOY_RETURNALL		(JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | \
				 JOY_RETURNR | JOY_RETURNU | JOY_RETURNV | \
				 JOY_RETURNPOV | JOY_RETURNBUTTONS)
#define JOY_CAL_READALWAYS	0x00010000l
#define JOY_CAL_READXYONLY	0x00020000l
#define JOY_CAL_READ3		0x00040000l
#define JOY_CAL_READ4		0x00080000l
#define JOY_CAL_READXONLY	0x00100000l
#define JOY_CAL_READYONLY	0x00200000l
#define JOY_CAL_READ5		0x00400000l
#define JOY_CAL_READ6		0x00800000l
#define JOY_CAL_READZONLY	0x01000000l
#define JOY_CAL_READRONLY	0x02000000l
#define JOY_CAL_READUONLY	0x04000000l
#define JOY_CAL_READVONLY	0x08000000l

/* joystick ID constants */
#define JOYSTICKID1         0
#define JOYSTICKID2         1

/* joystick driver capabilites */
#define JOYCAPS_HASZ		0x0001
#define JOYCAPS_HASR		0x0002
#define JOYCAPS_HASU		0x0004
#define JOYCAPS_HASV		0x0008
#define JOYCAPS_HASPOV		0x0010
#define JOYCAPS_POV4DIR		0x0020
#define JOYCAPS_POVCTS		0x0040

/* joystick device capabilities data structure */
#ifdef _WIN32

typedef struct tagJOYCAPSA {
    WORD    wMid;                /* manufacturer ID */
    WORD    wPid;                /* product ID */
    CHAR    szPname[MAXPNAMELEN];/* product name (NULL terminated string) */
    UINT    wXmin;               /* minimum x position value */
    UINT    wXmax;               /* maximum x position value */
    UINT    wYmin;               /* minimum y position value */
    UINT    wYmax;               /* maximum y position value */
    UINT    wZmin;               /* minimum z position value */
    UINT    wZmax;               /* maximum z position value */
    UINT    wNumButtons;         /* number of buttons */
    UINT    wPeriodMin;          /* minimum message period when captured */
    UINT    wPeriodMax;          /* maximum message period when captured */
#if (WINVER >= 0x0400)
    UINT    wRmin;               /* minimum r position value */
    UINT    wRmax;               /* maximum r position value */
    UINT    wUmin;               /* minimum u (5th axis) position value */
    UINT    wUmax;               /* maximum u (5th axis) position value */
    UINT    wVmin;               /* minimum v (6th axis) position value */
    UINT    wVmax;               /* maximum v (6th axis) position value */
    UINT    wCaps;	 	 /* joystick capabilites */
    UINT    wMaxAxes;	 	 /* maximum number of axes supported */
    UINT    wNumAxes;	 	 /* number of axes in use */
    UINT    wMaxButtons;	 /* maximum number of buttons supported */
    CHAR    szRegKey[MAXPNAMELEN];/* registry key */
    CHAR    szOEMVxD[MAX_JOYSTICKOEMVXDNAME]; /* OEM VxD in use */
#endif
} JOYCAPSA, *PJOYCAPSA, *NPJOYCAPSA, *LPJOYCAPSA;
typedef struct tagJOYCAPSW {
    WORD    wMid;                /* manufacturer ID */
    WORD    wPid;                /* product ID */
    WCHAR   szPname[MAXPNAMELEN];/* product name (NULL terminated string) */
    UINT    wXmin;               /* minimum x position value */
    UINT    wXmax;               /* maximum x position value */
    UINT    wYmin;               /* minimum y position value */
    UINT    wYmax;               /* maximum y position value */
    UINT    wZmin;               /* minimum z position value */
    UINT    wZmax;               /* maximum z position value */
    UINT    wNumButtons;         /* number of buttons */
    UINT    wPeriodMin;          /* minimum message period when captured */
    UINT    wPeriodMax;          /* maximum message period when captured */
#if (WINVER >= 0x0400)
    UINT    wRmin;               /* minimum r position value */
    UINT    wRmax;               /* maximum r position value */
    UINT    wUmin;               /* minimum u (5th axis) position value */
    UINT    wUmax;               /* maximum u (5th axis) position value */
    UINT    wVmin;               /* minimum v (6th axis) position value */
    UINT    wVmax;               /* maximum v (6th axis) position value */
    UINT    wCaps;	 	 /* joystick capabilites */
    UINT    wMaxAxes;	 	 /* maximum number of axes supported */
    UINT    wNumAxes;	 	 /* number of axes in use */
    UINT    wMaxButtons;	 /* maximum number of buttons supported */
    WCHAR   szRegKey[MAXPNAMELEN];/* registry key */
    WCHAR   szOEMVxD[MAX_JOYSTICKOEMVXDNAME]; /* OEM VxD in use */
#endif
} JOYCAPSW, *PJOYCAPSW, *NPJOYCAPSW, *LPJOYCAPSW;
#ifdef UNICODE
typedef JOYCAPSW JOYCAPS;
typedef PJOYCAPSW PJOYCAPS;
typedef NPJOYCAPSW NPJOYCAPS;
typedef LPJOYCAPSW LPJOYCAPS;
#else
typedef JOYCAPSA JOYCAPS;
typedef PJOYCAPSA PJOYCAPS;
typedef NPJOYCAPSA NPJOYCAPS;
typedef LPJOYCAPSA LPJOYCAPS;
#endif // UNICODE

#else
typedef struct joycaps_tag {
    WORD wMid;                  /* manufacturer ID */
    WORD wPid;                  /* product ID */
    char szPname[MAXPNAMELEN];  /* product name (NULL terminated string) */
    UINT wXmin;                 /* minimum x position value */
    UINT wXmax;                 /* maximum x position value */
    UINT wYmin;                 /* minimum y position value */
    UINT wYmax;                 /* maximum y position value */
    UINT wZmin;                 /* minimum z position value */
    UINT wZmax;                 /* maximum z position value */
    UINT wNumButtons;           /* number of buttons */
    UINT wPeriodMin;            /* minimum message period when captured */
    UINT wPeriodMax;            /* maximum message period when captured */
#if (WINVER >= 0x0400)
    UINT wRmin;                 /* minimum r position value */
    UINT wRmax;                 /* maximum r position value */
    UINT wUmin;                 /* minimum u (5th axis) position value */
    UINT wUmax;                 /* maximum u (5th axis) position value */
    UINT wVmin;                 /* minimum v (6th axis) position value */
    UINT wVmax;                 /* maximum v (6th axis) position value */
    UINT wCaps;                 /* joystick capabilites */
    UINT wMaxAxes;	 	/* maximum number of axes supported */
    UINT wNumAxes;	 	/* number of axes in use */
    UINT wMaxButtons;	 	/* maximum number of buttons supported */
    char szRegKey[MAXPNAMELEN]; /* registry key */
    char szOEMVxD[MAX_JOYSTICKOEMVXDNAME]; /* OEM VxD in use */
#endif
} JOYCAPS, *PJOYCAPS, NEAR *NPJOYCAPS, FAR *LPJOYCAPS;
#endif

/* joystick information data structure */
typedef struct joyinfo_tag {
    UINT wXpos;                 /* x position */
    UINT wYpos;                 /* y position */
    UINT wZpos;                 /* z position */
    UINT wButtons;              /* button states */
} JOYINFO, *PJOYINFO, *NPJOYINFO, *LPJOYINFO;

#if(WINVER >= 0x0400)
typedef struct joyinfoex_tag {
    DWORD dwSize;		 /* size of structure */
    DWORD dwFlags;		 /* flags to indicate what to return */
    DWORD dwXpos;                /* x position */
    DWORD dwYpos;                /* y position */
    DWORD dwZpos;                /* z position */
    DWORD dwRpos;		 /* rudder/4th axis position */
    DWORD dwUpos;		 /* 5th axis position */
    DWORD dwVpos;		 /* 6th axis position */
    DWORD dwButtons;             /* button states */
    DWORD dwButtonNumber;        /* current button number pressed */
    DWORD dwPOV;                 /* point of view state */
    DWORD dwReserved1;		 /* reserved for communication between winmm & driver */
    DWORD dwReserved2;		 /* reserved for future expansion */
} JOYINFOEX, *PJOYINFOEX, *NPJOYINFOEX, *LPJOYINFOEX;
#endif /* WINVER >= 0x0400 */

/* joystick function prototypes */

#ifdef UNICODE
#define joyGetDevCaps  joyGetDevCapsW
#else
#define joyGetDevCaps  joyGetDevCapsA
#endif // !UNICODE

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

UINT WINAPI joyGetNumDevs(void);

MMRESULT WINAPI joyGetDevCapsA(UINT uJoyID, LPJOYCAPSA pjc, UINT cbjc);
MMRESULT WINAPI joyGetDevCapsW(UINT uJoyID, LPJOYCAPSW pjc, UINT cbjc);

MMRESULT WINAPI joyGetPos(UINT uJoyID, LPJOYINFO pji);

#if(WINVER >= 0x0400)
MMRESULT WINAPI joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji);
#endif /* WINVER >= 0x0400 */

MMRESULT WINAPI joyGetThreshold(UINT uJoyID, LPUINT puThreshold);
MMRESULT WINAPI joyReleaseCapture(UINT uJoyID);
MMRESULT WINAPI joySetCapture(HWND hwnd, UINT uJoyID, UINT uPeriod,
    BOOL fChanged);
MMRESULT WINAPI joySetThreshold(UINT uJoyID, UINT uThreshold);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/****** ListBox control message APIs *****************************************/

#ifndef ListBox_Enable

   #define ListBox_Enable(hwndCtl, fEnable)            EnableWindow((hwndCtl), (fEnable))

   #define ListBox_GetCount(hwndCtl)                   ((int)(DWORD)SendMessage((hwndCtl), LB_GETCOUNT, 0L, 0L))
   #define ListBox_ResetContent(hwndCtl)               ((BOOL)(DWORD)SendMessage((hwndCtl), LB_RESETCONTENT, 0L, 0L))

   #define ListBox_AddString(hwndCtl, lpsz)            ((int)(DWORD)SendMessage((hwndCtl), LB_ADDSTRING, 0L, (LPARAM)(LPCTSTR)(lpsz)))
   #define ListBox_InsertString(hwndCtl, index, lpsz)  ((int)(DWORD)SendMessage((hwndCtl), LB_INSERTSTRING, (WPARAM)(int)(index), (LPARAM)(LPCTSTR)(lpsz)))

   #define ListBox_AddItemData(hwndCtl, data)          ((int)(DWORD)SendMessage((hwndCtl), LB_ADDSTRING, 0L, (LPARAM)(data)))
   #define ListBox_InsertItemData(hwndCtl, index, data) ((int)(DWORD)SendMessage((hwndCtl), LB_INSERTSTRING, (WPARAM)(int)(index), (LPARAM)(data)))

   #define ListBox_DeleteString(hwndCtl, index)        ((int)(DWORD)SendMessage((hwndCtl), LB_DELETESTRING, (WPARAM)(int)(index), 0L))

   #define ListBox_GetTextLen(hwndCtl, index)          ((int)(DWORD)SendMessage((hwndCtl), LB_GETTEXTLEN, (WPARAM)(int)(index), 0L))
   #define ListBox_GetText(hwndCtl, index, lpszBuffer)  ((int)(DWORD)SendMessage((hwndCtl), LB_GETTEXT, (WPARAM)(int)(index), (LPARAM)(LPCTSTR)(lpszBuffer)))

   #define ListBox_GetItemData(hwndCtl, index)         ((LRESULT)(DWORD)SendMessage((hwndCtl), LB_GETITEMDATA, (WPARAM)(int)(index), 0L))
   #define ListBox_SetItemData(hwndCtl, index, data)   ((int)(DWORD)SendMessage((hwndCtl), LB_SETITEMDATA, (WPARAM)(int)(index), (LPARAM)(data)))

   #if (WINVER >= 0x030a)
   #define ListBox_FindString(hwndCtl, indexStart, lpszFind) ((int)(DWORD)SendMessage((hwndCtl), LB_FINDSTRING, (WPARAM)(int)(indexStart), (LPARAM)(LPCTSTR)(lpszFind)))
   #define ListBox_FindItemData(hwndCtl, indexStart, data) ((int)(DWORD)SendMessage((hwndCtl), LB_FINDSTRING, (WPARAM)(int)(indexStart), (LPARAM)(data)))

   #define ListBox_SetSel(hwndCtl, fSelect, index)     ((int)(DWORD)SendMessage((hwndCtl), LB_SETSEL, (WPARAM)(BOOL)(fSelect), (LPARAM)(index)))
   #define ListBox_SelItemRange(hwndCtl, fSelect, first, last)    ((int)(DWORD)SendMessage((hwndCtl), LB_SELITEMRANGE, (WPARAM)(BOOL)(fSelect), MAKELPARAM((first), (last))))

   #define ListBox_GetCurSel(hwndCtl)                  ((int)(DWORD)SendMessage((hwndCtl), LB_GETCURSEL, 0L, 0L))
   #define ListBox_SetCurSel(hwndCtl, index)           ((int)(DWORD)SendMessage((hwndCtl), LB_SETCURSEL, (WPARAM)(int)(index), 0L))
 
   #define ListBox_SelectString(hwndCtl, indexStart, lpszFind) ((int)(DWORD)SendMessage((hwndCtl), LB_SELECTSTRING, (WPARAM)(int)(indexStart), (LPARAM)(LPCTSTR)(lpszFind)))
   #define ListBox_SelectItemData(hwndCtl, indexStart, data)   ((int)(DWORD)SendMessage((hwndCtl), LB_SELECTSTRING, (WPARAM)(int)(indexStart), (LPARAM)(data)))

   #define ListBox_GetSel(hwndCtl, index)              ((int)(DWORD)SendMessage((hwndCtl), LB_GETSEL, (WPARAM)(int)(index), 0L))
   #define ListBox_GetSelCount(hwndCtl)                ((int)(DWORD)SendMessage((hwndCtl), LB_GETSELCOUNT, 0L, 0L))
   #define ListBox_GetTopIndex(hwndCtl)                ((int)(DWORD)SendMessage((hwndCtl), LB_GETTOPINDEX, 0L, 0L))
   #define ListBox_GetSelItems(hwndCtl, cItems, lpItems) ((int)(DWORD)SendMessage((hwndCtl), LB_GETSELITEMS, (WPARAM)(int)(cItems), (LPARAM)(int *)(lpItems)))

   #define ListBox_SetTopIndex(hwndCtl, indexTop)      ((int)(DWORD)SendMessage((hwndCtl), LB_SETTOPINDEX, (WPARAM)(int)(indexTop), 0L))

   #define ListBox_SetColumnWidth(hwndCtl, cxColumn)   ((void)SendMessage((hwndCtl), LB_SETCOLUMNWIDTH, (WPARAM)(int)(cxColumn), 0L))
   #define ListBox_GetHorizontalExtent(hwndCtl)        ((int)(DWORD)SendMessage((hwndCtl), LB_GETHORIZONTALEXTENT, 0L, 0L))
   #define ListBox_SetHorizontalExtent(hwndCtl, cxExtent)     ((void)SendMessage((hwndCtl), LB_SETHORIZONTALEXTENT, (WPARAM)(int)(cxExtent), 0L))
 
   #define ListBox_SetTabStops(hwndCtl, cTabs, lpTabs) ((BOOL)(DWORD)SendMessage((hwndCtl), LB_SETTABSTOPS, (WPARAM)(int)(cTabs), (LPARAM)(int *)(lpTabs)))

   #define ListBox_GetItemRect(hwndCtl, index, lprc)   ((int)(DWORD)SendMessage((hwndCtl), LB_GETITEMRECT, (WPARAM)(int)(index), (LPARAM)(RECT *)(lprc)))

   #define ListBox_SetCaretIndex(hwndCtl, index)       ((int)(DWORD)SendMessage((hwndCtl), LB_SETCARETINDEX, (WPARAM)(int)(index), 0L))
   #define ListBox_GetCaretIndex(hwndCtl)              ((int)(DWORD)SendMessage((hwndCtl), LB_GETCARETINDEX, 0L, 0L))

   #define ListBox_FindStringExact(hwndCtl, indexStart, lpszFind) ((int)(DWORD)SendMessage((hwndCtl), LB_FINDSTRINGEXACT, (WPARAM)(int)(indexStart), (LPARAM)(LPCTSTR)(lpszFind)))

   #define ListBox_SetItemHeight(hwndCtl, index, cy)   ((int)(DWORD)SendMessage((hwndCtl), LB_SETITEMHEIGHT, (WPARAM)(int)(index), MAKELPARAM((cy), 0)))
   #define ListBox_GetItemHeight(hwndCtl, index)       ((int)(DWORD)SendMessage((hwndCtl), LB_GETITEMHEIGHT, (WPARAM)(int)(index), 0L))
   #endif  /* WINVER >= 0x030a */

   #define ListBox_Dir(hwndCtl, attrs, lpszFileSpec)   ((int)(DWORD)SendMessage((hwndCtl), LB_DIR, (WPARAM)(UINT)(attrs), (LPARAM)(LPCTSTR)(lpszFileSpec)))

#endif

/*
 * Multimedia API
 */

/*
 *  flag values for fuSound and fdwSound arguments on [snd]PlaySound
 */

#ifndef SND_SYNC

#define SND_SYNC            0x0000  /* play synchronously (default) */
#define SND_ASYNC           0x0001  /* play asynchronously */
#define SND_NODEFAULT       0x0002  /* silence (!default) if sound not found */
#define SND_MEMORY          0x0004  /* pszSound points to a memory file */
#define SND_LOOP            0x0008  /* loop the sound until next sndPlaySound */
#define SND_NOSTOP          0x0010  /* don't stop any currently playing sound */

#define SND_NOWAIT	0x00002000L /* don't wait if the driver is busy */
#define SND_ALIAS       0x00010000L /* name is a registry alias */
#define SND_ALIAS_ID	0x00110000L /* alias is a predefined ID */
#define SND_FILENAME    0x00020000L /* name is file name */
#define SND_RESOURCE    0x00040004L /* name is resource name or atom */
#if(WINVER >= 0x0400)
#define SND_PURGE           0x0040  /* purge non-static events for task */
#define SND_APPLICATION     0x0080  /* look for application specific association */
#endif /* WINVER >= 0x0400 */

#define SND_ALIAS_START	0           /* alias base */

#define	sndAlias(ch0, ch1)	(SND_ALIAS_START + (DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8))

#define SND_ALIAS_SYSTEMASTERISK        sndAlias('S', '*')
#define SND_ALIAS_SYSTEMQUESTION        sndAlias('S', '?')
#define SND_ALIAS_SYSTEMHAND            sndAlias('S', 'H')
#define SND_ALIAS_SYSTEMEXIT            sndAlias('S', 'E')
#define SND_ALIAS_SYSTEMSTART           sndAlias('S', 'S')
#define SND_ALIAS_SYSTEMWELCOME         sndAlias('S', 'W')
#define SND_ALIAS_SYSTEMEXCLAMATION     sndAlias('S', '!')
#define SND_ALIAS_SYSTEMDEFAULT         sndAlias('S', 'D')

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BOOL WINAPI PlaySoundA(LPCSTR pszSound, HMODULE hmod, DWORD fdwSound);
BOOL WINAPI PlaySoundW(LPCWSTR pszSound, HMODULE hmod, DWORD fdwSound);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef UNICODE
#define PlaySound  PlaySoundW
#else
#define PlaySound  PlaySoundA
#endif // !UNICODE

#endif
  // SND_SYNC (test for MM API already defined)

// Work around a bug in Cygwin headers

#undef TreeView_GetItemRect
#define TreeView_GetItemRect(hwnd, hitem, prc, code) \
  SendMessage((hwnd), TVM_GETITEMRECT, (WPARAM)(code), (LPARAM)(RECT *)(prc))

#endif
    /* __CYGWIN10__ */
#endif
    /* _WX_EXTRAH_ */
