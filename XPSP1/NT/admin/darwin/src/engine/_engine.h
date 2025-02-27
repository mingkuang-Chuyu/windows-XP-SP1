//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1996 - 1999
//
//  File:       _engine.h
//
//--------------------------------------------------------------------------

/* _engine.h - private definitions for CMsiEngine, CMsiConfigurationManager

 Included by object implementations, NOT by action implementations.
____________________________________________________________________________*/
#ifndef ___ENGINE
#define ___ENGINE
#include "engine.h"
#include "msi.h"
#include "msip.h"
#include "icust.h"
#include "remapi.h"

#include "_diagnos.h"

#define ENG  // ENG:: for readability, scoping globals within engine module
#define SRV  // SRV:: for readability, scoping services globals to engine module
#define MSI  // MSI:: for readability, namespace for MSI API

//__________________________________________________________________________
//
// Command-line options that are used within MSI
//__________________________________________________________________________

// Lower-case options are executed as they are seen.
// Upper-case options are executed last. There can only be 1 upper-case option specified on the command-line.

// The option can be specified in either upper or lower case on
// the command-line. 'a' and 'A' are the same option. They are
// just types of options.

#define NETWORK_PACKAGE_OPTION          'A'
#define REG_SHELL_DATA_OPTION           'D'
#define EMBEDDING_OPTION                'E'  // cannot change -- used by OLE
#define REPAIR_PACKAGE_OPTION           'F'
#define LANGUAGE_OPTION                 'g'
#define HELP_1_OPTION                   'H'
#define INSTALL_PACKAGE_OPTION          'I'
#define ADVERTISE_PACKAGE_OPTION        'J'
#define LOG_OPTION                      'l'
#define SMS_MIF_OPTION                  'm'
#define PROPERTIES_OPTION               'o'
#define APPLY_PATCH_OPTION              'P'
#define QUIET_OPTION                    'q'
#define REG_SERVER_OPTION               'R'
#define TRANSFORMS_OPTION               't'
#define UNREG_SERVER_OPTION             'U'
#define SERVICE_OPTION                  'V'
#define UNINSTALL_PACKAGE_OPTION        'X'
#define SELF_REG_OPTION                 'Y'
#define SELF_UNREG_OPTION               'Z'
#define HELP_2_OPTION                   '?'
#define INSTANCE_OPTION                 'n'
#define ADVERTISE_INSTANCE_OPTION       'c'

#define CHECKRUNONCE_OPTION             '@' // special option only allowed in RunOnce command-line
														  // so its not a part of the szCmdLineOptions array 

//__________________________________________________________________________
//
// Global variables in engine module
//__________________________________________________________________________

extern int g_cInstances; // defined by module.h within engine.cpp
extern scEnum g_scServerContext;
extern bool g_fWin9X;   // true if Windows 95 or 98, else false
extern bool g_fWinNT64; // true if 64-bit Windows NT, else false
extern int g_fSmartShell; // true if on shell that supports DD shortcuts
extern int g_iMajorVersion;
extern int g_iMinorVersion;
extern int g_iWindowsBuild;
extern HINSTANCE g_hInstance;
extern REGSAM g_samRead; // used in msinst.cpp to factor in the ability to read 64 bit hive from 32 bit msi.dll

// ShellFolder structures used in coreactn.cpp and execute.cpp for shell folders determination
// defined in services.cpp
extern const ShellFolder rgShellFolders[];
extern const ShellFolder rgAllUsersProfileShellFolders[];
extern const ShellFolder rgPersonalProfileShellFolders[];

// Reinstall mode flag chars
// WARNING: These characters must track the REINSTALLMODE bit flags in
//          msi.h. REINSTALLMODE 0x1 must correspond to the first reinstall 
//          mode specified here, 0x2 to the second, 0x4 to the third, etc...
//
//          Also, these modes must all be lower-case letters!

const ICHAR szReinstallMode[] ={'r',  // Reserved - unused
								'p',  // Reinstall only if file not present
								'o',  // Overwrite Older versioned files
								'e',  // Overwrite Equal versioned files
								'd',  // Overwrite files of Differing version (either older or newer)
								'c',  // Overwrite Corrupted exes and dlls
								'a',  // Overwrite All files, regardless of version
								'm',  // write required machine reg entries
								'u',  // write required user reg entries
								's',  // Install shortcuts, overwrite any existing
								'v',  // Re-install source install package
								0};


// Log file definitions
// WARNING: These characters must track the INSTALLLOGMODE bit flags in
//          msi.h. INSTALLLOGMODE 0x1 must correspond to the first log
//          mode specified here, 0x2 to the second, 0x4 to the third, etc...


const ICHAR szLogChars[] = {'m', // imtOutOfMemory
							'e', // imtError
							'w', // imtWarning
							'u', // imtUser
							'i', // imtInfo
							'f', // OBSOLETE: imtFilesInUse - only used as placeholder for this array
								  //           since INSTALLMESSAGE_FILESINUSE is in this place in the
								  //           INSTALLMESSAGE enum
							's', // imtResolveSource
							'o', // imtOutOfDiskSpace
							'a', // imtActionStart
							'r', // imtActionData (record)
							'p', // iLogPropertyDump
							'c', // imtCommonData
							'v', // verbose
								 // Place new true log modes here.
								 // See comment below on how these characters must track with lmaEnum
							'!', 
							 0};

#ifdef DEBUG
const ICHAR g_szNoSFCMessage[] = TEXT("Windows File Protection handle has not been initialized!");
#endif // DEBUG

// This should be the number of characters before the '!' character
const int cchLogModeCharsMax = 13;

enum lmaEnum	// Log Mode Attribute Enum
                // These should track the characters at the end of szLogChars
{
	lmaFlushEachLine = 0,
};


// Byte equivalents for each action
const int ibeRemoveFiles	       = 175000;
const int ibeRegisterFonts        = 1800000;
const int ibeUnregisterFonts      = 1800000;
const int ibeWriteRegistryValues  = 13200;
const int ibeRemoveRegistryValues = 13200;
const int ibeWriteIniValues       = 13200;
const int ibeRemoveIniValues      = 13200;
const int ibeSelfRegModules       = 1300000;
const int ibeSelfUnregModules     = 1300000;
const int ibeBindImage            = 800000;
const int ibeRegisterComponents   = 24000;
const int ibeUnregisterComponents = 24000;
const int ibeServiceControl       = 1300000;


const int iesReboot    = -1;     // private return Terminate: propogate reboot requirement to caller
const int iesRebootNow = -2;     // private return Terminate: propogate reboot requirement to caller
const int iesCallerReboot = -3;  // private return Terminate: expect caller to invoke reboot
const int iesRebootRejected = -4;// private return Terminate: reboot required but rejected by user

const int iesNotDoneYet = -1;     // private return from FindAndRunAction and RunThread
const int iesActionNotFound = -2; // private return from FindAndRunAction
const int iesExeLoadFailed  = -3; // private return from RunThread
const int iesDLLLoadFailed  = -5; // private return from RunThread
const int iesServiceConnectionFailed = -6; // private return from RunThread
const int iesUnsupportedScriptVersion = -10; // private return from CMsiExecute::RunScript

const int iesErrorIgnored   = -11; // private return from ixo* operations - indicates error occurred
											  // but was ignored so script processing should continue

const int msidbSumInfoSourceTypeURL = 0x8000; // private return from GetSourceType

const int iuiUseUninstallBannerText = 0x8000;
const int iuiNoModalDialogs   = 0x4000;
const int iuiDefault          = 0x2000;
const int iuiHideBasicUI      = 0x1000;
const int iuiHideCancel       = 0x0800;
const int iuiSourceResOnly    = 0x0400;

// Bit flags for the RuntimeFlags column of the component table
const int bfComponentCostMarker        = 0x01;
const int bfComponentCompressed        = 0x02;
const int bfComponentPatchable         = 0x04;
const int bfComponentDisabled          = 0x08;
const int bfComponentCostInitialized   = 0x10;
const int bfComponentNeverOverwrite    = 0x20;

// Bit flags for the RuntimeFlags column of the FeatureComponents table
const int bfComponentRegistered        = 0x01;

// Bit flags for the RuntimeFlags column of the Feature table
const int bfFeatureMark      = 0x01;
const int bfFeaturePatchable = 0x02;
const int bfFeatureCompressable = 0x04;

// MAINTAIN: compatibility with versions used to create script files
const int iScriptCurrentMinorVersion =  4; // bump when any change made to script format
const int iScriptCurrentMajorVersion = 21; // bump when non-backward-compatible change made to script format (SHOULD NEVER HAPPEN)

const int iScriptVersionMinimum = 18; // TEMP - should be set to iScriptCurrentMajorVersion when that is bumped to 21
const int iScriptVersionMaximum = iScriptCurrentMajorVersion;

// internal script flag that is set so that we honour the Assignment option in the script
// the MsiAdvertiseScript call ignores the Assignment option in the script so that the 
// user vs machine assignment is controlled by the flag in the MsiAdvertiseScript fn.
#define SCRIPTFLAGS_MACHINEASSIGN_SCRIPTSETTINGS 0x80000000L

// internal script flag that is set to force the reversal of the script operations 
// when unadvertising from MsiAdvertiseScript
#define SCRIPTFLAGS_REVERSE_SCRIPT 0x40000000L

// internal script flag to indicate we have been called via MsiAdvertiseScript
#define SCRIPTFLAGS_INPROC_ADVERTISEMENT 0x20000000L

// older SCRIPTFLAGS defines used for backward compatibility
const int SCRIPTFLAGS_REGDATA_OLD = 0x00000002L;
const int SCRIPTFLAGS_REGDATA_APPINFO_OLD = 0x00000010L;

enum ipiEnum  // GetInProgressInstallInfo record fields
{
	ipiProductKey = 1,
	ipiProductName,
	ipiLogonUser,
	ipiSelections,
	ipiFolders,
	ipiProperties,
	ipiDatabasePath,
	ipiDiskPrompt,
	ipiDiskSerial,
	ipiRunning,
	ipiTimeStamp,
	ipiSRSequence,   // System Restore Sequence # - Millenium only
	ipiAfterReboot,
	ipiEnumNext,
	ipiEnumCount = ipiEnumNext-1
};

enum ircSharedDllFlags
{
	ircenumRefCountDll        = 0x1,
	ircenumLegacyFileExisted  = 0x2,
};

enum tsEnum // TransformsSecure
{
	tsUnknown,   // Transforms are secure but we're not 
					 // sure yet whether they're relative or absolute
	tsNo,        // Transforms are not secure
	tsRelative,  // Transforms are secure and relatively pathed (i.e at-source)
	tsAbsolute   // Transforms are secure and absolutely pathed
};

// assignment types
enum iaaAppAssignment{
	iaaUserAssign = 0,
	iaaBegin = iaaUserAssign,
	iaaUserAssignNonManaged,
	iaaMachineAssign,
	iaaNone,
	iaaEnd = iaaNone,
};

// positions for applying appcompat transforms
enum iacpAppCompatTransformApplyPoint{
	iacpBeforeTransforms = 1,
	iacpAfterTransforms  = 2,
};

//__________________________________________________________________________
//
// Global factory functions in engine module
//__________________________________________________________________________

class CMsiEngine;

IMsiServices*  LoadServices();   // managed pointer, DO NOT RELEASE()
int            FreeServices();   // must be called for each LoadServices

IMsiServices*  CreateServices(); // should be removed from here and made private

IUnknown*    CreateEngine();
IMsiEngine*  CreateEngine(IMsiServer& riConfigManager);
IMsiEngine*  CreateEngine(IMsiDatabase& riDatabase);
IMsiEngine*  CreateEngine(IMsiStorage* piStorage, IMsiDatabase* piDatabase, CMsiEngine* piParentEngine, BOOL fServiceRequired);

IUnknown* CreateMessageHandler();
IMsiMessage* CreateMessageHandler(HWND hwndParent);

IUnknown* CreateExecutor();
IMsiExecute* CreateExecutor(IMsiConfigurationManager& riConfigurationManager,
									 IMsiMessage& riMessage, IMsiDirectoryManager* piDirectoryManager,
									 Bool fRollbackEnabled,
									 unsigned int fFlags = SCRIPTFLAGS_MACHINEASSIGN_SCRIPTSETTINGS | SCRIPTFLAGS_REGDATA | SCRIPTFLAGS_CACHEINFO | SCRIPTFLAGS_SHORTCUTS, HKEY* phKey = 0);
IMsiRecord*  CreateScriptEnumerator(const ICHAR* szScriptFile, IMsiServices& riServices,
												IEnumMsiRecord*& rpiEnum);

IMsiConfigurationManager* CreateConfigurationManager();
IMsiConfigurationManager* CreateConfigManagerAsServer();

IMsiCustomAction* CreateCustomAction();

IMsiServer* CreateMsiServerProxyFromRemote(IMsiServer& riDispatch);
IMsiRemoteAPI*    CreateMsiRemoteAPI();

const IMsiString& GetMsiDirectory();
const IMsiString& GetTempDirectory();

const ICHAR szLocalSystemSID[] = TEXT("S-1-5-18");
bool IsLocalSystemToken(HANDLE hToken);

void GetHomeEnvironmentVariables(const IMsiString*& rpiProperties);

IMsiRecord* GetSharedDLLCount(IMsiServices& riServices,
										const ICHAR* szFullFilePath,
										ibtBinaryType iType,
										const IMsiString*& rpistrCount);
