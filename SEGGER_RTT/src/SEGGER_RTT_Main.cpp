/*******************************************************************************
 * FILENAME: SEGGER_RTT_Main.cpp
 *
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    This file has the SEGGER's RTT plugin in it.
 *
 *    This plugin uses the URI format:
 *      RTT0://SerialNumber:USBAddress/TargetDeviceType
 *    Example:
 *      RTT0://158007529/CS32F103C8
 *      RTT0://123456:1/OZ93506F16LN
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
#include "SEGGER_RTT_Main.h"
#include "SEGGER_RTT_Common.h"
#include "PluginSDK/IODriver.h"
#include "PluginSDK/Plugin.h"
#include "OS/SEGGER_RTT_JLinkARM.h"
#include "SEGGER_RTT_AuxWidgets.h"
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string>

using namespace std;

/*** DEFINES                  ***/
#define SEGGER_RTT_URI_PREFIX                   "RTT"
#define REGISTER_PLUGIN_FUNCTION_PRIV_NAME      SEGGER_RTT  // The name to append on the RegisterPlugin() function for built in version
#define NEEDED_MIN_API_VERSION                  0x01000400

#define TARGETHISTORY_SIZE                      5

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/
struct SEGGER_RTT_ConWidgets
{
    t_WidgetSysHandle *WidgetHandle;

    struct PI_ComboBox *TargetID;
    struct PI_TextInput *ScriptFile;
    struct PI_ComboBox *TargetInterface;
    struct PI_ComboBox *TargetSpeed;
    struct PI_ComboBox *TargetJTAGChain;
    struct PI_ComboBox *TargetJTAGPos;
    struct PI_TextInput *TargetJTAGIRPre;
    struct PI_RadioBttnGroup *RTTCtrlBlock;
    struct PI_RadioBttn *RTTCtrlBlockAuto;
    struct PI_RadioBttn *RTTCtrlBlockAddress;
    struct PI_RadioBttn *RTTCtrlBlockSearch;
    struct PI_ButtonInput *SelectTarget;
    struct PI_ButtonInput *SelectScriptFile;
    struct PI_TextInput *RTTAddress;
};

/*** FUNCTION PROTOTYPES      ***/
PG_BOOL SEGGER_RTT_Init(void);
const struct IODriverInfo *SEGGER_RTT_GetDriverInfo(unsigned int *SizeOfInfo);
t_ConnectionWidgetsType *SEGGER_RTT_ConnectionOptionsWidgets_AllocWidgets(
        t_WidgetSysHandle *WidgetHandle);
void SEGGER_RTT_ConnectionOptionsWidgets_FreeWidgets(t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConOptions);
void SEGGER_RTT_ConnectionOptionsWidgets_StoreUI(t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConOptions,const char *DeviceUniqueID,t_PIKVList *Options);
void SEGGER_RTT_ConnectionOptionsWidgets_UpdateUI(t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConOptions,const char *DeviceUniqueID,t_PIKVList *Options);
PG_BOOL SEGGER_RTT_Convert_URI_To_Options(const char *URI,t_PIKVList *Options,
            char *DeviceUniqueID,unsigned int MaxDeviceUniqueIDLen,
            PG_BOOL Update);
PG_BOOL SEGGER_RTT_Convert_Options_To_URI(const char *DeviceUniqueID,
            t_PIKVList *Options,char *URI,unsigned int MaxURILen);
PG_BOOL SEGGER_RTT_GetConnectionInfo(const char *DeviceUniqueID,t_PIKVList *Options,struct IODriverDetectedInfo *RetInfo);
static void SelectScriptEventCB(const struct PIButtonEvent *Event,void *UserData);
static void SelectTargetEventCB(const struct PIButtonEvent *Event,void *UserData);
static void TargetInterfaceEventCB(const struct PICBEvent *Event,void *UserData);
static void SEGGER_RTT_RethinkWidgetsEnabled(struct SEGGER_RTT_ConWidgets *ConWidgets);
static void TargetJTAGChainEventCB(const struct PICBEvent *Event,void *UserData);
static void RTTCtrlBlockEventCB(const struct PIRBEvent *Event,void *UserData);
static void TargetJTAGPosEventCB(const struct PICBEvent *Event,void *UserData);

/*** VARIABLE DEFINITIONS     ***/
const struct IODriverAPI g_SEGGERRTTPluginAPI=
{
    SEGGER_RTT_Init,
    SEGGER_RTT_GetDriverInfo,
    NULL,                                               // InstallPlugin
    NULL,                                               // UnInstallPlugin
    SEGGER_RTT_DetectDevices,
    SEGGER_RTT_FreeDetectedDevices,
    SEGGER_RTT_GetConnectionInfo,
    SEGGER_RTT_ConnectionOptionsWidgets_AllocWidgets,
    SEGGER_RTT_ConnectionOptionsWidgets_FreeWidgets,
    SEGGER_RTT_ConnectionOptionsWidgets_StoreUI,
    SEGGER_RTT_ConnectionOptionsWidgets_UpdateUI,
    SEGGER_RTT_Convert_URI_To_Options,
    SEGGER_RTT_Convert_Options_To_URI,
    SEGGER_RTT_AllocateHandle,
    SEGGER_RTT_FreeHandle,
    SEGGER_RTT_Open,
    SEGGER_RTT_Close,
    SEGGER_RTT_Read,
    SEGGER_RTT_Write,
    SEGGER_RTT_ChangeOptions,
    NULL,                                                   // Transmit
    SEGGER_RTT_ConnectionAuxCtrlWidgets_AllocWidgets,
    SEGGER_RTT_ConnectionAuxCtrlWidgets_FreeWidgets,

    /********* Start of IODRIVER_API_VERSION_2 *********/
    SEGGER_RTT_GetLastErrorMessage,
};

struct IODriverInfo m_SEGGERRTTInfo=
{
    0,
    "<URI>RTT://[SerialNumber][:USBAddress]/[TargetDeviceType]</URI>"
    "<ARG>SerialNumber -- The serial number of the J-Link debug probe</ARG>"
    "<ARG>USBAddress -- If using an older-J-Link you need to provide the USBaddress as the Serial Number will always be 123456</ARG>"
    "<ARG>TargetDeviceType -- The type of target that will be connected to the JLink</ARG>"
    "<Example>RTT://158007529/CS32F103C8</Example>"
};

