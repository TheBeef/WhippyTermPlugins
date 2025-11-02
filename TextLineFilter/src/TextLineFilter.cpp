/*******************************************************************************
 * FILENAME: TextLineFilter.cpp
 *
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    This has a display processor that filters incoming messages to screen
 *    out traffic.  It is line based.
 *
 * COPYRIGHT:
 *    Copyright 07 Jul 2025 Paul Hutchinson.
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
 *    Paul Hutchinson (07 Jul 2025)
 *
 ******************************************************************************/

/*** HEADER FILES TO INCLUDE  ***/
#include "TextLineFilter.h"
#include "PluginSDK/Plugin.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <regex>

using namespace std;

/*** DEFINES                  ***/
#define REGISTER_PLUGIN_FUNCTION_PRIV_NAME      TextLineFilter // The name to append on the RegisterPlugin() function for built in version
#define NEEDED_MIN_API_VERSION                  0x02000000

#define SEARCH_STRINGS_GROW_SIZE                100 // Add 100 strings every time we need more
#define MAX_REGEX                               5

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/
struct SimpleFilter
{
    char *PatternStr;
    char **Patterns;
    uint_fast32_t PatternsCount;
};

struct TextLineFilterData
{
    bool FreezeStream;

    struct SimpleFilter SimpleFilterStartsWithPat;
    struct SimpleFilter SimpleFilterContainsPat;
    struct SimpleFilter SimpleFilterEndsWithPat;

    string RegexRemoveFilterStr[MAX_REGEX];
    string RegexIncludeFilterStr[MAX_REGEX];
};

struct TextLineFilter_SettingsWidgets
{
    t_WidgetSysHandle *HelpTabHandle;
    t_WidgetSysHandle *SimpleTabHandle;
    t_WidgetSysHandle *RegexTabHandle;

    struct PI_TextBox *HelpText;

    struct PI_TextInput *SimpleFilterStartsWith;
    struct PI_TextInput *SimpleFilterContains;
    struct PI_TextInput *SimpleFilterEndsWith;
    struct PI_TextBox *SimpleHelpText;

    struct PI_GroupBox *RegexRemoveGroup;
    struct PI_TextInput *RegexRemoveFilterWid[MAX_REGEX];
    struct PI_GroupBox *RegexIncludeGroup;
    struct PI_TextInput *RegexIncludeFilterWid[MAX_REGEX];
};

/*** FUNCTION PROTOTYPES      ***/
const struct DataProcessorInfo *TextLineFilter_GetProcessorInfo(
        unsigned int *SizeOfInfo);
void TextLineFilter_ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,PG_BOOL *Consumed);
static t_DataProSettingsWidgetsType *TextLineFilter_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings);
static void TextLineFilter_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData);
static void TextLineFilter_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings);
static void TextLineFilter_ApplySettings(t_DataProcessorHandleType *ConDataHandle,
        t_PIKVList *Settings);
static t_DataProcessorHandleType *TextLineFilter_AllocateData(void);
static void TextLineFilter_FreeData(t_DataProcessorHandleType *DataHandle);
static bool TextLineFilter_GrowSearchStringsIfNeeded(char **SearchStrings,uint_fast32_t *SearchStringsSize,uint_fast32_t Size);
static bool TextLineFilter_ProcessSimpleFilter(const char *Filter,struct SimpleFilter &Result);
static bool TextLineFilter_HandleLine(struct TextLineFilterData *Data);
static void TextLineFilter_FreeSimpleFilter(struct SimpleFilter &Pat);

/*** VARIABLE DEFINITIONS     ***/
struct DataProcessorAPI m_TextLineFilterCBs=
{
    TextLineFilter_AllocateData,
    TextLineFilter_FreeData,
    TextLineFilter_GetProcessorInfo,            // GetProcessorInfo
    NULL,                                       // ProcessKeyPress
    TextLineFilter_ProcessIncomingTextByte,     // ProcessIncomingTextByte
    NULL,                                       // ProcessIncomingBinaryByte
    /* V2 */
    NULL,                                       // ProcessOutGoingData
    TextLineFilter_AllocSettingsWidgets,
    TextLineFilter_FreeSettingsWidgets,
    TextLineFilter_SetSettingsFromWidgets,
    TextLineFilter_ApplySettings,
};