IMsiRecord* SetSharedDLLCount(IMsiServices& riServices,
										const ICHAR* szFullFilePath,
										ibtBinaryType iType,
										const IMsiString& ristrCount);
extern IMsiRegKey* g_piSharedDllsRegKey;
extern IMsiRegKey* g_piSharedDllsRegKey32;	// initialized only on Win64 (to the redirected 32-bit key)
class CWin64DualFolders;
extern CWin64DualFolders g_Win64DualFolders;


//IMsiConfigurationManager* CreateConfigurationManager(IMsiServices& riServices);

IDispatch* CreateAutoEngine(MSIHANDLE hEngine);  // in autoapi.cpp

class CCoUninitialize
{
public:
	CCoUninitialize(bool fCoUninitialize) : m_fCoUninitialize(fCoUninitialize) {}
	~CCoUninitialize() {if (m_fCoUninitialize) OLE32::CoUninitialize();}
protected:
	bool m_fCoUninitialize;
};


//__________________________________________________________________________
//
// global string objects exposed without implementation
// dummy implementation of IMsiString to allow external global string object refs

// NOTE: implementation required to get around compiler bug that won't allow
// externs to global string object refs.  This is a purely virtual class that mimics
// the CMsiStringBase: public IMsiString declaration.  Additionally, for support on Win64,
// must also include the same data members because on IA64, global variables come
// in 2 different flavors -- near and far -- depending on their size.  Without the data
// members, the global variable is generated as near because it has no data members.
// The linker error (lnk2003) results because the actual variable is far

//__________________________________________________________________________

class CMsiStringExternal : public IMsiString
{
 public:
	HRESULT       __stdcall QueryInterface(const IID& riid, void** ppvObj);
	unsigned long __stdcall AddRef();
	unsigned long __stdcall Release();
	const IMsiString&   __stdcall GetMsiStringValue() const;
	const ICHAR*  __stdcall GetString() const;
#ifdef USE_OBJECT_POOL
	unsigned int  __stdcall GetUniqueId() const;
	void          __stdcall SetUniqueId(unsigned int id);
#endif //USE_OBJECT_POOL
	int           __stdcall CopyToBuf(ICHAR* rgch, unsigned int cchMax) const;
	void          __stdcall SetString(const ICHAR* sz, const IMsiString*& rpi) const;
	int           __stdcall GetIntegerValue() const;
	int           __stdcall TextSize() const;
	int           __stdcall CharacterCount() const;
	Bool          __stdcall IsDBCS() const;
	void          __stdcall RefString(const ICHAR* sz, const IMsiString*& rpi) const;
	void          __stdcall RemoveRef(const IMsiString*& rpi) const;
	void          __stdcall SetChar  (ICHAR ch, const IMsiString*& rpi) const;
	void          __stdcall SetInteger(int i,   const IMsiString*& rpi) const;
	void          __stdcall SetBinary(const unsigned char* rgb, unsigned int cb, const IMsiString*& rpi) const;
	void          __stdcall AppendString(const ICHAR* sz, const IMsiString*& rpi) const;
	void          __stdcall AppendMsiString(const IMsiString& pi, const IMsiString*& rpi) const;
	const IMsiString&   __stdcall AddString(const ICHAR* sz) const;
	const IMsiString&   __stdcall AddMsiString(const IMsiString& ri) const;
	const IMsiString&   __stdcall Extract(iseEnum ase, unsigned int iLimit) const;
	Bool          __stdcall Remove(iseEnum ase, unsigned int iLimit, const IMsiString*& rpi) const;
	int           __stdcall Compare(iscEnum asc, const ICHAR* sz) const;
	void          __stdcall UpperCase(const IMsiString*& rpi) const;
	void          __stdcall LowerCase(const IMsiString*& rpi) const;
	ICHAR*        __stdcall AllocateString(unsigned int cb, Bool fDBCS, const IMsiString*& rpi) const;
 protected:  // state data
	int  m_iRefCnt;
	unsigned int  m_cchLen;
};
class CMsiStringNull : public CMsiStringExternal {};
class CMsiStringLive : public CMsiStringExternal {};

extern const CMsiStringNull g_MsiStringNull;     // THE only static null string object
extern const CMsiStringLive g_MsiStringDate;     // dynamic global date string object
extern const CMsiStringLive g_MsiStringTime;     // dynamic global time string object

//__________________________________________________________________________
//
// Global factory functions from services, available only in engine+services DLL
//__________________________________________________________________________

inline const IMsiString&  CreateString() {return g_MsiStringNull;};  //!! obsolete
IMsiRecord&  CreateRecord(unsigned int cParam);
ICHAR*       AllocateString(unsigned int cbSize, Bool fDBCS, const IMsiString*& rpiStr);
IMsiRecord*  CreateFileStream(const ICHAR* szFile, Bool fWrite, IMsiStream*& rpiStream);

//__________________________________________________________________________
//
// General utility functions
//__________________________________________________________________________

const IMsiString& GetInstallerMessage(UINT iError);
UINT              MapInitializeReturnToUINT(ieiEnum iei);
ieiEnum           MapStorageErrorToInitializeReturn(IMsiRecord* piError);
bool			  __stdcall FIsUpdatingProcess (void);
IMsiRecord*       GetServerPath(IMsiServices& riServices, bool fUNC, bool f64Bit,
										  const IMsiString*& rpistrServerPath);

void              CreateCabinetStreamList(IMsiEngine& riEngine, const IMsiString*& rpistrStreamList);

Bool              GetProductInfo(const ICHAR* szProductKey, const ICHAR* szProperty, CTempBufferRef<ICHAR>& rgchInfo);
Bool              GetPatchInfo(const ICHAR* szPatchCode, const ICHAR* szProperty, CTempBufferRef<ICHAR>& rgchInfo);

Bool              GetExpandedProductInfo(const ICHAR* szProductCode, const ICHAR* szProperty, 
                     CTempBufferRef<ICHAR>& rgchExpandedInfo, bool fPatch=false);
IMsiRecord*       GenerateSD(IMsiEngine& riEngine, IMsiView& riviewLockList, IMsiRecord* piExecute, IMsiStream*& rpiSD);
IMsiRecord*       GetSourcedir(IMsiDirectoryManager& riDirManager, const IMsiString*& rpiValue);
IMsiRecord*       GetSourcedir(IMsiDirectoryManager& riDirManager, IMsiPath*& rpiPath);
Bool              IsCachedPackage(IMsiEngine& riEngine, const IMsiString& riPackage, Bool fPatch = fFalse, const ICHAR* szPatchCode = 0);
Bool              FFeaturesInstalled(IMsiEngine& riEngine, Bool fAllClients = fTrue);
IMsiRecord*       GetProductClients(IMsiServices& riServices, const ICHAR* szProduct, const IMsiString*& rpistrClients);
void              ExpandEnvironmentStrings(const ICHAR* szString, const IMsiString*& rpiExpandedString);
IMsiRecord*       GetComponentPath(IMsiServices& riServices, const ICHAR* szUserId, const IMsiString& riProductKey, 
											  const IMsiString& riComponentCode, 
											  IMsiRecord *& rpiRec,
											  iaaAppAssignment* piaaAsgnType);
Bool              ProcessCommandLine(const ICHAR* szCommandLine,
											  const IMsiString** ppistrLanguage, const IMsiString** ppistrTransforms,
											  const IMsiString** ppistrPatch, const IMsiString** ppistrAction,
											  const IMsiString** ppistrDatabase,
											  const IMsiString* pistrOtherProp, const IMsiString** ppistrOtherPropValue,
											  Bool fUpperCasePropNames, const IMsiString** ppistrErrorInfo,
											  IMsiEngine* piEngine, bool fRejectDisallowedProperties=false);
unsigned int      ProductVersionStringToInt(const ICHAR* szVersion);
HANDLE            GetUserToken();

bool              __stdcall TestAndSet(int* pi);
extern "C" void   MsiInvalidateFeatureCache();
IMsiRecord* ExpandShellFolderTransformPath(const IMsiString& riOriginalPath, const IMsiString*& riExpandedPath, IMsiServices& riServices);

struct PatchTransformState
{
	int iMinDiskID;
	int iMaxDiskID;
	int iMinSequence;
	int iMaxSequence;
};


IMsiRecord* ApplyTransform(IMsiDatabase& riDatabase,
									IMsiStorage& riTransform,
									int iErrorConditions,
									bool fPatchOnlyTransform,
									PatchTransformState* piState);

bool PostScriptWriteError(IMsiMessage& riMessage);
bool WriteScriptRecord(CScriptGenerate* pScript, ixoEnum ixoOpCode, IMsiRecord& riParams,
							  bool fForceFlush, IMsiMessage& riMessage);

IMsiRecord* SetInProgressInstallInfo(IMsiServices& riServices, IMsiRecord& riRec);
IMsiRecord* UpdateInProgressInstallInfo(IMsiServices& riServices, IMsiRecord& riRec);
IMsiRecord* GetInProgressInstallInfo(IMsiServices& riServices, IMsiRecord*& rpiRec);
bool ClearInProgressInformation(IMsiServices& riServices);
DWORD GetFileLastWriteTime(const ICHAR* szSrcFile, FILETIME& rftLastWrite);
DWORD MsiSetFileTime(const ICHAR* szDestFile, FILETIME* pftLastWrite);


#ifdef DEBUG
void              DisplayAccountName(const ICHAR* szMessage, PISID pSid=0);
bool              GetAccountNameFromToken(HANDLE hToken, ICHAR* szAccount);
#define           DISPLAYACCOUNTNAMEFROMSID(m, s) DisplayAccountName(m, s)
#define           DISPLAYACCOUNTNAME(m)           DisplayAccountName(m)
#define           GETACCOUNTNAMEFROMTOKEN(t, a)   GetAccountNameFromToken(t,a);
#else
#define           DISPLAYACCOUNTNAMEFROMSID(m, s)
#define           DISPLAYACCOUNTNAME(m)
#define           GETACCOUNTNAMEFROMTOKEN(t, a)
#endif

//__________________________________________________________________________
//
// SID manipulation functions
//__________________________________________________________________________

void  GetStringSID(PISID pSID, ICHAR* szSID);
DWORD GetUserSID(HANDLE hToken, char* rgchSID);
//DWORD GetUserStringSID(HANDLE hToken, ICHAR* szSID);
DWORD GetCurrentUserSID(char* rgchSID);
DWORD GetCurrentUserStringSID(const IMsiString*& rpistrSid);
DWORD GetCurrentUserStringSID(ICHAR* szSID);
DWORD GetCurrentUserToken(HANDLE& hToken, bool& fCloseHandle);

struct ThreadIdImpersonate
{
	DWORD        m_dwThreadId;
	DWORD        m_dwClientThreadId;
};

// return values from PathType
enum iptEnum
{
	iptInvalid = 1,
	iptRelative,
	iptFull,
};
iptEnum      PathType(const ICHAR* szPath);


enum ielEnum
{
	ielNoAction = 0,
	ielLogFatalError,
	ielFatalErrorLogged,
	ielNextEnum
};

//____________________________________________________________________________
//
// Script record format definitions
//   all data is 16-bit aligned, except within non-Unicode strings
//____________________________________________________________________________


const int iScriptSignature     = 0x534f5849L; // signature to valid script file type

//____________________________________________________________________________
//
// User registraion and PID 2.0 definitions
//____________________________________________________________________________

// ProductId definitions
const int cchPidRpc    = 5;  // product code, followed by '-'
const int cchPidSite   = 3;  // site code, followed by '-'
const int cchPidSerial = 7;  // serial number with check digit, followed by '-'
const int cchPidUnique = 5;  // randomized per install, or part of OEM COA serial
const int cchPidTotal = cchPidRpc + 1 + cchPidSite + 1 + cchPidSerial + 1 + cchPidUnique;
const int cchPidCdKey = cchPidSite + 1 + cchPidSerial;
const ICHAR chPidSeparator = '-'; // dashes used to separate PID fields

// Next location to query for User/Company info, ACME installs, HKEY_CURRENT_USER,
// if MsiGetUserInfo fails to get the information
const ICHAR szUserInfoKey[] = TEXT("Software\\Microsoft\\MS Setup (ACME)\\User Info");
const ICHAR szDefName[]     = TEXT("DefName");
const ICHAR szDefOrg[]      = TEXT("DefCompany");
// Final location to query for User/Company info, OS installation, HKEY_LOCAL_MACHINE
const ICHAR szSysUserKey[]  = TEXT("Software\\Microsoft\\Windows\\CurrentVersion");
const ICHAR szSysUserKeyNT[]  = TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion");
const ICHAR szSysUserName[] = TEXT("RegisteredOwner");
const ICHAR szSysOrgName[]  = TEXT("RegisteredOrganization"); 

const int cchUserNameOrgMax = 62;

//____________________________________________________________________________
//
// Miscellaneous shared constants
//____________________________________________________________________________

const ICHAR szDefaultAction[] = TEXT("INSTALL");

const ICHAR szRunOnceKey[] = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce");
const int cbMaxSID                   = sizeof(SID) + SID_MAX_SUB_AUTHORITIES*sizeof(DWORD);
const ICHAR szBlankVolumeLabelToken[] = TEXT("?");

// string representing self as client, for parent installs
const ICHAR szSelfClientToken[] = TEXT(":");


