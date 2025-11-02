/*******************************************************************************
 * FILENAME: RxTxBinaryDisplay.cpp
 *
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    This has a binary display processor that highlights outgoing and incoming
 *    traffic in different colors.
 *
 * COPYRIGHT:
 *    Copyright 11 Jun 2025 Paul Hutchinson.
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
 *    Paul Hutchinson (11 Jun 2025)
 *
 ******************************************************************************/

/*** HEADER FILES TO INCLUDE  ***/
#include "RxTxBinaryDisplay.h"
#include "PluginSDK/Plugin.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*** DEFINES                  ***/
#define REGISTER_PLUGIN_FUNCTION_PRIV_NAME      RxTxBinaryDisplay // The name to append on the RegisterPlugin() function for built in version
#define NEEDED_MIN_API_VERSION                  0x01010000

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/
struct RxTxBinaryDisplay_TextStyle
{
    uint32_t FGColor;
    uint32_t BGColor;
    uint32_t Attribs;
};

struct RxTxBinaryDisplayData
{
    struct RxTxBinaryDisplay_TextStyle RxStyle;
    struct RxTxBinaryDisplay_TextStyle TxStyle;
};

struct SettingsStylingWidgetsSet
{
    struct PI_ColorPick *FgColor;
    struct PI_ColorPick *BgColor;
    struct PI_Checkbox *AttribUnderLine;
    struct PI_Checkbox *AttribOverLine;
    struct PI_Checkbox *AttribLineThrough;
    struct PI_Checkbox *AttribBold;
    struct PI_Checkbox *AttribItalic;
    struct PI_Checkbox *AttribOutLine;
};

struct SettingsWidgets
{
    t_WidgetSysHandle *RxTabHandle;
    t_WidgetSysHandle *TxTabHandle;

    struct SettingsStylingWidgetsSet RxStyles;
    struct SettingsStylingWidgetsSet TxStyles;
};

/*** FUNCTION PROTOTYPES      ***/
const struct DataProcessorInfo *RxTxBinaryDisplay_GetProcessorInfo(
        unsigned int *SizeOfInfo);
static void RxTxBinaryDisplay_ProcessIncomingBinaryByte(
        t_DataProcessorHandleType *DataHandle,const uint8_t Byte);
static void RxTxBinaryDisplay_ProcessOutGoingData(t_DataProcessorHandleType *DataHandle,
        const uint8_t *Data,int Bytes);
static t_DataProSettingsWidgetsType *RxTxBinaryDisplay_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings);
static void RxTxBinaryDisplay_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData);
static void RxTxBinaryDisplay_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings);
static void RxTxBinaryDisplay_AddSettingStyleWidgets(
        struct SettingsStylingWidgetsSet *Widgets,
        t_WidgetSysHandle *SysHandle);
static void RxTxBinaryDisplay_FreeSettingStyleWidgets(
        struct SettingsStylingWidgetsSet *Widgets,
        t_WidgetSysHandle *SysHandle);
static void RxTxBinaryDisplay_SetSettingStyleWidgets(t_PIKVList *Settings,
        struct SettingsStylingWidgetsSet *Widgets,t_WidgetSysHandle *SysHandle,
        const char *Prefix,uint32_t DefaultFG,uint32_t DefaultBG);
static uint32_t RxTxBinaryDisplay_GrabSettingKV(t_PIKVList *Settings,
        const char *Prefix,const char *Key,uint32_t DefaultValue,int Base);
static void RxTxBinaryDisplay_SetSettingKV(t_PIKVList *Settings,
        const char *Prefix,const char *Key,uint32_t Value,int Base);
static void RxTxBinaryDisplay_UpdateSettingFromStyleWidgets(t_PIKVList *Settings,
        struct SettingsStylingWidgetsSet *Widgets,t_WidgetSysHandle *SysHandle,
        const char *Prefix);
static void RxTxBinaryDisplay_ApplySettings(t_DataProcessorHandleType *ConDataHandle,
        t_PIKVList *Settings);
static t_DataProcessorHandleType *RxTxBinaryDisplay_AllocateData(void);
static void RxTxBinaryDisplay_FreeData(t_DataProcessorHandleType *DataHandle);
static void RxTxBinaryDisplay_ApplySetting_SetData(t_PIKVList *Settings,
        struct RxTxBinaryDisplay_TextStyle *Style,const char *Prefix,
        uint32_t DefaultFG,uint32_t DefaultBG);

