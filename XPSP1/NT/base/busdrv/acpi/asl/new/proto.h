/*** proto.h - Local function prototypes
 *
 *  Copyright (c) 1996,1997 Microsoft Corporation
 *  Author:     Michael Tsang (MikeTs)
 *  Created     09/07/96
 *
 *  MODIFICATION HISTORY
 */

#ifndef _PROTO_H
#define _PROTO_H

// asl.c
int LOCAL ReadBinFile(PSZ pszFile, PBYTE *ppb, PDWORD pdwTableSig);
int LOCAL InitNameSpace(VOID);
VOID LOCAL PrintLogo(VOID);
int LOCAL PrintHelp(char **ppszArg, PARGTYPE pAT);
VOID LOCAL PrintUsage(VOID);

// parseasl.c
int LOCAL ParseASLFile(PSZ pszFile);
int LOCAL ParseASLTerms(PTOKEN ptoken, int iNestLevel);
BOOL LOCAL ValidateTermClass(DWORD dwTermClass, PCODEOBJ pcParent);
int LOCAL ParseASLTerm(PTOKEN ptoken, int iNestLevel);
int LOCAL ParseFieldList(PTOKEN ptoken);
int LOCAL ParsePackageList(PTOKEN ptoken);
int LOCAL ParseBuffList(PTOKEN ptoken);
int LOCAL ParseDataList(PTOKEN ptoken, int icbDataSize);
int LOCAL ParseArgs(PTOKEN ptoken, PASLTERM pterm, int iNumArgs);
int LOCAL ParseUserTerm(PTOKEN ptoken, BOOL fNonMethodOK);
int LOCAL ParseName(PTOKEN ptoken, BOOL fEncode);
int LOCAL ParseSuperName(PTOKEN ptoken);
int LOCAL MakeIntData(DWORD dwData, PCODEOBJ pc);
int LOCAL GetIntData(PCODEOBJ pc, PDWORD pdwData);
int LOCAL ParseData(PTOKEN ptoken);
int LOCAL ParseInteger(PTOKEN ptoken, char c);
int LOCAL ParseOpcode(PTOKEN ptoken, char c);
int LOCAL ParseKeyword(PTOKEN ptoken, char chExpectType);
int LOCAL ParseString(PTOKEN ptoken);
int LOCAL CreateObject(PTOKEN ptoken, PSZ pszName, char c, PNSOBJ *ppns);
#ifdef __UNASM
int LOCAL CreateScopeObj(PSZ pszName, PNSOBJ *ppns);
#endif
int LOCAL ValidateObject(PTOKEN ptoken, PSZ pszName, char chActType,
                         char chArgType);
int LOCAL ValidateNSChkList(PNSCHK pnschkHead);
int LOCAL QueueNSChk(PTOKEN ptoken, PSZ pszObjName, ULONG dwExpectedType,
                     ULONG dwChkData);

// aslterms.c
int LOCAL DefinitionBlock(PTOKEN ptoken, BOOL fActionFL);
int LOCAL Include(PTOKEN ptoken, BOOL fActionFL);
int LOCAL External(PTOKEN ptoken, BOOL fActionFL);
int LOCAL Method(PTOKEN ptoken, BOOL fActionFL);
int LOCAL Alias(PTOKEN ptoken, BOOL fActionFL);
int LOCAL Name(PTOKEN ptoken, BOOL fActionFL);
int LOCAL Scope(PTOKEN ptoken, BOOL fActionFL);
int LOCAL Field(PTOKEN ptoken, BOOL fActionFL);
int LOCAL IndexField(PTOKEN ptoken, BOOL fActionFL);
int LOCAL BankField(PTOKEN ptoken, BOOL fActionFL);
int LOCAL OpRegion(PTOKEN ptoken, BOOL fActionFL);
int LOCAL EISAID(PTOKEN ptoken, BOOL fActionFL);
int LOCAL Match(PTOKEN ptoken, BOOL fActionFL);
int LOCAL AccessAs(PTOKEN ptoken, BOOL fActionFL);
int LOCAL Else(PTOKEN ptoken, BOOL fActionFL);