const ICHAR szUserEnvironmentSubKey[]    = TEXT("Environment");
const ICHAR szMachineEnvironmentSubKey[] = TEXT("System\\CurrentControlSet\\Control\\Session Manager\\Environment");

const ICHAR szNonEmptyPath[] = TEXT("TOKEN"); // token string to cause removal of the filename registration

const ICHAR szUnresolvedSourceRootTokenWithBS[] = TEXT("1\\");

//____________________________________________________________________________
//
// Miscellaneous queries used by several actions
//____________________________________________________________________________
const ICHAR sqlLockPermissions[] = TEXT("SELECT `Domain`,`User`,`Permission` FROM `LockPermissions` WHERE `Table`=? AND `LockObject`=? ORDER BY `Permission`");

//____________________________________________________________________________
//
// CMsiBindStatusCallback class - progress handler for internet download
//____________________________________________________________________________

DWORD DownloadUrlFile(const ICHAR* szPotentialURL, const IMsiString*& rpistrDownload, Bool& fURL, int cTicks = 0);

class CMsiBindStatusCallback : public IBindStatusCallback
{
 public: // IUnknown implemented virtual functions
	HRESULT         __stdcall QueryInterface(const IID& riid, void** ppvObj);
	unsigned long   __stdcall AddRef();
	unsigned long   __stdcall Release();
 public: // IBindStatusCallback implemented virtual functions

	/*----------------------------------------------------------------------------
	cTicks is the number of ticks we're allotted in the progress bar. If cTicks
	is 0 then we'll assume that we own the progress bar and use however many
	ticks we want, resetting the progress bar when we start and when we're done.
	If cTicks is set, however, we won't reset the progress bar. 
  -----------------------------------------------------------------------------*/
	CMsiBindStatusCallback(unsigned int cTicks = 0);

	HRESULT __stdcall OnStartBinding(DWORD, IBinding*) {return S_OK;}
	HRESULT __stdcall GetPriority(LONG*) {return S_OK;}
	HRESULT __stdcall OnLowResource(DWORD ) {return S_OK;}
	HRESULT __stdcall OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText);
	HRESULT __stdcall OnStopBinding(HRESULT, LPCWSTR ) {return S_OK;}
	HRESULT __stdcall GetBindInfo(DWORD*, BINDINFO*) {return S_OK;}
	HRESULT __stdcall OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) {return S_OK;}
	HRESULT __stdcall OnObjectAvailable(REFIID, IUnknown*) {return S_OK;}
 private:
	int          m_iRefCnt;
	PMsiRecord   m_pProgress;
	unsigned int m_cTicksSoFar;
	unsigned int m_cTotalTicks;
	Bool         m_fResetProgress;
};

//____________________________________________________________________________
//
// External handle management
//____________________________________________________________________________

typedef unsigned long MSIHANDLE;     // abstract generic handle, 0 == no handle
MSIHANDLE CreateMsiHandle(IUnknown* pi, int iid); // no AddRef called
MSIHANDLE CreateMsiProductHandle(IMsiEngine* pi); // no AddRef called

IMsiEngine* GetEngineFromHandle(MSIHANDLE h);
IUnknown* FindMsiHandle(MSIHANDLE h, int iid);

class CActionThreadData;
iesEnum ScheduledCustomAction(IMsiRecord& riParams, const IMsiString& ristrProductCode,
				LANGID langid, IMsiMessage& riMessage, bool fRunScriptElevated, bool fAppCompatEnabled, 
				const GUID* guidAppCompatDB, const GUID* guidAppCompatID);
MSIHANDLE CreateCustomActionContext(int icaFlags, const IMsiString& ristrCustomActionData,
					const IMsiString& ristrProductCode, LANGID langid, IMsiMessage& riMessage);
void WaitForCustomActionThreads(IMsiEngine* piEngine, Bool fTerminate, IMsiMessage& riMessage);

//____________________________________________________________________________
//
// Exception handling functions
//____________________________________________________________________________

extern void GenerateExceptionReport(LPEXCEPTION_POINTERS pExceptionInfo);
extern void GenerateExceptionReport(EXCEPTION_RECORD* pExceptionRecord, CONTEXT* pCtx);
extern int HandleException(LPEXCEPTION_POINTERS pExceptionInfo);

//____________________________________________________________________________
//
// String handling utilities
//____________________________________________________________________________

UINT FillBufferW(const ICHAR* psz, unsigned int cch, LPWSTR szBuf, DWORD* pcchBuf);
UINT FillBufferA(const ICHAR* psz, unsigned int cch, LPSTR szBuf, DWORD* pcchBuf);

inline UINT FillBufferA(const IMsiString* pistr, LPSTR szBuf, DWORD* pcchBuf)
{
	return FillBufferA((pistr ? pistr->GetString() : 0), (pistr ? pistr->TextSize() : 0), szBuf, pcchBuf);
}

inline UINT FillBufferW(const IMsiString* pistr, LPWSTR szBuf, DWORD* pcchBuf)
{
	return FillBufferW((pistr ? pistr->GetString() : 0), (pistr ? pistr->TextSize() : 0), szBuf, pcchBuf);
}

const IMsiString& GetMsiStringW(LPCWSTR sz);

// special chars in string

const ICHAR DELIMITER_BEGIN('[');
const ICHAR DELIMITER_END(']');
const ICHAR PATH_TOKEN('%');
const ICHAR FILE_TOKEN('#');
const ICHAR STORAGE_TOKEN(':');  // transform
const ICHAR PATCHONLY_TOKEN('#');  // transform containing patch information only
const ICHAR SHELLFOLDER_TOKEN('*');  // transform
const ICHAR SECURE_RELATIVE_TOKEN('@'); // transform
const ICHAR SECURE_ABSOLUTE_TOKEN('|'); // transform

//____________________________________________________________________________
//
// CMsiClientMessage definition - COM object to wrapper g_MessageContext.Invoke
//____________________________________________________________________________

class CMsiClientMessage: public IMsiMessage
{
 public: // IMsiMessage implemented virtual functions
	HRESULT         __stdcall QueryInterface(const IID& riid, void** ppvObj);
	unsigned long   __stdcall AddRef();
	unsigned long   __stdcall Release();
	imsEnum         __stdcall Message(imtEnum imt, IMsiRecord& riRecord);
	imsEnum         __stdcall MessageNoRecord(imtEnum imt);
 public: // constructor/destructor
	void *operator new(size_t cb) { return AllocSpc(cb); }
	void operator delete(void * pv) { FreeSpc(pv); }
	CMsiClientMessage() : m_iRefCnt(1) {g_cInstances++;}
 private:
	int                m_iRefCnt;
	bool               m_fMessageContextInitialized;
	friend IUnknown*   CreateMessageHandler();
};

//____________________________________________________________________________
//
// MsiUIMessageContext - Message dispatching definitions
//____________________________________________________________________________

#define imtInvalid  imtEnum(0x80000000)  // to detect invalid event triggers
#define imsInvalid  imsEnum(0x80000000)  // to detect invalid event triggers
#define imsBusy     imsEnum(0x80000001)  // in UI processing thread

class CBasicUI;
class CMsiConfigurationManager;

struct CMainThreadData  // temp arguments to CreateAndRunEngine passed to new thread
{
	CMainThreadData(ireEnum ireProductSpec) : m_ireProductSpec(ireProductSpec) {}
	ireEnum m_ireProductSpec;     // type of product specification
};

struct CEngineMainThreadData : public CMainThreadData
{
	CEngineMainThreadData(ireEnum ireProductSpec, const ICHAR* szProduct, const ICHAR* szAction, const ICHAR* szCmdLine, iioEnum iioOptions) : 
								CMainThreadData(ireProductSpec), 
								m_szProduct(szProduct), 
								m_szAction(szAction), 
								m_szCmdLine(szCmdLine),
								m_iioOptions(iioOptions) {}

	const ICHAR* m_szProduct;  // product specification
	const ICHAR* m_szAction;   // optional, engine defaults to "INSTALL"
	const ICHAR* m_szCmdLine;  // optional property list
	iioEnum      m_iioOptions; // install options
};

struct CInstallFinalizeMainThreadData : public CMainThreadData
{
	CInstallFinalizeMainThreadData(ireEnum ireProductSpec, iesEnum iesState, CMsiConfigurationManager* piConman) : 
							CMainThreadData(ireProductSpec), 
							m_iesState(iesState), 
							m_piConman(piConman) {}

	iesEnum                   m_iesState;
	CMsiConfigurationManager* m_piConman;
};

// Use this #define to enable use of the undocumented RtlSetCurrentEnvironment
//!! Need to decide whether we should define this or not

//#define FAST_BUT_UNDOCUMENTED

struct MsiUIMessageContext
{
 public: // data dynamically set by message handling before Invoke
	IMsiEngine*           m_piEngine;     // temp. for LoadHandler, not ref counted
	const ICHAR*          m_szAction;     // temp. for ShowDialog, not allocated
	CRITICAL_SECTION      m_csDispatch;   // serialization of UI message requests
	ICHAR                 m_rgchExceptionInfo[1024]; // stores exception message when we crash
 private:
	IMsiRecord*           m_pirecMessage; // current message
	imtEnum               m_imtMessage;   // type of current message or function request
 private:  // data set during creation of main thread, or by a function dispatch
	HANDLE  /* / [0] \ */ m_hUIRequest;   // UI request event, must preceed m_hMainThread
	HANDLE  /* \ [1] / */ m_hMainThread;  // main engine thread, must follow m_hUIRequest
	HANDLE  /* / [0] \ */ m_hUIReturn;    // event to unblock UI request thread, preceeds m_hUIThread
	HANDLE  /* \ [1] / */ m_hUIThread;    // UI thread if UI in child thread, must follow m_hUIReturn
	DWORD                 m_tidUIHandler; // thread ID used to identify call from UI thread
	DWORD                 m_tidMainThread; // thread ID used to identify call from MainEngineThread
	DWORD                 m_tidInitialize; // thread ID used to initialize this object
	DWORD                 m_tidDisableMessages; // disable messages for this thread; used for custom actions in the UI thread
	HINSTANCE             m_hinstHandler;  // DLL instance handle if UI handler used
	int                   m_iLogMode;      // mask of message types to log
	IMsiRecord*           m_pirecNoData;   // empty record used internally, access only via GetNoDataRecord()
	imsEnum               m_imsReturn;     // return status passed back to requestor
	bool                  m_fCancelPending; // UI cancel status, cached response to progress messages 
	bool                  m_fInitialized;  // message context initialized
#ifdef DEBUG
	bool                  m_fCancelReturned;// UI cancel status returned from progress message, save for assert
#endif
	HANDLE                m_hUserToken;    // user impersonation token
	int                   m_iBusyLock;     // 1 when message context busy (initialized), 0 when not
	HANDLE                m_hExternalMutex;// named mutex for testing by external processes, such as autorun
	IMsiHandler*          m_piHandlerSave; // in case handler disabled
//	IServerSecurity*      m_piServerSecurity;// call context to allow impersonation
	IMsiMessage*          m_piClientMessage; // message object from the client side
	HANDLE                m_hLogFile;        // handle to log file if open, else 0
	bool                  m_fLoggingFromPolicy; // policy has triggered logging  //TODO: rename to m_fTemporaryLog
	iuiEnum               m_iuiLevel;        // UI level
	int                   m_cTimeoutDisable; // for custom actions to disable timeout UI
	int                   m_cTimeoutSuppress;// for actions to suppress timeout when no messages being sent
	int                   m_iTimeoutRetry;   // current retry counter
	LPTOP_LEVEL_EXCEPTION_FILTER m_tlefOld;  // old exception fileter
	bool                  m_fHideBasicUI; // set to prevent basic UI from being initialized
	HWND                  m_hwndHidden;      // Hidden window
	bool                  m_fServicesAndCritSecInitialized;
	IMsiServices*         m_piServices;
	LANGID                m_iLangId;         // language of package, used to select resource strings
	unsigned int          m_iCodepage;       // codepage of package, used to select font charset
	bool                  m_fNoModalDialogs;
	bool                  m_fHideCancel;
	bool                  m_fUseUninstallBannerText;
	bool                  m_fSourceResolutionOnly;
	HWND                  m_hwndDebugLog;
	bool                  m_fOEMInstall;
	bool                  m_fOleInitialized; // whether OLE has been initialized on the thread
	bool                  m_fChildUIOleInitialized; // whether OLE has been initialized on the child UI thread
#ifdef FAST_BUT_UNDOCUMENTED
	WCHAR*                m_pchEnvironment;   // process environment block before it's filled with user data
#endif
 public: //!! only until GetHandler() fixed or better removed
	IMsiHandler*          m_piHandler;    // full UI handler, only if in use
	HANDLE m_hSfcHandle;   //  handle to Windows 2000 system file protection service
	SAFER_LEVEL_HANDLE m_hSaferLevel; // handle to Whistler SAFER authorization level object
 public:
	UINT    Initialize(bool fCreateUIThread, iuiEnum iuiLevel); // false if UI in main thread, true if UI in child thread
	bool    Terminate(bool fFatalExit);       // false for normal termination, true if main thread dead

