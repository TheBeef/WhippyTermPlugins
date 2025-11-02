/*******************************************************************************
 * FILENAME: InsertTimeStamp.cpp
 *
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    This has the new line processor that converts end of lines to always
 *    move the cursor to the new line.
 *
 * COPYRIGHT:
 *    Copyright 2025 Paul Hutchinson.
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
 *    Paul Hutchinson (23 Sep 2025)
 *
 ******************************************************************************/

/*** HEADER FILES TO INCLUDE  ***/
#include "InsertTimeStamp.h"
#include "PluginSDK/Plugin.h"
#include <string.h>
#include <time.h>
#include <string>

using namespace std;

/*** DEFINES                  ***/
#define REGISTER_PLUGIN_FUNCTION_PRIV_NAME      InsertTimeStamp // The name to append on the RegisterPlugin() function for built in version
#define NEEDED_MIN_API_VERSION                  0x01000000

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/
struct InsertTimeStampData
{
    string DateFormat;
    bool SeenNewLine;
};

struct InsertTimeStamp_SettingsWidgets
{
    t_WidgetSysHandle *WidgetHandle;
    struct PI_TextInput *DateFormatInput;
    struct PI_TextBox *ExplainInput;
};

/*** FUNCTION PROTOTYPES      ***/
static const struct DataProcessorInfo *InsertTimeStamp_GetProcessorInfo(unsigned int *SizeOfInfo);
static void InsertTimeStamp_ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,PG_BOOL *Consumed);
static t_DataProSettingsWidgetsType *InsertTimeStamp_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings);
static void InsertTimeStamp_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData);
static void InsertTimeStamp_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings);
static void InsertTimeStamp_ApplySettings(t_DataProcessorHandleType *ConDataHandle,
        t_PIKVList *Settings);
static t_DataProcessorHandleType *InsertTimeStamp_AllocateData(void);
static void InsertTimeStamp_FreeData(t_DataProcessorHandleType *DataHandle);

/*** VARIABLE DEFINITIONS     ***/
struct DataProcessorAPI m_InsertTimeStampCBs=
{
    InsertTimeStamp_AllocateData,
    InsertTimeStamp_FreeData,
    InsertTimeStamp_GetProcessorInfo,               // GetProcessorInfo
    NULL,                                           // ProcessKeyPress
    InsertTimeStamp_ProcessIncomingTextByte,        // ProcessIncomingTextByte
    NULL,                                           // ProcessIncomingBinaryByte
    /* V2 */
    NULL,                                           // ProcessOutGoingData
    InsertTimeStamp_AllocSettingsWidgets,
    InsertTimeStamp_FreeSettingsWidgets,
    InsertTimeStamp_SetSettingsFromWidgets,
    InsertTimeStamp_ApplySettings,
};
struct DataProcessorInfo m_InsertTimeStamp_Info=
{
    "Insert Timpstamp",
    "Added a timestamp after a '\\n'",
    "Adds a timestamp to the incoming text stream after a new line char",
    e_DataProcessorType_Text,
    e_TextDataProcessorClass_Other
};

static const struct DPS_API *m_ITS_DPS;
static const struct PI_UIAPI *m_ITS_UIAPI;
static const struct PI_SystemAPI *m_ITS_SysAPI;

