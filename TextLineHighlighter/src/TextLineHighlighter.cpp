/*******************************************************************************
 * FILENAME: TextLineHighlighter.cpp
 *
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    This has a display processor that highlights incoming messages.
 *    It is line based.
 *
 * COPYRIGHT:
 *    Copyright 01 Aug 2025 Paul Hutchinson.
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
 *    Paul Hutchinson (01 Aug 2025)
 *
 ******************************************************************************/

/*** HEADER FILES TO INCLUDE  ***/
#include "TextLineHighlighter.h"
#include "PluginSDK/Plugin.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <regex>

using namespace std;

/*** DEFINES                  ***/
#define REGISTER_PLUGIN_FUNCTION_PRIV_NAME      TextLineHighlighter // The name to append on the RegisterPlugin() function for built in version
#define NEEDED_MIN_API_VERSION                  0x02000000

#define NUM_OF_REGEXS               5
#define NUM_OF_SIMPLE               3
#define NUM_OF_STYLES               (NUM_OF_REGEXS+NUM_OF_SIMPLE)

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/
struct TextLineHighlighter_TextStyle
{
    uint32_t FGColor;
    uint32_t BGColor;
    uint32_t Attribs;
};

struct TextLineHighlighterSimpleData
{
    string StartsWith;
    string Contains;
    string EndsWith;
    int StyleIndex;
};

struct TextLineHighlighterRegexData
{
    string Pattern;
    int StyleIndex;
};

struct TextLineHighlighterData
{
    t_DataProMark *StartOfLineMarker;

    struct TextLineHighlighterSimpleData Simple[NUM_OF_SIMPLE];
    struct TextLineHighlighterRegexData Regex[NUM_OF_REGEXS];
    struct TextLineHighlighter_TextStyle Styles[NUM_OF_STYLES];

    bool GrabNewMark;
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

struct TextLineHighlighter_RegexWidgets
{
    struct PI_ComboBox *StyleList;
    struct PI_GroupBox *GroupBox;
    struct PI_TextInput *RegexWid;
};

struct TextLineHighlighter_SimpleWidgets
{
    struct PI_ComboBox *StyleList;
    struct PI_GroupBox *GroupBox;
    struct PI_TextInput *StartsWith;
    struct PI_TextInput *Contains;
    struct PI_TextInput *EndsWith;
};

struct TextLineHighlighter_SettingsWidgets
{
    t_WidgetSysHandle *SimpleTabHandle;
    t_WidgetSysHandle *RegexTabHandle;

    struct TextLineHighlighter_RegexWidgets Regex[NUM_OF_REGEXS];

    struct TextLineHighlighter_SimpleWidgets Simple[NUM_OF_SIMPLE];

    t_WidgetSysHandle *StylesTabHandle[NUM_OF_STYLES];
    struct SettingsStylingWidgetsSet Styles[NUM_OF_STYLES];
};

/*** FUNCTION PROTOTYPES      ***/
const struct DataProcessorInfo *TextLineHighlighter_GetProcessorInfo(
        unsigned int *SizeOfInfo);
void TextLineHighlighter_ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,PG_BOOL *Consumed);
static t_DataProSettingsWidgetsType *TextLineHighlighter_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings);
static void TextLineHighlighter_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData);
static void TextLineHighlighter_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings);
static void TextLineHighlighter_ApplySettings(t_DataProcessorHandleType *ConDataHandle,
        t_PIKVList *Settings);
static t_DataProcessorHandleType *TextLineHighlighter_AllocateData(void);
static void TextLineHighlighter_FreeData(t_DataProcessorHandleType *DataHandle);
static void TextLineHighlighter_AddSettingStyleWidgets(
        struct SettingsStylingWidgetsSet *Widgets,
        t_WidgetSysHandle *SysHandle);
static void TextLineHighlighter_FreeSettingStyleWidgets(
        struct SettingsStylingWidgetsSet *Widgets,
        t_WidgetSysHandle *SysHandle);
static void TextLineHighlighter_SetSettingStyleWidgets(t_PIKVList *Settings,
        struct SettingsStylingWidgetsSet *Widgets,t_WidgetSysHandle *SysHandle,
        const char *Prefix,uint32_t DefaultStyleSet);
static uint32_t TextLineHighlighter_GrabSettingKV(t_PIKVList *Settings,
        const char *Prefix,const char *Key,uint32_t DefaultValue,int Base);
static void TextLineHighlighter_UpdateSettingFromStyleWidgets(t_PIKVList *Settings,
        struct SettingsStylingWidgetsSet *Widgets,t_WidgetSysHandle *SysHandle,
        const char *Prefix);
static void TextLineHighlighter_SetSettingKV(t_PIKVList *Settings,
        const char *Prefix,const char *Key,uint32_t Value,int Base);
static void TextLineHighlighter_ApplySetting_SetData(t_PIKVList *Settings,
        struct TextLineHighlighter_TextStyle *Style,const char *Prefix,
        uint32_t DefaultStyleSet);
static void TextLineHighlighter_HandleLine(struct TextLineHighlighterData *Data);
static void TextLineHighlighter_ApplyStyleSet2Marker(struct TextLineHighlighterData *Data,
        int StyleIndex);