	UINT    RunInstall(CMainThreadData& riThreadData,
							 iuiEnum iuiLevel,
							 IMsiMessage* piClientMessage);// optional client message handler
	imsEnum Invoke(imtEnum imt, IMsiRecord* piRecord);
	HWND    GetCurrentWindow();
	const ICHAR* GetWindowCaption();
	bool    IsHandlerLoaded()        { return m_piHandler   != 0; }
	bool    IsInitialized()          { return m_fInitialized; }
//	bool    MainEngineThreadExists() { return m_hMainThread != 0; }
	bool    ChildUIThreadExists()    { return m_hUIThread   != 0; }
	bool    ChildUIThreadRunning()   { DWORD extCode; 
											if (ChildUIThreadExists())
												return !GetExitCodeThread(m_hUIThread, &extCode);
											return fFalse;
									   };
	bool    IsUIThread()             { return WIN::GetCurrentThreadId() == m_tidUIHandler; }
	bool    IsMainEngineThread()     { return WIN::GetCurrentThreadId() == m_tidMainThread; }
	int     GetLogMode()             { return m_iLogMode; }
	LANGID  GetCurrentUILanguage();
//	IServerSecurity* GetServerSecurity() { return m_piServerSecurity;}
	HANDLE           GetUserToken() { AssertSz(IsThreadSafeForSessionImpersonation(), "Security Warning: Accessing install session token from a direct COM thread. This is a security hole in multi-user scenarios.");
									  return m_hUserToken;
									};
	IMsiRecord* GetNoDataRecord();
	void    DisableTimeout()         { m_cTimeoutDisable++; }
	void    EnableTimeout()          { if (m_cTimeoutDisable) m_cTimeoutDisable--; }
	void    SuppressTimeout()        { m_cTimeoutSuppress++; }
	//!! the following should be imt operations or put in CriticalSection!
	void    DisableHandler()         { if (!m_piHandlerSave) m_piHandlerSave=m_piHandler, m_piHandler=0;}
	void    RestoreHandler()         { if (m_piHandlerSave)  m_piHandler=m_piHandlerSave, m_piHandlerSave=0;}
	UINT    SetUserToken(bool fReset=false);
	void    DisableThreadMessages(DWORD tid) { Assert(m_tidDisableMessages == 0); m_tidDisableMessages = tid;}
	void    EnableMessages() { Assert(m_tidDisableMessages != 0); m_tidDisableMessages = 0;}
#ifdef DEBUG
	bool    WasCancelReturned() {if (m_fCancelReturned){ m_fCancelReturned = false; return true; } return false;}
#endif
	iuiEnum GetUILevel() { return m_iuiLevel; } // For use *ONLY* to set the CLIENTUILEVEL property
	void    LogDebugMessage(const ICHAR* szMessage);
	inline bool IsOEMInstall() { return m_fOEMInstall; }
	inline void SetOEMInstall(bool fArg) { m_fOEMInstall = fArg; }
 private:
	static DWORD WINAPI MsiUIMessageContext::ChildUIThread(MsiUIMessageContext* This);
	static DWORD WINAPI MsiUIMessageContext::MainEngineThread(LPVOID);
	static LONG  WINAPI MsiUIMessageContext::ExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo);
	bool FCreateHiddenWindow();
	void KillHiddenWindow();
	imsEnum ProcessMessage(imtEnum imt, IMsiRecord* piRecord); // UI thread message processor
	bool    InitializeEnvironmentVariables();
	bool    RestoreEnvironmentVariables();
	bool    InitializeLog(bool fDynamicLog = false);

 public:  // static constructor
	MsiUIMessageContext()  // static constructor optimized to init static data
	: m_hUIRequest(0), m_hUIReturn(0), m_hMainThread(0), m_hUIThread(0), m_hExternalMutex(0)
	, m_piEngine(0), m_piHandler(0), m_hinstHandler(0), m_piHandlerSave(0)
//	, m_piServerSecurity(0)
	, m_pirecMessage(0), m_pirecNoData(0)
	, m_hLogFile(0), m_cTimeoutDisable(0), m_cTimeoutSuppress(0), m_hUserToken(0), m_iTimeoutRetry(0)
	, m_imtMessage(imtInvalid), m_imsReturn(imsInvalid), m_fCancelPending(false), m_iuiLevel((iuiEnum)iuiDefault)
	, m_fHideBasicUI(false), m_fHideCancel(false), m_fInitialized(false), m_fLoggingFromPolicy(false)
	, m_iBusyLock(0), m_tidUIHandler(0), m_tidMainThread(0), m_tidDisableMessages(0), m_hwndHidden(0)
	, m_piServices(0), m_iLangId(0), m_iCodepage(0)
	, m_hSfcHandle(0), m_hSaferLevel(0), m_fOEMInstall(false), m_fNoModalDialogs(false), m_fUseUninstallBannerText(false)
	, m_fOleInitialized(false), m_fChildUIOleInitialized(false), m_fSourceResolutionOnly(false)
#ifdef FAST_BUT_UNDOCUMENTED
	, m_pchEnvironment(0), m_hwndDebugLog(0)
#endif
#ifdef DEBUG
	, m_fCancelReturned(false) 
#endif
	{
		m_csDispatch.OwningThread = INVALID_HANDLE_VALUE;
	}
 friend bool CreateLog(const ICHAR* szFile, bool fAppend);
 friend bool LoggingEnabled();
 friend bool WriteLog(const ICHAR* szText);

};

extern MsiUIMessageContext g_MessageContext;  // only one per process


//____________________________________________________________________________
//
// CMsiEngine definitions   
//____________________________________________________________________________

enum scmEnum // m_scmScriptMode
{
	scmIdleScript, // not writing or running script
	scmWriteScript, // writing script
	scmRunScript, // running script
};

enum ippEnum // types of in-progress property strings
{
	ippSelection,
	ippFolder,
	ippProperty,
};

enum issEnum   // install sequence state/segment
{
	issNotSequenced = 0,     // no sequence running, actions called directly
	issPreExecution,         // before InstallInitialize (before script generation)
	issScriptGeneration,     // after InstallInitialize, before InstallFinalize
	issPostExecution,        // after InstallFinalize (after script execution)
};

enum ipitEnum // bit-flags: return from InProgressInstallType
{
	ipitSameConfig    = 0x00,
	ipitDiffUser      = 0x01,
	ipitDiffProduct   = 0x02,
	ipitDiffConfig    = 0x04,
};

enum ieftEnum // Index into the array of ints that gives us column numbers to the file table
{
	ieftKey,
	ieftComponent,
	ieftAttributes,
	ieftName,
	ieftMax,
};

enum ievtEnum // ValidateTransform return values
{
	ievtTransformValid,   // valid transform
	ievtTransformRejected,// transform rejected due to policy
	ievtTransformFailed,  // transform failed
};

class CActionThreadData;  // custom action thread data, defined in action.cpp

// token class used by engine
struct CClientEnumToken{
	CClientEnumToken():m_dwProductIndex(0), m_pCursor(0){}
	void Reset(){m_dwProductIndex = 0;m_pCursor = 0;}
	int m_dwProductIndex;
	PMsiCursor m_pCursor;
};

// Forward declaration
class CMsiServerConnMgr;

class CMsiEngine : public IMsiEngine,
						 public IMsiSelectionManager,
						 public IMsiDirectoryManager
#ifdef DEBUG
						, public IMsiDebug