/*** VARIABLE DEFINITIONS     ***/
struct DataProcessorAPI m_RxTxBinaryDisplayCBs=
{
    RxTxBinaryDisplay_AllocateData,
    RxTxBinaryDisplay_FreeData,
    RxTxBinaryDisplay_GetProcessorInfo,     // GetProcessorInfo
    NULL,                                   // ProcessKeyPress
    NULL,                                   // ProcessIncomingTextByte
    RxTxBinaryDisplay_ProcessIncomingBinaryByte,    // ProcessIncomingBinaryByte
    /* V2 */
    RxTxBinaryDisplay_ProcessOutGoingData,
    RxTxBinaryDisplay_AllocSettingsWidgets,
    RxTxBinaryDisplay_FreeSettingsWidgets,
    RxTxBinaryDisplay_SetSettingsFromWidgets,
    RxTxBinaryDisplay_ApplySettings,
};

struct DataProcessorInfo m_RxTxBinaryDisplay_Info=
{
    "RX/TX Binary Display Processor",
    "Binary Display Processor that highlights RX/TX traffic in different colors",
    "Highlights outgoing and incoming traffic in different colors",
    e_DataProcessorType_Binary,
    {
        .BinClass=e_BinaryDataProcessorClass_Decoder,
    },
    e_BinaryDataProcessorMode_Hex
};

static const struct PI_SystemAPI *m_RTBD_SysAPI;
static const struct DPS_API *m_RTBD_DPS;
static const struct PI_UIAPI *m_RTBD_UIAPI;

/*******************************************************************************
 * NAME:
 *    RxTxBinaryDisplay_RegisterPlugin
 *
 * SYNOPSIS:
 *    unsigned int RxTxBinaryDisplay_RegisterPlugin(const struct PI_SystemAPI *SysAPI,
 *          unsigned int Version);
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

        m_RTBD_SysAPI=SysAPI;
        m_RTBD_DPS=SysAPI->GetAPI_DataProcessors();
        m_RTBD_UIAPI=m_RTBD_DPS->GetAPI_UI();

        /* If we are have the correct experimental API */
        if(SysAPI->GetExperimentalID()>0 &&
                SysAPI->GetExperimentalID()<1)
        {
            return 0xFFFFFFFF;
        }

        m_RTBD_DPS->RegisterDataProcessor("RxTxBinaryDisplay",
                &m_RxTxBinaryDisplayCBs,sizeof(m_RxTxBinaryDisplayCBs));

        return 0;
    }
}

/*******************************************************************************
 * NAME:
 *    AllocateData
 *
 * SYNOPSIS:
 *    t_DataProcessorHandleType *AllocateData(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    This function allocates any needed data for this data processor.
 *
 * NOTES:
 *    You can not use most of the API 'DPS_API' because there is no connection
 *    when AllocateData() is called.
 *
 * RETURNS:
 *   A pointer to the data, NULL if there was an error.
 ******************************************************************************/
static t_DataProcessorHandleType *RxTxBinaryDisplay_AllocateData(void)
{
    struct RxTxBinaryDisplayData *Data;

    Data=(struct RxTxBinaryDisplayData *)malloc(sizeof(struct RxTxBinaryDisplayData));
    if(Data==NULL)
        return NULL;

    /* Set some defaults, these will be overriden by
       RxTxBinaryDisplay_ApplySettings() */
    Data->RxStyle.FGColor=0xFFFFFF;
    Data->RxStyle.BGColor=0x0000FF;
    Data->RxStyle.Attribs=0;

    Data->TxStyle.FGColor=0xFFFFFF;
    Data->TxStyle.BGColor=0xFF0000;
    Data->TxStyle.Attribs=0;

    return (t_DataProcessorHandleType *)Data;
}

/*******************************************************************************
 * NAME:
 *    FreeData
 *
 * SYNOPSIS:
 *    void FreeData(t_DataProcessorHandleType *DataHandle);
 *
 * PARAMETERS:
 *    DataHandle [I] -- The data handle to free.  This will need to be
 *                      case to your internal data type before you use it.
 *
 * FUNCTION:
 *    This function frees the memory allocated with AllocateData().
 *
 * RETURNS:
 *    NONE
 ******************************************************************************/
static void RxTxBinaryDisplay_FreeData(t_DataProcessorHandleType *DataHandle)
{
    struct RxTxBinaryDisplayData *Data=(struct RxTxBinaryDisplayData *)DataHandle;

    free(Data);
}

