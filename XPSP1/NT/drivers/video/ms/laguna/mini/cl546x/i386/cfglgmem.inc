;*******************************************************************************
;
; Copyright (c) 1996-1998, Cirrus Logic, Inc.
; All Rights Reserved.
;
; FILE: $Workfile:   CfgLgMem.inc  $
;
; DESCRIPTION:
;
; REVISION HISTORY: $Log:   //uinac/Log/Log/Laguna/NT35/Miniport/CL546x/i386/CfgLgMem.inc  $
; 
;    Rev 1.0   Jan 22 1998 16:26:58   frido
; Initial revision.
;
;****************************************************************************

IFNDEF _CFGLGMEM_INC_
_CFGLGMEM_INC_			EQU		1

;****************************************************************************
;* E Q U A T E S
;****************************************************************************

; eflags bits.
FLAGS_AC_BIT			EQU		00040000h
FLAGS_ID_BIT			EQU		00200000h

; CPUID	info feature flags & processor signature equates.
STEPPING_MASK			EQU		00000000fh
MODEL_MASK			EQU		0000000f0h
FAMILY_MASK			EQU		000000f00h
UP_TYPE_MASK			EQU		000003000h

FAMILY_386			EQU		000000300h
FAMILY_486			EQU		000000400h
FAMILY_PENTIUM			EQU		000000500h
FAMILY_PENTIUM_PRO		EQU		000000600h

FEATURE_FPU			EQU		000000001h
FEATURE_VME			EQU		000000002h
FEATURE_PSE			EQU		000000008h
FEATURE_TSC			EQU		000000010h
FEATURE_MSR			EQU		000000020h
FEATURE_PAE			EQU		000000040h
FEATURE_MCE			EQU		000000080h
FEATURE_CX8			EQU		000000100h
FEATURE_APIC			EQU		000000200h
FEATURE_MTRR			EQU		000001000h
FEATURE_PGE			EQU		000002000h
FEATURE_MCA			EQU		000004000h
FEATURE_CMOV			EQU		000008000h

; MSR MTRR registers, masks, bits, etc.
MSR_MTRRcapsReg			EQU		0FEh
MSR_MTRRphysBase0Reg		EQU		200h
MSR_MTRRphysMask0Reg		EQU		201h
MSR_MTRRphysBase1Reg		EQU		202h
MSR_MTRRphysMask1Reg		EQU		203h
MSR_MTRRphysBase2Reg		EQU		204h
MSR_MTRRphysMask2Reg		EQU		205h
MSR_MTRRphysBase3Reg		EQU		206h
MSR_MTRRphysMask3Reg		EQU		207h
MSR_MTRRphysBase4Reg		EQU		208h
MSR_MTRRphysMask4Reg		EQU		209h
MSR_MTRRphysBase5Reg		EQU		20Ah
MSR_MTRRphysMask5Reg		EQU		20Bh
MSR_MTRRphysBase6Reg		EQU		20Ch
MSR_MTRRphysMask6Reg		EQU		20Dh
MSR_MTRRphysBase7Reg		EQU		20Eh
MSR_MTRRphysMask7Reg		EQU		20Fh
MSR_MTRRfix64K_00000Reg		EQU		250h
MSR_MTRRfix64K_80000Reg		EQU		258h
MSR_MTRRfix64K_A0000Reg		EQU		259h
MSR_MTRRfix64K_C0000Reg		EQU		268h
MSR_MTRRfix64K_C8000Reg		EQU		269h
MSR_MTRRfix64K_D0000Reg		EQU		26Ah
MSR_MTRRfix64K_D8000Reg		EQU		26Bh
MSR_MTRRfix64K_E0000Reg		EQU		26Ch
MSR_MTRRfix64K_E8000Reg		EQU		26Dh
MSR_MTRRfix64K_F0000Reg		EQU		26Eh
MSR_MTRRfix64K_F8000Reg		EQU		26Fh
MSR_MTRRdefTypeReg		EQU		2FFh

MSR_DefMemTypeMask		EQU		07h
MSR_FixedMTRREnable		EQU		0400h
MSR_MTRREnable			EQU		0800h

MTRRcap_WC_BIT			EQU		0400h
MTRRcap_FIX_BIT			EQU		0100h
MTRRcap_VCNT_MASK		EQU		0FFh

MTRRdefType_E_BIT		EQU		0800h
MTRRdefType_FE_BIT		EQU		0400h
MTRRdefType_DEF_TYPE_MASK	EQU		0FFh

MTRRphysBase_PHYS_BASE_MASK_LO	EQU		0FFFFF000h
MTRRphysBase_PHYS_BASE_MASK_HI	EQU		0Fh
MTRRphysBase_TYPE_MASK		EQU		0FFh
MTRRphysMask_PHYS_MASK_MASK_LO	EQU		0FFFFF000h
MTRRphysMask_PHYS_MASK_MASK_HI	EQU		0Fh
MTRRphysMask_V_BIT		EQU		0800h

; Memory types available on P6.
UNCACHEABLE			EQU		0
WRITE_COMBINING			EQU		1
WRITE_THROUGH			EQU		4
WRITE_BACK			EQU		5
WRITE_PROTECTED			EQU		6

; CR0 bits.
CR0_CD_BIT			EQU		40000000h
CR0_NW_BIT			EQU		20000000h

; Error codes.
mVDD_SUCCESS			EQU		0
mVDDERR_INVALID_PARAMETER	EQU		57h
mVDDERR_NOT_SUPPORTED		EQU		32h
mVDDERR_ALREADY_ASSIGNED	EQU		55h
mVDDERR_ACCESS_DENIED		EQU		5h

ENDIF ; _CFGLGMEM_INC_