const struct IOS_API *g_SRTT_IOSystem;
const struct PI_UIAPI *g_SRTT_UI;
const struct PI_SystemAPI *g_SRTT_System;

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_RegisterPlugin
 *
 * SYNOPSIS:
 *    unsigned int SEGGER_RTT_RegisterPlugin(const struct PI_SystemAPI *SysAPI,
 *          int Version);
 *
 * PARAMETERS:
 *    SysAPI [I] -- The main API to WhippyTerm
 *    Version [I] -- What version of WhippyTerm is running.  This is used
 *                   to make sure we are compatible.  This is in the
 *                   Major<<24 | Minor<<16 | Rev<<8 | Patch format
 *
 * FUNCTION:
 *    This function registers this plugin with the system.
 *
 * RETURNS:
 *    0 if we support this version of WhippyTerm, and the minimum version
 *    we need if we are not.
 *
 * NOTES:
 *    This function is normally is called from the RegisterPlugin() when
 *    it is being used as a normal plugin.  As a std plugin it is called
 *    from RegisterStdPlugins() instead.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
/* This needs to be extern "C" because it is the main entry point for the
   plugin system */
extern "C"
{
    unsigned int REGISTER_PLUGIN_FUNCTION(const struct PI_SystemAPI *SysAPI,
            unsigned int Version)
    {
        if(Version<NEEDED_MIN_API_VERSION)
            return NEEDED_MIN_API_VERSION;

        g_SRTT_System=SysAPI;
        g_SRTT_IOSystem=g_SRTT_System->GetAPI_IO();
        g_SRTT_UI=g_SRTT_IOSystem->GetAPI_UI();

        /* If we are have the correct experimental API */
        if(g_SRTT_System->GetExperimentalID()>0 &&
                g_SRTT_System->GetExperimentalID()<1)
        {
            return 0xFFFFFFFF;
        }

        g_SRTT_IOSystem->RegisterDriver("SEGGERRTT",SEGGER_RTT_URI_PREFIX,
                &g_SEGGERRTTPluginAPI,sizeof(g_SEGGERRTTPluginAPI));

        return 0;
    }
}

/*******************************************************************************
 * NAME:
 *   SEGGER_RTT_GetDriverInfo
 *
 * SYNOPSIS:
 *   const struct IODriverInfo *(*SEGGER_RTT_GetDriverInfo)(
 *              unsigned int *SizeOfInfo);
 *
 * PARAMETERS:
 *   SizeOfInfo [O] -- The size of 'struct IODriverInfo'.  This is used
 *                     for forward / backward compatibility.
 *
 * FUNCTION:
 *   This function gets info about the plugin.
 *
 * RETURNS:
 *   NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
const struct IODriverInfo *SEGGER_RTT_GetDriverInfo(unsigned int *SizeOfInfo)
{
    *SizeOfInfo=sizeof(struct IODriverInfo);
    return &m_SEGGERRTTInfo;
}

/*******************************************************************************
 * NAME:
 *    SelectScriptEventCB
 *
 * SYNOPSIS:
 *    void SelectScriptEventCB(const struct PIButtonEvent *Event,void *UserData);
 *
 * PARAMETERS:
 *    Event [I] -- The button event details
 *    UserData [I] -- The 'struct SEGGER_RTT_ConWidgets' handle that has
 *                    our data in it.
 *
 * FUNCTION:
 *    This function is called when the select script button is pressed.
 *    It prompts the user to a file name (and path) and stores that in the
 *    script file input.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void SelectScriptEventCB(const struct PIButtonEvent *Event,void *UserData)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets=(struct SEGGER_RTT_ConWidgets *)UserData;
    char *Path;
    char *File;
    string FullPath;

    if(g_SRTT_UI->FileReq==NULL)
        return;

    try
    {
        if(g_SRTT_UI->FileReq(e_FileReqType_Load,"Select script file",&Path,
                &File,"All Files|*\n",0))
        {
            SEGGER_RTT_AppendFilename2Path(FullPath,Path,File);

            g_SRTT_UI->SetTextInputText(ConWidgets->WidgetHandle,
                    ConWidgets->ScriptFile->Ctrl,FullPath.c_str());

            g_SRTT_UI->FreeFileReqPathAndFile(&Path,&File);
        }
    }
    catch(...)
    {
    }
}

/*******************************************************************************
 * NAME:
 *    SelectTargetEventCB
 *
 * SYNOPSIS:
 *    void SelectTargetEventCB(const struct PIButtonEvent *Event,void *UserData);
 *
 * PARAMETERS:
 *    Event [I] -- The button event details
 *    UserData [I] -- The 'struct SEGGER_RTT_ConWidgets' handle that has
 *                    our data in it.
 *
 * FUNCTION:
 *    This function is called when the select target button is pressed.
 *    It prompts the user to select a target device type and the fills
 *    in the target ID input.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void SelectTargetEventCB(const struct PIButtonEvent *Event,void *UserData)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets=(struct SEGGER_RTT_ConWidgets *)UserData;
    struct JLINKARM_DEVICE_SELECT_INFO Info;
    struct JLINKARM_DEVICE_INFO DevInfo;
    int32_t SelectedTargetIdx;

    Info.Size=sizeof(Info);
    SelectedTargetIdx=g_SRTT_JLinkAPI.DEVICE_SelectDialog(NULL,0,&Info);
    if(SelectedTargetIdx<0)
        return;

    /* Ok, now get some info about this target index */
    DevInfo.Size=sizeof(DevInfo);
    g_SRTT_JLinkAPI.DEVICE_GetInfo(SelectedTargetIdx,&DevInfo);
    g_SRTT_UI->SetComboBoxText(ConWidgets->WidgetHandle,
            ConWidgets->TargetID->Ctrl,DevInfo.Name);
}

