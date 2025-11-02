/*******************************************************************************
 * FILENAME: SEGGER_RTT_Common.cpp
 *
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    
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
 * CREATED BY:
 *    Paul Hutchinson (25 May 2025)
 *
 ******************************************************************************/

/*** HEADER FILES TO INCLUDE  ***/
#include "PluginSDK/Plugin.h"
#include "OS/SEGGER_RTT_JLinkARM.h"
#include "SEGGER_RTT_Common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>

using namespace std;

/*** DEFINES                  ***/

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/

/*** FUNCTION PROTOTYPES      ***/

/*** VARIABLE DEFINITIONS     ***/
struct JLinkARMAPI g_SRTT_JLinkAPI;

/*******************************************************************************
 * NAME:
 *    DetectDevices
 *
 * SYNOPSIS:
 *    const struct IODriverDetectedInfo *DetectDevices(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    This function detects different devices for this driver.  It will
 *    allocate a linked list of detected devices filling in a
 *    'struct IODriverDetectedInfo' structure for each detected device.
 *
 *    The 'struct IODriverDetectedInfo' has the following fields:
 *      Next -- A pointer to the next entry in the list or NULL if this was
 *              the last entry.
 *      StructureSize -- The size of the allocated structure.  This must be
 *              set to sizeof(struct IODriverDetectedInfo)
 *      Flags [I] -- What flags apply to this device:
 *                   IODRV_DETECTFLAG_INUSE -- The devices has been detected
 *                          as in use.
 *      DeviceUniqueID -- This is a string that can be used to identify this
 *                        particular delected device later.
 *                        This maybe stored to the disk so it needs to
 *                        be built in such a away as you can extract the
 *                        device again.
 *                        For example the new connection system uses this to
 *                        store the options for this connection into session
 *                        file so it can restore it next time this device is
 *                        selected.
 *      Name [I] -- The string used for the user to select this device.  This
 *                  should be recognisable on it's own to the user as what
 *                  driver this goes with as the system only shows this driver
 *                  without modifying it.
 *      Title -- The string that has a short name for this device.
 *               This will be used for titles of windows, tab's and the like.
 *
 * RETURNS:
 *    The first node in the linked list or NULL if there where no devices
 *    detected or an error.
 *
 * SEE ALSO:
 *    FreeDetectedDevices()
 ******************************************************************************/
const struct IODriverDetectedInfo *SEGGER_RTT_DetectDevices(void)
{
    int32_t Total;
    int32_t Entry;
    struct JLINKARM_EMU_CONNECT_INFO *Infos;
    struct IODriverDetectedInfo *FirstDI;
    struct IODriverDetectedInfo *Next;
    struct IODriverDetectedInfo *NewDetectInfo;
    int ConType;

    Infos=NULL;
    FirstDI=NULL;
    try
    {
        Total=g_SRTT_JLinkAPI.EMU_GetNumDevices();
        if(Total==0)
            throw(0);

        Infos=(struct JLINKARM_EMU_CONNECT_INFO *)malloc(
                sizeof(struct JLINKARM_EMU_CONNECT_INFO)*Total);
        if(Infos==NULL)
            throw(0);

        Total=g_SRTT_JLinkAPI.EMU_GetList(JLINKARM_HOSTIF_USB|
                JLINKARM_HOSTIF_IP,Infos,Total);
        if(Total<=0)
            throw(0);

        for(Entry=0;Entry<Total;Entry++)
        {
            if(Infos[Entry].Connection==JLINKARM_HOSTIF_USB)
                ConType=0;
            else
                ConType=1;
            NewDetectInfo=(struct IODriverDetectedInfo *)
                    malloc(sizeof(struct IODriverDetectedInfo));
            if(NewDetectInfo==NULL)
                throw(0);

            NewDetectInfo->StructureSize=sizeof(struct IODriverDetectedInfo);
            NewDetectInfo->Flags=0;
            snprintf(NewDetectInfo->DeviceUniqueID,
                    sizeof(NewDetectInfo->DeviceUniqueID),"%d:%d:%d",
                    Infos[Entry].SerialNumber,
                    Infos[Entry].USBAddr,
                    ConType);

            if(Infos[Entry].SerialNumber==123456)
            {
                snprintf(NewDetectInfo->Name,
                        sizeof(NewDetectInfo->Name),"SEGGER RTT %d[%d]",
                        Infos[Entry].SerialNumber,Infos[Entry].USBAddr);

                snprintf(NewDetectInfo->Title,
                        sizeof(NewDetectInfo->Title),"RTT %d[%d]",
                        Infos[Entry].SerialNumber,
                        Infos[Entry].USBAddr);
            }
            else
            {
                snprintf(NewDetectInfo->Name,
                        sizeof(NewDetectInfo->Name),"SEGGER RTT %d",
                        Infos[Entry].SerialNumber);

                snprintf(NewDetectInfo->Title,
                        sizeof(NewDetectInfo->Title),"RTT %d",
                        Infos[Entry].SerialNumber);
            }

            NewDetectInfo->Next=FirstDI;
            FirstDI=NewDetectInfo;
        }

        free(Infos);
        Infos=NULL;
    }
    catch(...)
    {
        if(Infos!=NULL)
            free(Infos);
        Infos=NULL;
        while(FirstDI!=NULL)
        {
            Next=FirstDI->Next;
            free(FirstDI);
            FirstDI=Next;
        }
    }

    return FirstDI;
}