/*******************************************************************************
 * NAME:
 *    RxTxBinaryDisplay_GetProcessorInfo
 *
 * SYNOPSIS:
 *    const struct DataProcessorInfo *RxTxBinaryDisplay_GetProcessorInfo(
 *              unsigned int *SizeOfInfo);
 *
 * PARAMETERS:
 *    SizeOfInfo [O] -- The size of 'struct DataProcessorInfo'.  This is used
 *                        for forward / backward compatibility.
 *
 * FUNCTION:
 *    This function gets info about the plugin.  'DataProcessorInfo' has
 *    the following fields:
 *          DisplayName -- The name we show the user
 *          Tip -- A tool tip (for when you hover the mouse over this plugin)
 *          Help -- A help string for this plugin.
 *          ProType -- The type of process.  Supported values:
 *                  e_DataProcessorType_Text -- This is a text processor.
 *                      This is the more clasic type of processor (like VT100).
 *                  e_DataProcessorType_Binary -- This is a binary processor.
 *                      These are processors for binary protocol.  This may
 *                      be something as simple as a hex dump.
 *          TxtClass -- This only applies to 'e_DataProcessorType_Text' type
 *              processors. This is what class of text processor is
 *              this.  Supported classes:
 *                      e_TextDataProcessorClass_Other -- This is a generic class
 *                          more than one of these processors can be active
 *                          at a time but no other requirements exist.
 *                      e_TextDataProcessorClass_CharEncoding -- This is a
 *                          class that converts the raw stream into some kind
 *                          of char encoding.  For example unicode is converted
 *                          from a number of bytes to chars in the system.
 *                      e_TextDataProcessorClass_TermEmulation -- This is a
 *                          type of terminal emulator.  An example of a
 *                          terminal emulator is VT100.
 *                      e_TextDataProcessorClass_Highlighter -- This is a processor
 *                          that highlights strings as they come in the input
 *                          stream.  For example a processor that underlines
 *                          URL's.
 *                      e_TextDataProcessorClass_Logger -- This is a processor
 *                          that saves the input.  It may save to a file or
 *                          send out a debugging service.  And example is
 *                          a processor that saves all the raw bytes to a file.
 * SEE ALSO:
 *    
 ******************************************************************************/
const struct DataProcessorInfo *RxTxBinaryDisplay_GetProcessorInfo(
        unsigned int *SizeOfInfo)
{
    *SizeOfInfo=sizeof(struct DataProcessorInfo);
    return &m_RxTxBinaryDisplay_Info;
}

/*******************************************************************************
 * NAME:
 *    ProcessIncomingBinaryByte
 *
 * SYNOPSIS:
 *    void ProcessIncomingBinaryByte(t_DataProcessorHandleType *DataHandle,
 *          const uint8_t Byte);
 *
 * PARAMETERS:
 *      DataHandle [I] -- The data handle to work on.  This is your internal
 *                        data.
 *      Byte [I] -- This is the byte that came in.
 *
 * FUNCTION:
 *      This function is called for each byte that comes in if you are a
 *      'e_DataProcessorType_Binary' type of processor.
 *
 *      You process this byte and call one of the add to screen functions (or
 *      all of them if you like).  See BinaryAddText()
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    BinaryAddText()
 ******************************************************************************/
static void RxTxBinaryDisplay_ProcessIncomingBinaryByte(
        t_DataProcessorHandleType *DataHandle,const uint8_t Byte)
{
    struct RxTxBinaryDisplayData *Data=(struct RxTxBinaryDisplayData *)DataHandle;

    m_RTBD_DPS->SetFGColor(Data->RxStyle.FGColor);
    m_RTBD_DPS->SetULineColor(Data->RxStyle.FGColor);
    m_RTBD_DPS->SetBGColor(Data->RxStyle.BGColor);
    m_RTBD_DPS->SetAttribs(Data->RxStyle.Attribs);

    m_RTBD_DPS->BinaryAddHex(Byte);
}

/*******************************************************************************
 * NAME:
 *    ProcessOutGoingData
 *
 * SYNOPSIS:
 *    void ProcessOutGoingData(t_DataProcessorHandleType *DataHandle,
 *          const uint8_t *Data,int Bytes);
 *
 * PARAMETERS:
 *      DataHandle [I] -- The data handle to work on.  This is your internal
 *                        data.
 *      Data [I] -- This is the block of data that is about to be sent
 *      Bytes [I] -- The number of bytes in the 'Data' block.
 *
 * FUNCTION:
 *      This function is called for block of data being send to a IO driver.
 *      This is in addition to ProcessKeyPress(), this is called for every
 *      byte being sent, where as ProcessKeyPress() is only called for
 *      key presses.  You will be called 2 times for key presses, one for
 *      ProcessKeyPress(), and then this function.  For things like Send Buffers
 *      no ProcessKeyPress() will be called only this function.
 *      So if you want to see all bytes going out use this function.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    ProcessKeyPress()
 ******************************************************************************/
