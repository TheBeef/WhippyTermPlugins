/*******************************************************************************
 * FILENAME: SEGGER_RTT_Common.h
 * 
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    Common code from running the SEGGER RTT JLinkARM dll
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
#ifndef __SEGGER_RTT_COMMON_H_
#define __SEGGER_RTT_COMMON_H_

/***  HEADER FILES TO INCLUDE          ***/
#include "PluginSDK/Plugin.h"
#include "SEGGER_RTT_Main.h"
#include <stdint.h>
#include <string>

/***  DEFINES                          ***/

/***  MACROS                           ***/

/***  TYPE DEFINITIONS                 ***/
struct SEGGER_RTT_ConAuxWidgets
{
    t_WidgetSysHandle *WidgetHandle;
    t_DriverIOHandleType *IOHandle;
    struct PI_ButtonInput *Halt;
    struct PI_ButtonInput *Go;
    struct PI_ButtonInput *Reset;
};

struct SEGGER_RTT_Common
{
    t_IOSystemHandle *IOHandle;
    std::string LastErrorMsg;
    uint8_t ReadBuffer[100];
    int ReadBufferBytes;
    bool ReadDataAvailable;
    std::string DeviceUniqueID;
    struct SEGGER_RTT_ConAuxWidgets *AuxWidgets;
};

/***  CLASS DEFINITIONS                ***/

/***  GLOBAL VARIABLE DEFINITIONS      ***/
extern struct JLinkARMAPI g_SRTT_JLinkAPI;

/***  EXTERNAL FUNCTION PROTOTYPES     ***/
const struct IODriverDetectedInfo *SEGGER_RTT_DetectDevices(void);
void SEGGER_RTT_FreeDetectedDevices(const struct IODriverDetectedInfo *Devices);
PG_BOOL SEGGER_RTT_Commom_Open(t_DriverIOHandleType *DriverIO,
        const t_PIKVList *Options,struct SEGGER_RTT_Common *CommonData);
void SEGGER_RTT_Commom_PollingThread(struct SEGGER_RTT_Common *CommonData);
int SEGGER_RTT_Common_Read(t_DriverIOHandleType *DriverIO,uint8_t *Data,
        int MaxBytes,struct SEGGER_RTT_Common *CommonData);
int SEGGER_RTT_Common_Write(t_DriverIOHandleType *DriverIO,const uint8_t *Data,
        int Bytes,struct SEGGER_RTT_Common *CommonData);
void SEGGER_RTT_Commom_Close(t_DriverIOHandleType *DriverIO,
        struct SEGGER_RTT_Common *CommonData);
t_ConnectionWidgetsType *SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_AllocWidgets(t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle);
void SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_FreeWidgets(t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConAuxCtrls);

#endif   /* end of "#ifndef __SEGGER_RTT_COMMON_H_" */
