/*******************************************************************************
 * FILENAME: SEGGER_RTT_OS_JLinkARM.cpp
 *
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    This file has the Linux version of the JLinkARM shared libary access
 *    in it.
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
#include "../SEGGER_RTT_JLinkARM.h"
#include "../../SEGGER_RTT_Main.h"
#include "../../SEGGER_RTT_Common.h"
#include "../../SEGGER_RTT_AuxWidgets.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>

/*** DEFINES                  ***/

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/
struct SEGGER_RTT_OurData
{
    struct SEGGER_RTT_Common CommonData;
    pthread_t ThreadInfo;
    pthread_mutex_t lock;
    volatile bool RequestThreadQuit;
    volatile bool ThreadHasQuit;
    volatile bool Opened;
};

/*** FUNCTION PROTOTYPES      ***/
static void *SEGGER_RTT_OS_PollThread(void *arg);

/*** VARIABLE DEFINITIONS     ***/

void SEGGER_RTT_AppendFilename2Path(std::string &FullPath,const char *Path,const char *File)
{
    FullPath=Path;
    if(FullPath.back()!='/')
        FullPath+="/";
    FullPath+=File;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Init
 *
 * SYNOPSIS:
 *    PG_BOOL SEGGER_RTT_Init(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    This function init's anything needed for the plugin.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error.  The main system will tell the user that
 *             the plugin failed and remove this plugin from the list of
 *             available plugins.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
PG_BOOL SEGGER_RTT_Init(void)
{
    void *SoHandle;

    /* Find the JLinkARM .so and then get all the functions we use */
    SoHandle=dlopen("/opt/SEGGER/JLink/libjlinkarm.so",RTLD_NOW);
    if(SoHandle==NULL)
        return false;

    g_SRTT_JLinkAPI.Open=(const char *(*)(void))dlsym(SoHandle,"JLINKARM_Open");
    g_SRTT_JLinkAPI.Close=(void (*)(void))dlsym(SoHandle,"JLINKARM_Close");
    g_SRTT_JLinkAPI.Go=(void (*)(void))dlsym(SoHandle,"JLINKARM_Go");
    g_SRTT_JLinkAPI.Halt=(char (*)(void))dlsym(SoHandle,"JLINKARM_Halt");
    g_SRTT_JLinkAPI.Halt=(char (*)(void))dlsym(SoHandle,"JLINKARM_Halt");
    g_SRTT_JLinkAPI.ResetNoHalt=(void (*)(void))dlsym(SoHandle,"JLINKARM_ResetNoHalt");
    g_SRTT_JLinkAPI.Connect=(int (*)(void))dlsym(SoHandle,"JLINKARM_Connect");
    g_SRTT_JLinkAPI.DEVICE_GetIndex=(int (*)(const char *DeviceName))dlsym(SoHandle,"JLINKARM_DEVICE_GetIndex");
    g_SRTT_JLinkAPI.DEVICE_SelectDialog=(int (*)(void *hParent,uint32_t Flags,struct JLINKARM_DEVICE_SELECT_INFO *Info))dlsym(SoHandle,"JLINKARM_DEVICE_SelectDialog");
    g_SRTT_JLinkAPI.TIF_Select=(int (*)(int Interface))dlsym(SoHandle,"JLINKARM_TIF_Select");
    g_SRTT_JLinkAPI.SelDevice=(void (*)(uint16_t DeviceIndex))dlsym(SoHandle,"JLINKARM_SelDevice");
    g_SRTT_JLinkAPI.DEVICE_GetInfo=(int (*)(int DeviceIndex,struct JLINKARM_DEVICE_INFO *DeviceInfo))dlsym(SoHandle,"JLINKARM_DEVICE_GetInfo");
    g_SRTT_JLinkAPI.ExecCommand=(int (*)(const char *In,char *Error,int BufferSize))dlsym(SoHandle,"JLINKARM_ExecCommand");
    g_SRTT_JLinkAPI.SetSpeed=(void (*)(uint32_t Speed))dlsym(SoHandle,"JLINKARM_SetSpeed");
    g_SRTT_JLinkAPI.EMU_SelectIP=(int (*)(char *IPAddr,int BufferSize,uint16_t *pPort))dlsym(SoHandle,"JLINKARM_EMU_SelectIP");
    g_SRTT_JLinkAPI.ConfigJTAG=(void (*)(int,int))dlsym(SoHandle,"JLINKARM_ConfigJTAG");
    g_SRTT_JLinkAPI.EMU_GetList=(int (*)(int HostIFs,struct JLINKARM_EMU_CONNECT_INFO *ConnectInfo,int MaxInfos))dlsym(SoHandle,"JLINKARM_EMU_GetList");
    g_SRTT_JLinkAPI.EMU_GetNumDevices=(uint32_t (*)(void))dlsym(SoHandle,"JLINKARM_EMU_GetNumDevices");
    g_SRTT_JLinkAPI.EMU_SelectByUSBSN=(int (*)(uint32_t))dlsym(SoHandle,"JLINKARM_EMU_SelectByUSBSN");
    g_SRTT_JLinkAPI.SelectUSB=(char (*)(int))dlsym(SoHandle,"JLINKARM_SelectUSB");
    g_SRTT_JLinkAPI.EMU_SelectIPBySN=(void (*)(uint32_t))dlsym(SoHandle,"JLINKARM_EMU_SelectIPBySN");
    g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Control=(int (*)(uint32_t Cmd, void *parms))dlsym(SoHandle,"JLINK_RTTERMINAL_Control");
    g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Read=(int (*)(uint32_t BufferIndex,char *Buffer,uint32_t BufferSize))dlsym(SoHandle,"JLINK_RTTERMINAL_Read");
    g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Write=(int (*)(uint32_t BufferIndex,const char *Buffer,uint32_t BufferSize))dlsym(SoHandle,"JLINK_RTTERMINAL_Write");

    if(g_SRTT_JLinkAPI.Open==NULL ||
            g_SRTT_JLinkAPI.Close==NULL ||
            g_SRTT_JLinkAPI.Go==NULL ||
            g_SRTT_JLinkAPI.Halt==NULL ||
            g_SRTT_JLinkAPI.ResetNoHalt==NULL ||
            g_SRTT_JLinkAPI.Connect==NULL ||
            g_SRTT_JLinkAPI.DEVICE_GetIndex==NULL ||
            g_SRTT_JLinkAPI.DEVICE_SelectDialog==NULL ||
            g_SRTT_JLinkAPI.TIF_Select==NULL ||
            g_SRTT_JLinkAPI.SelDevice==NULL ||
            g_SRTT_JLinkAPI.DEVICE_GetInfo==NULL ||
            g_SRTT_JLinkAPI.ExecCommand==NULL ||
            g_SRTT_JLinkAPI.SetSpeed==NULL ||
            g_SRTT_JLinkAPI.EMU_SelectIP==NULL ||
            g_SRTT_JLinkAPI.ConfigJTAG==NULL ||
            g_SRTT_JLinkAPI.EMU_GetList==NULL ||
            g_SRTT_JLinkAPI.EMU_GetNumDevices==NULL ||
            g_SRTT_JLinkAPI.EMU_SelectByUSBSN==NULL ||
            g_SRTT_JLinkAPI.SelectUSB==NULL ||
            g_SRTT_JLinkAPI.EMU_SelectIPBySN==NULL ||
            g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Control==NULL ||
            g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Read==NULL ||
            g_SRTT_JLinkAPI.JLINK_RTTERMINAL_Write==NULL)
    {
        dlclose(SoHandle);
        return false;
    }

    return true;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_AllocateHandle
 *
 * SYNOPSIS:
 *    t_DriverIOHandleType *SEGGER_RTT_AllocateHandle(const char *DeviceUniqueID,
 *          t_IOSystemHandle *IOHandle);
 *
 * PARAMETERS:
 *    DeviceUniqueID [I] -- This is the unique ID for the device we are working
 *                          on.
 *    IOHandle [I] -- A handle to the IO system.  This is used when talking
 *                    the IO system (just store it and pass it went needed).
 *
 * FUNCTION:
 *    This function allocates a connection to the device.
 *
 *    The system uses two different objects to talk in/out of a device driver.
 *
 *    The first is the Detect Device ID.  This is used to assign a value to
 *    all the devices detected on the system.  This is just a number (that
 *    is big enough to store a pointer).  The might be the comport number
 *    (COM1 and COM2 might be 1,2) or a pointer to a string with the
 *    path to the OS device driver (or really anything the device driver wants
 *    to use).
 *    This is used by the system to know what device out of all the available
 *    devices it is talk about.
 *    These may be allocated when the system wants a list of devices, or it
 *    may not be allocated at all.
 *
 *    The second is the t_DriverIOHandleType.  This is a handle is allocated
 *    when the user opens a new connection (new tab).  It contains any needed
 *    data for a connection.  This is not the same as opening the connection
 *    (which actually opens the device with the OS).
 *    This may include things like allocating buffers, a place to store the
 *    handle returned when the driver actually opens the device with the OS
 *    and anything else the driver needs.
 *
 * RETURNS:
 *    Newly allocated data for this connection or NULL on error.
 *
 * SEE ALSO:
 *    ConvertConnectionUniqueID2DriverID(), DetectNextConnection(),
 *    FreeHandle()
 ******************************************************************************/
t_DriverIOHandleType *SEGGER_RTT_AllocateHandle(const char *DeviceUniqueID,
        t_IOSystemHandle *IOHandle)
{
    struct SEGGER_RTT_OurData *NewData;

    NewData=NULL;
    try
    {
        NewData=new struct SEGGER_RTT_OurData;
        NewData->CommonData.IOHandle=IOHandle;
        NewData->CommonData.DeviceUniqueID=DeviceUniqueID;
        NewData->RequestThreadQuit=false;
        NewData->ThreadHasQuit=false;
        NewData->Opened=false;

        if(pthread_mutex_init(&NewData->lock,NULL)!=0)
            throw(0);

        /* Startup the thread for polling if we have data available */
        if(pthread_create(&NewData->ThreadInfo,NULL,SEGGER_RTT_OS_PollThread,
                NewData)<0)
        {
            throw(0);
        }
    }
    catch(...)
    {
        if(NewData!=NULL)
            delete NewData;
        return NULL;
    }

    return (t_DriverIOHandleType *)NewData;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_FreeHandle
 *
 * SYNOPSIS:
 *    void SEGGER_RTT_FreeHandle(t_DriverIOHandleType *DriverIO);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *
 * FUNCTION:
 *    This function frees the data allocated with AllocateHandle().
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    AllocateHandle()
 ******************************************************************************/
void SEGGER_RTT_FreeHandle(t_DriverIOHandleType *DriverIO)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;

    /* Tell thread to quit */
    OurData->RequestThreadQuit=true;

    /* Wait for the thread to exit */
    while(!OurData->ThreadHasQuit)
        usleep(1000);   // Wait 1 ms

    pthread_mutex_destroy(&OurData->lock);

    delete OurData;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Open
 *
 * SYNOPSIS:
 *    PG_BOOL SEGGER_RTT_Open(t_DriverIOHandleType *DriverIO,const t_PIKVList *Options);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *    Options [I] -- The options to apply to this connection.
 *
 * FUNCTION:
 *    This function opens the OS device.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error.
 *
 * SEE ALSO:
 *    SEGGER_RTT_Close(), SEGGER_RTT_Read(), SEGGER_RTT_Write(),
 *    SEGGER_RTT_ChangeOptions()
 ******************************************************************************/
PG_BOOL SEGGER_RTT_Open(t_DriverIOHandleType *DriverIO,const t_PIKVList *Options)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;
    PG_BOOL RetValue;

    pthread_mutex_lock(&OurData->lock);
    RetValue=SEGGER_RTT_Commom_Open(DriverIO,Options,&OurData->CommonData);
    pthread_mutex_unlock(&OurData->lock);

    if(RetValue)
    {
        OurData->Opened=true;
        g_SRTT_IOSystem->DrvDataEvent(OurData->CommonData.IOHandle,
                e_DataEventCode_Connected);
    }

    return RetValue;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Close
 *
 * SYNOPSIS:
 *    void SEGGER_RTT_Close(t_DriverIOHandleType *DriverIO);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *
 * FUNCTION:
 *    This function closes a connection that was opened with Open()
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    SEGGER_RTT_Open()
 ******************************************************************************/
void SEGGER_RTT_Close(t_DriverIOHandleType *DriverIO)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;

    /* Close it before we do anything else so the polling thread does try
       to read more data */
    OurData->Opened=false;

    pthread_mutex_lock(&OurData->lock);
    SEGGER_RTT_Commom_Close(DriverIO,&OurData->CommonData);
    pthread_mutex_unlock(&OurData->lock);
    g_SRTT_IOSystem->DrvDataEvent(OurData->CommonData.IOHandle,
            e_DataEventCode_Disconnected);
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Read
 *
 * SYNOPSIS:
 *    int SEGGER_RTT_Read(t_DriverIOHandleType *DriverIO,uint8_t *Data,int Bytes);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *    Data [I] -- A buffer to store the data that was read.
 *    Bytes [I] -- The max number of bytes that can be stored in 'Data'
 *
 * FUNCTION:
 *    This function reads data from the device and stores it in 'Data'
 *
 * RETURNS:
 *    The number of bytes that was read or:
 *      RETERROR_NOBYTES -- No bytes was read (0)
 *      RETERROR_DISCONNECT -- This device is no longer open.
 *      RETERROR_IOERROR -- There was an IO error.
 *      RETERROR_BUSY -- The device is currently busy.  Try again later
 *
 * SEE ALSO:
 *    SEGGER_RTT_Open(), SEGGER_RTT_Write()
 ******************************************************************************/
int SEGGER_RTT_Read(t_DriverIOHandleType *DriverIO,uint8_t *Data,int MaxBytes)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;
    int RetValue;

    pthread_mutex_lock(&OurData->lock);
    RetValue=SEGGER_RTT_Common_Read(DriverIO,Data,MaxBytes,&OurData->CommonData);
    pthread_mutex_unlock(&OurData->lock);
    return RetValue;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Write
 *
 * SYNOPSIS:
 *    int SEGGER_RTT_Write(t_DriverIOHandleType *DriverIO,const uint8_t *Data,int Bytes);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *    Data [I] -- The data to write to the device.
 *    Bytes [I] -- The number of bytes to write.
 *
 * FUNCTION:
 *    This function writes (sends) data to the device.
 *
 * RETURNS:
 *    The number of bytes written or:
 *      RETERROR_NOBYTES -- No bytes was written (0)
 *      RETERROR_DISCONNECT -- This device is no longer open.
 *      RETERROR_IOERROR -- There was an IO error.
 *      RETERROR_BUSY -- The device is currently busy.  Try again later
 *
 * SEE ALSO:
 *    SEGGER_RTT_Open(), SEGGER_RTT_Read()
 ******************************************************************************/
int SEGGER_RTT_Write(t_DriverIOHandleType *DriverIO,const uint8_t *Data,int Bytes)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;
    int RetValue;

    pthread_mutex_lock(&OurData->lock);
    RetValue=SEGGER_RTT_Common_Write(DriverIO,Data,Bytes,&OurData->CommonData);
    pthread_mutex_unlock(&OurData->lock);
    return RetValue;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_ChangeOptions
 *
 * SYNOPSIS:
 *    PG_BOOL SEGGER_RTT_ChangeOptions(t_DriverIOHandleType *DriverIO,
 *          const t_PIKVList *Options);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *    Options [I] -- The options to apply to this connection.
 *
 * FUNCTION:
 *    This function changes the connection options on an open device.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error.
 *
 * SEE ALSO:
 *    Open()
 ******************************************************************************/
PG_BOOL SEGGER_RTT_ChangeOptions(t_DriverIOHandleType *DriverIO,
        const t_PIKVList *Options)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;

    if(OurData->Opened)
    {
        SEGGER_RTT_Close(DriverIO);
        return SEGGER_RTT_Open(DriverIO,Options);
    }
    return true;
}

/*******************************************************************************
 * NAME:
 *    ConnectionAuxCtrlWidgets_AllocWidgets
 *
 * SYNOPSIS:
 *    t_ConnectionWidgetsType *ConnectionAuxCtrlWidgets_AllocWidgets(
 *          t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *    WidgetHandle [I] -- The handle to send to the widgets
 *
 * FUNCTION:
 *    This function adds aux control widgets to the aux control tab in the
 *    main window.  The aux controls are for extra controls that do things /
 *    display things going on with the driver.  For example there are things
 *    like the status of the CTS line and to set the RTS line.
 *
 *    The device driver needs to keep handles to the widgets added because it
 *    needs to free them when ConnectionAuxCtrlWidgets_FreeWidgets() called.
 *
 * RETURNS:
 *    The private options data that you want to use.  This is a private
 *    structure that you allocate and then cast to
 *    (t_ConnectionAuxCtrlWidgetsType *) when you return.
 *
 * NOTES:
 *    These widgets can only be accessed in the main thread.  They are not
 *    thread safe.
 *
 * SEE ALSO:
 *    ConnectionAuxCtrlWidgets_FreeWidgets()
 ******************************************************************************/
t_ConnectionWidgetsType *SEGGER_RTT_ConnectionAuxCtrlWidgets_AllocWidgets(t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;
    t_ConnectionWidgetsType *RetValue;

    pthread_mutex_lock(&OurData->lock);
    RetValue=SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_AllocWidgets(
            DriverIO,WidgetHandle,&OurData->CommonData);
    pthread_mutex_unlock(&OurData->lock);
    return RetValue;
}


/*******************************************************************************
 * NAME:
 *    ConnectionAuxCtrlWidgets_FreeWidgets
 *
 * SYNOPSIS:
 *    void ConnectionAuxCtrlWidgets_FreeWidgets(t_DriverIOHandleType *DriverIO,
 *          t_WidgetSysHandle *WidgetHandle,
 *          t_ConnectionWidgetsType *ConAuxCtrls);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *    WidgetHandle [I] -- The handle to send to the widgets
 *    ConAuxCtrls [I] -- The aux controls data that was allocated with
 *          ConnectionAuxCtrlWidgets_AllocWidgets().
 *
 * FUNCTION:
 *    Frees the widgets added with ConnectionAuxCtrlWidgets_AllocWidgets()
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    ConnectionAuxCtrlWidgets_AllocWidgets()
 ******************************************************************************/
void SEGGER_RTT_ConnectionAuxCtrlWidgets_FreeWidgets(t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConAuxCtrls)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;

    pthread_mutex_lock(&OurData->lock);
    SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_FreeWidgets(DriverIO,
            WidgetHandle,ConAuxCtrls,&OurData->CommonData);
    pthread_mutex_unlock(&OurData->lock);
}

void SEGGER_RTT_LockMutex(t_DriverIOHandleType *DriverIO)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;

    pthread_mutex_lock(&OurData->lock);
}

void SEGGER_RTT_UnLockMutex(t_DriverIOHandleType *DriverIO)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;

    pthread_mutex_unlock(&OurData->lock);
}