static void RxTxBinaryDisplay_ProcessOutGoingData(t_DataProcessorHandleType *DataHandle,
        const uint8_t *TxData,int Bytes)
{
    struct RxTxBinaryDisplayData *Data=(struct RxTxBinaryDisplayData *)DataHandle;
    int r;

    m_RTBD_DPS->SetFGColor(Data->TxStyle.FGColor);
    m_RTBD_DPS->SetULineColor(Data->TxStyle.FGColor);
    m_RTBD_DPS->SetBGColor(Data->TxStyle.BGColor);
    m_RTBD_DPS->SetAttribs(Data->TxStyle.Attribs);

    for(r=0;r<Bytes;r++)
        m_RTBD_DPS->BinaryAddHex(TxData[r]);
}

/*******************************************************************************
 * NAME:
 *    AllocSettingsWidgets
 *
 * SYNOPSIS:
 *    t_DataProSettingsWidgetsType *AllocSettingsWidgets(
 *              t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings);
 *
 * PARAMETERS:
 *    WidgetHandle [I] -- The handle to add new widgets to
 *    Settings [I] -- The current settings.  This is a standard key/value
 *                    list.
 *
 * FUNCTION:
 *    This function is called when the user presses the "Settings" button
 *    to change any settings for this plugin (in the settings dialog).  If
 *    this is NULL then the user can not press the settings button.
 *
 *    When the plugin settings dialog is open it will have a tab control in
 *    it with a "Generic" tab opened.  Your widgets will be added to this
 *    tab.  If you want to add a new tab use can call the DPS_API
 *    function AddNewSettingsTab().  If you want to change the name of the
 *    first tab call SetCurrentSettingsTabName() before you add a new tab.
 *
 * RETURNS:
 *    The private settings data that you want to use.  This is a private
 *    structure that you allocate and then cast to
 *    (t_DataProSettingsWidgetsType *) when you return.  It's up to you what
 *    you want to do with this data (if you do not want to use it return
 *    a fixed int set to 1, and ignore it in FreeSettingsWidgets.  If you
 *    return NULL it is considered an error.
 *
 * SEE ALSO:
 *    FreeSettingsWidgets(), SetCurrentSettingsTabName(), AddNewSettingsTab()
 ******************************************************************************/
t_DataProSettingsWidgetsType *RxTxBinaryDisplay_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings)
{
    struct SettingsWidgets *WData;

    try
    {
        WData=new SettingsWidgets;

        m_RTBD_DPS->SetCurrentSettingsTabName("Incoming (Rx)");
        WData->RxTabHandle=WidgetHandle;
        RxTxBinaryDisplay_AddSettingStyleWidgets(&WData->RxStyles,
                WData->RxTabHandle);

        WData->TxTabHandle=m_RTBD_DPS->AddNewSettingsTab("Outgoing (Tx)");
        if(WData->TxTabHandle==NULL)
            throw(0);
        RxTxBinaryDisplay_AddSettingStyleWidgets(&WData->TxStyles,
                WData->TxTabHandle);

        /* Set UI to settings values */
        RxTxBinaryDisplay_SetSettingStyleWidgets(Settings,&WData->RxStyles,
                WData->RxTabHandle,"Rx",
                m_RTBD_DPS->GetSysDefaultColor(e_DefaultColors_FG),
                m_RTBD_DPS->GetSysColor(e_SysColShade_Normal,e_SysCol_Blue));
        RxTxBinaryDisplay_SetSettingStyleWidgets(Settings,&WData->TxStyles,
                WData->TxTabHandle,"Tx",
                m_RTBD_DPS->GetSysDefaultColor(e_DefaultColors_FG),
                m_RTBD_DPS->GetSysColor(e_SysColShade_Normal,e_SysCol_Red));
    }
    catch(...)
    {
        if(WData!=NULL)
        {
            RxTxBinaryDisplay_FreeSettingsWidgets(
                    (t_DataProSettingsWidgetsType *)WData);
        }
        return NULL;
    }

    return (t_DataProSettingsWidgetsType *)WData;
}

/*******************************************************************************
 * NAME:
 *    FreeSettingsWidgets
 *
 * SYNOPSIS:
 *    void FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData);
 *
 * PARAMETERS:
 *    PrivData [I] -- The private data to free
 *
 * FUNCTION:
 *      This function is called when the system frees the settings widets.
 *      It should free any private data you allocated in AllocSettingsWidgets().
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    AllocSettingsWidgets()
 ******************************************************************************/