#endif //DEBUG
{
 public: // IMsiEngine implemented virtual functions
	HRESULT         __stdcall QueryInterface(const IID& riid, void** ppvObj);
	unsigned long   __stdcall AddRef();
	unsigned long   __stdcall Release();
	ieiEnum         __stdcall Initialize(const ICHAR* szDatabase,
													 iuiEnum iuiLevel,
													 const ICHAR* szCommandLine,
													 const ICHAR* szProductCode,
													 iioEnum iioOptions);
	iesEnum         __stdcall Terminate(iesEnum iesState);
	IMsiServices*   __stdcall GetServices();
	IMsiHandler*    __stdcall GetHandler();
	IMsiDatabase*   __stdcall GetDatabase();
	IMsiServer*     __stdcall GetConfigurationServer();
	LANGID          __stdcall GetLanguage();
	int             __stdcall GetMode();
	void            __stdcall SetMode(int iefMode, Bool fState);
	iesEnum         __stdcall DoAction(const ICHAR* szAction);
	iesEnum         __stdcall Sequence(const ICHAR* szColumn);
	iesEnum         __stdcall ExecuteRecord(ixoEnum ixoOpCode, IMsiRecord& riParams);
	imsEnum         __stdcall Message(imtEnum imt, IMsiRecord& riRecord);
	imsEnum         __stdcall MessageNoRecord(imtEnum imt);
	int             __stdcall SelectLanguage(const ICHAR* szLangList, const ICHAR* szCaption);
	IMsiRecord*     __stdcall OpenView(const ICHAR* szName, ivcEnum ivcIntent,
												  IMsiView*& rpiView);
	const IMsiString&     __stdcall FormatText(const IMsiString& riTextString);
	iecEnum         __stdcall EvaluateCondition(const ICHAR* szCondition);
	Bool            __stdcall SetProperty(const IMsiString& riPropertyString,const IMsiString& rData);
	Bool            __stdcall SetPropertyInt(const IMsiString& riPropertyString, int iData);
	const IMsiString&     __stdcall GetProperty(const IMsiString& riPropertyString);
	const IMsiString&     __stdcall GetPropertyFromSz(const ICHAR* szPropertyString);
	const IMsiString&     __stdcall GetEnvironmentVariable(const ICHAR* szEnvVar);
	int             __stdcall GetPropertyInt(const IMsiString& riPropertyString);
	int             __stdcall GetPropertyLen(const IMsiString& riPropertyString);
	Bool            __stdcall ResolveFolderProperty(const IMsiString& riPropertyString);
	iesEnum         __stdcall FatalError(IMsiRecord& riRecord);
	iesEnum         __stdcall RegisterProduct();
	iesEnum         __stdcall UnregisterProduct();
	iesEnum         __stdcall UnpublishProduct();
	iesEnum         __stdcall RegisterUser(bool fDirect);
	const IMsiString& __stdcall GetProductKey();
	iesEnum         __stdcall CreateProductInfoRec(IMsiRecord*& rpiRec);
	Bool            __stdcall ValidateProductID(bool fForce);
	imsEnum         __stdcall ActionProgress();
	IMsiRecord*     __stdcall ComposeDescriptor(const IMsiString& riFeature, const IMsiString& riComponent,
												 IMsiRecord& riRecord, unsigned int iField);
	iesEnum         __stdcall RunExecutionPhase(const ICHAR* szActionOrSequence,
															  bool fSequence);
	iesEnum         __stdcall RunNestedInstall(const IMsiString& ristrProduct,
															 Bool fProductCode, // else package path
															 const ICHAR* szAction,
															 const IMsiString& ristrCommandLine,
															 iioEnum iioOptions,
															 bool fIgnoreFailure);
	bool              __stdcall SafeSetProperty(const IMsiString& ristrProperty, const IMsiString& rData);
	const IMsiString& __stdcall SafeGetProperty(const IMsiString& ristrProperty);
	iesEnum         __stdcall BeginTransaction();
	iesEnum         __stdcall RunScript(bool fForceIfMergedChild);
	iesEnum         __stdcall EndTransaction(iesEnum iesStatus);
	CMsiFile*       __stdcall GetSharedCMsiFile();
	void            __stdcall ReleaseSharedCMsiFile();
	IMsiRecord*     __stdcall CreateTempActionTable(ttblEnum iTable);
	const IMsiString& __stdcall GetErrorTableString(int iError);
	UINT            __stdcall ShutdownCustomActionServer();
	CMsiCustomActionManager* __stdcall GetCustomActionManager();
	IMsiRecord*     __stdcall GetAssemblyInfo(const IMsiString& rstrComponent, iatAssemblyType& riatAssemblyType,  const IMsiString** ppistrAssemblyName, const IMsiString** ppistrManifestFileKey);

	IMsiRecord*     __stdcall GetFileHashInfo(const IMsiString& ristrFileKey, DWORD dwFileSize,
															MD5Hash& rhHash, bool& fHashInfo);
	IMsiRecord*    __stdcall GetFolderCachePath(const int iFolderId, IMsiPath*& rpiPath);
	int            __stdcall GetDeterminedPackageSourceType();
	bool           __stdcall FSafeForFullUninstall(iremEnum iremUninstallType);



 public: // IMsiDirectoryManager implemented virtual functions
	IMsiRecord*    __stdcall LoadDirectoryTable(const ICHAR* szTableName);
	IMsiTable*     __stdcall GetDirectoryTable();
	void           __stdcall FreeDirectoryTable();
	IMsiRecord*    __stdcall CreateTargetPaths();
	IMsiRecord*    __stdcall CreateSourcePaths();
	IMsiRecord*    __stdcall ResolveSourceSubPaths();

	IMsiRecord*    __stdcall GetTargetPath(const IMsiString& piDest,IMsiPath*& rpiPath);
	IMsiRecord*    __stdcall SetTargetPath(const IMsiString& piDest, const ICHAR* szPath, Bool fWriteCheck);
	IMsiRecord*    __stdcall GetSourcePath(const IMsiString& riDirKey,IMsiPath*& rpiPath); 
	IMsiRecord*    __stdcall GetSourceSubPath(const IMsiString& riDirKey, bool fPrependSourceDirToken,
															const IMsiString*& rpistrSubPath);
	IMsiRecord*    __stdcall GetSourceRootAndType(IMsiPath*& rpiSourceRoot, int& iSourceType);

 public: // IMsiSelectionManager implemented virtual functions
	IMsiRecord*    __stdcall LoadSelectionTables();
	IMsiTable*     __stdcall GetComponentTable();
	IMsiTable*     __stdcall GetFeatureTable();
	IMsiTable*     __stdcall GetVolumeCostTable();
	IMsiRecord*	   __stdcall SetReinstallMode(const IMsiString& riModeString);
	IMsiRecord*    __stdcall ConfigureFeature(const IMsiString& riFeatureString,iisEnum iisActionRequest);
	IMsiRecord*    __stdcall ProcessConditionTable();
	Bool           __stdcall FreeSelectionTables();
	Bool           __stdcall SetFeatureHandle(const IMsiString& riFeature, INT_PTR iHandle);
	IMsiRecord*    __stdcall GetDescendentFeatureCost(const IMsiString& riFeatureString, iisEnum iisAction, int& iCost);
	IMsiRecord*    __stdcall GetFeatureCost(const IMsiString& riFeatureString, iisEnum iisAction, int& iCost);
	IMsiRecord*    __stdcall SetComponentSz(const ICHAR* szComponentString, iisEnum iRequestedSelectState);
	IMsiRecord*    __stdcall SetComponent(const MsiStringId idComponentString, iisEnum iRequestedSelectState);
	IMsiRecord*    __stdcall SetInstallLevel(int iInstallLevel);
	IMsiRecord*    __stdcall SetAllFeaturesLocal();
	IMsiRecord*    __stdcall InitializeComponents();
	IMsiRecord*    __stdcall InitializeDynamicCost(bool fReinitialize);
	IMsiRecord*    __stdcall RegisterCostAdjuster(IMsiCostAdjuster& riCostAdjuster);
	IMsiRecord*    __stdcall RecostDirectory(const IMsiString& riDestString, IMsiPath& riOldPath);
	IMsiRecord*    __stdcall GetFeatureValidStates(MsiStringId idFeatureName,int& iValidStates);
	IMsiRecord*    __stdcall GetFeatureValidStatesSz(const ICHAR *szFeatureName,int& iValidStates);
	Bool           __stdcall DetermineOutOfDiskSpace(Bool* pfOutOfNoRbDiskSpace, Bool* pfUserCancelled);
	IMsiRecord*    __stdcall DetermineEngineCostOODS();
	IMsiRecord*    __stdcall RegisterFeatureCostLinkedComponent(const IMsiString& riFeatureString, const IMsiString& riComponentString);
	IMsiRecord*    __stdcall RegisterCostLinkedComponent(const IMsiString& riComponentString, const IMsiString& riRecostComponentString);
	IMsiRecord*    __stdcall RegisterComponentDirectory(const IMsiString& riComponentString, const IMsiString& riDirectoryString);
	IMsiRecord*    __stdcall RegisterComponentDirectoryId(const MsiStringId idComponentString, const MsiStringId idDirectoryString);
	Bool           __stdcall GetFeatureInfo(const IMsiString& riFeature, const IMsiString*& rpiTitle, const IMsiString*& rpiHelp, int& iAttributes);
	IMsiRecord*    __stdcall GetFeatureStates(const IMsiString& riFeatureString,iisEnum* iisInstalled, iisEnum* iisAction);
	IMsiRecord*    __stdcall GetFeatureStates(const MsiStringId idFeatureString,iisEnum* iisInstalled, iisEnum* iisAction);
	IMsiRecord*    __stdcall GetComponentStates(const IMsiString& riComponentString,iisEnum* iisInstalled, iisEnum* iisAction);
	IMsiRecord*    __stdcall GetAncestryFeatureCost(const IMsiString& riFeatureString, iisEnum iisAction, int& iCost);
	IMsiRecord*    __stdcall GetFeatureConfigurableDirectory(const IMsiString& riFeatureString, const IMsiString*& rpiDirKey);
	IMsiRecord*    __stdcall CostOneComponent(const IMsiString& riComponentString);
	bool           __stdcall IsCostingComplete();
	IMsiRecord*    __stdcall RecostAllComponents(Bool& fCancel);
	virtual void   __stdcall EnableRollback(Bool fEnable);
	IMsiRecord*    __stdcall IsPathWritable(IMsiPath& riPath, Bool& fIsWritable);
	IMsiRecord*    __stdcall CheckFeatureTreeGrayState(const IMsiString& riFeatureString, bool& rfIsGray);
	IMsiTable*     __stdcall GetFeatureComponentsTable();
	bool           __stdcall IsBackgroundCostingEnabled();
	IMsiRecord*    __stdcall SetFeatureAttributes(const IMsiString& ristrFeature, int iAttributes);
	IMsiRecord*    __stdcall EnumComponentCosts(const IMsiString& riComponentName, const iisEnum iisAction, const DWORD dwIndex, IMsiVolume*& rpiVolume, int& iCost, int& iTempCost);
	IMsiRecord*    __stdcall EnumEngineCostsPerVolume(const DWORD dwIndex, IMsiVolume*& rpiVolume, int& iCost, int& iTempCost);
	bool           __stdcall FChildInstall() { return m_fChildInstall; }
	const IMsiString& __stdcall GetPackageName() { return m_strPackageName.Return(); }

	ieiEnum        __stdcall LoadUpgradeUninstallMessageHeaders(IMsiDatabase* piDatabase, bool fUninstallHeaders);
	iitEnum        __stdcall GetInstallType();
	IMsiRecord*    __stdcall GetAssemblyNameSz(const IMsiString& rstrComponent, iatAssemblyType iatAT, bool fOldPatchAssembly, const IMsiString*& rpistrAssemblyName);
	IMsiRecord*    __stdcall GetFeatureRuntimeFlags(const MsiStringId idFeatureString,int *piRuntimeFlags);
	bool           __stdcall FPerformAppcompatFix(iacsAppCompatShimFlags iacsFlag);

#ifdef DEBUG
 public: // IMsiDebug
	void           __stdcall SetAssertFlag(Bool fShowAsserts);
	void           __stdcall SetDBCSSimulation(char chLeadByte);
	Bool		   __stdcall WriteLog(const ICHAR* szText);
	void		   __stdcall AssertNoObjects(void);
	void  		   __stdcall SetRefTracking(long iid, Bool fTrack);

#endif //DEBUG
 public:  // constructor/destructor
	void *operator new(size_t cb) { return AllocSpc(cb); }
	void operator delete(void * pv) { FreeSpc(pv); }
	CMsiEngine(IMsiServices& riServices, IMsiServer * piServer, 
				  IMsiStorage* piStorage, IMsiDatabase* piDatabase, CMsiEngine* piParentEngine);
 protected:
  ~CMsiEngine();  // protected to prevent construction on stack
	iesEnum FindAndRunAction(const ICHAR* szAction);
	Bool GetActionText(const ICHAR* szAction,
							 const IMsiString*& rpistrDescription,
							 const IMsiString*& rpistrTemplate);
	IMsiRecord*  FetchSingleRow(const ICHAR* szQuery, const ICHAR* szValue);
	HRESULT      SetLanguage(LANGID iLangId);
	ieiEnum      DoInitialize(const ICHAR* szDatabase,
									  iuiEnum iuiLevel,
									  const ICHAR* szCommandLine,
									  const ICHAR* szProductCode,
									  iioEnum iioOptions);
	void         InitializeUserInfo(const IMsiString& ristrProductKey);
	ieiEnum      InitializeUI(iuiEnum iuiLevel);
	void         InitializeExtendedSystemFeatures();
	ieiEnum      ApplyLanguageTransform(int iLanguage, IMsiDatabase& riDatabase);
	Bool         CreatePropertyTable(IMsiDatabase& riDatabase, const ICHAR* szSourceTable,
												Bool fLoadPersistent);
	void         ClearEngineData();  // called from Initialize and Terminate
	void         ReleaseHandler();   // called from Initialize and Terminate
	void         FormatLog(IMsiRecord& riRecord);
#ifdef OBSOLETE
	Bool         ProcessPropertyFile(const ICHAR* szFile);
#endif // OBSOLETE
	int          ChecksumUserInfo();
	ieiEnum      InitializeTransforms(IMsiDatabase& riDatabase, IMsiStorage* piStorage,
												  const IMsiString& riTransforms,
												  Bool fValidateAll, const IMsiString** ppistrValidTransforms,
												  bool fTransformsFromPatch,
												  bool fProcessingInstanceMst,
												  bool fUseLocalCacheForSecureTransforms,
												  int *pcTranformsProcessed=0,
												  const ICHAR* szSourceDir=0,
												  const ICHAR* szCurrentDirectory=0,
												  const IMsiString** ppistrRecacheTransforms=0,
												  tsEnum *ptsTransformsSecure=0,
												  const IMsiString** ppistrNewTransformsList=0);
	ievtEnum     ValidateTransform(IMsiStorage& riStorage, const ICHAR* szProductKey,
											 const ICHAR* szProductVersion, const ICHAR* szUpgradeCode,
											 int& iTransErrors, bool fCallSAFER, const ICHAR* szFriendlyName, bool fSkipValidation, int* piTransValidation);
	IMsiRecord*  LoadComponentTable();
	IMsiRecord*  LoadFeatureTable();
	IMsiRecord*  ProcessPropertyFeatureRequests(int* iRequestCountParam, Bool fCountOnly);
	IMsiRecord*  ConfigureAllFeatures(iisEnum iisActionRequest);
	IMsiRecord*  ConfigureFile(const IMsiString& riFileString,iisEnum iisActionRequest);
	IMsiRecord*  ConfigureComponent(const IMsiString& riComponentString,iisEnum iisActionRequest);
	IMsiRecord*  ConfigureThisFeature(const IMsiString& riFeatureString,iisEnum iisActionRequest, Bool fThisOnly);
	IMsiRecord*  SetThisFeature(const IMsiString& riFeatureString, iisEnum iisRequestedState, Bool fSettingAll);
	IMsiRecord*  SetFeature(const IMsiString& riFeature, iisEnum iRequestedSelectState);
	iisEnum      GetFeatureComponentsInstalledState(const MsiStringId idFeatureString, bool fIgnoreAddedComponents, int& cComponents);
	IMsiRecord*  DetermineFeatureInstalledStates();
	IMsiRecord*  GetFeatureCompositeInstalledState(const IMsiString& riFeatureString, iisEnum& riisInstalled);
	IMsiRecord*  CalculateFeatureInstalledStates();
	int          GetFeatureRegisteredComponentTotal(const IMsiString& riProductString, const IMsiString& riFeatureString);
	IMsiRecord*  DetermineComponentInstalledStates();
	IMsiRecord*  SetFeatureComponents(const MsiStringId idFeatureString);
	IMsiRecord*  UpdateFeatureActionState(const IMsiString* piFeatureString,Bool fTrackParent, IMsiCursor* piFeatureComponentCursor = 0, IMsiCursor* piFeatureCursor = 0);
	IMsiRecord*  UpdateThisFeatureActionState(IMsiCursor* piCursor);
	IMsiRecord*  UpdateComponentActionStates(const MsiStringId idComponent, iisEnum iRequestedActionState, iisEnum iActionRequestState, bool fComponentEnabled);
	IMsiRecord*  UpdateFeatureComponents(const IMsiString* piFeatureString);
	IMsiRecord*  GetComponentCost(IMsiCursor* piCursor, int& iTotalCost, int& iNoRbTotalCost, int& iARPTotalCost, int& iNoRbARPTotalCost);
	IMsiRecord*  GetComponentActionCost(IMsiCursor* piCursor, iisEnum iisAction, int& iActionCost, int& iNoRbTotalCost, int& iARPActionCost, int& iNoRbARPTotalCost);
	IMsiRecord*  GetTotalSubComponentActionCost(const IMsiString& riComponentString, iisEnum iisAction, int& iTotalCost, int& iNoRbTotalCost);
	IMsiRecord*  AddCostToVolumeTable(IMsiPath* piDestPath, int iCost, int iNoRbCost, int iARPCost, int iNoRbARPCost);
	IMsiRecord*  RecostComponentDirectoryChange(IMsiCursor* piCursor, IMsiPath* piOldPath, bool fCostLinked);
	IMsiRecord*  RecostComponentActionChange(IMsiCursor* piCursor, iisEnum iisOldAction);
	IMsiRecord*  RecostComponent(const MsiStringId idComponentString, bool fCostLinked);
	void         ResetComponentCostMarkers();
	IMsiRecord*  ValidateFeatureSelectState(const IMsiString& riFeatureString,iisEnum iisRequestedState,
												   iisEnum& iisValidState);
	IMsiRecord*  GetFeatureParent(const IMsiString& riFeatureString,const IMsiString*& rpiParentString);
	int          GetComponentColumnIndex(const ICHAR* szColumnName);
	int          GetFeatureColumnIndex(const ICHAR* szColumnName);
	int          GetFeatureComponentsColumnIndex(const ICHAR* szColumnName);
	IMsiRecord*  MarkOrResetFeatureTree(const IMsiString& riFeatureString, Bool fMark);
	IMsiRecord*  RecostLinkedComponents(const IMsiString& riComponentString);
	IMsiRecord*  RecostFeatureLinkedComponents(const IMsiString& riFeatureString);
	ieiEnum      PostInitializeError(IMsiRecord* piError, const IMsiString& ristrErrorInfo, ieiEnum ieiError);
	IMsiRecord*  CreatePathObject(const IMsiString& riPathString,IMsiPath*& rpiPath);
	const IMsiString&  ValidatePIDSegment(const IMsiString& ristrSegment, Bool fUser);
	Bool         PIDCheckSum(const IMsiString& ristrDigits);
	unsigned int ProductVersion();
	IMsiRecord*  GetCurrentSelectState(const IMsiString*& rpistrSelections,
												 const IMsiString*& rpistrProperties,
												 const IMsiString** ppistrLoggedProperties,
												 const IMsiString** ppistrFolders,
												 Bool fReturnPresetSelections);
	Bool CheckInProgressProperties(const IMsiString& ristrInProgressProperties, ippEnum ippType);
	IMsiRecord*  SetDirectoryNonConfigurable(const IMsiString& ristrDirKey);
	ieiEnum      ProcessInProgressInstall();
	ieiEnum      InitializePatch(IMsiDatabase& riDatabase, const IMsiString& ristrPatchPackage,
										  const ICHAR* szProductKey, Bool fApplyExisting, const ICHAR* szCurrentDirectory,
										  iuiEnum iuiLevel);
	ieiEnum      InitializeLogging();
	ieiEnum      ProcessPreselectedAndResumeInfo();
	void         GetSummaryInfoProperties(IMsiSummaryInfo& riSummary, const IMsiString *&rpiTemplate, int &iSourceType);
	imsEnum      LoadHandler();
	ieiEnum      ProcessLanguage(const IMsiString& riAvailableLanguages, const IMsiString& riLanguage, unsigned short& iBaseLangId, Bool fNoUI, bool fIgnoreCurrentMachineLanguage);
	ieiEnum      ProcessPlatform(const IMsiString& riAvailablePlatforms, WORD& wChosenPlatform);
	ieiEnum      LoadMessageHeaders(IMsiDatabase* piDatabase);
	void         ResetEngineCosts();
	IMsiRecord*  EnumEngineCosts(int iIndex, Bool fRecalc, Bool fExact, Bool& fValidEnum, IMsiPath*& rpiPath, int& iCost, int& iNoRbCost, Bool* pfUserCancelled);
	IMsiRecord*  DetermineEngineCost(int* piNetCost, int* piNetNoRbCost);
	bool         AdjustForScriptGuess(int& iVolCost, int &iNoRbVolCost, int iVolSpace, Bool* pfUserCancelled);
	IMsiRecord*  ComponentIDToComponent(const IMsiString& riIDString, const IMsiString*& rpiComponentString);
	static int __stdcall FormatTextCallback(const ICHAR* pch, int cch, CTempBufferRef<ICHAR>&, 
															  Bool& fPropMissing,
															  Bool& fUnresolvedProp,
															  Bool& fSFN,
															  IUnknown* piContext);
	static int __stdcall FormatTextCallbackEx(const ICHAR* pch, int cch, CTempBufferRef<ICHAR>&, 
															  Bool& fPropMissing,
															  Bool& fUnresolvedProp,
															  Bool& fSFN,
															  IUnknown* piContext);
	static int __stdcall FormatTextCallbackCore(const ICHAR* pch, int cch, CTempBufferRef<ICHAR>&, 
															  Bool& fPropMissing,
															  Bool& fUnresolvedProp,
															  Bool& fSFN,
															  IUnknown* piContext,
															  bool fUseRequestedComponentState);
	IMsiRecord* DoStateTransitionForSharedUninstalls(iisEnum& riisAction, const IMsiRecord& riComponentPathRec);
	IMsiRecord* DoStateTransitionForSharedInstalls(const MsiStringId idComponentString, iisEnum& riisAction);
	IMsiRecord* CheckNeverOverwriteForRegKeypath(const MsiStringId idComponentString, iisEnum& riisAction);

	IMsiRecord* CheckLegacyAppsForSharedUninstalls(const IMsiString& riComponentId, iisEnum& riisAction, const IMsiRecord& riComponentPathRec);
	IMsiRecord* GetProductClientState(const ICHAR* szProductCode, const ICHAR* szComponentCode, INSTALLSTATE& riState, const IMsiString*& rpistrLocalPath);
	IMsiRecord* CachePatchInfo(IMsiDatabase& riDatabase, const IMsiString& ristrPatchCode,
										const IMsiString& ristrPackageName, const IMsiString& ristrSourceList,
										const IMsiString& ristrTransformList, const IMsiString& ristrLocalPackagePath,
										const IMsiString& ristrSourcePath, Bool fExisting, Bool fUnregister,
										int iSequence);
	Bool        SetPatchSourceProperties();
	bool		CreateFolderCache(IMsiDatabase& riDatabase);
	IMsiRecord* ResolveSource(const ICHAR* szProductKey=0, bool fPatchKey = false, const ICHAR* szOriginalDatabasePath=0, iuiEnum iuiLevel=(iuiEnum)-1, Bool fMaintenanceMode=(Bool)-1, const IMsiString** ppiSourceDir=0, const IMsiString** ppiSourceDirProduct=0);
	Bool        InTransaction();
	const IMsiString& GetRootParentProductKey();
	IMsiRecord* SetFileComponentStates(IMsiCursor* pComponentCursor, IMsiCursor* pFileCursor, IMsiCursor* pPatchCursor);
	iesEnum     CacheDatabaseIfNecessary();
	const IMsiString& GetProperty(IMsiCursor& riPropCursor, const IMsiString& riProperty);
	iesEnum     RunNestedInstallCustomAction(const IMsiString& ristrProduct,
														  const IMsiString& ristrCommandLine,
														  const ICHAR* szAction,
														  int icaFlags,
														  iioEnum iioOptions);
	void        ReportToEventLog(WORD wEventType, int iEventLogTemplate, IMsiRecord& riRecord);
	INSTALLSTATE GetProductState(const ICHAR* szProductKey, Bool& rfRegistered, Bool& rfAdvertised);
	const IMsiString& GetDefaultDir(const IMsiString& ristrValue, bool fSource);
	IMsiRecord* LockInstallServer(IMsiRecord* piSetInProgressInfo,
											IMsiRecord*& rpiCurrentInProgressInfo);
	Bool        UnlockInstallServer(Bool fSuspend);
	bool        GetInProgressInfo(IMsiRecord*& rpiInProgressInfo);
	iesEnum     RollbackSuspendedInstall(IMsiRecord& riInProgressParams, Bool fPrompt,
													 Bool& fRollbackAttempted, Bool fUserChangedDuringInstall);
	ipitEnum    InProgressInstallType(IMsiRecord& riInProgressInfo);
	IMsiRecord* CreateTargetPathsCore(const IMsiString* piDirKey);
	void        SetProductAlienClientsFlag();
	void        SetCostingComplete(bool fCostingComplete);
	IMsiRecord*	LoadFileTable(int cAddColumns, IMsiTable*& pFileTable);
	IMsiRecord* GetScriptCost(int* piScriptCost, int* piScriptEvents, Bool fExact, Bool* pfUserCancelled);
	bool        WriteExecuteScriptRecord(ixoEnum ixoOpCode, IMsiRecord& riParams);
	bool        WriteSaveScriptRecord(ixoEnum ixoOpCode, IMsiRecord& riParams);
	IMsiRecord* SetFeatureChildren(const IMsiString& riFeatureString, iisEnum iisRequestedState);
	IMsiRecord* SetComponentState(IMsiCursor *piCursor, int colFeature, const MsiStringId idComponent, iisEnum iisComponentInstalled);
	IMsiRecord* CreateComponentFeatureTable(IMsiTable*& rpiCompFeatureTable);
	IMsiRecord* GetFileInstalledLocation(const IMsiString& ristrFile, const IMsiString*& rpistrFilePath, bool fUseRequestedComponentState = false, bool *pfSourceResolutionAttempted=0);	
	IMsiRecord* GetFeatureValidStates(MsiStringId idFeatureName,int& iValidStates, IMsiCursor* piFeatureComponentsCursor, IMsiCursor* piComponentCursor);
	bool        TerminalServerInstallsAreAllowed(bool fAdminUser);
	int         GetTotalCostAcrossVolumes(bool fRollbackCost, bool fARPCost);
	const IMsiString& GetEstimatedInstallSize();
	Bool        m_fAlienClients;
	bool        OpenHydraRegistryWindow(bool fNewTransaction);
	bool        CloseHydraRegistryWindow(bool Commit);
	void        BeginSystemChange();
	void        EndSystemChange(bool fCommitChange, INT64 iSequenceNo);
	void        EndSystemChange(bool fCommitChange, const ICHAR *szSequenceNo);
	bool        IsPropertyHidden(const ICHAR* szProperty, const ICHAR* szHiddenProperties, IMsiTable* rpiControlTable, IMsiDatabase&, bool* pfError); // if called w/ szHiddenProperties set to NULL, it will get it from the database
	void        LogCommandLine(const ICHAR* szCommandLine,  IMsiDatabase&);
	bool        IgnoreMachineState();
	bool        IgnoreReboot();
	bool        ApplyAppCompatTransforms(IMsiDatabase& riDatabase,
													 const IMsiString& ristrProductCode,
													 const IMsiString& ristrPackageCode,
													 iacpAppCompatTransformApplyPoint iacpApplyPoint,
													 iacsAppCompatShimFlags& iacsFlags,
													 bool fQuiet,
													 bool fProductCodeChanged,
													 bool& fDontInstallPackage);
	IMsiRecord* CreateNewPatchTableSchema(IMsiDatabase& riDatabase);
	void		AddFileToCleanupList(const ICHAR* szFileToCleanup);


 protected: //  state data
	int           m_iRefCnt;
	LANGID        m_iLangId;
	int           m_fMode;
	Bool          m_fLogAction; // logging for current action - false when LOGACTION set but not for action
	MsiString     m_istrLogActions;
	iuiEnum       m_iuiLevel;   // UI level, initialized to 0 == iuiFull
	ixmEnum       m_ixmExecuteMode;  // execution mode, initialized to 0 == ixmScript
	Bool          m_fInitialized;
	Bool          m_fRegistered;  // registered with config mgr, output to maint.db.
	Bool          m_fAdvertised;  // the product had been previously advertised
	Bool          m_fInParentTransaction; // nested install inside main engine's transaction
	Bool          m_fMergingScriptWithParent; // in parent transaction and merging script operations
	Bool          m_fCustomActionTable; // custom action table present in database
	Bool          m_fServerLocked;
	Bool          m_fJustGotBackFromServer;
	CScriptGenerate* m_pExecuteScript;
	CScriptGenerate* m_pSaveScript;
	Bool          m_fConfigDatabaseOpen;
	IMsiServices& m_riServices;
	IMsiServer*   m_piServer;
	IMsiConfigurationManager* m_piConfigManager;
	IMsiDatabase* m_piDatabase;
	IMsiStorage*  m_piExternalStorage;
	IMsiDatabase* m_piExternalDatabase;
	CMsiEngine*   m_piParentEngine;
	IMsiCursor*   m_piPropertyCursor;
	IMsiCursor*   m_piActionTextCursor;
	const IMsiString*   m_rgpiMessageHeader[cCachedHeaders];
	const IMsiString*   m_piActionDataFormat;
	const IMsiString*   m_piActionDataLogFormat;
	bool          m_fProgressByData;
	Bool          m_fSummaryInfo;
	const IMsiString*   m_pistrSummaryComments;
	const IMsiString*   m_pistrSummaryKeywords;
	const IMsiString*   m_pistrSummaryTitle;
	const IMsiString*   m_pistrSummaryProduct;
	const IMsiString*   m_pistrSummaryPackageCode;
	MsiDate       m_idSummaryCreateDateTime;
	MsiDate       m_idSummaryInstallDateTime;
	MsiDate       m_idSummaryModifyDateTime;
	int           m_iCodePage;
	const IMsiString*   m_piProductKey;
	const IMsiString*   m_pistrPlatform;
	const IMsiString*   m_pistrExecuteScript; // script executed by server
	const IMsiString*   m_pistrSaveScript;  // script containing all operations for this install
	const IMsiString*   m_piErrorInfo;  // used only to return strings from DoInitialize to Initialize
	int           m_iDatabaseVersion;
	PMsiRecord    m_pCachedActionStart; // held for use by ExecuteRecord and Message
	PMsiRecord    m_pActionStartLogRec; // record for imsgActionStarted and imsgActionEnded - for log use only
	scmEnum       m_scmScriptMode;  // script mode: write, run, idle
	issEnum       m_issSegment;     // current sequence window
	Bool          m_fInExecuteRecord; // true when making recursive call to ExecuteRecord
	Bool          m_fDispatchedActionStart; // true when Message dispatched ActionStart for current action
	Bool          m_fExecutedActionStart; // true when ExecuteRecord executed ixoActionStart
	int           m_cSequenceLevels; // count of levels of recursion to Sequence, used to determine outermost call
	int           m_cExecutionPhaseSequenceLevel; // sequence level count when execution phase was begun
	Bool          m_fDisabledRollbackInScript; // disabled rollback in the middle of script generation
	MsiString     m_strPackagePath; // path to package we are running from
	MsiString     m_strPackageName;
	int           m_iProgressTotal;
	PMsiRecord    m_pActionProgressRec;
	PMsiRecord    m_pScriptProgressRec;
	bool          m_fBeingUpgraded;
	bool          m_fChildInstall;
	bool          m_fEndDialog;
	bool          m_fRunScriptElevated;
	iioEnum       m_iioOptions;
	bool          m_fSourceResolutionAttempted;
	int           m_iSourceType;   // source type suminfo property from SOURCE package - set by GetSourceType
	int           m_iCachedPackageSourceType;  // source type suminfo property from CACHED package - set in Engine::Initialize
	WORD          m_wPackagePlatform; // platform chosen by ProcessPlatform()
	bool          m_fRestrictedEngine; // whether or not the engine is restricted to only running SAFE actions
	bool          m_fRemapHKCUInCAServers; // whether or not to remap HKCU in the custom action servers
	iacsAppCompatShimFlags m_iacsShimFlags;
	bool          m_fNewInstance; // whether this is a new instance install or not

	// cached patch information
	PMsiTable     m_pPatchCacheTable;
	PMsiCursor    m_pPatchCacheCursor;
	int           m_colPatchCachePatchId;
	int           m_colPatchCachePackageName;
	int           m_colPatchCacheSourceList;
	int           m_colPatchCacheTransformList;
	int           m_colPatchCacheTempCopy;
	int           m_colPatchCacheSourcePath;
	int           m_colPatchCacheExisting;
	int           m_colPatchCacheUnregister;
	int           m_colPatchCacheSequence;
	PatchTransformState m_ptsState;

	// custom action information
	CRITICAL_SECTION         m_csCreateProxy;
	CMsiCustomActionManager* m_pCustomActionManager;
	
	// assembly data
	bool m_fAssemblyTableExists;
	PMsiView m_pViewFusion;
	PMsiView m_pViewFusionNameName;
	PMsiView m_pViewFusionName;
	PMsiView m_pViewOldPatchFusionNameName;
	PMsiView m_pViewOldPatchFusionName;

	// IMsiSelectionManager data
	IMsiTable*  m_piFeatureTable;
	IMsiCursor* m_piFeatureCursor;
	IMsiTable*  m_piFeatureComponentsTable;
	IMsiCursor* m_piFeatureComponentsCursor;
	IMsiTable*  m_piComponentTable;
	IMsiCursor* m_piComponentCursor;
	IMsiTable*  m_piCostAdjusterTable;
	int         m_colCostAdjuster;
	IMsiTable*  m_piVolumeCostTable;
	IMsiTable*  m_piCostLinkTable;
	IMsiTable*  m_piFeatureCostLinkTable;
	int         m_colVolumeObject;
	int         m_colVolumeCost;
	int         m_colNoRbVolumeCost;
	int         m_colVolumeARPCost;
	int         m_colNoRbVolumeARPCost;
	int         m_colCostLinkComponent;
	int         m_colCostLinkRecostComponent;
	int         m_colFeatureCostLinkFeature;
	int         m_colFeatureCostLinkComponent;
	bool        m_fCostingComplete;
	bool        m_fSelManInitComplete;
	Bool        m_fExclusiveComponentCost;
	Bool        m_fForceRequestedState;
	int         m_colFeatureKey;
	int         m_colFeatureParent;
	int         m_colFeatureLevel;
	int         m_colFeatureAuthoredLevel;
	int         m_colFeatureHandle;
	int         m_colFeatureSelect;
	int         m_colFeatureAction;
	int         m_colFeatureActionRequested;
	int         m_colFeatureInstalled;
	int         m_colFeatureAttributes;
	int         m_colFeatureAuthoredAttributes;
	int         m_colFeatureComponentsFeature;
	int         m_colFeatureComponentsComponent;
	int         m_colFeatureComponentsRuntimeFlags;
	int         m_colFeatureRuntimeFlags;
	int         m_colFeatureTitle;
	int         m_colFeatureConfigurableDir;
	int         m_colFeatureDescription;
	int         m_colFeatureDefaultSelect;
	int         m_colFeatureDisplay;
	int         m_colComponentKey;
	int         m_colComponentParent;
	int         m_colComponentDir;
	int         m_colComponentAttributes;
	int         m_colComponentInstalled;
	int         m_colComponentCondition;
	int         m_colComponentAction;
	int         m_colComponentActionRequest;
	int         m_colComponentLocalCost;
	int         m_colComponentNoRbLocalCost;
	int         m_colComponentSourceCost;
	int         m_colComponentNoRbSourceCost;
	int         m_colComponentRemoveCost;
	int         m_colComponentNoRbRemoveCost;
	int         m_colComponentARPLocalCost;
	int         m_colComponentNoRbARPLocalCost;
	int         m_colComponentRuntimeFlags;
	int			m_colComponentID;
	int			m_colComponentKeyPath;
	int			m_colComponentForceLocalFiles;
	int			m_colComponentLegacyFileExisted;
	int         m_colComponentTrueInstallState;
	int         m_fForegroundCostingInProgress;

	// IMsiDirectoryManager data
	bool        m_fDirectoryManagerInitialized;
	IMsiTable*  m_piDirTable;
	int         m_colDirKey;
	int         m_colDirParent;
	int         m_colDirSubPath;
	int         m_colDirTarget;
	int         m_colDirSource;
	int         m_colDirNonConfigurable;
	int         m_colDirPreconfigured;
	int         m_colDirLongSourceSubPath;
	int         m_colDirShortSourceSubPath;
	PMsiCursor  m_pCostingCursor;
	bool        m_fReinitializeComponentCost;
	bool        m_fSourceResolved;
	bool        m_fSourcePathsCreated;
	bool        m_fSourceSubPathsResolved;
	PMsiCursor  m_pTempCostsCursor;
	int         m_colTempCostsVolume;
	int         m_colTempCostsTempCost;

	// shell and folder caching
	PMsiTable  m_pFolderCacheTable;
	PMsiCursor m_pFolderCacheCursor;
	int        m_colFolderCacheFolderId;
	int        m_colFolderCacheFolder;
	int        m_colFolderCacheFolderPath;

	// transform and patch temp copy cleanup list
	MsiString  m_strTempFileCopyCleanupList;

	// File table information
	int			m_mpeftCol[ieftMax];

	// Patch table information
	int			m_colPatchKey;
	int			m_colPatchAttributes;

	// MsiFileHash table information
	bool        m_fLookedForFileHashTable;
	PMsiCursor  m_pFileHashCursor;
	int         m_colFileHashKey;
	int         m_colFileHashOptions;
	int         m_colFileHashPart1;
	int         m_colFileHashPart2;
	int         m_colFileHashPart3;
	int         m_colFileHashPart4;

	// AppCompat information for CA Shims
	bool        m_fCAShimsEnabled;
	GUID        m_guidAppCompatDB;
	GUID        m_guidAppCompatID;

	// Internal Engine costing
	int			m_iDatabaseCost;
	int         m_iScriptCost;
	int         m_iScriptCostGuess;
	int         m_iRollbackScriptCost;
	int         m_iRollbackScriptCostGuess;
	int         m_iPatchPackagesCost;

	INT64       m_i64PCHEalthSequenceNo;   //  Windows Millenium System Restore sequence number

	bool		m_fResultEventLogged;
	
	CMsiFile*	m_pcmsiFile;
	int         m_fcmsiFileInUse;

	IMsiTable*  m_piRegistryActionTable;
	IMsiTable*  m_piFileActionTable;
	int         m_iScriptEvents;
	DWORD       m_lTickNextProgress;
	friend const IMsiString& FormatTextEx(const IMsiString& riTextString, IMsiEngine& riEngine, bool fUseRequestedComponentState);
	friend const IMsiString& FormatTextSFN(const IMsiString& riTextString, IMsiEngine& riEngine, int rgiSFNPos[][2], int& riSFNPos, bool fUseRequestedComponentState);
    friend class CMsiServerConnMgr;
};