struct DataProcessorInfo m_TextLineFilter_Info=
{
    "Text Line Filter",
    "Filters out lines using regex's or a simple string matching.",
    "Filters out lines using regex's or a simple string matching.",
    e_DataProcessorType_Text,
    {
        .TxtClass=e_TextDataProcessorClass_Other,
    },
    e_BinaryDataProcessorMode_Hex
};

static const struct PI_SystemAPI *m_TLF_SysAPI;
static const struct DPS_API *m_TLF_DPS;
static const struct PI_UIAPI *m_TLF_UIAPI;

/*******************************************************************************
 * NAME:
 *    TextLineFilter_RegisterPlugin
 *
 * SYNOPSIS:
 *    unsigned int TextLineFilter_RegisterPlugin(const struct PI_SystemAPI *SysAPI,
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

        m_TLF_DPS->RegisterDataProcessor("TextLineFilter",
                &m_TextLineFilterCBs,sizeof(m_TextLineFilterCBs));

        return 0;
    }
}

static t_DataProcessorHandleType *TextLineFilter_AllocateData(void)
{
    struct TextLineFilterData *Data;

    Data=NULL;
    try
    {
        Data=new struct TextLineFilterData;
        if(Data==NULL)
            return NULL;

        Data->FreezeStream=true;

        Data->SimpleFilterStartsWithPat.PatternStr=NULL;
        Data->SimpleFilterStartsWithPat.Patterns=NULL;
        Data->SimpleFilterStartsWithPat.PatternsCount=0;

        Data->SimpleFilterContainsPat.PatternStr=NULL;
        Data->SimpleFilterContainsPat.Patterns=NULL;
        Data->SimpleFilterContainsPat.PatternsCount=0;

        Data->SimpleFilterEndsWithPat.PatternStr=NULL;
        Data->SimpleFilterEndsWithPat.Patterns=NULL;
        Data->SimpleFilterEndsWithPat.PatternsCount=0;
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

static void TextLineFilter_FreeData(t_DataProcessorHandleType *DataHandle)
{
    struct TextLineFilterData *Data=(struct TextLineFilterData *)DataHandle;

    TextLineFilter_FreeSimpleFilter(Data->SimpleFilterStartsWithPat);
    TextLineFilter_FreeSimpleFilter(Data->SimpleFilterContainsPat);
    TextLineFilter_FreeSimpleFilter(Data->SimpleFilterEndsWithPat);

    delete Data;
}

/*******************************************************************************
 * NAME:
 *    TextLineFilter_GetProcessorInfo
 *
 * SYNOPSIS:
 *    const struct DataProcessorInfo *TextLineFilter_GetProcessorInfo(
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
const struct DataProcessorInfo *TextLineFilter_GetProcessorInfo(
        unsigned int *SizeOfInfo)
{
    *SizeOfInfo=sizeof(struct DataProcessorInfo);
    return &m_TextLineFilter_Info;
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
void TextLineFilter_ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,
        const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,
        PG_BOOL *Consumed)
{
    struct TextLineFilterData *Data=(struct TextLineFilterData *)DataHandle;

    /* If we haven't allocated a marker yet, then we need to (we can't
       allocate this in the AllocateData() because the m_TLF_DPS API doesn't
       work in that function) */
    if(Data->FreezeStream)
    {
        m_TLF_DPS->FreezeStream();
        Data->FreezeStream=false;
    }

    if(RawByte=='\n')
    {
        /* We are at the end of the line, see if it matches anything */
        if(TextLineFilter_HandleLine(Data))
            *Consumed=true;
    }
}