/*******************************************************************************
 * NAME:
 *    InsertTimeStamp_RegisterPlugin
 *
 * SYNOPSIS:
 *    unsigned int InsertTimeStamp_RegisterPlugin(const struct PI_SystemAPI *SysAPI,
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

        m_ITS_SysAPI=SysAPI;
        m_ITS_DPS=SysAPI->GetAPI_DataProcessors();
        m_ITS_UIAPI=m_ITS_DPS->GetAPI_UI();

        /* If we are have the correct experimental API */
        if(SysAPI->GetExperimentalID()>0 &&
                SysAPI->GetExperimentalID()<1)
        {
            return 0xFFFFFFFF;
        }

        m_ITS_DPS->RegisterDataProcessor("InsertTimeStamp",
                &m_InsertTimeStampCBs,sizeof(m_InsertTimeStampCBs));

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
static t_DataProcessorHandleType *InsertTimeStamp_AllocateData(void)
{
    struct InsertTimeStampData *Data;

    Data=NULL;
    try
    {
        Data=new struct InsertTimeStampData;

        Data->SeenNewLine=false;
        Data->DateFormat="%c:";
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
static void InsertTimeStamp_FreeData(t_DataProcessorHandleType *DataHandle)
{
    struct InsertTimeStampData *Data=(struct InsertTimeStampData *)DataHandle;

    delete Data;
}

/*******************************************************************************
 * NAME:
 *    InsertTimeStamp_GetProcessorInfo
 *
 * SYNOPSIS:
 *    const struct DataProcessorInfo *InsertTimeStamp_GetProcessorInfo(
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
const struct DataProcessorInfo *InsertTimeStamp_GetProcessorInfo(
        unsigned int *SizeOfInfo)
{
    *SizeOfInfo=sizeof(struct DataProcessorInfo);
    return &m_InsertTimeStamp_Info;
}

/*******************************************************************************
 *  NAME:
 *    InsertTimeStamp_ProcessIncomingTextByte
 *
 *  SYNOPSIS:
 *    void InsertTimeStamp_ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,
 *          const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,
 *          PG_BOOL *Consumed)
 *
 *  PARAMETERS:
 *    DataHandle [I] -- The data handle to free.  This will need to be
 *                      case to your internal data type before you use it.
 *    ConnectionID [I] -- The connection to work on
 *    RawByte [I] -- The raw byte to process.  This is the byte that came in.
 *    ProcessedChar [I/O] -- This is a unicode char that has already been
 *                         processed by some of the other input filters.  You
 *                         can change this as you need.  It must remain only
 *                         one unicode char.
 *    CharLen [I/O] -- This number of bytes in 'ProcessedChar'
 *    Consumed [I/O] -- This tells the system (and other filters) if the
 *                      char has been used up and will not be added to the
 *                      screen.
 *    Style [I/O] -- This is the current style that the char will be added
 *                   with.  You can change this to change how the char will
 *                   be added.
 *
 *  FUNCTION:
 *    This function is called for each byte that comes in.
 *
 *  RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void InsertTimeStamp_ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,
        const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,
        PG_BOOL *Consumed)
{
    struct InsertTimeStampData *Data=(struct InsertTimeStampData *)DataHandle;
    char buff[100];
    time_t current_time;
    struct tm *time_info;

    if(Data->SeenNewLine && RawByte!='\r')
    {
        Data->SeenNewLine=false;
        time(&current_time);
        time_info = localtime(&current_time);
        strftime(buff,sizeof(buff)-1,Data->DateFormat.c_str(),time_info);
        m_ITS_DPS->InsertString((uint8_t *)buff,strlen(buff));
    }

    if(RawByte=='\n')
    {
        /* We are at the end of the line, note that we should output a new
           timestamp , see if it matches anything */
        Data->SeenNewLine=true;
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
t_DataProSettingsWidgetsType *InsertTimeStamp_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings)
{
    struct InsertTimeStamp_SettingsWidgets *WData;
    const char *Str;

    try
    {
        WData=new InsertTimeStamp_SettingsWidgets;
        WData->WidgetHandle=WidgetHandle;

        /* Zero everything */
        WData->DateFormatInput=NULL;
        WData->ExplainInput=NULL;

        /* Add widgets */
        m_ITS_DPS->SetCurrentSettingsTabName("Timestamp");

        WData->DateFormatInput=m_ITS_UIAPI->AddTextInput(WidgetHandle,
                "Date Format",NULL,NULL);
        if(WData->DateFormatInput==NULL)
            throw(0);

        WData->ExplainInput=m_ITS_UIAPI->AddTextBox(WidgetHandle,
                "Help",
                "The date format uses the same format as strftime():\n\n"
                "%a -- Abbreviated name of the day of the week\n"
                "%A -- Full name of the day of the week\n"
                "%b -- Abbreviated month name\n"
                "%B -- Full month name\n"
                "%c -- Date and time (current locale)\n"
                "%d -- Day of the month (01-31)\n"
                "%F -- %Y-%m-%d (ISO 8601 date format).\n"
                "%H -- Hour (24-hour) (00-23)\n"
                "%I -- Hour (12-hour) (01-12)\n"
                "%j -- Day of the year (001-366)\n"
                "%k -- Hour (24-hour) (0-23)\n"
                "%l -- Hour (12-hour) (1-12)\n"
                "%m -- Month as a number (01-12)\n"
                "%M -- Minute (00-59)\n"
                "%n -- A newline character\n"
                "%p -- AM/PM\n"
                "%P -- am/pm\n"
                "%s -- Number of seconds since Unix Epoch\n"
                "%S -- Seconds (00-60)\n"
                "%u -- Day of the week (1-7) Monday=1\n"
                "%U -- Week number (00-53)\n"
                "%w -- Day of the week (0-6) Sunday=0\n"
                "%W -- Week number (00-53)\n"
                "%y -- Year (00-99)\n"
                "%Y -- Year (0000-9999)\n"
                "%z -- +hhmm or -hhmm numeric timezone\n");
        if(WData->ExplainInput==NULL)
            throw(0);

        /* Set widgets to stored settings */
        Str=m_ITS_SysAPI->KVGetItem(Settings,"DateFormat");
        if(Str==NULL)
            Str="%c:";
        m_ITS_UIAPI->SetTextInputText(WidgetHandle,WData->DateFormatInput->Ctrl,
                Str);
    }
    catch(...)
    {
        if(WData!=NULL)
        {
            InsertTimeStamp_FreeSettingsWidgets(
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
void InsertTimeStamp_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData)
{
    struct InsertTimeStamp_SettingsWidgets *WData=(struct InsertTimeStamp_SettingsWidgets *)PrivData;

    /* Free everything in reverse order */
    if(WData->DateFormatInput!=NULL)
        m_ITS_UIAPI->FreeTextInput(WData->WidgetHandle,WData->DateFormatInput);

    if(WData->ExplainInput!=NULL)
        m_ITS_UIAPI->FreeTextBox(WData->WidgetHandle,WData->ExplainInput);

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
void InsertTimeStamp_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings)
{
    struct InsertTimeStamp_SettingsWidgets *WData=(struct InsertTimeStamp_SettingsWidgets *)PrivData;
    string Str;

    Str=m_ITS_UIAPI->GetTextInputText(WData->WidgetHandle,
            WData->DateFormatInput->Ctrl);
    m_ITS_SysAPI->KVAddItem(Settings,"DateFormat",Str.c_str());
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
static void InsertTimeStamp_ApplySettings(t_DataProcessorHandleType *DataHandle,
        t_PIKVList *Settings)
{
    struct InsertTimeStampData *Data=(struct InsertTimeStampData *)DataHandle;
    const char *Str;

    Str=m_ITS_SysAPI->KVGetItem(Settings,"DateFormat");
    if(Str==NULL)
        Str="%c:";
    Data->DateFormat=Str;
}