/*
    Class for managing the connection to the server. This class was introduced
    because MSI now fails if CreateMsiServer cannot connect to the service. Due
    to this change, certain APIs like MsiOpenPackage which don't really need the
    service and used to fall back on the in-proc processing would have failed.

    Using this class, we can delay the connection to the service until it is
    absolutely necessary. Thus, we can prevent APIs MsiOpenPackage from failing
    right off the bat. This ensures backward compatibility. Also, callers of
    this API may not necessarily need to connect to the service and we do not
    want them to fail unnecessarily. Note: One of the main reasons for
    failure to connect to the service is when the MsiOpenPackage API is called
    from threads that have not done a CoInit. This is not very uncommon, so
    we need to make sure that we do not fail there.

    The objects of this class cleanup after themselves. i.e., if they create the
    connection to the service, then upon destruction, the connection is removed.
*/
class CMsiServerConnMgr
{
public:
    // Construction and destruction
    CMsiServerConnMgr (CMsiEngine* pEngine);
    ~CMsiServerConnMgr();

private:
    // Data members
    BOOL                        m_fOleInitialized;
    BOOL                        m_fCreatedConnection;
    BOOL                        m_fObtainedConfigManager;
    IMsiServer**                m_ppServer;
    IMsiConfigurationManager**  m_ppConfigManager;
};