t_DataProSettingsWidgetsType *TextLineFilter_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings)
{
    struct TextLineFilter_SettingsWidgets *WData;
    const char *SimpleFilter_StartingWith;
    const char *SimpleFilter_EndingWith;
    const char *SimpleFilter_Contains;
    const char *RegexFilter_RemoveFilter[MAX_REGEX];
    const char *RegexFilter_IncludeFilter[MAX_REGEX];
    int r;
    char buff[100];

    try
    {
        WData=new TextLineFilter_SettingsWidgets;
        WData->HelpTabHandle=NULL;
        WData->SimpleTabHandle=NULL;
        WData->RegexTabHandle=NULL;
        WData->HelpText=NULL;
        WData->SimpleFilterStartsWith=NULL;
        WData->SimpleFilterContains=NULL;
        WData->SimpleFilterEndsWith=NULL;
        WData->SimpleHelpText=NULL;
        WData->RegexRemoveGroup=NULL;
        for(r=0;r<MAX_REGEX;r++)
        {
            WData->RegexRemoveFilterWid[r]=NULL;
            WData->RegexIncludeFilterWid[r]=NULL;
        }
        WData->RegexIncludeGroup=NULL;

        m_TLF_DPS->SetCurrentSettingsTabName("Help");
        WData->HelpTabHandle=WidgetHandle;

        WData->HelpText=m_TLF_UIAPI->AddTextBox(WData->HelpTabHandle,NULL,
                "This plugin strips lines out of the incoming stream.\n"
                "\n"
                "You can use a simple filter or more complex rxgex's\n"
                "\n"
                "You can only use simple or rxgex filtering not both at "
                "the same time.  When you enable simple it will disable "
                "rxgex and enabling rxgex will disable simple.");
        if(WData->HelpText==NULL)
            throw(0);

        WData->SimpleTabHandle=m_TLF_DPS->AddNewSettingsTab("Simple");
        if(WData->SimpleTabHandle==NULL)
            throw(0);

        WData->SimpleFilterStartsWith=m_TLF_UIAPI->AddTextInput(WData->SimpleTabHandle,
                "Filter Lines that start with",NULL,NULL);
        if(WData->SimpleFilterStartsWith==NULL)
            throw(0);

        WData->SimpleFilterContains=m_TLF_UIAPI->AddTextInput(WData->SimpleTabHandle,
                "Filter Lines that contain",NULL,NULL);
        if(WData->SimpleFilterContains==NULL)
            throw(0);

        WData->SimpleFilterEndsWith=m_TLF_UIAPI->AddTextInput(WData->SimpleTabHandle,
                "Filter Lines that end with",NULL,NULL);
        if(WData->SimpleFilterEndsWith==NULL)
            throw(0);

        WData->SimpleHelpText=m_TLF_UIAPI->AddTextBox(WData->SimpleTabHandle,NULL,
                "All inputs are a list of words that will be matched, "
                "seperated by spaces.\n"
                "\n"
                "If you want to include spaces in your matched word put "
                "the string in quote's.\n"
                "\n"
                "If you want to include a quote in your matched word "
                "prefix it with a backslash (\\)\n"
                "\n"
                "If you want to include a backslash in your matched "
                "word use two backslashes (\\\\)");
        if(WData->SimpleHelpText==NULL)
            throw(0);

        WData->RegexTabHandle=m_TLF_DPS->AddNewSettingsTab("Regex");
        if(WData->RegexTabHandle==NULL)
            throw(0);

        WData->RegexRemoveGroup=m_TLF_UIAPI->AddGroupBox(WData->RegexTabHandle,
                "Remove lines matching");
        if(WData->RegexRemoveGroup==NULL)
            throw(0);
        for(r=0;r<MAX_REGEX;r++)
        {
            sprintf(buff,"Filter %d",r+1);
            WData->RegexRemoveFilterWid[r]=m_TLF_UIAPI->AddTextInput(WData->
                    RegexRemoveGroup->GroupWidgetHandle,buff,NULL,NULL);
            if(WData->RegexRemoveFilterWid[r]==NULL)
                throw(0);
        }

        WData->RegexIncludeGroup=m_TLF_UIAPI->AddGroupBox(WData->RegexTabHandle,
                "Only include lines matching");
        if(WData->RegexIncludeGroup==NULL)
            throw(0);
        for(r=0;r<MAX_REGEX;r++)
        {
            sprintf(buff,"Filter %d",r+1);
            WData->RegexIncludeFilterWid[r]=m_TLF_UIAPI->AddTextInput(WData->
                    RegexIncludeGroup->GroupWidgetHandle,buff,NULL,NULL);
            if(WData->RegexIncludeFilterWid[r]==NULL)
                throw(0);
        }

        /* Set UI to settings values */
        SimpleFilter_StartingWith=m_TLF_SysAPI->KVGetItem(Settings,"SimpleFilter_StartingWith");
        SimpleFilter_EndingWith=m_TLF_SysAPI->KVGetItem(Settings,"SimpleFilter_EndingWith");
        SimpleFilter_Contains=m_TLF_SysAPI->KVGetItem(Settings,"SimpleFilter_Contains");
        for(r=0;r<MAX_REGEX;r++)
        {
            sprintf(buff,"RegexFilter_RemoveFilter%d",r+1);
            RegexFilter_RemoveFilter[r]=
                    m_TLF_SysAPI->KVGetItem(Settings,buff);
        }
        for(r=0;r<MAX_REGEX;r++)
        {
            sprintf(buff,"RegexFilter_IncludeFilter%d",r+1);
            RegexFilter_IncludeFilter[r]=m_TLF_SysAPI->KVGetItem(Settings,buff);
        }

        /* Set defaults */
        if(SimpleFilter_StartingWith==NULL)
            SimpleFilter_StartingWith="";
        if(SimpleFilter_EndingWith==NULL)
            SimpleFilter_EndingWith="";
        if(SimpleFilter_Contains==NULL)
            SimpleFilter_Contains="";
        for(r=0;r<MAX_REGEX;r++)
        {
            if(RegexFilter_RemoveFilter[r]==NULL)
                RegexFilter_RemoveFilter[r]="";
            if(RegexFilter_IncludeFilter[r]==NULL)
                RegexFilter_IncludeFilter[r]="";
        }

        /* Set the widgets */

        /** Simple **/
        m_TLF_UIAPI->SetTextInputText(WData->SimpleTabHandle,
                WData->SimpleFilterStartsWith->Ctrl,SimpleFilter_StartingWith);
        m_TLF_UIAPI->SetTextInputText(WData->SimpleTabHandle,
                WData->SimpleFilterContains->Ctrl,SimpleFilter_Contains);
        m_TLF_UIAPI->SetTextInputText(WData->SimpleTabHandle,
                WData->SimpleFilterEndsWith->Ctrl,SimpleFilter_EndingWith);

        /** Regex **/
        /*** Remove ***/
        for(r=0;r<MAX_REGEX;r++)
        {
            m_TLF_UIAPI->SetTextInputText(WData->RegexRemoveGroup->
                    GroupWidgetHandle,WData->RegexRemoveFilterWid[r]->Ctrl,
                    RegexFilter_RemoveFilter[r]);
        }

        /*** Include ***/
        for(r=0;r<MAX_REGEX;r++)
        {
            m_TLF_UIAPI->SetTextInputText(WData->RegexIncludeGroup->
                    GroupWidgetHandle,WData->RegexIncludeFilterWid[r]->Ctrl,
                    RegexFilter_IncludeFilter[r]);
        }
    }
    catch(...)
    {
        if(WData!=NULL)
        {
            TextLineFilter_FreeSettingsWidgets(
                    (t_DataProSettingsWidgetsType *)WData);
        }
        return NULL;
    }

    return (t_DataProSettingsWidgetsType *)WData;
}