/*******************************************************************************
 * NAME:
 *    FreeDetectedDevices
 *
 * SYNOPSIS:
 *    void FreeDetectedDevices(const struct IODriverDetectedInfo *Devices);
 *
 * PARAMETERS:
 *    Devices [I] -- The linked list to free
 *
 * FUNCTION:
 *    This function frees all the links in the linked list allocated with
 *    DetectDevices()
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void SEGGER_RTT_FreeDetectedDevices(const struct IODriverDetectedInfo *Devices)
{
    const struct IODriverDetectedInfo *Next;

    while(Devices!=NULL)
    {
        Next=Devices->Next;
        free((void *)Devices);
        Devices=Next;
    }
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Commom_Open
 *
 * SYNOPSIS:
 *    PG_BOOL SEGGER_RTT_Commom_Open(t_DriverIOHandleType *DriverIO,
 *          const t_PIKVList *Options,struct SEGGER_RTT_Common *CommonData);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *    Options [I] -- The options to apply to this connection.
 *    CommonData [I] -- The common code data.
 *
 * FUNCTION:
 *    This function is called from the OS version of Open().  It opens a
 *    connection to the JLink and starts RTT.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
PG_BOOL SEGGER_RTT_Commom_Open(t_DriverIOHandleType *DriverIO,
        const t_PIKVList *Options,struct SEGGER_RTT_Common *CommonData)
{
    const char *ErrorMsg;
    string Cmd;
    const char *TargetIDStr;
    const char *TargetInterfaceStr;
    const char *TargetSpeedStr;
    const char *ScriptFileStr;
    const char *RTTCtrlBlockModeStr;
    const char *RTTAddressStr;
    const char *JTAGScanChainStr;
    const char *JTAGPositionStr;
    const char *JTAGIRPreStr;
    bool BeenOpened;
    struct JLINK_RTTERMINAL_START RTTCmdStart;
    int RTTCtrlBlockModeID;
    uint32_t SerialNum;
    uint32_t USBAddr;
    char *TmpStr;
    int ConType;
    int JTAGPosition;
    int JTAGIRPre;
    int JTAGScanChain;

    BeenOpened=false;
    try
    {
        CommonData->LastErrorMsg="";

        TargetIDStr=g_SRTT_System->KVGetItem(Options,"TargetID");
        ScriptFileStr=g_SRTT_System->KVGetItem(Options,"ScriptFile");
        TargetInterfaceStr=g_SRTT_System->KVGetItem(Options,"TargetInterface");
        TargetSpeedStr=g_SRTT_System->KVGetItem(Options,"TargetSpeed");
        RTTCtrlBlockModeStr=g_SRTT_System->KVGetItem(Options,"RTTCtrlBlockMode");
        RTTAddressStr=g_SRTT_System->KVGetItem(Options,"RTTAddress");
        JTAGScanChainStr=g_SRTT_System->KVGetItem(Options,"JTAG_ScanChainMode");
        JTAGPositionStr=g_SRTT_System->KVGetItem(Options,"JTAG_Position");
        JTAGIRPreStr=g_SRTT_System->KVGetItem(Options,"JTAG_IRPre");

        if(TargetIDStr==NULL)
            throw(0);

        /* Apply defaults */
        if(TargetSpeedStr==NULL)
            TargetSpeedStr="4000";
        if(RTTCtrlBlockModeStr==NULL)
            RTTCtrlBlockModeStr="0";
        if(JTAGScanChainStr==NULL)
            JTAGScanChainStr="0";
        if(JTAGPositionStr==NULL)
            JTAGPositionStr="0";
        if(JTAGIRPreStr==NULL)
            JTAGIRPreStr="0";
        if(TargetInterfaceStr==NULL)
            TargetInterfaceStr="1";

        SerialNum=strtol(CommonData->DeviceUniqueID.c_str(),&TmpStr,10);
        TmpStr++;
        USBAddr=strtol(TmpStr,&TmpStr,10);
        TmpStr++;
        ConType=strtol(TmpStr,NULL,10);

        RTTCtrlBlockModeID=strtoll(RTTCtrlBlockModeStr,NULL,10);
        JTAGScanChain=strtoll(JTAGScanChainStr,NULL,10);
        JTAGPosition=strtoll(JTAGPositionStr,NULL,10);
        JTAGIRPre=strtoll(JTAGIRPreStr,NULL,10);

        if(ConType==0)
        {
            /* USB */
            if(SerialNum==123456)
            {
                if(g_SRTT_JLinkAPI.SelectUSB(USBAddr)==1)
                {
                    CommonData->LastErrorMsg="Failed to select JLink";
                    return false;
                }
            }
            else
            {
                if(g_SRTT_JLinkAPI.EMU_SelectByUSBSN(SerialNum)<0)
                {
                    CommonData->LastErrorMsg="Failed to select JLink";
                    return false;
                }
            }
        }
        else
        {
            /* IP */
            g_SRTT_JLinkAPI.EMU_SelectIPBySN(SerialNum);
        }

        ErrorMsg=g_SRTT_JLinkAPI.Open();
        if(ErrorMsg!=NULL)
        {
            /* We had an error */
            CommonData->LastErrorMsg=ErrorMsg;
            return false;
        }
        BeenOpened=true;

