/*******************************************************************************
 * FILENAME: SEGGER_RTT_JLinkARM.h
 * 
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    This is the common header file for the socket sub systems of the
 *    JLink ARM.  There are different versions for different OS's.
 *
 * COPYRIGHT:
 *    Copyright 25 May 2025 Paul Hutchinson.
 *
 *    This program is free software: you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the
 *    Free Software Foundation, either version 3 of the License, or (at your
 *    option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program. If not, see https://www.gnu.org/licenses/.
 *
 * HISTORY:
 *    Paul Hutchinson (25 May 2025)
 *       Created
 *
 *******************************************************************************/
#ifndef __SEGGER_RTT_JLINKARM_H_
#define __SEGGER_RTT_JLINKARM_H_

/***  HEADER FILES TO INCLUDE          ***/
#include "../SEGGER_RTT_Common.h"
#include "PluginSDK/Plugin.h"
#include <string.h>
#include <string>

/***  DEFINES                          ***/
#define JLINKARM_HOSTIF_USB (1<<0)
#define JLINKARM_HOSTIF_IP  (1<<1)

#define JLINKARM_TIF_JTAG       0
#define JLINKARM_TIF_SWD        1

/***  MACROS                           ***/

/***  TYPE DEFINITIONS                 ***/
struct JLINK_RTTERMINAL_START
{
    uint32_t ConfigBlockAddress;
    uint32_t Dummy0;
    uint32_t Dummy1;
    uint32_t Dummy2;
};

struct JLINK_RTTERMINAL_STOP
{
    uint8_t InvalidateTargetCB;
    uint8_t Padding[3];
    uint32_t Dummy0;
    uint32_t Dummy1;
    uint32_t Dummy2;
};

#define JLINKARM_RTTERMINAL_CMD_START       0
#define JLINKARM_RTTERMINAL_CMD_STOP        1
#define JLINKARM_RTTERMINAL_CMD_GETNUMBUF   2
#define JLINKARM_RTTERMINAL_CMD_GETDESC     3

struct JLINKARM_DEVICE_SELECT_INFO
{
    uint32_t Size;
    uint32_t CoreIndex;
};

struct FLASH_AREA_INFO
{
    uint32_t Addr;
    uint32_t Size;
};

struct RAM_AREA_INFO
{
    uint32_t Addr;
    uint32_t Size;
};

struct JLINKARM_DEVICE_INFO
{
    int Size;
    char *Name;
    uint32_t CoreId;
    uint32_t FlashAddr;
    uint32_t RAMAddr;
    uint32_t EndianMode;
    uint32_t FlashSize;
    uint32_t RAMSize;
    char *Manu;
    struct FLASH_AREA_INFO FlashArea[32];
    struct RAM_AREA_INFO RamArea[32];
    uint32_t Core;
};

struct JLINKARM_EMU_CONNECT_INFO
{
    uint32_t SerialNumber;
    uint32_t Connection;
    uint32_t USBAddr;
    uint8_t IPAddr[16];
    uint32_t Time;
    uint64_t Time_us;
    uint32_t HWVersion;
    uint8_t MACAddr[6];
    char Product[32];
    char NickName[32];
    char FWString[112];
    char ISDHCPAssignedIP;
    char IsHDCPAssginedIPIsValid;
    char NumIPConnections;
    char NumIPConnectionsIsValid;
    uint8_t Padding[34];
};

/* These are guesses based on info I found on the internet.  Seem to work but
   could have a few things off */
struct JLinkARMAPI
{
    const char *(*Open)(void);
    void (*Close)(void);
    void (*Go)(void);
    char (*Halt)(void);
    void (*ResetNoHalt)(void);
    int (*Connect)(void);
    int (*DEVICE_GetIndex)(const char *DeviceName);
    int (*DEVICE_SelectDialog)(void *hParent,uint32_t Flags,struct JLINKARM_DEVICE_SELECT_INFO *Info);
    int (*TIF_Select)(int Interface);
    void (*SelDevice)(uint16_t DeviceIndex);
    int (*DEVICE_GetInfo)(int DeviceIndex,struct JLINKARM_DEVICE_INFO *DeviceInfo);
    int (*ExecCommand)(const char *In,char *Error,int BufferSize);
    void (*SetSpeed)(uint32_t Speed);
    int (*EMU_SelectIP)(char *IPAddr,int BufferSize,uint16_t *pPort);
    void (*ConfigJTAG)(int IRPre,int DRPre);
    int (*EMU_GetList)(int HostIFs,struct JLINKARM_EMU_CONNECT_INFO *ConnectInfo,int MaxInfos);
    uint32_t (*EMU_GetNumDevices)(void);
    int (*EMU_SelectByUSBSN)(uint32_t SerialNo);
    char (*SelectUSB)(int Port);
    void (*EMU_SelectIPBySN)(uint32_t SerialNo);
    int (*JLINK_RTTERMINAL_Control)(uint32_t Cmd, void *parms);
    int (*JLINK_RTTERMINAL_Read)(uint32_t BufferIndex,char *Buffer,uint32_t BufferSize);
    int (*JLINK_RTTERMINAL_Write)(uint32_t BufferIndex,const char *Buffer,uint32_t BufferSize);
};

/***  CLASS DEFINITIONS                ***/

/***  GLOBAL VARIABLE DEFINITIONS      ***/

/***  EXTERNAL FUNCTION PROTOTYPES     ***/
PG_BOOL SEGGER_RTT_Init(void);
t_DriverIOHandleType *SEGGER_RTT_AllocateHandle(const char *DeviceUniqueID,
        t_IOSystemHandle *IOHandle);
void SEGGER_RTT_FreeHandle(t_DriverIOHandleType *DriverIO);
PG_BOOL SEGGER_RTT_Open(t_DriverIOHandleType *DriverIO,const t_PIKVList *Options);
int SEGGER_RTT_Write(t_DriverIOHandleType *DriverIO,const uint8_t *Data,int Bytes);
int SEGGER_RTT_Read(t_DriverIOHandleType *DriverIO,uint8_t *Data,int MaxBytes);
void SEGGER_RTT_Close(t_DriverIOHandleType *DriverIO);
PG_BOOL SEGGER_RTT_ChangeOptions(t_DriverIOHandleType *DriverIO,
        const t_PIKVList *Options);
void SEGGER_RTT_ConnectionAuxCtrlWidgets_FreeWidgets(t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConAuxCtrls);
t_ConnectionWidgetsType *SEGGER_RTT_ConnectionAuxCtrlWidgets_AllocWidgets(t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle);
const char *SEGGER_RTT_GetLastErrorMessage(t_DriverIOHandleType *DriverIO);

void SEGGER_RTT_AppendFilename2Path(std::string &FullPath,const char *Path,const char *File);

void SEGGER_RTT_LockMutex(t_DriverIOHandleType *DriverIO);
void SEGGER_RTT_UnLockMutex(t_DriverIOHandleType *DriverIO);

#endif   /* end of "#ifndef __SEGGER_RTT_JLINKARM_H_" */