void TextLineFilter_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData)
{
    struct TextLineFilter_SettingsWidgets *WData=(struct TextLineFilter_SettingsWidgets *)PrivData;
    int_fast32_t r;

    for(r=MAX_REGEX-1;r>=0;r--)
    {
        if(WData->RegexIncludeFilterWid[r]!=NULL)
        {
            m_TLF_UIAPI->FreeTextInput(WData->RegexIncludeGroup->
                    GroupWidgetHandle,WData->RegexIncludeFilterWid[r]);
        }
        if(WData->RegexRemoveFilterWid[r]!=NULL)
        {
            m_TLF_UIAPI->FreeTextInput(WData->RegexRemoveGroup->GroupWidgetHandle,
                    WData->RegexRemoveFilterWid[r]);
        }
    }
    if(WData->RegexIncludeGroup!=NULL)
        m_TLF_UIAPI->FreeGroupBox(WData->RegexTabHandle,WData->RegexIncludeGroup);

    if(WData->RegexRemoveGroup!=NULL)
        m_TLF_UIAPI->FreeGroupBox(WData->RegexTabHandle,WData->RegexRemoveGroup);

    if(WData->SimpleHelpText!=NULL)
        m_TLF_UIAPI->FreeTextBox(WData->SimpleTabHandle,WData->SimpleHelpText);
    if(WData->SimpleFilterEndsWith!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->SimpleTabHandle,WData->SimpleFilterEndsWith);
    if(WData->SimpleFilterContains!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->SimpleTabHandle,WData->SimpleFilterContains);
    if(WData->SimpleFilterStartsWith!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->SimpleTabHandle,WData->SimpleFilterStartsWith);

    if(WData->HelpText!=NULL)
        m_TLF_UIAPI->FreeTextBox(WData->HelpTabHandle,WData->HelpText);

    delete WData;
}

