//////////////////////////////////////////////////////////////////////
// File - pmac_lib.h
//
// Library for accessing the PMAC card,
// Code was generated by DriverWizard v6.02 - http://www.jungo.com.
// The library accesses the hardware via WinDriver functions.
// 
// Copyright (c) 2003 Jungo Ltd.  http://www.jungo.com
// 
//////////////////////////////////////////////////////////////////////

#ifndef _PMAC_LIB_H_
#define _PMAC_LIB_H_

#include "c:/windriver6/include/windrvr.h"
#include "c:/windriver6/include/windrvr_int_thread.h"
#include "c:/windriver6/include/windrvr_events.h"
#include "c:/windriver6/samples/shared/pci_regs.h"
#include "c:/windriver6/samples/shared/bits.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { PMAC_DEFAULT_VENDOR_ID = 0x1172 };
enum { PMAC_DEFAULT_DEVICE_ID = 0x1 };

enum
{
    PMAC_MODE_BYTE   = 0,
    PMAC_MODE_WORD   = 1,
    PMAC_MODE_DWORD  = 2
};
typedef DWORD PMAC_MODE;

enum
{
    PMAC_AD_BAR0 = AD_PCI_BAR0,
    PMAC_AD_BAR1 = AD_PCI_BAR1,
    PMAC_AD_BAR2 = AD_PCI_BAR2,
    PMAC_AD_BAR3 = AD_PCI_BAR3,
    PMAC_AD_BAR4 = AD_PCI_BAR4,
    PMAC_AD_BAR5 = AD_PCI_BAR5,
    PMAC_AD_EPROM = AD_PCI_BAR_EPROM,
};
typedef DWORD PMAC_ADDR;

// Number of IO and memory ranges
enum { PMAC_ITEMS = AD_PCI_BARS };

typedef struct PMAC_STRUCT *PMAC_HANDLE;

typedef struct
{
    DWORD dwCounter;   // number of interrupts received
    DWORD dwLost;      // number of interrupts yet to be handled
    BOOL fStopped;     // was the interrupt disabled during wait
} PMAC_INT_RESULT;

typedef void (*PMAC_INT_HANDLER)(PMAC_HANDLE hPMAC, PMAC_INT_RESULT *intResult);

// Function: PMAC_Open()
//   Register a PCI card that meets the given criteria to enable working with it.
//   The handle returned from this function is used by most of the functions in this file.
BOOL PMAC_Open (PMAC_HANDLE *phPMAC, DWORD dwVendorID, DWORD dwDeviceID, DWORD nCardNum);

// Function: PMAC_RegisterWinDriver()
//   Enter a license string into WinDriver module.
void PMAC_RegisterWinDriver();

// Function: PMAC_Close()
//   Unregister an opened card.
void PMAC_Close(PMAC_HANDLE hPMAC);

// Function: PMAC_CountCards()
//   Count the number of PCI cards that meets the given criteria.
DWORD PMAC_CountCards (DWORD dwVendorID, DWORD dwDeviceID);

// Function: PMAC_IsAddrSpaceActive()
//   Check if the given address space is configured and active.
BOOL PMAC_IsAddrSpaceActive(PMAC_HANDLE hPMAC, PMAC_ADDR addrSpace);

// Function: PMAC_GetPciSlot()
//   Return the logical location (bus, slot and function) of the PCI card.
void PMAC_GetPciSlot(PMAC_HANDLE hPMAC, WD_PCI_SLOT *pPciSlot);

// General read/write functions

// Function: PMAC_ReadWriteBlock()
//   Read/Write data from/to the card's memory/IO into/from a given buffer.
void PMAC_ReadWriteBlock(PMAC_HANDLE hPMAC, PMAC_ADDR addrSpace,
    DWORD dwOffset, BOOL fRead, PVOID buf, DWORD dwBytes, PMAC_MODE mode);

// Function: PMAC_ReadByte()
//   Read a Byte from the card's memory/IO.
BYTE PMAC_ReadByte (PMAC_HANDLE hPMAC, PMAC_ADDR addrSpace, DWORD dwOffset);

// Function: PMAC_ReadWord()
//   Read a Word from the card's memory/IO.
WORD PMAC_ReadWord (PMAC_HANDLE hPMAC, PMAC_ADDR addrSpace, DWORD dwOffset);

// Function: PMAC_ReadDword()
//   Read a Dword from the card's memory/IO.
DWORD PMAC_ReadDword (PMAC_HANDLE hPMAC, PMAC_ADDR addrSpace, DWORD dwOffset);

// Function: PMAC_WriteByte()
//   Write a Byte to the card's memory/IO.
void PMAC_WriteByte (PMAC_HANDLE hPMAC, PMAC_ADDR addrSpace, DWORD dwOffset, BYTE data);

// Function: PMAC_WriteWord()
//   Write a Word to the card's memory/IO.
void PMAC_WriteWord (PMAC_HANDLE hPMAC, PMAC_ADDR addrSpace, DWORD dwOffset, WORD data);

// Function: PMAC_WriteDword()
//   Write a Dword to the card's memory/IO.
void PMAC_WriteDword (PMAC_HANDLE hPMAC, PMAC_ADDR addrSpace, DWORD dwOffset, DWORD data);


// Interrupts handling functions

// Function: PMAC_IntIsEnabled()
//   Check if the interrupt is enabled.
BOOL PMAC_IntIsEnabled (PMAC_HANDLE hPMAC);

// Function: PMAC_IntEnable()
//   Enable the interrupt.
BOOL PMAC_IntEnable (PMAC_HANDLE hPMAC, PMAC_INT_HANDLER funcIntHandler);

// Function: PMAC_IntDisable()
//   Disable the interrupt.
void PMAC_IntDisable (PMAC_HANDLE hPMAC);

// Access to PCI configuration registers

// Function: PMAC_WritePCIReg()
//   Write a DWORD to the PCI configuration space.
void PMAC_WritePCIReg(PMAC_HANDLE hPMAC, DWORD dwReg, DWORD dwData);

// Function: PMAC_ReadPCIReg()
//   Read a DWORD from the PCI configuration space.
DWORD PMAC_ReadPCIReg(PMAC_HANDLE hPMAC, DWORD dwReg);


// Function: PMAC_RegisterEvent()
//   Register to receive Plug-and-Play and power notification events according to the given criteria.
BOOL PMAC_RegisterEvent(PMAC_HANDLE hPMAC, DWORD dwAction, DWORD dwVendorID, DWORD dwDeviceID,
    WD_PCI_SLOT pciSlot, EVENT_HANDLER funcHandler);

// Function: PMAC_UnregisterEvent()
//   Unregister events notification.
void PMAC_UnregisterEvent(PMAC_HANDLE hPMAC);

// If an error occurs, this string will be set to contain a relevant error message
extern CHAR PMAC_ErrorString[];

// get base address of PMAC: w/VB AControl app changing offset, this is address of azimuth. 
unsigned long *PMAC_GetBasePtr(PMAC_HANDLE hPMAC,PMAC_ADDR addrSpace);

// get physical base address of PMAC: w/VB AControl app changing offset, this is address of azimuth. 
unsigned long *PMAC_GetBasePtrPhysical(PMAC_HANDLE hPMAC,PMAC_ADDR addrSpace);

#ifdef __cplusplus
}
#endif

#endif