/*** VARIABLE DEFINITIONS     ***/
struct DataProcessorAPI m_TextLineHighlighterCBs=
{
    TextLineHighlighter_AllocateData,
    TextLineHighlighter_FreeData,
    TextLineHighlighter_GetProcessorInfo,               // GetProcessorInfo
    NULL,                                               // ProcessKeyPress
    TextLineHighlighter_ProcessIncomingTextByte,        // ProcessIncomingTextByte
    NULL,                                               // ProcessIncomingBinaryByte
    /* V2 */
    NULL,                                   // ProcessOutGoingData
    TextLineHighlighter_AllocSettingsWidgets,
    TextLineHighlighter_FreeSettingsWidgets,
    TextLineHighlighter_SetSettingsFromWidgets,
    TextLineHighlighter_ApplySettings,
};


struct DataProcessorInfo m_TextLineHighlighter_Info=
{
    "Text Line Highlighter",
    "Highlights lines using regex's or a simple string matching.",
    "Highlights lines using regex's or a simple string matching.",
    e_DataProcessorType_Text,
    {
        .TxtClass=e_TextDataProcessorClass_Highlighter,
    },
    e_BinaryDataProcessorMode_Hex
};

static const struct PI_SystemAPI *m_TLF_SysAPI;
static const struct DPS_API *m_TLF_DPS;
static const struct PI_UIAPI *m_TLF_UIAPI;