void TextLineFilter_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings)
{
    struct TextLineFilter_SettingsWidgets *WData=(struct TextLineFilter_SettingsWidgets *)PrivData;
    string SimpleFilter_StartingWith;
    string SimpleFilter_EndingWith;
    string SimpleFilter_Contains;
    string RegexFilter_RemoveFilter[MAX_REGEX];
    string RegexFilter_IncludeFilter[MAX_REGEX];
    int r;
    char buff[100];

    /** Simple **/
    SimpleFilter_StartingWith=m_TLF_UIAPI->GetTextInputText(WData->SimpleTabHandle,WData->SimpleFilterStartsWith->Ctrl);
    SimpleFilter_Contains=m_TLF_UIAPI->GetTextInputText(WData->SimpleTabHandle,WData->SimpleFilterContains->Ctrl);
    SimpleFilter_EndingWith=m_TLF_UIAPI->GetTextInputText(WData->SimpleTabHandle,WData->SimpleFilterEndsWith->Ctrl);

    /** Regex **/
    /*** Remove ***/
    for(r=0;r<MAX_REGEX;r++)
    {
        RegexFilter_RemoveFilter[r]=m_TLF_UIAPI->GetTextInputText(WData->
                RegexRemoveGroup->GroupWidgetHandle,
                WData->RegexRemoveFilterWid[r]->Ctrl);
    }

    /*** Include ***/
    for(r=0;r<MAX_REGEX;r++)
    {
        RegexFilter_IncludeFilter[r]=m_TLF_UIAPI->GetTextInputText(WData->
                RegexIncludeGroup->GroupWidgetHandle,
                WData->RegexIncludeFilterWid[r]->Ctrl);
    }

    m_TLF_SysAPI->KVAddItem(Settings,"SimpleFilter_StartingWith",SimpleFilter_StartingWith.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"SimpleFilter_EndingWith",SimpleFilter_EndingWith.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"SimpleFilter_Contains",SimpleFilter_Contains.c_str());
    for(r=0;r<MAX_REGEX;r++)
    {
        sprintf(buff,"RegexFilter_RemoveFilter%d",r+1);
        m_TLF_SysAPI->KVAddItem(Settings,buff,
                RegexFilter_RemoveFilter[r].c_str());

        sprintf(buff,"RegexFilter_IncludeFilter%d",r+1);
        m_TLF_SysAPI->KVAddItem(Settings,buff,
                RegexFilter_IncludeFilter[r].c_str());
    }
}