void RxTxBinaryDisplay_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData)
{
    struct SettingsWidgets *WData=(struct SettingsWidgets *)PrivData;

    RxTxBinaryDisplay_FreeSettingStyleWidgets(&WData->RxStyles,WData->RxTabHandle);
    RxTxBinaryDisplay_FreeSettingStyleWidgets(&WData->TxStyles,WData->TxTabHandle);

    delete WData;
}

/*******************************************************************************
 * NAME:
 *    SetSettingsFromWidgets
 *
 * SYNOPSIS:
 *    void SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,
 *              t_PIKVList *Settings);
 *
 * PARAMETERS:
 *    PrivData [I] -- Your private data allocated in AllocSettingsWidgets()
 *    Settings [O] -- This is where you store the settings.
 *
 * FUNCTION:
 *    This function takes the widgets added with AllocSettingsWidgets() and
 *    stores them is a key/value pair list in 'Settings'.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    AllocSettingsWidgets()
 ******************************************************************************/
void RxTxBinaryDisplay_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings)
{
    struct SettingsWidgets *WData=(struct SettingsWidgets *)PrivData;

    RxTxBinaryDisplay_UpdateSettingFromStyleWidgets(Settings,
        &WData->RxStyles,WData->RxTabHandle,"Rx");
    RxTxBinaryDisplay_UpdateSettingFromStyleWidgets(Settings,
        &WData->TxStyles,WData->TxTabHandle,"Tx");
}

/*******************************************************************************
 * NAME:
 *    ApplySettings
 *
 * SYNOPSIS:
 *    void ApplySettings(t_DataProcessorHandleType *DataHandle,
 *              t_PIKVList *Settings);
 *
 * PARAMETERS:
 *    DataHandle [I] -- The data handle to work on.  This is your internal
 *                      data.
 *    Settings [I] -- This is where you get your settings from.
 *
 * FUNCTION:
 *    This function takes the settings from 'Settings' and setups the
 *    plugin to use them when new bytes come in or out.  It will normally
 *    copy the settings from key/value pairs to internal data structures.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void RxTxBinaryDisplay_ApplySettings(t_DataProcessorHandleType *DataHandle,
        t_PIKVList *Settings)
{
    struct RxTxBinaryDisplayData *Data=(struct RxTxBinaryDisplayData *)DataHandle;

    RxTxBinaryDisplay_ApplySetting_SetData(Settings,&Data->RxStyle,"Rx",
            m_RTBD_DPS->GetSysDefaultColor(e_DefaultColors_FG),
            m_RTBD_DPS->GetSysColor(e_SysColShade_Normal,e_SysCol_Blue));
    RxTxBinaryDisplay_ApplySetting_SetData(Settings,&Data->TxStyle,"Tx",
            m_RTBD_DPS->GetSysDefaultColor(e_DefaultColors_FG),
            m_RTBD_DPS->GetSysColor(e_SysColShade_Normal,e_SysCol_Red));
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * NAME:
 *    RxTxBinaryDisplay_AddSettingStyleWidgets
 *
 * SYNOPSIS:
 *    static void RxTxBinaryDisplay_AddSettingStyleWidgets(
 *          struct SettingsStylingWidgetsSet *Widgets,
 *          t_WidgetSysHandle *SysHandle);
 *
 * PARAMETERS:
 *    Widgets [O] -- Where to store the pointers to the widgets
 *    SysHandle [I] -- The widget handle to use when adding the widgets.
 *
 * FUNCTION:
 *    This is a helper function that allocates a set of styling widgets.
 *
 * RETURNS:
 *    NONE
 *
 * NOTES:
 *    Will throw(0) if there is an error.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void RxTxBinaryDisplay_AddSettingStyleWidgets(
        struct SettingsStylingWidgetsSet *Widgets,
        t_WidgetSysHandle *SysHandle)
{
    Widgets->FgColor=m_RTBD_UIAPI->AddColorPick(SysHandle,"Forground Color",0x000000,NULL,NULL);
    if(Widgets->FgColor==NULL)
        throw(0);

    Widgets->BgColor=m_RTBD_UIAPI->AddColorPick(SysHandle,"Background Color",0x000000,NULL,NULL);
    if(Widgets->BgColor==NULL)
        throw(0);

    Widgets->AttribUnderLine=m_RTBD_UIAPI->AddCheckbox(SysHandle,"Underline",NULL,NULL);
    if(Widgets->AttribUnderLine==NULL)
        throw(0);

    Widgets->AttribOverLine=m_RTBD_UIAPI->AddCheckbox(SysHandle,"Overline",NULL,NULL);
    if(Widgets->AttribOverLine==NULL)
        throw(0);

    Widgets->AttribLineThrough=m_RTBD_UIAPI->AddCheckbox(SysHandle,"Line through",NULL,NULL);
    if(Widgets->AttribLineThrough==NULL)
        throw(0);

    Widgets->AttribBold=m_RTBD_UIAPI->AddCheckbox(SysHandle,"Bold",NULL,NULL);
    if(Widgets->AttribBold==NULL)
        throw(0);

    Widgets->AttribItalic=m_RTBD_UIAPI->AddCheckbox(SysHandle,"Italic",NULL,NULL);
    if(Widgets->AttribItalic==NULL)
        throw(0);

    Widgets->AttribOutLine=m_RTBD_UIAPI->AddCheckbox(SysHandle,"Outline",NULL,NULL);
    if(Widgets->AttribOutLine==NULL)
        throw(0);
}

/*******************************************************************************
 * NAME:
 *    RxTxBinaryDisplay_FreeSettingStyleWidgets
 *
 * SYNOPSIS:
 *    static void RxTxBinaryDisplay_FreeSettingStyleWidgets(
 *          struct SettingsStylingWidgetsSet *Widgets,
 *          t_WidgetSysHandle *SysHandle);
 *
 * PARAMETERS:
 *    Widgets [I] -- The widgets style set to free
 *    SysHandle [I] -- The widget system handle use to allocate these widgets.
 *
 * FUNCTION:
 *    This function frees the widgets that was allocated with
 *    RxTxBinaryDisplay_AddSettingStyleWidgets()
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    RxTxBinaryDisplay_AddSettingStyleWidgets()
 ******************************************************************************/