extern CActionThreadData* g_pActionThreadHead;  // linked list of custom action threads
void InsertInCustomActionList(CActionThreadData* pData);
void RemoveFromCustomActionList(CActionThreadData* pData);
bool FIsCustomActionThread(DWORD dwThreadId);
bool FFileIsCompressed(int iSourceType, int iFileAttributes);
bool FSourceIsLFN(int iSourceType, IMsiPath& riPath);

// fn to convert multisz to wide
#ifndef UNICODE
void ConvertMultiSzToWideChar(const IMsiString& ristrFileNames, CTempBufferRef<WCHAR>& rgch);
#endif


class CMsiFileBase
{
public:

	// Enums for accessing file record fields
	enum ifqEnum
	{
		ifqFileName = 1,
		ifqVersion,
		ifqState,
		ifqAttributes,
		ifqTempAttributes,
		ifqFileKey,
		ifqFileSize,
		ifqLanguage,
		ifqSequence,
		ifqDirectory,
		ifqInstalled,
		ifqAction,
		ifqComponent,
		ifqForceLocalFiles,
		ifqComponentId,
		ifqNextEnum
	};
public:
	CMsiFileBase::CMsiFileBase(IMsiEngine& riEngine);
	virtual ~CMsiFileBase();
	IMsiRecord* GetFileRecord( void );
	IMsiRecord* GetTargetPath(IMsiPath*& rpiDestPath);
	IMsiRecord* GetExtractedTargetFileName(IMsiPath& riPath,const IMsiString*& rpistrFileName);
protected:
	IMsiEngine& m_riEngine;
	IMsiServices& m_riServices;
	PMsiRecord  m_pFileRec;
};

class CMsiFile : public CMsiFileBase
/*------------------------------------------
Simple internal class for managing queries
into the File Table
-------------------------------------------*/
{

public:
	CMsiFile::CMsiFile(IMsiEngine& riEngine);
	virtual ~CMsiFile();
	IMsiRecord* FetchFile(const IMsiString& riFileKeyString);
private:
	IMsiRecord* ExecuteView(const IMsiString& riFileKeyString);
protected:
	PMsiView    m_pFileView;
};

//
// Resets the shared CMsiFile cursor when we are done with it
//
class PMsiFile 
{
	public:
		inline PMsiFile(IMsiEngine& riEngine, CMsiFile*& pcmsiFileRet) :
			m_riEngine(riEngine)
		{
			pcmsiFileRet = m_riEngine.GetSharedCMsiFile();
		}
		inline ~PMsiFile()
		{
			m_riEngine.ReleaseSharedCMsiFile();
		}
	public:
		IMsiEngine&  m_riEngine;
};


#define GetSharedEngineCMsiFile(var, engine)		CMsiFile* var; PMsiFile CSharedMsiFile(engine, var)

// function to determine whether a file is to be installed to the system
IMsiRecord* GetFileInstallState(IMsiEngine& riEngine,IMsiRecord& riFileRec,
										  IMsiRecord* piCompanionFileRec,
									   unsigned int* puiExistingClusteredSize, Bool* pfInUse,
									   ifsEnum* pifsState, bool fIgnoreCompanionParentAction, bool fIncludeHashCheck, int *pfVersioning);


class CMsiFileInstall : public CMsiFileBase
{
public:
	CMsiFileInstall::CMsiFileInstall(IMsiEngine& riEngine);
	virtual ~CMsiFileInstall();
	IMsiRecord* TotalBytesToCopy(unsigned int& uiTotalBytesToCopy);
	IMsiRecord* FetchFile();
private:
	IMsiRecord* Initialize();
	IMsiView*		m_piView;
	bool    m_fInitialized;
};

class CMsiFileRemove
{
public:
	enum ifqrEnum
	{
		ifqrFileName = 1,
		ifqrDirectory,
		ifqrComponentId,
		ifqrNextEnum,
	};
	
	CMsiFileRemove::CMsiFileRemove(IMsiEngine& riEngine);
	virtual ~CMsiFileRemove();
	IMsiRecord* TotalFilesToDelete(unsigned int& uiTotalFileCount);
	IMsiRecord* FetchFile(IMsiRecord*&);
	IMsiRecord* GetExtractedTargetFileName(IMsiPath& riPath,const IMsiString*& rpistrFileName);
private:
	IMsiRecord* Initialize();
	bool    m_fInitialized;
	bool    m_fEmpty;
	int		m_colFileName;
	int		m_colFileActionDir;
	int     m_colFileKey;
	int     m_colFileActKey;
	int		m_colFileActAction;
	int 	m_colFileActInstalled;
	int 	m_colFileActComponentId;
	IMsiCursor*		m_piCursor;
	IMsiCursor*		m_piCursorFile;
	IMsiEngine& m_riEngine;
	IMsiServices& m_riServices;
	PMsiRecord  m_pFileRec;
};