//        /* Do the project file (we don't actually support this) */
//        if(ProjectFileStr!=NULL && *ProjectFileStr!=0)
//        {
//            Cmd="ProjectFile = ";
//            Cmd+=ProjectFileStr;
//            g_SRTT_JLinkAPI.ExecCommand(Cmd.c_str(),NULL,0);
//        }

        /* JLinkRTTViewer does this so we do too :) */
        g_SRTT_JLinkAPI.ExecCommand("SetSkipInitECCRAMOnConnect = 1",NULL,0);

        /* Set the device */
        Cmd="device = ";
        Cmd+=TargetIDStr;
        g_SRTT_JLinkAPI.ExecCommand(Cmd.c_str(),NULL,0);

        /* Set the script file (untested) */
        if(ScriptFileStr!=NULL && *ScriptFileStr!=0)
        {
            Cmd="ScriptFile = ";
            Cmd+=ScriptFileStr;
            g_SRTT_JLinkAPI.ExecCommand(Cmd.c_str(),NULL,0);
        }

        g_SRTT_JLinkAPI.TIF_Select(atoi(TargetInterfaceStr));
        g_SRTT_JLinkAPI.SetSpeed(strtol(TargetSpeedStr,NULL,10));

        if(ConType==1 && JTAGScanChain==1)
            g_SRTT_JLinkAPI.ConfigJTAG(JTAGIRPre,JTAGPosition);

        if(g_SRTT_JLinkAPI.Connect()<0)
        {
            CommonData->LastErrorMsg="Failed to connect the JLink to target";
            throw(0);
        }

        /* Start RTT */
        switch(RTTCtrlBlockModeID)
        {
            case 0: // Auto
                g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Control(JLINKARM_RTTERMINAL_CMD_START,NULL);
            break;
            case 1: // Address
                if(RTTAddressStr==NULL)
                    throw(0);
                RTTCmdStart.ConfigBlockAddress=strtoll(RTTAddressStr,NULL,0);
                g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Control(JLINKARM_RTTERMINAL_CMD_START,&RTTCmdStart);
            break;
            case 2: // Scan
                Cmd="SetRTTSearchRanges ";
                Cmd+=RTTAddressStr;
                g_SRTT_JLinkAPI.ExecCommand(Cmd.c_str(),NULL,0);
                g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Control(JLINKARM_RTTERMINAL_CMD_START,NULL);
            break;
        }
    }
    catch(...)
    {
        if(BeenOpened)
            g_SRTT_JLinkAPI.Close();
        BeenOpened=false;

        return false;
    }

    return true;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Commom_Close
 *
 * SYNOPSIS:
 *    void SEGGER_RTT_Commom_Close(t_DriverIOHandleType *DriverIO);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *
 * FUNCTION:
 *    This function is called from the OS version of Close().  It stops the
 *    RTT and closes the connection.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    SEGGER_RTT_Open()
 ******************************************************************************/