static void RxTxBinaryDisplay_FreeSettingStyleWidgets(
        struct SettingsStylingWidgetsSet *Widgets,
        t_WidgetSysHandle *SysHandle)
{
    if(Widgets->AttribOutLine!=NULL)
        m_RTBD_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribOutLine);
    if(Widgets->AttribItalic!=NULL)
        m_RTBD_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribItalic);
    if(Widgets->AttribBold!=NULL)
        m_RTBD_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribBold);
    if(Widgets->AttribLineThrough!=NULL)
        m_RTBD_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribLineThrough);
    if(Widgets->AttribOverLine!=NULL)
        m_RTBD_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribOverLine);
    if(Widgets->AttribUnderLine!=NULL)
        m_RTBD_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribUnderLine);

    if(Widgets->BgColor!=NULL)
        m_RTBD_UIAPI->FreeColorPick(SysHandle,Widgets->BgColor);
    if(Widgets->FgColor!=NULL)
        m_RTBD_UIAPI->FreeColorPick(SysHandle,Widgets->FgColor);
}

/*******************************************************************************
 * NAME:
 *    RxTxBinaryDisplay_SetSettingStyleWidgets
 *
 * SYNOPSIS:
 *    static void RxTxBinaryDisplay_SetSettingStyleWidgets(t_PIKVList *Settings,
 *          struct SettingsStylingWidgetsSet *Widgets,
 *          t_WidgetSysHandle *SysHandle,const char *Prefix,uint32_t DefaultFG,
 *          uint32_t DefaultBG)
 *
 * PARAMETERS:
 *    Settings [I] -- The settings to apply
 *    Widgets [I] -- The widget set to set to the settings
 *    SysHandle [I] -- The widget system handle that these widgets where added
 *                     with.
 *    Prefix [I] -- The prefix for the styling names in the settings.  This
 *                  function will take this string and add it to the start
 *                  of the setting KV name before trying to read it from
 *                  settings.
 *    DefaultFG [I] -- The default color for the FG if there isn't one found
 *                     in settings.
 *    DefaultBG [I] -- The default color for the BG if there isn't one found
 *                     in settings.
 *
 * FUNCTION:
 *    This is a helper function that reads the styling widgets out of settings
 *    and applies it to the widgets.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void RxTxBinaryDisplay_SetSettingStyleWidgets(t_PIKVList *Settings,
        struct SettingsStylingWidgetsSet *Widgets,t_WidgetSysHandle *SysHandle,
        const char *Prefix,uint32_t DefaultFG,uint32_t DefaultBG)
{
    uint32_t Num;

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"FGColor",DefaultFG,16);
    m_RTBD_UIAPI->SetColorPickValue(SysHandle,Widgets->FgColor->Ctrl,Num);

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"BGColor",DefaultBG,16);
    m_RTBD_UIAPI->SetColorPickValue(SysHandle,Widgets->BgColor->Ctrl,Num);

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribUnderLine",0,10);
    m_RTBD_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribUnderLine->Ctrl,Num);

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribOverLine",0,10);
    m_RTBD_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribOverLine->Ctrl,Num);

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribLineThrough",0,10);
    m_RTBD_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribLineThrough->Ctrl,Num);

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribBold",0,10);
    m_RTBD_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribBold->Ctrl,Num);

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribItalic",0,10);
    m_RTBD_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribItalic->Ctrl,Num);

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribOutLine",0,10);
    m_RTBD_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribOutLine->Ctrl,Num);
}

/*******************************************************************************
 * NAME:
 *    RxTxBinaryDisplay_GrabSettingKV
 *
 * SYNOPSIS:
 *    static uint32_t RxTxBinaryDisplay_GrabSettingKV(t_PIKVList *Settings,
 *          const char *Prefix,const char *Key,uint32_t DefaultValue,int Base);
 *
 * PARAMETERS:
 *    Settings [I] -- The settings to apply
 *    Prefix [I] -- The prefix for the styling names in the settings.  This
 *                  function will take this string and add it to the start
 *                  of the setting KV name before trying to read it from
 *                  settings.
 *    Key [I] -- The key to look for in the settings
 *    DefaultValue [I] -- What value to apply if it wasn't found
 *    Base [I] -- What number base is the setting value stored as (sent into
 *                strtol()).
 *
 * FUNCTION:
 *    This is a helper function for RxTxBinaryDisplay_SetSettingStyleWidgets()
 *    that tries to find a setting and if it's not found applies a default
 *    value.
 *
 * RETURNS:
 *    The value that was in settings or 'DefaultValue' if it was not found.
 *
 * SEE ALSO:
 *    RxTxBinaryDisplay_SetSettingStyleWidgets()
 ******************************************************************************/