struct TTBD		// Temp TaBle Definition
{
	int icd;
	const ICHAR *szColName;
};

UINT __stdcall CheckAllHandlesClosed(bool fClose, DWORD dwThreadId);

// flags for Darwin Descriptor optimization
const int ofSingleComponent = 0x1;
const int ofSingleFeature   = 0x2;

void SetNoPowerdown();
void ClearNoPowerdown();

// additional define for registry root types to do HKLM or HKCU based on ALLUSERS
const int rrkUserOrMachineRoot = -1;

// special format texts used by WriteRegistryValues for using the special callback fn AND SFN handling
const IMsiString& FormatTextEx(const IMsiString& riTextString, IMsiEngine& riEngine, bool fUseRequestedComponentState);
const IMsiString& FormatTextSFN(const IMsiString& riTextString, IMsiEngine& riEngine, int rgiSFNPos[][2], int& riSFNPos, bool fUseRequestedComponentState);

typedef iesEnum (__stdcall *PCustomActionEntry)(MSIHANDLE);
DWORD CallCustomDllEntrypoint(PCustomActionEntry pfEntry, bool fDebugBreak, MSIHANDLE hInstall, const ICHAR* szAction);

// GUID that represents the product key of the system
const ICHAR szSystemProductKey[] = TEXT("{00000000-0000-0000-0000-000000000000}");

enum cpConvertType 
{
	cpToLong  = 0,
	cpToShort = 1,
};

Bool ConvertPathName(const ICHAR* pszPathFormatIn, CTempBufferRef<ICHAR>& rgchPathFormatOut, cpConvertType cpTo);
bool DetermineLongFileNameOnly(const ICHAR* pszPathFormatIn, CTempBufferRef<ICHAR>& rgchFileNameOut);

//____________________________________________________________________________
//
// CMsiRemoteAPI - Stub for remoted MSI API
//____________________________________________________________________________

class CMsiRemoteAPI : public IMsiRemoteAPI
{
 public:
	HRESULT         __stdcall QueryInterface(const IID& riid, void** ppvObj);
	unsigned long   __stdcall AddRef();
	unsigned long   __stdcall Release();
	HRESULT         __stdcall GetProperty(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ICHAR* szName, ICHAR* szValue, unsigned long cchValue, unsigned long* pcchValueRes);
	HRESULT         __stdcall CreateRecord(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned int cParams,unsigned long* pHandle);
	HRESULT         __stdcall CloseAllHandles(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie);
	HRESULT         __stdcall CloseHandle(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hAny);
	HRESULT         __stdcall DatabaseOpenView(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hDatabase, const ichar* szQuery,unsigned long* phView);
	HRESULT         __stdcall ViewGetError(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hView, ichar* szColumnNameBuffer, unsigned long cchBuf, unsigned long* pcchBufRes, int *pMsidbError);
	HRESULT         __stdcall ViewExecute(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hView, unsigned long hRecord);
	HRESULT         __stdcall ViewFetch(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hView,unsigned long*  phRecord);
	HRESULT         __stdcall ViewModify(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hView, long eUpdateMode, unsigned long hRecord);
	HRESULT         __stdcall ViewClose(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hView);
	HRESULT         __stdcall OpenDatabase(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, const ichar* szDatabasePath, const ichar* szPersist,unsigned long *phDatabase);
	HRESULT         __stdcall DatabaseCommit(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hDatabase);
	HRESULT         __stdcall DatabaseGetPrimaryKeys(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hDatabase, const ichar * szTableName,unsigned long *phRecord);
	HRESULT         __stdcall RecordIsNull(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord, unsigned int iField, boolean *pfIsNull);
	HRESULT         __stdcall RecordDataSize(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord, unsigned int iField,unsigned int* puiSize);
	HRESULT         __stdcall RecordSetInteger(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord, unsigned int iField, int iValue);
	HRESULT         __stdcall RecordSetString(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord,	unsigned int iField, const ichar* szValue);
	HRESULT         __stdcall RecordGetInteger(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord, unsigned int iField, int *piValue);
	HRESULT         __stdcall RecordGetString(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord, unsigned int iField, ichar* szValueBuf, unsigned long cchValueBuf,unsigned long *pcchValueRes);
	HRESULT         __stdcall RecordGetFieldCount(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord,unsigned int* piCount);
	HRESULT         __stdcall RecordSetStream(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord, unsigned int iField, const ichar* szFilePath);
	HRESULT         __stdcall RecordReadStream(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord, unsigned int iField, boolean fBufferIsNull, char *szDataBuf,unsigned long *pcbDataBuf);
	HRESULT         __stdcall RecordClearData(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hRecord);
	HRESULT         __stdcall GetSummaryInformation(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hDatabase, const ichar*  szDatabasePath, unsigned int    uiUpdateCount, unsigned long *phSummaryInfo);
	HRESULT         __stdcall SummaryInfoGetPropertyCount(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hSummaryInfo,	unsigned int *puiPropertyCount);
	HRESULT         __stdcall SummaryInfoSetProperty(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hSummaryInfo,unsigned intuiProperty, unsigned intuiDataType, int iValue, FILETIME *pftValue, const ichar* szValue); 
	HRESULT         __stdcall SummaryInfoGetProperty(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hSummaryInfo,unsigned intuiProperty,unsigned int *puiDataType, int *piValue, FILETIME *pftValue, ichar* szValueBuf, unsigned long cchValueBuf, unsigned long *pcchValueBufRes);
	HRESULT         __stdcall SummaryInfoPersist(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hSummaryInfo);
	HRESULT         __stdcall GetActiveDatabase(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall,unsigned long* phDatabase);
	HRESULT         __stdcall SetProperty(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szName, const ichar* szValue);
	HRESULT         __stdcall GetLanguage(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall,unsigned short* pLangId);
	HRESULT         __stdcall GetMode(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, long eRunMode, boolean* pfSet); 
	HRESULT         __stdcall SetMode(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, long eRunMode, boolean fState);
	HRESULT         __stdcall FormatRecord(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, unsigned long hRecord, ichar* szResultBuf, unsigned long cchResultBuf, unsigned long *pcchResultBufRes);
	HRESULT         __stdcall DoAction(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szAction);    
	HRESULT         __stdcall Sequence(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szTable, int iSequenceMode);   
	HRESULT         __stdcall ProcessMessage(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, long eMessageType, unsigned long hRecord, int* piRes);
	HRESULT         __stdcall EvaluateCondition(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szCondition, int *piCondition);
	HRESULT         __stdcall GetFeatureState(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szFeature, long *piInstalled, long *piAction);
	HRESULT         __stdcall SetFeatureState(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szFeature, long iState);
	HRESULT         __stdcall GetComponentState(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szComponent, long *piInstalled, long *piAction);
	HRESULT         __stdcall SetComponentState(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar*     szComponent, long iState);
	HRESULT         __stdcall GetFeatureCost(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szFeature, int iCostTree, long iState, int *piCost);
	HRESULT         __stdcall SetInstallLevel(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, int iInstallLevel);
	HRESULT         __stdcall GetFeatureValidStates(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szFeature,unsigned long *dwInstallStates);
	HRESULT         __stdcall DatabaseIsTablePersistent(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hDatabase, const ichar* szTableName, int *piCondition);
	HRESULT         __stdcall ViewGetColumnInfo(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hView, long eColumnInfo,unsigned long *phRecord);
	HRESULT         __stdcall GetLastErrorRecord(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long *phRecord);
	HRESULT         __stdcall GetSourcePath(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szFolder, ichar* szPathBuf, unsigned long cchPathBuf, unsigned long *pcchPathBufRes);
	HRESULT         __stdcall GetTargetPath(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szFolder, ichar* szPathBuf, unsigned long cchPathBuf, unsigned long *pcchPathBufRes); 
	HRESULT         __stdcall SetTargetPath(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szFolder, const ichar* szFolderPath);
	HRESULT         __stdcall VerifyDiskSpace(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall);
	HRESULT         __stdcall SetFeatureAttributes(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie,  unsigned long hInstall,  const ichar* szFeature,  long iAttributes);
	HRESULT         __stdcall EnumComponentCosts(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, unsigned long hInstall, const ichar* szComponent, unsigned long iIndex, long iState, ichar* szDrive, unsigned long cchDrive, unsigned long* pcchDriveSize, int *piCost, int *piTempCost);
	HRESULT         __stdcall GetInstallerObject(const int icacContext, const unsigned long dwThreadId, const unsigned char* rgchCookie, const int cbCookie, IDispatch** piDispatch);

 public:  // constructor
 	void *operator new(size_t cb) { return AllocSpc(cb); }
	void operator delete(void * pv) { FreeSpc(pv); }
	CMsiRemoteAPI();
	bool SetCookie(const int icacContext, const unsigned char *rgchCookie, const int cbCookie);
	HRESULT BeginAction(const int icacContext);
	HRESULT EndAction(const int icacContext);

 protected:
	~CMsiRemoteAPI();  // protected to prevent creation on stack
 private:
 	long           m_rgiActionCount[icacNext];
 	unsigned char *m_rgchCookie[icacNext];
 	int            m_cbCookie;
 	DWORD          m_dwRemoteAPIThread;
	bool           m_fPerformSystemUserTranslation;
 	bool ValidateCookie(const int icacContext, const unsigned char *rgchCookie, const int cbCookie) const;
 	
	long  m_iRefCnt;
};

#define EVENTLOG_ERROR_OFFSET      10000

enum ieSwapAttrib
{
	ieSwapInvalid      = -1,
	ieSwapAlways       = 0,
	ieSwapForSharedDll = 1,
	ieSwapAttribFirst  = ieSwapForSharedDll,
	ieSwapAttribLast   = ieSwapForSharedDll,
//	ieNextThing        = 2,...         // better make the next one(s) multiples of 2
//	ieSwapAttribLast   = ieNextThing,  // ieSwapAttribLast needs to be updated too
};

struct strFolderPairs
{
	MsiString     str64bit;
	MsiString     str32bit;
	int           iSwapAttrib;
	strFolderPairs() {}
	strFolderPairs(const ICHAR* sz64bit, const ICHAR* sz32bit) : str64bit(sz64bit), str32bit(sz32bit), iSwapAttrib(ieSwapAlways)
		{Assert(str64bit.TextSize() <= MAX_PATH); Assert(str32bit.TextSize() <= MAX_PATH);}
	strFolderPairs(const ICHAR* sz64bit, const ICHAR* sz32bit, int iAttrib)
	{
		*this = strFolderPairs(sz64bit, sz32bit);
		if ( IsValidSwapAttrib(iAttrib) )
			iSwapAttrib = iAttrib;
		else
		{
			Assert(0);
			iSwapAttrib = ieSwapAlways;
		}
	}
	static bool IsValidSwapAttrib(int iArg)
	{
		if ( iArg == ieSwapAlways )
			return true;
		int iTest = ieSwapAttribFirst;
		do
		{
			if ( (iArg & iTest) == iTest )
				iArg &= ~iTest;
			iTest <<= 1;
		} while ( iTest <= ieSwapAttribLast );

		return iArg ? false : true;
	}
};

enum ieFolderSwapType
{
	ie32to64 = 0,
	ie64to32,
};

enum ieIsDualFolder
{
	ieisNotInitialized = 0,
	ieisNotWin64DualFolder,
	ieisWin64DualFolder,
};

enum ieSwappedFolder
{
	iesrError = 0,
	iesrNotInitialized,
	iesrNotSwapped,
	iesrSwapped,
};

class CWin64DualFolders
{
private:
	bool m_f32bitPackage;
	strFolderPairs* m_prgFolderPairs;
	bool CopyArray(const strFolderPairs* pArg);
	ieIsDualFolder IsWin64DualFolder(ieFolderSwapType iConvertFrom,
												const ICHAR* szFolder,
												int& iSwapAttrib,
												int* iCharsToSubstite = 0,
												ICHAR* szToSubstituteWith = 0);

public:
	CWin64DualFolders() : m_f32bitPackage(false), m_prgFolderPairs(NULL) {}
	~CWin64DualFolders() { Clear(); }
	CWin64DualFolders& operator = (const CWin64DualFolders& Arg);
	CWin64DualFolders(bool fArg, strFolderPairs* pArg) : m_f32bitPackage(fArg), m_prgFolderPairs(NULL)
		{CopyArray(pArg);}
	void Clear();

	bool ShouldCheckFolders() { return m_f32bitPackage; }
	ieSwappedFolder SwapFolder(ieFolderSwapType iConvert,
										const ICHAR* szFolder, ICHAR* szSubstituted,
										int iSwapMask = ieSwapAlways);
};

// global fn to post assembly errors, in addition to posting error, this fn also logs the formatmessage string for the error
IMsiRecord* PostAssemblyError(const ICHAR* szComponentId, HRESULT hResult, const ICHAR* szInterface, const ICHAR* szFunction, const ICHAR* szAssemblyName, iatAssemblyType iatAT);

#endif // ___ENGINE