static void TextLineFilter_ApplySettings(t_DataProcessorHandleType *DataHandle,
        t_PIKVList *Settings)
{
    struct TextLineFilterData *Data=(struct TextLineFilterData *)DataHandle;
    const char *SimpleFilter_StartingWith;
    const char *SimpleFilter_EndingWith;
    const char *SimpleFilter_Contains;
    const char *RegexFilter_RemoveFilter[MAX_REGEX];
    const char *RegexFilter_IncludeFilter[MAX_REGEX];
    int r;
    char buff[100];

    SimpleFilter_StartingWith=m_TLF_SysAPI->KVGetItem(Settings,"SimpleFilter_StartingWith");
    SimpleFilter_EndingWith=m_TLF_SysAPI->KVGetItem(Settings,"SimpleFilter_EndingWith");
    SimpleFilter_Contains=m_TLF_SysAPI->KVGetItem(Settings,"SimpleFilter_Contains");
    for(r=0;r<MAX_REGEX;r++)
    {
        sprintf(buff,"RegexFilter_RemoveFilter%d",r+1);
        RegexFilter_RemoveFilter[r]=m_TLF_SysAPI->KVGetItem(Settings,buff);
        sprintf(buff,"RegexFilter_IncludeFilter%d",r+1);
        RegexFilter_IncludeFilter[r]=m_TLF_SysAPI->KVGetItem(Settings,buff);
    }

    /* Set defaults */
    if(SimpleFilter_StartingWith==NULL)
        SimpleFilter_StartingWith="";
    if(SimpleFilter_EndingWith==NULL)
        SimpleFilter_EndingWith="";
    if(SimpleFilter_Contains==NULL)
        SimpleFilter_Contains="";
    for(r=0;r<MAX_REGEX;r++)
    {
        if(RegexFilter_RemoveFilter[r]==NULL)
            RegexFilter_RemoveFilter[r]="";
        if(RegexFilter_IncludeFilter[r]==NULL)
            RegexFilter_IncludeFilter[r]="";
    }

    TextLineFilter_ProcessSimpleFilter(SimpleFilter_StartingWith,
            Data->SimpleFilterStartsWithPat);
    TextLineFilter_ProcessSimpleFilter(SimpleFilter_Contains,
            Data->SimpleFilterContainsPat);
    TextLineFilter_ProcessSimpleFilter(SimpleFilter_EndingWith,
            Data->SimpleFilterEndsWithPat);

    for(r=0;r<MAX_REGEX;r++)
    {
        Data->RegexRemoveFilterStr[r]=RegexFilter_RemoveFilter[r];
        Data->RegexIncludeFilterStr[r]=RegexFilter_IncludeFilter[r];
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static bool TextLineFilter_GrowSearchStringsIfNeeded(char **SearchStrings,uint_fast32_t *SearchStringsSize,uint_fast32_t Size)
{
    char **NewSearchStr;
    if(Size>=*SearchStringsSize)
    {
        *SearchStringsSize+=SEARCH_STRINGS_GROW_SIZE;
        NewSearchStr=(char **)realloc((void *)SearchStrings,
                (*SearchStringsSize)*sizeof(char **));
        if(NewSearchStr==NULL)
            return false;
    }
    return true;
}

static bool TextLineFilter_ProcessSimpleFilter(const char *Filter,
        struct SimpleFilter &Result)
{
    uint_fast32_t Len;
    const char *s;
    char *d;
    uint_fast32_t SearchStringsSize;
    bool DoingQuote;
    bool DoingEsc;

    Len=strlen(Filter);

    Result.PatternStr=(char *)malloc(Len+1);
    if(Result.PatternStr==NULL)
        return false;

    SearchStringsSize=SEARCH_STRINGS_GROW_SIZE;
    Result.Patterns=(char **)malloc((SearchStringsSize)*sizeof(char **));
    if(Result.Patterns==NULL)
    {
        free(Result.PatternStr);
        return false;
    }

    /* Copy into our patterns buffer break it up as needed */
    s=Filter;
    d=Result.PatternStr;
    Result.PatternsCount=0;
    Result.Patterns[Result.PatternsCount]=d;
    DoingQuote=false;
    DoingEsc=false;
    while(*s!=0)
    {
        if(DoingEsc)
        {
            switch(*s)
            {
                case 'a':
                    *d++=0x07;
                break;
                case 'e':
                    *d++=0x1B;
                break;
                case 'f':
                    *d++=0x0C;
                break;
                case 't':
                    *d++=0x09;
                break;
                case 'v':
                    *d++=0x0B;
                break;
                case '\'':
                    *d++=0x27;
                break;
                case '\"':
                    *d++=0x22;
                break;
                case '\?':
                    *d++=0x3F;
                break;
                case '\\':
                    *d++='\\';
                break;
                default:
                    *d++=*s;
            }
            DoingEsc=false;
        }
        else
        {
            switch(*s)
            {
                case ' ':
                    if(DoingQuote)
                    {
                        *d++=*s;
                    }
                    else
                    {
                        /* If we don't have a blank string then add it */
                        if(Result.Patterns[Result.PatternsCount]!=d)
                        {
                            *d++=0;
                            if(!TextLineFilter_GrowSearchStringsIfNeeded(
                                    Result.Patterns,
                                    &SearchStringsSize,Result.PatternsCount+1))
                            {
                                return false;
                            }
                            Result.PatternsCount++;
                            Result.Patterns[Result.PatternsCount]=d;
                        }
                    }
                break;
                case '\\':
                    DoingEsc=true;
                break;
                case '\"':
                    DoingQuote=!DoingQuote;
                break;
                default:
                    *d++=*s;
            }
        }
        s++;
    }
    if(Result.Patterns[Result.PatternsCount]!=d)
    {
        *d++=0;
        if(!TextLineFilter_GrowSearchStringsIfNeeded(Result.Patterns,
                &SearchStringsSize,Result.PatternsCount+1))
        {
            return false;
        }
        Result.PatternsCount++;
        Result.Patterns[Result.PatternsCount]=d;
    }
    *d++=0; // End it as a string

    return true;
}

static void TextLineFilter_FreeSimpleFilter(struct SimpleFilter &Pat)
{
    free(Pat.PatternStr);
    free(Pat.Patterns);

    Pat.PatternStr=NULL;
    Pat.Patterns=NULL;
    Pat.PatternsCount=0;
}

/*******************************************************************************
 * NAME:
 *    TextLineFilter_HandleLine
 *
 * SYNOPSIS:
 *    void TextLineFilter_HandleLine(struct TextLineFilterData *Data);
 *
 * PARAMETERS:
 *    Data [I] -- Our data
 *
 * FUNCTION:
 *    This function handles when we finish reading a line.  It will check
 *    for any matches and delete / allow the line as needed.
 *
 *    It will then reset the mark.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static bool TextLineFilter_HandleLine(struct TextLineFilterData *Data)
{
    const char *Line;
    uint32_t Bytes;
    uint_fast32_t r;
    size_t Len;
    size_t LineLen;
    bool DeleteLine;
    const char *Pat;
    bool FoundMatch;
    bool AllBlank;

    Line=(const char *)m_TLF_DPS->GetFrozenString(&Bytes);
    if(Line==NULL)
        return false;

    DeleteLine=false;

    /* Simple patterns */
    for(r=0;r<Data->SimpleFilterStartsWithPat.PatternsCount;r++)
    {
        Pat=Data->SimpleFilterStartsWithPat.Patterns[r];
        Len=strlen(Pat);
        if(strncmp(Line,Pat,Len)==0)
        {
            DeleteLine=true;
            break;  // No need to finish
        }
    }

    if(!DeleteLine)
    {
        for(r=0;r<Data->SimpleFilterContainsPat.PatternsCount;r++)
        {
            Pat=Data->SimpleFilterContainsPat.Patterns[r];
            Len=strlen(Pat);
            if(strstr(Line,Pat)!=NULL)
            {
                DeleteLine=true;
                break;  // No need to finish
            }
        }
    }

    if(!DeleteLine)
    {
        for(r=0;r<Data->SimpleFilterEndsWithPat.PatternsCount;r++)
        {
            Pat=Data->SimpleFilterEndsWithPat.Patterns[r];
            Len=strlen(Pat);
            LineLen=strlen(Line);
            if(LineLen>Len && strncmp(&Line[LineLen-Len],Pat,Len)==0)
            {
                DeleteLine=true;
                break;  // No need to finish
            }
        }
    }

    /* Handle regex's that delete lines */
    if(!DeleteLine)
    {
        for(r=0;r<MAX_REGEX;r++)
        {
            if(!Data->RegexRemoveFilterStr[r].empty())
            {
                regex RxPattern(Data->RegexRemoveFilterStr[r]);
                if(regex_search((const char *)Line,RxPattern))
                {
                    DeleteLine=true;
                    break;  // No need to finish
                }
            }
        }
    }

    if(!DeleteLine)
    {
        FoundMatch=false;
        AllBlank=true;
        for(r=0;r<MAX_REGEX;r++)
        {
            if(!Data->RegexIncludeFilterStr[r].empty())
            {
                AllBlank=false;
                regex RxPattern(Data->RegexIncludeFilterStr[r]);
                if(regex_search((const char *)Line,RxPattern))
                {
                    FoundMatch=true;
                    break;  // No need to finish
                }
            }
        }
        /* If we didn't find a match then we delete the line */
        if(!AllBlank && !FoundMatch)
            DeleteLine=true;
    }

    if(DeleteLine)
        m_TLF_DPS->ClearFrozenStream();

    m_TLF_DPS->ReleaseFrozenStream();

    Data->FreezeStream=true;

    return DeleteLine;
}