// pnpmacro.c
int LOCAL XferCodeToBuff(PBYTE pbBuff, PDWORD pdwcb, PCODEOBJ pcCode);
int LOCAL ResourceTemplate(PTOKEN ptoken, BOOL fActionFL);
int LOCAL AddSmallOffset(PTOKEN ptoken, BOOL fActionFL);
int LOCAL StartDependentFn(PTOKEN ptoken, BOOL fActionFL);
int LOCAL IRQDesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL DMADesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL IODesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL FixedIODesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL VendorDesc(PTOKEN ptoken, DWORD dwMaxSize);
int LOCAL VendorShort(PTOKEN ptoken, BOOL fActionFL);
int LOCAL InsertDescLength(PCODEOBJ pcode, DWORD dwDescLen);
int LOCAL Memory24Desc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL VendorLong(PTOKEN ptoken, BOOL fActionFL);
int LOCAL Memory32Desc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL FixedMemory32Desc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL MemSpaceDesc(PTOKEN ptoken, DWORD dwMinLen, PRESFIELD ResFields);
int LOCAL IOSpaceDesc(PTOKEN ptoken, DWORD dwMinLen, PRESFIELD ResFields);
int LOCAL DWordMemDesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL DWordIODesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL WordIODesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL WordBusNumDesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL InterruptDesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL QWordMemDesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL QWordIODesc(PTOKEN ptoken, BOOL fActionFL);
int LOCAL CreateResFields(PTOKEN ptoken, PNSOBJ pnsParent, PRESFIELD prf);

// acpins.c
int LOCAL GetNameSpaceObj(PSZ pszObjPath, PNSOBJ pnsScope, PPNSOBJ ppns,
                          DWORD dwfNS);
int LOCAL CreateNameSpaceObj(PTOKEN ptoken, PSZ pszName, PNSOBJ pnsScope,
                             PNSOBJ pnsOwner, PPNSOBJ ppns, DWORD dwfNS);
VOID LOCAL DumpNameSpacePaths(PNSOBJ pnsObj, FILE *pfileOut);
PSZ LOCAL GetObjectPath(PNSOBJ pns);
PSZ LOCAL GetObjectTypeName(DWORD dwObjType);

// misc.c
BOOL LOCAL ValidASLNameSeg(PTOKEN ptoken, PSZ pszToken, int icbLen);
BOOL LOCAL ValidASLName(PTOKEN ptoken, PSZ pszToken);
int LOCAL EncodeName(PSZ pszName, PBYTE pbBuff, PDWORD pdwLen);
int LOCAL EncodePktLen(DWORD dwCodeLen, PDWORD pdwPktLen, PINT picbEncoding);
VOID LOCAL EncodeKeywords(PCODEOBJ pArgs, DWORD dwSrcArgs, int iDstArgNum);
int LOCAL DecodeName(PBYTE pb, PSZ pszName, int iLen);
int LOCAL SetDefMissingKW(PCODEOBJ pArg, DWORD dwDefID);
VOID LOCAL SetIntObject(PCODEOBJ pc, DWORD dwData, DWORD dwLen);
VOID LOCAL ComputeChildChkSumLen(PCODEOBJ pcParent, PCODEOBJ pcChild);
VOID LOCAL ComputeArgsChkSumLen(PCODEOBJ pcode);
VOID LOCAL ComputeChkSumLen(PCODEOBJ pcode);
BYTE LOCAL ComputeDataChkSum(PBYTE pb, DWORD dwLen);
int LOCAL ComputeEISAID(PSZ pszID, PDWORD pdwEISAID);
int LOCAL LookupIDIndex(LONG lID, PDWORD pdwTermIndex);
int LOCAL WriteAMLFile(int fhAML, PCODEOBJ pcode, PDWORD pdwOffset);
VOID LOCAL FreeCodeObjs(PCODEOBJ pcodeRoot);