/*******************************************************************************
 * NAME:
 *    TargetInterfaceEventCB
 *
 * SYNOPSIS:
 *    static void TargetInterfaceEventCB(const struct PICBEvent *Event,
 *          void *UserData);
 *
 * PARAMETERS:
 *    Event [I] -- The button event details
 *    UserData [I] -- The 'struct SEGGER_RTT_ConWidgets' handle that has
 *                    our data in it.
 *
 * FUNCTION:
 *    This function is called when the target interface is changed.  It
 *    enables / disables other controls.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void TargetInterfaceEventCB(const struct PICBEvent *Event,void *UserData)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets=(struct SEGGER_RTT_ConWidgets *)UserData;

    SEGGER_RTT_RethinkWidgetsEnabled(ConWidgets);
}

/*******************************************************************************
 * NAME:
 *    TargetJTAGChainEventCB
 *
 * SYNOPSIS:
 *    static void TargetJTAGChainEventCB(const struct PICBEvent *Event,
 *          void *UserData);
 *
 * PARAMETERS:
 *    Event [I] -- The button event details
 *    UserData [I] -- The 'struct SEGGER_RTT_ConWidgets' handle that has
 *                    our data in it.
 *
 * FUNCTION:
 *    This function is called when the target interface is changed.  It
 *    enables / disables other controls.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void TargetJTAGChainEventCB(const struct PICBEvent *Event,void *UserData)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets=(struct SEGGER_RTT_ConWidgets *)UserData;

    SEGGER_RTT_RethinkWidgetsEnabled(ConWidgets);
}

/*******************************************************************************
 * NAME:
 *    RTTCtrlBlockEventCB
 *
 * SYNOPSIS:
 *    static void RTTCtrlBlockEventCB(const struct PIRBEvent *Event,
 *          void *UserData);
 *
 * PARAMETERS:
 *    Event [I] -- The button event details
 *    UserData [I] -- The 'struct SEGGER_RTT_ConWidgets' handle that has
 *                    our data in it.
 *
 * FUNCTION:
 *    This function is called when the RTT control block is changed.  It
 *    enables / disables other controls.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void RTTCtrlBlockEventCB(const struct PIRBEvent *Event,void *UserData)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets=(struct SEGGER_RTT_ConWidgets *)UserData;

    SEGGER_RTT_RethinkWidgetsEnabled(ConWidgets);
}

/*******************************************************************************
 * NAME:
 *    TargetJTAGPosEventCB
 *
 * SYNOPSIS:
 *    static void TargetJTAGPosEventCB(const struct PIRBEvent *Event,
 *          void *UserData);
 *
 * PARAMETERS:
 *    Event [I] -- The button event details
 *    UserData [I] -- The 'struct SEGGER_RTT_ConWidgets' handle that has
 *                    our data in it.
 *
 * FUNCTION:
 *    This function is called when the JTag position is changed.  It
 *    fills in 'TargetJTAGIRPre'.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void TargetJTAGPosEventCB(const struct PICBEvent *Event,void *UserData)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets=(struct SEGGER_RTT_ConWidgets *)UserData;
    int SelPos;
    char buff[100];

    SelPos=g_SRTT_UI->GetComboBoxSelectedEntry(ConWidgets->WidgetHandle,
            ConWidgets->TargetJTAGPos->Ctrl);
    sprintf(buff,"%d",SelPos*4);
    g_SRTT_UI->SetTextInputText(ConWidgets->WidgetHandle,
            ConWidgets->TargetJTAGIRPre->Ctrl,buff);
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_ConnectionOptionsWidgets_AllocWidgets
 *
 * SYNOPSIS:
 *    t_ConnectionWidgetsType *SEGGER_RTT_ConnectionOptionsWidgets_AllocWidgets(
 *          t_WidgetSysHandle *WidgetHandle);
 *
 * PARAMETERS:
 *    WidgetHandle [I] -- The handle to send to the widgets
 *
 * FUNCTION:
 *    This function adds options widgets to a container widget.  These are
 *    options for the connection.  It's things like bit rate, parity, or
 *    any other options the device supports.
 *
 *    The device driver needs to keep handles to the widgets added because it
 *    needs to free them when RemoveNewConnectionOptionsWidgets() called.
 *
 * RETURNS:
 *    The private options data that you want to use.  This is a private
 *    structure that you allocate and then cast to
 *    (t_ConnectionWidgetsType *) when you return.
 *
 * NOTES:
 *    This function must be reentrant.  The system may allocate many sets
 *    of option widgets and free them in any order.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
t_ConnectionWidgetsType *SEGGER_RTT_ConnectionOptionsWidgets_AllocWidgets(
        t_WidgetSysHandle *WidgetHandle)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets;
    int r;
    char buff[100];

    ConWidgets=NULL;
    try
    {
        ConWidgets=new struct SEGGER_RTT_ConWidgets;

        ConWidgets->WidgetHandle=WidgetHandle;

        ConWidgets->TargetID=g_SRTT_UI->AddComboBox(WidgetHandle,true,
                "Target Device Type",NULL,NULL);
        if(ConWidgets->TargetID==NULL)
            throw(0);

        ConWidgets->SelectTarget=g_SRTT_UI->AddButtonInput(WidgetHandle,
                "Select Target Device...",SelectTargetEventCB,ConWidgets);
        if(ConWidgets->SelectTarget==NULL)
            throw(0);

        ConWidgets->ScriptFile=g_SRTT_UI->AddTextInput(WidgetHandle,
                "Script file (optional)",NULL,NULL);
        if(ConWidgets->ScriptFile==NULL)
            throw(0);

        /* This is V2 API function so if it's not available then we don't
           add the button. */
        ConWidgets->SelectScriptFile=NULL;
        if(g_SRTT_UI->FileReq!=NULL)
        {
            ConWidgets->SelectScriptFile=g_SRTT_UI->AddButtonInput(WidgetHandle,
                    "Select script file...",SelectScriptEventCB,ConWidgets);
            if(ConWidgets->SelectScriptFile==NULL)
                throw(0);
        }

        ConWidgets->TargetInterface=g_SRTT_UI->AddComboBox(WidgetHandle,false,
                "Interface",TargetInterfaceEventCB,ConWidgets);
        if(ConWidgets->TargetInterface==NULL)
            throw(0);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetInterface->
                Ctrl,"JTAG",JLINKARM_TIF_JTAG);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetInterface->
                Ctrl,"SWD",JLINKARM_TIF_SWD);

        ConWidgets->TargetSpeed=g_SRTT_UI->AddComboBox(WidgetHandle,false,
                "Speed",NULL,NULL);
        if(ConWidgets->TargetSpeed==NULL)
            throw(0);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "5kHz",5);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "10kHz",10);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "20kHz",20);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "30kHz",30);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "50kHz",50);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "100kHz",100);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "200kHz",200);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "300kHz",300);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "400kHz",400);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "500kHz",500);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "600kHz",600);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "750kHz",750);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "900kHz",900);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "1000kHz",1000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "1334kHz",1334);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "1600kHz",1600);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "2000kHz",2000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "2667kHz",2667);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "3200kHz",3200);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "4000kHz",4000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "4800kHz",4800);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "5334kHz",5334);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "6000kHz",6000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "8000kHz",8000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "9600kHz",9600);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "12000kHz",12000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "15000kHz",15000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "20000kHz",20000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "25000kHz",25000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "30000kHz",30000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "40000kHz",40000);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,
                "50000kHz",50000);

        ConWidgets->TargetJTAGChain=g_SRTT_UI->AddComboBox(WidgetHandle,false,
                "JTAG scan chain",TargetJTAGChainEventCB,ConWidgets);
        if(ConWidgets->TargetJTAGChain==NULL)
            throw(0);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetJTAGChain->
                Ctrl,"Auto detaction",0);
        g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetJTAGChain->
                Ctrl,"Simple Configuration",1);

        ConWidgets->TargetJTAGPos=g_SRTT_UI->AddComboBox(WidgetHandle,false,
                "JTAG Position",TargetJTAGPosEventCB,ConWidgets);
        if(ConWidgets->TargetJTAGPos==NULL)
            throw(0);
        for(r=0;r<32;r++)
        {
            sprintf(buff,"%d",r);
            g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetJTAGPos->
                    Ctrl,buff,r);
        }

        ConWidgets->TargetJTAGIRPre=g_SRTT_UI->AddTextInput(WidgetHandle,
                "JTAG IRPre",NULL,NULL);
        if(ConWidgets->TargetJTAGIRPre==NULL)
            throw(0);

        ConWidgets->RTTCtrlBlock=g_SRTT_UI->AllocRadioBttnGroup(WidgetHandle,
                "RTT Control Block");
        if(ConWidgets->RTTCtrlBlock==NULL)
            throw(0);
        ConWidgets->RTTCtrlBlockAuto=g_SRTT_UI->AddRadioBttn(WidgetHandle,
                ConWidgets->RTTCtrlBlock,"Auto Detection",RTTCtrlBlockEventCB,
                ConWidgets);
        if(ConWidgets->RTTCtrlBlockAuto==NULL)
            throw(0);
        ConWidgets->RTTCtrlBlockAddress=g_SRTT_UI->AddRadioBttn(WidgetHandle,
                ConWidgets->RTTCtrlBlock,"Address",RTTCtrlBlockEventCB,
                ConWidgets);
        if(ConWidgets->RTTCtrlBlockAddress==NULL)
            throw(0);
        ConWidgets->RTTCtrlBlockSearch=g_SRTT_UI->AddRadioBttn(WidgetHandle,
                ConWidgets->RTTCtrlBlock,"Search Range",RTTCtrlBlockEventCB,
                ConWidgets);
        if(ConWidgets->RTTCtrlBlockSearch==NULL)
            throw(0);

        ConWidgets->RTTAddress=g_SRTT_UI->AddTextInput(WidgetHandle,
                "RTT Control Block Address",NULL,NULL);
        if(ConWidgets->RTTAddress==NULL)
            throw(0);

        SEGGER_RTT_RethinkWidgetsEnabled(ConWidgets);
    }
    catch(...)
    {
        if(ConWidgets!=NULL)
        {
            if(ConWidgets->TargetID!=NULL)
                g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetID);

            if(ConWidgets->ScriptFile!=NULL)
                g_SRTT_UI->FreeTextInput(WidgetHandle,ConWidgets->ScriptFile);

            if(ConWidgets->TargetInterface!=NULL)
                g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetInterface);

            if(ConWidgets->TargetSpeed!=NULL)
                g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetSpeed);

            if(ConWidgets->RTTCtrlBlockAuto!=NULL)
                g_SRTT_UI->FreeRadioBttn(WidgetHandle,ConWidgets->RTTCtrlBlockAuto);
            if(ConWidgets->RTTCtrlBlockAddress!=NULL)
                g_SRTT_UI->FreeRadioBttn(WidgetHandle,ConWidgets->RTTCtrlBlockAddress);
            if(ConWidgets->RTTCtrlBlockSearch!=NULL)
                g_SRTT_UI->FreeRadioBttn(WidgetHandle,ConWidgets->RTTCtrlBlockSearch);
            if(ConWidgets->RTTCtrlBlock!=NULL)
                g_SRTT_UI->FreeRadioBttnGroup(WidgetHandle,ConWidgets->RTTCtrlBlock);

            if(ConWidgets->SelectTarget!=NULL)
                g_SRTT_UI->FreeButtonInput(WidgetHandle,ConWidgets->SelectTarget);

            if(ConWidgets->SelectScriptFile!=NULL)
                g_SRTT_UI->FreeButtonInput(WidgetHandle,ConWidgets->SelectScriptFile);

            if(ConWidgets->TargetJTAGChain!=NULL)
                g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetJTAGChain);

            if(ConWidgets->TargetJTAGPos!=NULL)
                g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetJTAGPos);

            if(ConWidgets->TargetJTAGIRPre!=NULL)
                g_SRTT_UI->FreeTextInput(WidgetHandle,ConWidgets->TargetJTAGIRPre);

            if(ConWidgets->RTTAddress!=NULL)
                g_SRTT_UI->FreeTextInput(WidgetHandle,ConWidgets->RTTAddress);

            delete ConWidgets;
        }
        return NULL;
    }

    return (t_ConnectionWidgetsType *)ConWidgets;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_ConnectionOptionsWidgets_FreeWidgets
 *
 * SYNOPSIS:
 *    void SEGGER_RTT_ConnectionOptionsWidgets_FreeWidgets(
 *          t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConOptions);
 *
 * PARAMETERS:
 *    ConOptions [I] -- The options data that was allocated with
 *          ConnectionOptionsWidgets_AllocWidgets().
 *    WidgetHandle [I] -- The handle to send to the widgets
 *
 * FUNCTION:
 *    Frees the widgets added with ConnectionOptionsWidgets_AllocWidgets()
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void SEGGER_RTT_ConnectionOptionsWidgets_FreeWidgets(
        t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConOptions)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets=(struct SEGGER_RTT_ConWidgets *)ConOptions;

    if(ConWidgets->TargetID!=NULL)
        g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetID);

    if(ConWidgets->ScriptFile!=NULL)
        g_SRTT_UI->FreeTextInput(WidgetHandle,ConWidgets->ScriptFile);

    if(ConWidgets->TargetInterface!=NULL)
        g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetInterface);

    if(ConWidgets->TargetSpeed!=NULL)
        g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetSpeed);

    if(ConWidgets->RTTCtrlBlockAuto!=NULL)
        g_SRTT_UI->FreeRadioBttn(WidgetHandle,ConWidgets->RTTCtrlBlockAuto);
    if(ConWidgets->RTTCtrlBlockAddress!=NULL)
        g_SRTT_UI->FreeRadioBttn(WidgetHandle,ConWidgets->RTTCtrlBlockAddress);
    if(ConWidgets->RTTCtrlBlockSearch!=NULL)
        g_SRTT_UI->FreeRadioBttn(WidgetHandle,ConWidgets->RTTCtrlBlockSearch);
    if(ConWidgets->RTTCtrlBlock!=NULL)
        g_SRTT_UI->FreeRadioBttnGroup(WidgetHandle,ConWidgets->RTTCtrlBlock);

    if(ConWidgets->SelectTarget!=NULL)
        g_SRTT_UI->FreeButtonInput(WidgetHandle,ConWidgets->SelectTarget);

    if(ConWidgets->SelectScriptFile!=NULL)
        g_SRTT_UI->FreeButtonInput(WidgetHandle,ConWidgets->SelectScriptFile);

    if(ConWidgets->TargetJTAGChain!=NULL)
        g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetJTAGChain);

    if(ConWidgets->TargetJTAGPos!=NULL)
        g_SRTT_UI->FreeComboBox(WidgetHandle,ConWidgets->TargetJTAGPos);

    if(ConWidgets->TargetJTAGIRPre!=NULL)
        g_SRTT_UI->FreeTextInput(WidgetHandle,ConWidgets->TargetJTAGIRPre);

    if(ConWidgets->RTTAddress!=NULL)
        g_SRTT_UI->FreeTextInput(WidgetHandle,ConWidgets->RTTAddress);

    delete ConWidgets;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_ConnectionOptionsWidgets_StoreUI
 *
 * SYNOPSIS:
 *      void SEGGER_RTT_ConnectionOptionsWidgets_StoreUI(
 *              t_WidgetSysHandle *WidgetHandle,
 *              t_ConnectionWidgetsType *ConOptions,const char *DeviceUniqueID,
 *              t_PIKVList *Options);
 *
 * PARAMETERS:
 *    ConOptions [I] -- The options data that was allocated with
 *          ConnectionOptionsWidgets_AllocWidgets().
 *    WidgetHandle [I] -- The handle to send to the widgets
 *    DeviceUniqueID [I] -- This is the unique ID for the device we are working
 *                          on.
 *    Options [O] -- The options for this connection.
 *
 * FUNCTION:
 *    This function takes the widgets added with
 *    ConnectionOptionsWidgets_AllocWidgets() and stores them is a key/value pair
 *    list in 'Options'.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void SEGGER_RTT_ConnectionOptionsWidgets_StoreUI(
        t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConOptions,
        const char *DeviceUniqueID,t_PIKVList *Options)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets=(struct SEGGER_RTT_ConWidgets *)ConOptions;
    char buff[100];
    const char *ScriptFileStr;
    const char *TargetIDStr;
    uintptr_t TargetInterfaceSel;
    uintptr_t TargetSpeedSel;
    uintptr_t Selected;
    const char *RTTCtrlBlockMode;
    const char *TargetHistory;
    const char *TmpString;
    int r;
    bool Found;

    if(ConWidgets->TargetID==NULL ||
            ConWidgets->ScriptFile==NULL ||
            ConWidgets->TargetInterface==NULL ||
            ConWidgets->TargetSpeed==NULL || ConWidgets->RTTCtrlBlock==NULL ||
            ConWidgets->SelectTarget==NULL ||
            ConWidgets->RTTCtrlBlockAuto==NULL ||
            ConWidgets->RTTCtrlBlockAddress==NULL ||
            ConWidgets->TargetJTAGChain==NULL ||
            ConWidgets->TargetJTAGPos==NULL ||
            ConWidgets->TargetJTAGIRPre==NULL ||
            ConWidgets->RTTAddress==NULL ||
            ConWidgets->RTTCtrlBlockSearch==NULL)
    {
        return;
    }

    TargetIDStr=g_SRTT_UI->GetComboBoxText(WidgetHandle,
            ConWidgets->TargetID->Ctrl);
    g_SRTT_System->KVAddItem(Options,"TargetID",TargetIDStr);

    /* Add to history */
    Found=false;
    for(r=0;r<TARGETHISTORY_SIZE;r++)
    {
        sprintf(buff,"TargetIDHistory%d",r);
        TargetHistory=g_SRTT_System->KVGetItem(Options,buff);
        if(TargetHistory!=NULL)
        {
            if(strcmp(TargetHistory,TargetIDStr)==0)
                Found=true;
        }
    }
    if(!Found)
    {
        sprintf(buff,"TargetIDHistory%d",TARGETHISTORY_SIZE-1);
        TargetHistory=g_SRTT_System->KVGetItem(Options,buff);
        if(TargetHistory!=NULL)
        {
            /* Remove the oldest and then add a new one to the end */
            for(r=1;r<TARGETHISTORY_SIZE;r++)
            {
                sprintf(buff,"TargetIDHistory%d",r);
                TargetHistory=g_SRTT_System->KVGetItem(Options,buff);
                if(TargetHistory!=NULL)
                {
                    sprintf(buff,"TargetIDHistory%d",r-1);
                    g_SRTT_System->KVAddItem(Options,buff,TargetHistory);
                }
            }
            sprintf(buff,"TargetIDHistory%d",TARGETHISTORY_SIZE-1);
            g_SRTT_System->KVAddItem(Options,buff,TargetIDStr);
        }
        else
        {
            /* Just add to the end */
            for(r=0;r<TARGETHISTORY_SIZE;r++)
            {
                sprintf(buff,"TargetIDHistory%d",r);
                TargetHistory=g_SRTT_System->KVGetItem(Options,buff);
                if(TargetHistory==NULL)
                    break;
            }
            if(r!=TARGETHISTORY_SIZE)
            {
                sprintf(buff,"TargetIDHistory%d",r);
                g_SRTT_System->KVAddItem(Options,buff,TargetIDStr);
            }
        }
    }

    ScriptFileStr=g_SRTT_UI->GetTextInputText(WidgetHandle,
            ConWidgets->ScriptFile->Ctrl);
    g_SRTT_System->KVAddItem(Options,"ScriptFile",ScriptFileStr);

    TargetInterfaceSel=g_SRTT_UI->GetComboBoxSelectedEntry(WidgetHandle,
            ConWidgets->TargetInterface->Ctrl);
    sprintf(buff,"%" PRIuPTR,TargetInterfaceSel);
    g_SRTT_System->KVAddItem(Options,"TargetInterface",buff);

    TargetSpeedSel=g_SRTT_UI->GetComboBoxSelectedEntry(WidgetHandle,
            ConWidgets->TargetSpeed->Ctrl);
    sprintf(buff,"%" PRIuPTR,TargetSpeedSel);
    g_SRTT_System->KVAddItem(Options,"TargetSpeed",buff);

    RTTCtrlBlockMode="0";
    if(g_SRTT_UI->IsRadioBttnChecked(WidgetHandle,ConWidgets->RTTCtrlBlockAuto))
        RTTCtrlBlockMode="0";
    if(g_SRTT_UI->IsRadioBttnChecked(WidgetHandle,ConWidgets->RTTCtrlBlockAddress))
        RTTCtrlBlockMode="1";
    if(g_SRTT_UI->IsRadioBttnChecked(WidgetHandle,ConWidgets->RTTCtrlBlockSearch))
        RTTCtrlBlockMode="2";
    g_SRTT_System->KVAddItem(Options,"RTTCtrlBlockMode",RTTCtrlBlockMode);

    Selected=g_SRTT_UI->GetComboBoxSelectedEntry(WidgetHandle,
            ConWidgets->TargetJTAGChain->Ctrl);
    sprintf(buff,"%" PRIuPTR,Selected);
    g_SRTT_System->KVAddItem(Options,"JTAG_ScanChainMode",buff);

    Selected=g_SRTT_UI->GetComboBoxSelectedEntry(WidgetHandle,
            ConWidgets->TargetJTAGPos->Ctrl);
    sprintf(buff,"%" PRIuPTR,Selected);
    g_SRTT_System->KVAddItem(Options,"JTAG_Position",buff);

    TmpString=g_SRTT_UI->GetTextInputText(WidgetHandle,
            ConWidgets->TargetJTAGIRPre->Ctrl);
    g_SRTT_System->KVAddItem(Options,"JTAG_IRPre",TmpString);

    TmpString=g_SRTT_UI->GetTextInputText(WidgetHandle,
            ConWidgets->RTTAddress->Ctrl);
    g_SRTT_System->KVAddItem(Options,"RTTAddress",TmpString);
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_ConnectionOptionsWidgets_UpdateUI
 *
 * SYNOPSIS:
 *    void SEGGER_RTT_ConnectionOptionsWidgets_UpdateUI(
 *          t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConOptions,
 *          const char *DeviceUniqueID,t_PIKVList *Options);
 *
 * PARAMETERS:
 *    ConOptions [I] -- The options data that was allocated with
 *                      ConnectionOptionsWidgets_AllocWidgets().
 *    WidgetHandle [I] -- The handle to send to the widgets
 *    DeviceUniqueID [I] -- This is the unique ID for the device we are working
 *                          on.
 *    Options [I] -- The options for this connection.
 *
 * FUNCTION:
 *    This function takes the widgets added with
 *    ConnectionOptionsWidgets_AllocWidgets() and sets them to the values stored in
 *    the key/value pair list in 'Options'.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    ConnectionOptionsWidgets_StoreUI()
 ******************************************************************************/