void SEGGER_RTT_Commom_Close(t_DriverIOHandleType *DriverIO,
        struct SEGGER_RTT_Common *CommonData)
{
    g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Control(JLINKARM_RTTERMINAL_CMD_STOP,NULL);
    g_SRTT_JLinkAPI.Close();

    /* Flush any unread data */
    CommonData->ReadDataAvailable=false;
    CommonData->ReadBufferBytes=0;
}

/*******************************************************************************
 * NAME:
 *    
 *
 * SYNOPSIS:
 *    
 *
 * PARAMETERS:
 *    
 *
 * FUNCTION:
 *    
 *
 * RETURNS:
 *    
 *
 * NOTES:
 *    
 *
 * LIMITATIONS:
 *    
 *
 * EXAMPLE:
 *    
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void SEGGER_RTT_Commom_PollingThread(struct SEGGER_RTT_Common *CommonData)
{
    if(!CommonData->ReadDataAvailable)
    {
        CommonData->ReadBufferBytes=g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Read(0,
                (char *)CommonData->ReadBuffer,sizeof(CommonData->ReadBuffer));

        if(CommonData->ReadBufferBytes<0)
        {
            /* Error */
            CommonData->ReadBufferBytes=0;
            return;
        }

        CommonData->ReadDataAvailable=true;

        g_SRTT_IOSystem->DrvDataEvent(CommonData->IOHandle,
                e_DataEventCode_BytesAvailable);
    }
}

int SEGGER_RTT_Common_Read(t_DriverIOHandleType *DriverIO,uint8_t *Data,
        int MaxBytes,struct SEGGER_RTT_Common *CommonData)
{
    int BytesRead;

    if(!CommonData->ReadDataAvailable)
        return RETERROR_NOBYTES;

    BytesRead=CommonData->ReadBufferBytes;
    memcpy(Data,CommonData->ReadBuffer,CommonData->ReadBufferBytes);

    CommonData->ReadDataAvailable=false;

    return BytesRead;
}

int SEGGER_RTT_Common_Write(t_DriverIOHandleType *DriverIO,const uint8_t *Data,
        int Bytes,struct SEGGER_RTT_Common *CommonData)
{
    return g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Write(0,(char *)Data,Bytes);
}