static uint32_t RxTxBinaryDisplay_GrabSettingKV(t_PIKVList *Settings,
        const char *Prefix,const char *Key,uint32_t DefaultValue,int Base)
{
    char buff[100];
    const char *Str;
    uint32_t Number;

    sprintf(buff,"%s_%s",Prefix,Key);
    Str=m_RTBD_SysAPI->KVGetItem(Settings,buff);
    if(Str!=NULL)
        Number=strtol(Str,NULL,Base);
    else
        Number=DefaultValue;

    return Number;
}

/*******************************************************************************
 * NAME:
 *    RxTxBinaryDisplay_UpdateSettingFromStyleWidgets
 *
 * SYNOPSIS:
 *    static void RxTxBinaryDisplay_UpdateSettingFromStyleWidgets(
 *          t_PIKVList *Settings,struct SettingsStylingWidgetsSet *Widgets,
 *          t_WidgetSysHandle *SysHandle,const char *Prefix);
 *
 * PARAMETERS:
 *    Settings [O] -- The settings to set the data in
 *    Widgets [I] -- The widgets to read the data from
 *    SysHandle [I] -- The widget system handle that these widgets where added
 *                     with.
 *    Prefix [I] -- The prefix for the styling names in the settings.  This
 *                  function will take this string and add it to the start
 *                  of the setting KV name before trying to read it from
 *                  settings.
 *
 * FUNCTION:
 *    This function takes and reads the values from the widgets and put them
 *    in 'Settings'.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void RxTxBinaryDisplay_UpdateSettingFromStyleWidgets(t_PIKVList *Settings,
        struct SettingsStylingWidgetsSet *Widgets,t_WidgetSysHandle *SysHandle,
        const char *Prefix)
{
    RxTxBinaryDisplay_SetSettingKV(Settings,Prefix,"FGColor",
            m_RTBD_UIAPI->GetColorPickValue(SysHandle,Widgets->FgColor->Ctrl),
            16);

    RxTxBinaryDisplay_SetSettingKV(Settings,Prefix,"BGColor",
            m_RTBD_UIAPI->GetColorPickValue(SysHandle,Widgets->BgColor->Ctrl),
            16);

    RxTxBinaryDisplay_SetSettingKV(Settings,Prefix,"AttribUnderLine",
            m_RTBD_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribUnderLine->Ctrl),16);

    RxTxBinaryDisplay_SetSettingKV(Settings,Prefix,"AttribOverLine",
            m_RTBD_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribOverLine->Ctrl),16);

    RxTxBinaryDisplay_SetSettingKV(Settings,Prefix,"AttribLineThrough",
            m_RTBD_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribLineThrough->Ctrl),16);

    RxTxBinaryDisplay_SetSettingKV(Settings,Prefix,"AttribBold",
            m_RTBD_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribBold->Ctrl),16);

    RxTxBinaryDisplay_SetSettingKV(Settings,Prefix,"AttribItalic",
            m_RTBD_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribItalic->Ctrl),16);

    RxTxBinaryDisplay_SetSettingKV(Settings,Prefix,"AttribOutLine",
            m_RTBD_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribOutLine->Ctrl),16);
}

/*******************************************************************************
 * NAME:
 *    RxTxBinaryDisplay_SetSettingKV
 *
 * SYNOPSIS:
 *    static void RxTxBinaryDisplay_SetSettingKV(t_PIKVList *Settings,
 *              const char *Prefix,const char *Key,uint32_t Value,int Base);
 *
 * PARAMETERS:
 *    Settings [O] -- The settings to set the data in
 *    Prefix [I] -- The prefix for the styling names in the settings.  This
 *                  function will take this string and add it to the start
 *                  of the setting KV name before trying to read it from
 *                  settings.
 *    Key [I] -- The key to look for in the settings
 *    Value [I] -- The value to store
 *    Base [I] -- What number base is the setting value stored as.
 *
 * FUNCTION:
 *    This is a helper function for
 *    RxTxBinaryDisplay_UpdateSettingFromStyleWidgets() that sets the value in
 *    'Settings'.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    RxTxBinaryDisplay_UpdateSettingFromStyleWidgets()
 ******************************************************************************/