void SEGGER_RTT_ConnectionOptionsWidgets_UpdateUI(
        t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConOptions,
        const char *DeviceUniqueID,t_PIKVList *Options)
{
    struct SEGGER_RTT_ConWidgets *ConWidgets=(struct SEGGER_RTT_ConWidgets *)ConOptions;
    int r;
    char buff[100];
    const char *TargetIDStr;
    const char *ScriptFileStr;
    const char *TargetInterfaceStr;
    const char *TargetSpeedStr;
    const char *RTTCtrlBlockModeStr;
    const char *JTAGScanChainStr;
    const char *JTAGPositionStr;
    const char *JTAGIRPreStr;
    const char *RTTAddressStr;
    uintptr_t TargetInterfaceID;
    uintptr_t TargetSpeedID;
    uintptr_t RTTCtrlBlockModeID;
    uintptr_t JTAGScanChainID;
    uintptr_t JTAGPositionID;
    const char *TargetHistory;

    if(ConWidgets->TargetID==NULL ||
            ConWidgets->ScriptFile==NULL ||
            ConWidgets->TargetInterface==NULL ||
            ConWidgets->TargetSpeed==NULL || ConWidgets->RTTCtrlBlock==NULL ||
            ConWidgets->SelectTarget==NULL ||
            ConWidgets->RTTCtrlBlockAuto==NULL ||
            ConWidgets->RTTCtrlBlockAddress==NULL ||
            ConWidgets->TargetJTAGChain==NULL ||
            ConWidgets->TargetJTAGPos==NULL ||
            ConWidgets->TargetJTAGIRPre==NULL ||
            ConWidgets->RTTAddress==NULL ||
            ConWidgets->RTTCtrlBlockSearch==NULL)
    {
        return;
    }

    TargetIDStr=g_SRTT_System->KVGetItem(Options,"TargetID");
    ScriptFileStr=g_SRTT_System->KVGetItem(Options,"ScriptFile");
    TargetInterfaceStr=g_SRTT_System->KVGetItem(Options,"TargetInterface");
    TargetSpeedStr=g_SRTT_System->KVGetItem(Options,"TargetSpeed");
    RTTCtrlBlockModeStr=g_SRTT_System->KVGetItem(Options,"RTTCtrlBlockMode");
    JTAGScanChainStr=g_SRTT_System->KVGetItem(Options,"JTAG_ScanChainMode");
    JTAGPositionStr=g_SRTT_System->KVGetItem(Options,"JTAG_Position");
    JTAGIRPreStr=g_SRTT_System->KVGetItem(Options,"JTAG_IRPre");
    RTTAddressStr=g_SRTT_System->KVGetItem(Options,"RTTAddress");

    if(TargetIDStr==NULL)
        TargetIDStr="";

    if(ScriptFileStr==NULL)
        ScriptFileStr="";

    if(TargetInterfaceStr==NULL)
        TargetInterfaceStr="1";

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

    if(RTTAddressStr==NULL)
        RTTAddressStr="";

    TargetInterfaceID=strtoll(TargetInterfaceStr,NULL,10);
    TargetSpeedID=strtoll(TargetSpeedStr,NULL,10);
    RTTCtrlBlockModeID=strtoll(RTTCtrlBlockModeStr,NULL,10);
    JTAGScanChainID=strtoll(JTAGScanChainStr,NULL,10);
    JTAGPositionID=strtoll(JTAGPositionStr,NULL,10);

    /* Add history */
    g_SRTT_UI->ClearComboBox(WidgetHandle,ConWidgets->TargetID->Ctrl);
    for(r=0;r<TARGETHISTORY_SIZE;r++)
    {
        sprintf(buff,"TargetIDHistory%d",r);
        TargetHistory=g_SRTT_System->KVGetItem(Options,buff);
        if(TargetHistory!=NULL)
        {
            g_SRTT_UI->AddItem2ComboBox(WidgetHandle,ConWidgets->TargetID->Ctrl,
                    TargetHistory,0);
        }
    }
    g_SRTT_UI->SetComboBoxText(WidgetHandle,ConWidgets->TargetID->Ctrl,
            TargetIDStr);

    g_SRTT_UI->SetTextInputText(WidgetHandle,ConWidgets->ScriptFile->Ctrl,
            ScriptFileStr);

    g_SRTT_UI->SetComboBoxSelectedEntry(WidgetHandle,ConWidgets->TargetInterface->Ctrl,TargetInterfaceID);
    g_SRTT_UI->SetComboBoxSelectedEntry(WidgetHandle,ConWidgets->TargetSpeed->Ctrl,TargetSpeedID);

    g_SRTT_UI->SetComboBoxSelectedEntry(WidgetHandle,
            ConWidgets->TargetJTAGChain->Ctrl,JTAGScanChainID);
    g_SRTT_UI->SetComboBoxSelectedEntry(WidgetHandle,
            ConWidgets->TargetJTAGPos->Ctrl,JTAGPositionID);
    g_SRTT_UI->SetTextInputText(WidgetHandle,ConWidgets->TargetJTAGIRPre->Ctrl,
            JTAGIRPreStr);

    switch(RTTCtrlBlockModeID)
    {
        case 0:
        default:
            g_SRTT_UI->SetRadioBttnChecked(WidgetHandle,
                    ConWidgets->RTTCtrlBlockAuto,true);
        break;
        case 1:
            g_SRTT_UI->SetRadioBttnChecked(WidgetHandle,
                    ConWidgets->RTTCtrlBlockAddress,true);
        break;
        case 2:
            g_SRTT_UI->SetRadioBttnChecked(WidgetHandle,
                    ConWidgets->RTTCtrlBlockSearch,true);
        break;
    }

    g_SRTT_UI->SetTextInputText(WidgetHandle,ConWidgets->RTTAddress->Ctrl,
            RTTAddressStr);

    SEGGER_RTT_RethinkWidgetsEnabled(ConWidgets);
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Convert_Options_To_URI
 *
 * SYNOPSIS:
 *    PG_BOOL SEGGER_RTT_Convert_Options_To_URI(const char *DeviceUniqueID,
 *          t_PIKVList *Options,char *URI,int MaxURILen);
 *
 * PARAMETERS:
 *    DeviceUniqueID [I] -- This is the unique ID for the device we are working
 *                          on.
 *    Options [I] -- The options for this connection.
 *    URI [O] -- A buffer to fill with the URI for this connection.
 *    MaxURILen [I] -- The size of the 'URI' buffer.
 *
 * FUNCTION:
 *    This function builds a URI for the device and options and fills it into
 *    a buffer.
 *
 *    This is in the format of:
 *      "RTT://1545011:0"
 *
 * RETURNS:
 *    true -- all ok
 *    false -- There was an error
 *
 * SEE ALSO:
 *    Convert_URI_To_Options
 ******************************************************************************/
PG_BOOL SEGGER_RTT_Convert_Options_To_URI(const char *DeviceUniqueID,
            t_PIKVList *Options,char *URI,unsigned int MaxURILen)
{
    const char *TargetIDStr;
    char buff[100];
    string BuildURI;
    char *TmpStr;
    uint32_t SerialNum;
    uint32_t USBAdd;

    try
    {
        TargetIDStr=g_SRTT_System->KVGetItem(Options,"TargetID");
        if(TargetIDStr==NULL)
            return false;

        SerialNum=strtol(DeviceUniqueID,&TmpStr,10);
        USBAdd=0;
        if(TmpStr!=NULL)
        {
            TmpStr++;
            USBAdd=strtol(TmpStr,NULL,10);
        }

        snprintf(buff,sizeof(buff),"%s://%d",SEGGER_RTT_URI_PREFIX,SerialNum);
        BuildURI=buff;

        if(USBAdd!=0)
        {
            sprintf(buff,":%d",USBAdd);
            BuildURI+=buff;
        }
        BuildURI+="/";
        BuildURI+=TargetIDStr;

        if(BuildURI.length()>MaxURILen-1)
            return false;

        strcpy(URI,BuildURI.c_str());
    }
    catch(...)
    {
        return false;
    }

    return true;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Convert_URI_To_Options
 *
 * SYNOPSIS:
 *    PG_BOOL SEGGER_RTT_Convert_URI_To_Options(const char *URI,t_PIKVList *Options,
 *          char *DeviceUniqueID,unsigned int MaxDeviceUniqueIDLen,
 *          PG_BOOL Update);
 *
 * PARAMETERS:
 *    URI [I] -- The URI to convert to a device ID and options.
 *    Options [O] -- The options for this new connection.  Any options
 *                   that don't come from the URI should be defaults.
 *    DeviceUniqueID [O] -- The unique ID for this device build from the 'URI'
 *    MaxDeviceUniqueIDLen [I] -- The max length of the buffer for
 *          'DeviceUniqueID'
 *    Update [I] -- If this is true then we are updating 'Options'.  If
 *                  false then you should default 'Options' before you
 *                  fill in the options.
 *
 * FUNCTION:
 *    This function converts a a URI string into a unique ID and options for
 *    a connection to be opened.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
PG_BOOL SEGGER_RTT_Convert_URI_To_Options(const char *URI,t_PIKVList *Options,
            char *DeviceUniqueID,unsigned int MaxDeviceUniqueIDLen,
            PG_BOOL Update)
{
    char buff[100];
    const char *Pos;
    const char *USBAddrStr;
    const char *SerialNumStr;
    const char *TargetStr;
    const char *TargetStrEnd;
    uint32_t SerialNum;
    uint32_t USBAddr;
    PG_BOOL RetValue;
    struct JLINKARM_EMU_CONNECT_INFO *Infos;
    int32_t Total;
    int32_t Entry;
    int ConType;
    string Target;

    RetValue=false;
    Infos=NULL;
    try
    {
        /* Make sure it starts with HTTP:// */
        if(strncasecmp(URI,SEGGER_RTT_URI_PREFIX "://",
                (sizeof(SEGGER_RTT_URI_PREFIX)-1)+2)!=0)   // +2 for '//'
        {
            return false;
        }

        /* Pull apart the URI */
        Pos=URI;
        Pos+=sizeof(SEGGER_RTT_URI_PREFIX)-1;  // -1 because of the \0
        Pos+=3;    // Slip ://

        SerialNumStr=Pos;
        USBAddrStr=NULL;
        TargetStr=NULL;
        while(*Pos!=0)
        {
            /* See if we have USBAddr */
            if(*Pos==':' && USBAddrStr==NULL)
            {
                /* Yep, note it */
                USBAddrStr=Pos+1;
            }
            if(*Pos=='/')
            {
                /* Start of target ID */
                TargetStr=Pos+1;
                break;
            }
            Pos++;
        }
        if(TargetStr==NULL || *TargetStr==0)
        {
            /* Error we need the target ID */
            return false;
        }

        /* Find the final / if there is one */
        Pos=TargetStr;
        while(*Pos!=0)
        {
            Pos++;
        }
        TargetStrEnd=Pos;

        Target.assign(TargetStr,TargetStrEnd-TargetStr);

        if(USBAddrStr==NULL)
            USBAddrStr="0";

        SerialNum=strtol(SerialNumStr,NULL,10);
        USBAddr=strtol(USBAddrStr,NULL,10);

        g_SRTT_System->KVAddItem(Options,"TargetID",Target.c_str());

        /* Build the Unique ID */

        /* We need extra info about this device (to get this info we have
           to look at all JLinks) */
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

        /* Find the JLink from the URI */
        for(Entry=0;Entry<Total;Entry++)
            if(Infos[Entry].SerialNumber==SerialNum)
                break;
        if(Entry==Total)
            throw(0);

        if(Infos[Entry].Connection==JLINKARM_HOSTIF_USB)
            ConType=0;
        else
            ConType=1;

        sprintf(buff,"%d:%d:%d",SerialNum,USBAddr,ConType);

        if(strlen(buff)+1>=MaxDeviceUniqueIDLen)
            throw(0);

        strcpy(DeviceUniqueID,buff);

        RetValue=true;
    }
    catch(...)
    {
        RetValue=false;
    }

    if(Infos!=NULL)
        free(Infos);
    Infos=NULL;

    return RetValue;
}

/*******************************************************************************
 * NAME:
 *    GetConnectionInfo
 *
 * SYNOPSIS:
 *    PG_BOOL GetConnectionInfo(const char *DeviceUniqueID,
 *              struct IODriverDetectedInfo *RetInfo);
 *
 * PARAMETERS:
 *    DeviceUniqueID [I] -- This is the unique ID for the device we are working
 *                          on.
 *    RetInfo [O] -- The structure to fill in with info about this device.
 *                   See DetectDevices() for a description of this structure.
 *                   The 'Next' must be set to NULL.
 *
 * FUNCTION:
 *    Get info about this connection.  This info is used at different times
 *    for different things in the system.
 *
 * RETURNS:
 *    true -- 'RetInfo' has been filled in.
 *    false -- There was an error in getting the info.
 *
 * SEE ALSO:
 *    DetectDevices()
 ******************************************************************************/
PG_BOOL SEGGER_RTT_GetConnectionInfo(const char *DeviceUniqueID,
        t_PIKVList *Options,struct IODriverDetectedInfo *RetInfo)
{
    const char *TargetIDStr;
    uint32_t SerialNum;

    SerialNum=strtol(DeviceUniqueID,NULL,10);
    if(strlen(DeviceUniqueID)+1>=sizeof(RetInfo->DeviceUniqueID))
        return false;

    /* Fill in defaults */
    RetInfo->Next=NULL;
    snprintf(RetInfo->Name,sizeof(RetInfo->Name),"RTT %d",SerialNum);
    RetInfo->Flags=0;
    strcpy(RetInfo->DeviceUniqueID,DeviceUniqueID);

    TargetIDStr=g_SRTT_System->KVGetItem(Options,"TargetID");
    if(TargetIDStr==NULL)
        TargetIDStr="";

    snprintf(RetInfo->Title,sizeof(RetInfo->Title),"RTT %d (%s)",SerialNum,
            TargetIDStr);

    return true;
}

static void SEGGER_RTT_RethinkWidgetsEnabled(struct SEGGER_RTT_ConWidgets *ConWidgets)
{
    bool TargetJTAGChainEnabled;
    bool TargetJTAGPosEnabled;
    bool TargetJTAGIRPreEnabled;
    int Selected;

    TargetJTAGChainEnabled=false;
    TargetJTAGPosEnabled=false;
    TargetJTAGIRPreEnabled=false;

    Selected=g_SRTT_UI->GetComboBoxSelectedEntry(ConWidgets->WidgetHandle,
            ConWidgets->TargetInterface->Ctrl);
    switch(Selected)
    {
        case JLINKARM_TIF_JTAG:
            TargetJTAGChainEnabled=true;
            switch(g_SRTT_UI->GetComboBoxSelectedEntry(ConWidgets->WidgetHandle,
                    ConWidgets->TargetJTAGChain->Ctrl))
            {
                case 0: // Auto
                    TargetJTAGPosEnabled=false;
                    TargetJTAGIRPreEnabled=false;
                break;
                case 1: // Simple
                    TargetJTAGPosEnabled=true;
                    TargetJTAGIRPreEnabled=true;
                break;
            }
        break;
        case JLINKARM_TIF_SWD:
            TargetJTAGChainEnabled=false;
            TargetJTAGPosEnabled=false;
            TargetJTAGIRPreEnabled=false;
        break;
        default:
        break;
    }
    g_SRTT_UI->EnableComboBox(ConWidgets->WidgetHandle,
            ConWidgets->TargetJTAGChain->Ctrl,TargetJTAGChainEnabled);
    g_SRTT_UI->EnableComboBox(ConWidgets->WidgetHandle,
            ConWidgets->TargetJTAGPos->Ctrl,TargetJTAGPosEnabled);
    g_SRTT_UI->EnableTextInput(ConWidgets->WidgetHandle,
            ConWidgets->TargetJTAGIRPre->Ctrl,TargetJTAGIRPreEnabled);

    if(g_SRTT_UI->IsRadioBttnChecked(ConWidgets->WidgetHandle,
            ConWidgets->RTTCtrlBlockAuto))
    {
        g_SRTT_UI->EnableTextInput(ConWidgets->WidgetHandle,
                ConWidgets->RTTAddress->Ctrl,false);
    }
    else
    {
        g_SRTT_UI->EnableTextInput(ConWidgets->WidgetHandle,
                ConWidgets->RTTAddress->Ctrl,true);
    }
}