// unasm.c
int LOCAL UnAsmFile(PSZ pszAMLName, FILE *pfileOut);
int LOCAL BuildNameSpace(PSZ pszAMLName, DWORD dwAddr, PBYTE pb);
int LOCAL UnAsmAML(PSZ pszAMLName, DWORD dwAddr, PBYTE pb, FILE *pfileOut);
int LOCAL UnAsmHeader(PSZ pszAMLName, PDESCRIPTION_HEADER pdh, FILE *pfileOut);
VOID LOCAL DumpBytes(PBYTE pb, DWORD dwLen, FILE *pfileOut);
VOID LOCAL DumpCode(PBYTE pbOp, FILE *pfileOut);
VOID LOCAL PrintIndent(int iLevel, FILE *pfileOut);
BYTE LOCAL FindOpClass(BYTE bOp, POPMAP pOpTable);
PASLTERM LOCAL FindOpTerm(DWORD dwOpcode);
PASLTERM LOCAL FindKeywordTerm(char cKWGroup, BYTE bDate);
int LOCAL UnAsmScope(PBYTE *ppbOp, PBYTE pbEnd, FILE *pfileOut);
int LOCAL UnAsmOpcode(PBYTE *ppbOp, FILE *pfileOut);
int LOCAL UnAsmDataObj(PBYTE *ppbOp, FILE *pfileOut);
int LOCAL UnAsmNameObj(PBYTE *ppbOp, FILE *pfileOut, PNSOBJ *ppns, char c);
int LOCAL ParseNameTail(PBYTE *ppbOp, PSZ pszBuff, int iLen);
int LOCAL UnAsmTermObj(PASLTERM pterm, PBYTE *ppbOp, FILE *pfileOut);
int LOCAL UnAsmSuperName(PBYTE *ppbOp, FILE *pfileOut);
int LOCAL UnAsmArgs(PSZ pszUnAsmArgTypes, PSZ pszArgActions, DWORD dwTermData,
                    PBYTE *ppbOp, PNSOBJ *ppns, FILE *pfileOut);
DWORD LOCAL ParsePackageLen(PBYTE *ppbOp, PBYTE *ppbOpNext);
int LOCAL UnAsmDataList(PBYTE *ppbOp, PBYTE pbEnd, FILE *pfileOut);
int LOCAL UnAsmPkgList(PBYTE *ppbOp, PBYTE pbEnd, FILE *pfileOut);
int LOCAL UnAsmFieldList(PBYTE *ppbOp, PBYTE pbEnd, FILE *pfileOut);
int LOCAL UnAsmField(PBYTE *ppbOp, FILE *pfileOut, PDWORD pdwBitPos);

#ifdef __UNASM
// tables.c
BOOL LOCAL IsWinNT(VOID);
#ifndef WINNT
HANDLE LOCAL OpenVxD(VOID);
VOID LOCAL CloseVxD(HANDLE hVxD);
PBYTE LOCAL VxDGetTableBySig(DWORD dwTabSig, PDWORD pdwTableAddr);
PBYTE LOCAL VxDGetTableByAddr(DWORD dwTableAddr, PDWORD pdwTableSig);
#endif
PBYTE LOCAL GetNTTable(DWORD dwTabSig);
PBYTE LOCAL GetTableBySig(DWORD dwTabSig, PDWORD pdwTableAddr);
PBYTE LOCAL GetTableByAddr(DWORD dwTableAddr, PDWORD pdwTableSig);
int LOCAL DumpAllTables(FILE *pfileOut);
int LOCAL DumpTableBySig(FILE *pfileOut, DWORD dwTableSig);
int LOCAL DumpTableByAddr(FILE *pfileOut, DWORD dwTableAddr);
int LOCAL DumpRSDP(FILE *pfileOut, PBYTE pb, DWORD dwAddr);
int LOCAL DumpTable(FILE *pfileOut, PBYTE pb, DWORD dwTableAddr,
                    DWORD dwTableSig);
int LOCAL DumpTableTxt(FILE *pfileOut, PBYTE pb, DWORD dwTableAddr,
                       DWORD dwTableSig);
int LOCAL DumpTableBin(DWORD dwTableSig, DWORD dwAddr, PBYTE pb, DWORD dwLen);
int LOCAL FindTableFmt(DWORD dwTableSig, PFMT *ppfmt, PDWORD pdwFlags);
PSZ LOCAL GetTableSigStr(DWORD dwTableSig);
#endif

// debug.c
VOID CDECL ErrPrintf(char *pszFormat, ...);
#ifdef TRACING
VOID LOCAL OpenTrace(char *pszTraceOut);
VOID LOCAL CloseTrace(VOID);
VOID CDECL EnterProc(int n, char *pszFormat, ...);
VOID CDECL ExitProc(int n, char *pszForamt, ...);
#endif

#endif  //ifndef _PROTO_H