static void RxTxBinaryDisplay_SetSettingKV(t_PIKVList *Settings,
        const char *Prefix,const char *Key,uint32_t Value,int Base)
{
    char buff[100];
    char buff2[100];

    sprintf(buff,"%s_%s",Prefix,Key);
    if(Base==10)
        sprintf(buff2,"%d",Value);
    else
        sprintf(buff2,"%06X",Value);
    m_RTBD_SysAPI->KVAddItem(Settings,buff,buff2);
}

/*******************************************************************************
 * NAME:
 *    RxTxBinaryDisplay_ApplySetting_SetData
 *
 * SYNOPSIS:
 *    static void RxTxBinaryDisplay_ApplySetting_SetData(t_PIKVList *Settings,
 *              struct TextLineHighlighter_TextStyle *Style,const char *Prefix,
 *              uint32_t DefaultFG,uint32_t DefaultBG);
 *
 * PARAMETERS:
 *    Settings [I] -- The settings to read out
 *    Style [O] -- The style to be set to what is in 'Settings'
 *    Prefix [I] -- The prefix for the styling names in the settings.  This
 *                  function will take this string and add it to the start
 *                  of the setting KV name before trying to read it from
 *                  settings.
 *    DefaultFG [I] -- The default color for the FG if there isn't one found
 *                     in settings.
 *    DefaultBG [I] -- The default color for the BG if there isn't one found
 *                     in settings.
 *
 * FUNCTION:
 *    This function takes the settings in 'Settings' copies / converts then
 *    to the styling in 'Style'.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void RxTxBinaryDisplay_ApplySetting_SetData(t_PIKVList *Settings,
        struct RxTxBinaryDisplay_TextStyle *Style,const char *Prefix,
        uint32_t DefaultFG,uint32_t DefaultBG)
{
    uint32_t Num;

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"FGColor",DefaultFG,16);
    Style->FGColor=Num;

    Num=RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"BGColor",DefaultBG,16);
    Style->BGColor=Num;

    Style->Attribs=0;
    if(RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribUnderLine",0,10))
        Style->Attribs|=TXT_ATTRIB_UNDERLINE;

    if(RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribOverLine",0,10))
        Style->Attribs|=TXT_ATTRIB_OVERLINE;

    if(RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribLineThrough",0,10))
        Style->Attribs|=TXT_ATTRIB_LINETHROUGH;

    if(RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribBold",0,10))
        Style->Attribs|=TXT_ATTRIB_BOLD;

    if(RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribItalic",0,10))
        Style->Attribs|=TXT_ATTRIB_ITALIC;

    if(RxTxBinaryDisplay_GrabSettingKV(Settings,Prefix,"AttribOutLine",0,10))
        Style->Attribs|=TXT_ATTRIB_OUTLINE;
}