static const struct TextLineHighlighter_TextStyle m_DefaultStyleSets[NUM_OF_STYLES]=
{
    {0xFFFFFF,0xFF0000,0},                      // 0
    {0x000000,0x00FF00,0},                      // 1
    {0xFFFFFF,0x0000FF,0},                      // 2
    {0xFFFFFF,0xFF00FF,0},                      // 3
    {0x000000,0x00FFFF,0},                      // 4
    {0xFFFFFF,0x000000,TXT_ATTRIB_UNDERLINE},   // 5
    {0xFFFFFF,0x000000,TXT_ATTRIB_BOLD},        // 6
    {0xFFFFFF,0x000000,TXT_ATTRIB_ITALIC},      // 7
//    {0xFF0000,0x000000,0},                      // 8
//    {0x00FF00,0x000000,0},                      // 9
};

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_RegisterPlugin
 *
 * SYNOPSIS:
 *    unsigned int TextLineHighlighter_RegisterPlugin(const struct PI_SystemAPI *SysAPI,
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

        m_TLF_SysAPI=SysAPI;
        m_TLF_DPS=SysAPI->GetAPI_DataProcessors();
        m_TLF_UIAPI=m_TLF_DPS->GetAPI_UI();

        /* If we are have the correct experimental API */
        if(SysAPI->GetExperimentalID()>0 &&
                SysAPI->GetExperimentalID()<1)
        {
            return 0xFFFFFFFF;
        }

        m_TLF_DPS->RegisterDataProcessor("TextLineHighlighter",
                &m_TextLineHighlighterCBs,sizeof(m_TextLineHighlighterCBs));

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
static t_DataProcessorHandleType *TextLineHighlighter_AllocateData(void)
{
    struct TextLineHighlighterData *Data;

    Data=NULL;
    try
    {
        Data=new struct TextLineHighlighterData;

        Data->StartOfLineMarker=NULL;
        Data->GrabNewMark=false;
    }
    catch(...)
    {
        if(Data!=NULL)
        {
            delete Data;
        }
        return NULL;
    }

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
static void TextLineHighlighter_FreeData(t_DataProcessorHandleType *DataHandle)
{
    struct TextLineHighlighterData *Data=(struct TextLineHighlighterData *)DataHandle;

    if(Data->StartOfLineMarker!=NULL)
        m_TLF_DPS->FreeMark(Data->StartOfLineMarker);

    delete Data;
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_GetProcessorInfo
 *
 * SYNOPSIS:
 *    const struct DataProcessorInfo *TextLineHighlighter_GetProcessorInfo(
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
const struct DataProcessorInfo *TextLineHighlighter_GetProcessorInfo(
        unsigned int *SizeOfInfo)
{
    *SizeOfInfo=sizeof(struct DataProcessorInfo);
    return &m_TextLineHighlighter_Info;
}

/*******************************************************************************
 * NAME:
 *   ProcessIncomingTextByte
 *
 * SYNOPSIS:
 *   void ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,
 *       const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,
 *       PG_BOOL *Consumed);
 *
 * PARAMETERS:
 *   DataHandle [I] -- The data handle to work on.  This is your internal
 *                     data.
 *   RawByte [I] -- This is the byte that came in.
 *   ProcessedChar [I/O] -- This is a unicode char that has already been
 *                        processed by some of the other input filters.  You
 *                        can change this as you need.  It must remain only
 *                        one unicode char.
 *   CharLen [I/O] -- This number of bytes in 'ProcessedChar'
 *   Consumed [I/O] -- This tells the system (and other filters) if the
 *                     char has been used up and will not be added to the
 *                     screen.
 *
 * FUNCTION:
 *   This function is called for each byte that comes in if you are a
 *   'e_DataProcessorType_Text' type of processor.  You work on the
 *   'ProcessedChar' to change the byte as needed.
 *
 *   If you set 'Consumed' to true then the 'ProcessedChar' will not be added
 *   to the display (or passed to other processors).  If it is set to
 *   false then it will be added to the screen.
 *
 * RETURNS:
 *   NONE
 ******************************************************************************/
void TextLineHighlighter_ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,
        const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,
        PG_BOOL *Consumed)
{
    struct TextLineHighlighterData *Data=(struct TextLineHighlighterData *)DataHandle;

    /* If we haven't allocated a marker yet, then we need to (we can't
       allocate this in the AllocateData() because the m_TLF_DPS API doesn't
       work in that function) */
    if(Data->StartOfLineMarker==NULL)
    {
        Data->StartOfLineMarker=m_TLF_DPS->AllocateMark();
        if(Data->StartOfLineMarker==NULL)
            return;
        Data->GrabNewMark=true;
    }

    if(Data->GrabNewMark)
    {
        m_TLF_DPS->SetMark2CursorPos(Data->StartOfLineMarker);
        Data->GrabNewMark=false;
    }

    if(RawByte=='\n')
    {
        /* We are at the end of the line, see if it matches anything */
        TextLineHighlighter_HandleLine(Data);
    }
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
t_DataProSettingsWidgetsType *TextLineHighlighter_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings)
{
    struct TextLineHighlighter_SettingsWidgets *WData;
    const char *RegexStr;
    const char *Str;
    int r;
    int c;
    char buff[100];

    try
    {
        WData=new TextLineHighlighter_SettingsWidgets;

        /* Zero everything */
        WData->SimpleTabHandle=NULL;
        WData->RegexTabHandle=NULL;
        for(r=0;r<NUM_OF_REGEXS;r++)
        {
            WData->Regex[r].StyleList=NULL;
            WData->Regex[r].RegexWid=NULL;
            WData->Regex[r].GroupBox=NULL;
        }
        for(r=0;r<NUM_OF_SIMPLE;r++)
        {
            WData->Simple[r].StyleList=NULL;
            WData->Simple[r].GroupBox=NULL;
            WData->Simple[r].StartsWith=NULL;
            WData->Simple[r].Contains=NULL;
            WData->Simple[r].EndsWith=NULL;
        }
        for(r=0;r<NUM_OF_STYLES;r++)
            WData->StylesTabHandle[r]=NULL;

        /* Add widgets */

        m_TLF_DPS->SetCurrentSettingsTabName("Simple");
        WData->SimpleTabHandle=WidgetHandle;

        WData->RegexTabHandle=m_TLF_DPS->AddNewSettingsTab("Regex");
        if(WData->RegexTabHandle==NULL)
            throw(0);

        for(r=0;r<NUM_OF_REGEXS;r++)
        {
            sprintf(buff,"Regex Match %d",r+1);
            WData->Regex[r].GroupBox=m_TLF_UIAPI->AddGroupBox(WData->
                RegexTabHandle,buff);
            if(WData->Regex[r].GroupBox==NULL)
                throw(0);

            WData->Regex[r].RegexWid=m_TLF_UIAPI->AddTextInput(WData->
                    Regex[r].GroupBox->GroupWidgetHandle,"Regex",NULL,NULL);
            if(WData->Regex[r].RegexWid==NULL)
                throw(0);

            WData->Regex[r].StyleList=m_TLF_UIAPI->AddComboBox(WData->
                    Regex[r].GroupBox->GroupWidgetHandle,false,"Style",
                    NULL,NULL);
            if(WData->Regex[r].StyleList==NULL)
                throw(0);
            for(c=0;c<NUM_OF_STYLES;c++)
            {
                sprintf(buff,"Color Set %d",c+1);
                m_TLF_UIAPI->AddItem2ComboBox(WData->Regex[r].GroupBox->
                        GroupWidgetHandle,WData->Regex[r].StyleList->Ctrl,
                        buff,c);
            }
        }

        for(r=0;r<NUM_OF_SIMPLE;r++)
        {
            sprintf(buff,"Simple Match %d",r+1);
            WData->Simple[r].GroupBox=m_TLF_UIAPI->AddGroupBox(WData->
                SimpleTabHandle,buff);
            if(WData->Simple[r].GroupBox==NULL)
                throw(0);

            WData->Simple[r].StartsWith=m_TLF_UIAPI->AddTextInput(WData->
                    Simple[r].GroupBox->GroupWidgetHandle,
                    "Lines that start with",NULL,NULL);
            if(WData->Simple[r].StartsWith==NULL)
                throw(0);

            WData->Simple[r].Contains=m_TLF_UIAPI->
                    AddTextInput(WData->Simple[r].GroupBox->GroupWidgetHandle,
                    "Lines that contain",NULL,NULL);
            if(WData->Simple[r].Contains==NULL)
                throw(0);

            WData->Simple[r].EndsWith=m_TLF_UIAPI->
                    AddTextInput(WData->Simple[r].GroupBox->GroupWidgetHandle,
                    "Lines that end with",NULL,NULL);
            if(WData->Simple[r].EndsWith==NULL)
                throw(0);

            WData->Simple[r].StyleList=m_TLF_UIAPI->AddComboBox(WData->
                    Simple[r].GroupBox->GroupWidgetHandle,false,"Style",
                    NULL,NULL);
            if(WData->Simple[r].StyleList==NULL)
                throw(0);
            for(c=0;c<NUM_OF_STYLES;c++)
            {
                sprintf(buff,"Color Set %d",c+1);
                m_TLF_UIAPI->AddItem2ComboBox(WData->Simple[r].GroupBox->
                        GroupWidgetHandle,WData->Simple[r].StyleList->Ctrl,
                        buff,c);
            }
        }

        /** Regex **/
        for(r=0;r<NUM_OF_REGEXS;r++)
        {
            sprintf(buff,"RegexStr%d",r);
            RegexStr=m_TLF_SysAPI->KVGetItem(Settings,buff);
            if(RegexStr==NULL)
                RegexStr="";
            m_TLF_UIAPI->SetTextInputText(WData->Regex[r].GroupBox->
                    GroupWidgetHandle,WData->Regex[r].RegexWid->Ctrl,RegexStr);

            sprintf(buff,"RegexStyle%d",r);
            Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
            if(Str==NULL)
                Str="0";

            m_TLF_UIAPI->SetComboBoxSelectedEntry(WData->Regex[r].GroupBox->
                    GroupWidgetHandle,WData->Regex[r].StyleList->Ctrl,
                    atoi(Str));
        }

        for(r=0;r<NUM_OF_SIMPLE;r++)
        {
            sprintf(buff,"SimpleStart%d",r);
            Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
            if(Str==NULL)
                Str="";
            m_TLF_UIAPI->SetTextInputText(WData->Simple[r].GroupBox->
                    GroupWidgetHandle,WData->Simple[r].StartsWith->Ctrl,Str);

            sprintf(buff,"SimpleContains%d",r);
            Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
            if(Str==NULL)
                Str="";
            m_TLF_UIAPI->SetTextInputText(WData->Simple[r].GroupBox->
                    GroupWidgetHandle,WData->Simple[r].Contains->Ctrl,Str);

            sprintf(buff,"SimpleEnd%d",r);
            Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
            if(Str==NULL)
                Str="";
            m_TLF_UIAPI->SetTextInputText(WData->Simple[r].GroupBox->
                    GroupWidgetHandle,WData->Simple[r].EndsWith->Ctrl,Str);

            sprintf(buff,"SimpleStyle%d",r);
            Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
            if(Str==NULL)
                Str="0";

            m_TLF_UIAPI->SetComboBoxSelectedEntry(WData->Simple[r].GroupBox->
                    GroupWidgetHandle,WData->Simple[r].StyleList->Ctrl,
                    atoi(Str));
        }

        /* Styling tabs (colors) */
        for(r=0;r<NUM_OF_STYLES;r++)
        {
            sprintf(buff,"Colors %d",r+1);
            WData->StylesTabHandle[r]=m_TLF_DPS->AddNewSettingsTab(buff);
            if(WData->StylesTabHandle[r]==NULL)
                throw(0);
            TextLineHighlighter_AddSettingStyleWidgets(&WData->Styles[r],
                    WData->StylesTabHandle[r]);

            sprintf(buff,"Colors%d",r);
            TextLineHighlighter_SetSettingStyleWidgets(Settings,&WData->Styles[r],
                    WData->StylesTabHandle[r],buff,r);
        }
    }
    catch(...)
    {
        if(WData!=NULL)
        {
            TextLineHighlighter_FreeSettingsWidgets(
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
void TextLineHighlighter_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData)
{
    struct TextLineHighlighter_SettingsWidgets *WData=(struct TextLineHighlighter_SettingsWidgets *)PrivData;
    int r;

    /* Free everything in reverse order */

    /* Styling tabs (colors) */
    for(r=NUM_OF_STYLES-1;r>=0;r--)
    {
        TextLineHighlighter_FreeSettingStyleWidgets(&WData->Styles[r],
                WData->StylesTabHandle[r]);
    }

    for(r=NUM_OF_SIMPLE-1;r>=0;r--)
    {
        if(WData->Simple[r].StyleList!=NULL)
        {
            m_TLF_UIAPI->FreeComboBox(WData->Simple[r].GroupBox->
                    GroupWidgetHandle,WData->Simple[r].StyleList);
        }
        if(WData->Simple[r].EndsWith!=NULL)
        {
            m_TLF_UIAPI->FreeTextInput(WData->Simple[r].GroupBox->
                    GroupWidgetHandle,WData->Simple[r].EndsWith);
        }
        if(WData->Simple[r].Contains!=NULL)
        {
            m_TLF_UIAPI->FreeTextInput(WData->Simple[r].GroupBox->
                    GroupWidgetHandle,WData->Simple[r].Contains);
        }
        if(WData->Simple[r].StartsWith!=NULL)
        {
            m_TLF_UIAPI->FreeTextInput(WData->Simple[r].GroupBox->
                    GroupWidgetHandle,WData->Simple[r].StartsWith);
        }
        if(WData->Simple[r].GroupBox!=NULL)
        {
            m_TLF_UIAPI->FreeGroupBox(WData->SimpleTabHandle,
                    WData->Simple[r].GroupBox);
        }
    }
    for(r=NUM_OF_REGEXS-1;r>=0;r--)
    {
        if(WData->Regex[r].StyleList!=NULL)
        {
            m_TLF_UIAPI->FreeComboBox(WData->Regex[r].GroupBox->
                    GroupWidgetHandle,WData->Regex[r].StyleList);
        }
        if(WData->Regex[r].RegexWid!=NULL)
        {
            m_TLF_UIAPI->FreeTextInput(WData->Regex[r].GroupBox->
                    GroupWidgetHandle,WData->Regex[r].RegexWid);
        }
        if(WData->Regex[r].GroupBox!=NULL)
        {
            m_TLF_UIAPI->FreeGroupBox(WData->RegexTabHandle,
                    WData->Regex[r].GroupBox);
        }
    }

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
void TextLineHighlighter_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings)
{
    struct TextLineHighlighter_SettingsWidgets *WData=(struct TextLineHighlighter_SettingsWidgets *)PrivData;
    string Str;
    int r;
    char buff[100];
    char buff2[100];
    uint32_t Num;

    /** Simple **/
    for(r=0;r<NUM_OF_SIMPLE;r++)
    {
        Str=m_TLF_UIAPI->GetTextInputText(WData->Simple[r].GroupBox->
                GroupWidgetHandle,WData->Simple[r].StartsWith->Ctrl);
        sprintf(buff,"SimpleStart%d",r);
        m_TLF_SysAPI->KVAddItem(Settings,buff,Str.c_str());

        Str=m_TLF_UIAPI->GetTextInputText(WData->Simple[r].GroupBox->
                GroupWidgetHandle,WData->Simple[r].Contains->Ctrl);
        sprintf(buff,"SimpleContains%d",r);
        m_TLF_SysAPI->KVAddItem(Settings,buff,Str.c_str());

        Str=m_TLF_UIAPI->GetTextInputText(WData->Simple[r].GroupBox->
                GroupWidgetHandle,WData->Simple[r].EndsWith->Ctrl);
        sprintf(buff,"SimpleEnd%d",r);
        m_TLF_SysAPI->KVAddItem(Settings,buff,Str.c_str());

        Num=m_TLF_UIAPI->GetComboBoxSelectedEntry(WData->Simple[r].GroupBox->
                GroupWidgetHandle,WData->Simple[r].StyleList->Ctrl);
        sprintf(buff,"SimpleStyle%d",r);
        sprintf(buff2,"%d",Num);
        m_TLF_SysAPI->KVAddItem(Settings,buff,buff2);
    }

    /** Regex **/
    for(r=0;r<NUM_OF_REGEXS;r++)
    {
        Str=m_TLF_UIAPI->GetTextInputText(WData->Regex[r].GroupBox->
                GroupWidgetHandle,WData->Regex[r].RegexWid->Ctrl);

        sprintf(buff,"RegexStr%d",r);
        m_TLF_SysAPI->KVAddItem(Settings,buff,Str.c_str());

        Num=m_TLF_UIAPI->GetComboBoxSelectedEntry(WData->Regex[r].GroupBox->
                GroupWidgetHandle,WData->Regex[r].StyleList->Ctrl);
        sprintf(buff,"RegexStyle%d",r);
        sprintf(buff2,"%d",Num);
        m_TLF_SysAPI->KVAddItem(Settings,buff,buff2);
    }

    /* Styling tabs (colors) */
    for(r=0;r<NUM_OF_STYLES;r++)
    {
        sprintf(buff,"Colors%d",r);
        TextLineHighlighter_UpdateSettingFromStyleWidgets(Settings,
                &WData->Styles[r],WData->StylesTabHandle[r],buff);
    }
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
static void TextLineHighlighter_ApplySettings(t_DataProcessorHandleType *DataHandle,
        t_PIKVList *Settings)
{
    struct TextLineHighlighterData *Data=(struct TextLineHighlighterData *)DataHandle;
    const char *Str;
    int r;
    char buff[100];

    for(r=0;r<NUM_OF_SIMPLE;r++)
    {
        sprintf(buff,"SimpleStart%d",r);
        Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
        if(Str==NULL)
            Str="";
        Data->Simple[r].StartsWith=Str;

        sprintf(buff,"SimpleContains%d",r);
        Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
        if(Str==NULL)
            Str="";
        Data->Simple[r].Contains=Str;

        sprintf(buff,"SimpleEnd%d",r);
        Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
        if(Str==NULL)
            Str="";
        Data->Simple[r].EndsWith=Str;

        sprintf(buff,"SimpleStyle%d",r);
        Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
        if(Str==NULL)
            Str="0";
        Data->Simple[r].StyleIndex=atoi(Str);
    }

    for(r=0;r<NUM_OF_REGEXS;r++)
    {
        sprintf(buff,"RegexStr%d",r);
        Str=m_TLF_SysAPI->KVGetItem(Settings,buff);

        if(Str==NULL)
            Str="";

        Data->Regex[r].Pattern=Str;

        sprintf(buff,"RegexStyle%d",r);
        Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
        if(Str==NULL)
            Str="0";
        Data->Regex[r].StyleIndex=atoi(Str);
    }

    /* Styling tabs (colors) */
    for(r=0;r<NUM_OF_STYLES;r++)
    {
        sprintf(buff,"Colors%d",r);
        TextLineHighlighter_ApplySetting_SetData(Settings,&Data->Styles[r],buff,
                r);
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_AddSettingStyleWidgets
 *
 * SYNOPSIS:
 *    static void TextLineHighlighter_AddSettingStyleWidgets(
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
static void TextLineHighlighter_AddSettingStyleWidgets(
        struct SettingsStylingWidgetsSet *Widgets,
        t_WidgetSysHandle *SysHandle)
{
    Widgets->FgColor=m_TLF_UIAPI->AddColorPick(SysHandle,"Forground Color",0x000000,NULL,NULL);
    if(Widgets->FgColor==NULL)
        throw(0);

    Widgets->BgColor=m_TLF_UIAPI->AddColorPick(SysHandle,"Background Color",0x000000,NULL,NULL);
    if(Widgets->BgColor==NULL)
        throw(0);

    Widgets->AttribUnderLine=m_TLF_UIAPI->AddCheckbox(SysHandle,"Underline",NULL,NULL);
    if(Widgets->AttribUnderLine==NULL)
        throw(0);

    Widgets->AttribOverLine=m_TLF_UIAPI->AddCheckbox(SysHandle,"Overline",NULL,NULL);
    if(Widgets->AttribOverLine==NULL)
        throw(0);

    Widgets->AttribLineThrough=m_TLF_UIAPI->AddCheckbox(SysHandle,"Line through",NULL,NULL);
    if(Widgets->AttribLineThrough==NULL)
        throw(0);

    Widgets->AttribBold=m_TLF_UIAPI->AddCheckbox(SysHandle,"Bold",NULL,NULL);
    if(Widgets->AttribBold==NULL)
        throw(0);

    Widgets->AttribItalic=m_TLF_UIAPI->AddCheckbox(SysHandle,"Italic",NULL,NULL);
    if(Widgets->AttribItalic==NULL)
        throw(0);

    Widgets->AttribOutLine=m_TLF_UIAPI->AddCheckbox(SysHandle,"Outline",NULL,NULL);
    if(Widgets->AttribOutLine==NULL)
        throw(0);
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_FreeSettingStyleWidgets
 *
 * SYNOPSIS:
 *    static void TextLineHighlighter_FreeSettingStyleWidgets(
 *          struct SettingsStylingWidgetsSet *Widgets,
 *          t_WidgetSysHandle *SysHandle);
 *
 * PARAMETERS:
 *    Widgets [I] -- The widgets style set to free
 *    SysHandle [I] -- The widget system handle use to allocate these widgets.
 *
 * FUNCTION:
 *    This function frees the widgets that was allocated with
 *    TextLineHighlighter_AddSettingStyleWidgets()
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    TextLineHighlighter_AddSettingStyleWidgets()
 ******************************************************************************/
static void TextLineHighlighter_FreeSettingStyleWidgets(
        struct SettingsStylingWidgetsSet *Widgets,
        t_WidgetSysHandle *SysHandle)
{
    if(Widgets->AttribOutLine!=NULL)
        m_TLF_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribOutLine);
    if(Widgets->AttribItalic!=NULL)
        m_TLF_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribItalic);
    if(Widgets->AttribBold!=NULL)
        m_TLF_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribBold);
    if(Widgets->AttribLineThrough!=NULL)
        m_TLF_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribLineThrough);
    if(Widgets->AttribOverLine!=NULL)
        m_TLF_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribOverLine);
    if(Widgets->AttribUnderLine!=NULL)
        m_TLF_UIAPI->FreeCheckbox(SysHandle,Widgets->AttribUnderLine);

    if(Widgets->BgColor!=NULL)
        m_TLF_UIAPI->FreeColorPick(SysHandle,Widgets->BgColor);
    if(Widgets->FgColor!=NULL)
        m_TLF_UIAPI->FreeColorPick(SysHandle,Widgets->FgColor);
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_SetSettingStyleWidgets
 *
 * SYNOPSIS:
 *    static void TextLineHighlighter_SetSettingStyleWidgets(t_PIKVList *Settings,
 *          struct SettingsStylingWidgetsSet *Widgets,
 *          t_WidgetSysHandle *SysHandle,const char *Prefix,
 *          uint32_t DefaultStyleSet)
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
 *    DefaultStyleSet [I] -- The default colors and style to use if a setting
 *                           isn't found in settings.
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
static void TextLineHighlighter_SetSettingStyleWidgets(t_PIKVList *Settings,
        struct SettingsStylingWidgetsSet *Widgets,t_WidgetSysHandle *SysHandle,
        const char *Prefix,uint32_t DefaultStyleSet)
{
    uint32_t Num;

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"FGColor",
            m_DefaultStyleSets[DefaultStyleSet].FGColor,16);
    m_TLF_UIAPI->SetColorPickValue(SysHandle,Widgets->FgColor->Ctrl,Num);

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"BGColor",
            m_DefaultStyleSets[DefaultStyleSet].BGColor,16);
    m_TLF_UIAPI->SetColorPickValue(SysHandle,Widgets->BgColor->Ctrl,Num);

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribUnderLine",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_UNDERLINE,10);
    m_TLF_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribUnderLine->Ctrl,
            Num?true:false);

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribOverLine",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_OVERLINE,10);
    m_TLF_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribOverLine->Ctrl,
            Num?true:false);

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribLineThrough",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_LINETHROUGH,10);
    m_TLF_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribLineThrough->Ctrl,
            Num?true:false);

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribBold",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_BOLD,10);
    m_TLF_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribBold->Ctrl,
            Num?true:false);

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribItalic",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_ITALIC,10);
    m_TLF_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribItalic->Ctrl,
            Num?true:false);

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribOutLine",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_OUTLINE,10);
    m_TLF_UIAPI->SetCheckboxChecked(SysHandle,Widgets->AttribOutLine->Ctrl,
            Num?true:false);
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_GrabSettingKV
 *
 * SYNOPSIS:
 *    static uint32_t TextLineHighlighter_GrabSettingKV(t_PIKVList *Settings,
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
 *    This is a helper function for TextLineHighlighter_SetSettingStyleWidgets()
 *    that tries to find a setting and if it's not found applies a default
 *    value.
 *
 * RETURNS:
 *    The value that was in settings or 'DefaultValue' if it was not found.
 *
 * SEE ALSO:
 *    TextLineHighlighter_SetSettingStyleWidgets()
 ******************************************************************************/
static uint32_t TextLineHighlighter_GrabSettingKV(t_PIKVList *Settings,
        const char *Prefix,const char *Key,uint32_t DefaultValue,int Base)
{
    char buff[100];
    const char *Str;
    uint32_t Number;

    sprintf(buff,"%s_%s",Prefix,Key);
    Str=m_TLF_SysAPI->KVGetItem(Settings,buff);
    if(Str!=NULL)
        Number=strtol(Str,NULL,Base);
    else
        Number=DefaultValue;

    return Number;
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_UpdateSettingFromStyleWidgets
 *
 * SYNOPSIS:
 *    static void TextLineHighlighter_UpdateSettingFromStyleWidgets(
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
static void TextLineHighlighter_UpdateSettingFromStyleWidgets(t_PIKVList *Settings,
        struct SettingsStylingWidgetsSet *Widgets,t_WidgetSysHandle *SysHandle,
        const char *Prefix)
{
    TextLineHighlighter_SetSettingKV(Settings,Prefix,"FGColor",
            m_TLF_UIAPI->GetColorPickValue(SysHandle,Widgets->FgColor->Ctrl),
            16);

    TextLineHighlighter_SetSettingKV(Settings,Prefix,"BGColor",
            m_TLF_UIAPI->GetColorPickValue(SysHandle,Widgets->BgColor->Ctrl),
            16);

    TextLineHighlighter_SetSettingKV(Settings,Prefix,"AttribUnderLine",
            m_TLF_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribUnderLine->Ctrl),16);

    TextLineHighlighter_SetSettingKV(Settings,Prefix,"AttribOverLine",
            m_TLF_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribOverLine->Ctrl),16);

    TextLineHighlighter_SetSettingKV(Settings,Prefix,"AttribLineThrough",
            m_TLF_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribLineThrough->Ctrl),16);

    TextLineHighlighter_SetSettingKV(Settings,Prefix,"AttribBold",
            m_TLF_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribBold->Ctrl),16);

    TextLineHighlighter_SetSettingKV(Settings,Prefix,"AttribItalic",
            m_TLF_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribItalic->Ctrl),16);

    TextLineHighlighter_SetSettingKV(Settings,Prefix,"AttribOutLine",
            m_TLF_UIAPI->IsCheckboxChecked(SysHandle,
            Widgets->AttribOutLine->Ctrl),16);
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_SetSettingKV
 *
 * SYNOPSIS:
 *    static void TextLineHighlighter_SetSettingKV(t_PIKVList *Settings,
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
 *    TextLineHighlighter_UpdateSettingFromStyleWidgets() that sets the value in
 *    'Settings'.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    TextLineHighlighter_UpdateSettingFromStyleWidgets()
 ******************************************************************************/
static void TextLineHighlighter_SetSettingKV(t_PIKVList *Settings,
        const char *Prefix,const char *Key,uint32_t Value,int Base)
{
    char buff[100];
    char buff2[100];

    sprintf(buff,"%s_%s",Prefix,Key);
    if(Base==10)
        sprintf(buff2,"%d",Value);
    else
        sprintf(buff2,"%06X",Value);
    m_TLF_SysAPI->KVAddItem(Settings,buff,buff2);
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_ApplySetting_SetData
 *
 * SYNOPSIS:
 *    static void TextLineHighlighter_ApplySetting_SetData(t_PIKVList *Settings,
 *              struct TextLineHighlighter_TextStyle *Style,const char *Prefix,
 *              uint32_t DefaultStyleSet);
 *
 * PARAMETERS:
 *    Settings [I] -- The settings to read out
 *    Style [O] -- The style to be set to what is in 'Settings'
 *    Prefix [I] -- The prefix for the styling names in the settings.  This
 *                  function will take this string and add it to the start
 *                  of the setting KV name before trying to read it from
 *                  settings.
 *    DefaultStyleSet [I] -- The default colors and style to use if a setting
 *                           isn't found in settings.
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
static void TextLineHighlighter_ApplySetting_SetData(t_PIKVList *Settings,
        struct TextLineHighlighter_TextStyle *Style,const char *Prefix,
        uint32_t DefaultStyleSet)
{
    uint32_t Num;

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"FGColor",
            m_DefaultStyleSets[DefaultStyleSet].FGColor,16);
    Style->FGColor=Num;

    Num=TextLineHighlighter_GrabSettingKV(Settings,Prefix,"BGColor",
            m_DefaultStyleSets[DefaultStyleSet].BGColor,16);
    Style->BGColor=Num;

    Style->Attribs=0;
    if(TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribUnderLine",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_UNDERLINE,
            10))
    {
        Style->Attribs|=TXT_ATTRIB_UNDERLINE;
    }

    if(TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribOverLine",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_OVERLINE,
            10))
    {
        Style->Attribs|=TXT_ATTRIB_OVERLINE;
    }

    if(TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribLineThrough",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_LINETHROUGH,
            10))
    {
        Style->Attribs|=TXT_ATTRIB_LINETHROUGH;
    }

    if(TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribBold",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_BOLD,
            10))
    {
        Style->Attribs|=TXT_ATTRIB_BOLD;
    }

    if(TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribItalic",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_ITALIC,
            10))
    {
        Style->Attribs|=TXT_ATTRIB_ITALIC;
    }

    if(TextLineHighlighter_GrabSettingKV(Settings,Prefix,"AttribOutLine",
            m_DefaultStyleSets[DefaultStyleSet].Attribs&TXT_ATTRIB_OUTLINE,
            10))
    {
        Style->Attribs|=TXT_ATTRIB_OUTLINE;
    }
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_HandleLine
 *
 * SYNOPSIS:
 *    void TextLineHighlighter_HandleLine(struct TextLineHighlighterData *Data);
 *
 * PARAMETERS:
 *    Data [I] -- Our data
 *
 * FUNCTION:
 *    This function handles when we finish reading a line.  It will check
 *    for any matches and color the line as needed.
 *
 *    It will then reset the mark.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void TextLineHighlighter_HandleLine(struct TextLineHighlighterData *Data)
{
    const uint8_t *Line;
    uint32_t Bytes;
    int r;
    size_t Len;

    if(Data->StartOfLineMarker==NULL)
        return;

    Line=m_TLF_DPS->GetMarkString(Data->StartOfLineMarker,&Bytes,0,0);
    if(Line==NULL)
    {
        Data->GrabNewMark=true;
        return;
    }

    for(r=0;r<NUM_OF_SIMPLE;r++)
    {
        if(!Data->Simple[r].StartsWith.empty())
        {
            Len=Data->Simple[r].StartsWith.length();
            if(strncmp((char *)Line,Data->Simple[r].StartsWith.c_str(),Len)==0)
            {
                TextLineHighlighter_ApplyStyleSet2Marker(Data,
                        Data->Simple[r].StyleIndex);
            }
        }
        if(!Data->Simple[r].Contains.empty())
        {
            if(strstr((char *)Line,Data->Simple[r].Contains.c_str())!=NULL)
            {
                TextLineHighlighter_ApplyStyleSet2Marker(Data,
                        Data->Simple[r].StyleIndex);
            }
        }
        if(!Data->Simple[r].EndsWith.empty())
        {
            Len=Data->Simple[r].EndsWith.length();
            if(Bytes>=Len && strcmp((char *)&Line[Bytes-Len],
                    Data->Simple[r].EndsWith.c_str())==0)
            {
                TextLineHighlighter_ApplyStyleSet2Marker(Data,
                        Data->Simple[r].StyleIndex);
            }
        }
    }

    for(r=0;r<NUM_OF_REGEXS;r++)
    {
        if(!Data->Regex[r].Pattern.empty())
        {
            regex RxPattern(Data->Regex[r].Pattern);
            if(regex_search((const char *)Line,RxPattern))
            {
                TextLineHighlighter_ApplyStyleSet2Marker(Data,
                    Data->Regex[r].StyleIndex);
            }
        }
    }

    /* Ok, reset the mark */
    Data->GrabNewMark=true;
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_ApplyStyleSet2Marker
 *
 * SYNOPSIS:
 *    static void TextLineHighlighter_ApplyStyleSet2Marker(struct TextLineHighlighterData *Data,
 *              int StyleIndex);
 *
 * PARAMETERS:
 *    Data [I] -- Our data
 *    StyleIndex [I] -- The index of the style to apply.
 *
 * FUNCTION:
 *    This function applies a style to the current marker to the cursor.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void TextLineHighlighter_ApplyStyleSet2Marker(struct TextLineHighlighterData *Data,
        int StyleIndex)
{
    m_TLF_DPS->ApplyAttrib2Mark(Data->StartOfLineMarker,
            Data->Styles[StyleIndex].Attribs,0,0);
    m_TLF_DPS->ApplyFGColor2Mark(Data->StartOfLineMarker,
            Data->Styles[StyleIndex].FGColor,0,0);
    m_TLF_DPS->ApplyBGColor2Mark(Data->StartOfLineMarker,
            Data->Styles[StyleIndex].BGColor,0,0);
}