static void *SEGGER_RTT_OS_PollThread(void *arg)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)arg;

    while(!OurData->RequestThreadQuit)
    {
        if(!OurData->Opened)
        {
            usleep(100000);   // Wait 100ms
            continue;
        }
        else
        {
            /* Poll for data */
            pthread_mutex_lock(&OurData->lock);
            SEGGER_RTT_Commom_PollingThread(&OurData->CommonData);
            pthread_mutex_unlock(&OurData->lock);
        }
        usleep(1000);   // Wait 1ms
    }

    OurData->ThreadHasQuit=true;

    return 0;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_GetLastErrorMessage
 *
 * SYNOPSIS:
 *    const char *SEGGER_RTT_GetLastErrorMessage(t_DriverIOHandleType *DriverIO);
 *
 * PARAMETERS:
 *    DriverIO [I] -- The handle to this connection
 *
 * FUNCTION:
 *    This function is optional.
 *
 *    This function gets info about the last error.  If there wasn't an error
 *    or no error info is available then this function returns NULL.
 *
 * RETURNS:
 *    A static buffer with the error message in it.  This buffer must remain
 *    valid until another call to the driver (at which point it can be free'ed
 *    for overriden).
 *
 *    If there isn't an error or an error message could not be built then
 *    this function can return NULL.
 *
 * API VERSION:
 *    2
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
const char *SEGGER_RTT_GetLastErrorMessage(t_DriverIOHandleType *DriverIO)
{
    struct SEGGER_RTT_OurData *OurData=(struct SEGGER_RTT_OurData *)DriverIO;

    if(OurData->CommonData.LastErrorMsg=="")
        return NULL;

    return OurData->CommonData.LastErrorMsg.c_str();
}
