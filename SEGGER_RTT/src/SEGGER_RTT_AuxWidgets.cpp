/*******************************************************************************
 * FILENAME: SEGGER_RTT_AuxWidgets.cpp
 *
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    This file has the aux widget handling in it.
 *
 * COPYRIGHT:
 *    Copyright 26 May 2025 Paul Hutchinson.
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
 *    Paul Hutchinson (26 May 2025)
 *
 ******************************************************************************/

/*** HEADER FILES TO INCLUDE  ***/
#include "SEGGER_RTT_AuxWidgets.h"
#include "OS/SEGGER_RTT_JLinkARM.h"
#include <string.h>

/*** DEFINES                  ***/

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/

/*** FUNCTION PROTOTYPES      ***/
static void SEGGER_RTT_HaltButton(const struct PIButtonEvent *Event,void *UserData);
static void SEGGER_RTT_GoButton(const struct PIButtonEvent *Event,void *UserData);
static void SEGGER_RTT_ResetButton(const struct PIButtonEvent *Event,void *UserData);

/*** VARIABLE DEFINITIONS     ***/

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
t_ConnectionWidgetsType *SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_AllocWidgets(
        t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle,
        struct SEGGER_RTT_Common *CommonData)
{
    struct SEGGER_RTT_ConAuxWidgets *ConAuxWidgets;

    ConAuxWidgets=NULL;
    try
    {
        ConAuxWidgets=new struct SEGGER_RTT_ConAuxWidgets;
        ConAuxWidgets->IOHandle=DriverIO;
        ConAuxWidgets->WidgetHandle=WidgetHandle;
        ConAuxWidgets->Halt=NULL;
        ConAuxWidgets->Go=NULL;
        ConAuxWidgets->Reset=NULL;

        CommonData->AuxWidgets=ConAuxWidgets;

        ConAuxWidgets->Halt=g_SRTT_UI->AddButtonInput(WidgetHandle,
                "Halt",SEGGER_RTT_HaltButton,CommonData);
        if(ConAuxWidgets->Halt==NULL)
            throw(0);

        ConAuxWidgets->Go=g_SRTT_UI->AddButtonInput(WidgetHandle,
                "Go",SEGGER_RTT_GoButton,CommonData);
        if(ConAuxWidgets->Go==NULL)
            throw(0);

        ConAuxWidgets->Reset=g_SRTT_UI->AddButtonInput(WidgetHandle,
                "Reset",SEGGER_RTT_ResetButton,CommonData);
        if(ConAuxWidgets->Reset==NULL)
            throw(0);
    }
    catch(...)
    {
        if(ConAuxWidgets!=NULL)
        {
            SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_FreeWidgets(DriverIO,
                    WidgetHandle,(t_ConnectionWidgetsType *)ConAuxWidgets,CommonData);
        }
        ConAuxWidgets=NULL;
    }

    return (t_ConnectionWidgetsType *)ConAuxWidgets;
}

/*******************************************************************************
 * NAME:
 *    SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_FreeWidgets
 *
 * SYNOPSIS:
 *    void SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_FreeWidgets(t_DriverIOHandleType *DriverIO,
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
void SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_FreeWidgets(
        t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle,
        t_ConnectionWidgetsType *ConAuxCtrls,
        struct SEGGER_RTT_Common *CommonData)
{
    struct SEGGER_RTT_ConAuxWidgets *ConAuxWidgets=CommonData->AuxWidgets;

    if(CommonData->AuxWidgets!=NULL)
    {
        if(ConAuxWidgets->Halt!=NULL)
            g_SRTT_UI->FreeButtonInput(WidgetHandle,ConAuxWidgets->Halt);

        if(ConAuxWidgets->Go!=NULL)
            g_SRTT_UI->FreeButtonInput(WidgetHandle,ConAuxWidgets->Go);

        if(ConAuxWidgets->Reset!=NULL)
            g_SRTT_UI->FreeButtonInput(WidgetHandle,ConAuxWidgets->Reset);

        delete ConAuxWidgets;
    }
}

void SEGGER_RTT_HaltButton(const struct PIButtonEvent *Event,void *UserData)
{
    struct SEGGER_RTT_Common *CommonData=(struct SEGGER_RTT_Common *)UserData;

    switch(Event->EventType)
    {
        case e_PIEButton_Press:
            SEGGER_RTT_LockMutex(CommonData->AuxWidgets->IOHandle);
            g_SRTT_JLinkAPI.Halt();
            SEGGER_RTT_UnLockMutex(CommonData->AuxWidgets->IOHandle);
        break;
        case e_PIEButtonMAX:
        default:
        break;
    }
}

void SEGGER_RTT_GoButton(const struct PIButtonEvent *Event,void *UserData)
{
    struct SEGGER_RTT_Common *CommonData=(struct SEGGER_RTT_Common *)UserData;

    switch(Event->EventType)
    {
        case e_PIEButton_Press:
            SEGGER_RTT_LockMutex(CommonData->AuxWidgets->IOHandle);
            g_SRTT_JLinkAPI.Go();
            SEGGER_RTT_UnLockMutex(CommonData->AuxWidgets->IOHandle);
        break;
        case e_PIEButtonMAX:
        default:
        break;
    }
}

void SEGGER_RTT_ResetButton(const struct PIButtonEvent *Event,void *UserData)
{
    struct SEGGER_RTT_Common *CommonData=(struct SEGGER_RTT_Common *)UserData;

    switch(Event->EventType)
    {
        case e_PIEButton_Press:
            SEGGER_RTT_LockMutex(CommonData->AuxWidgets->IOHandle);
            g_SRTT_JLinkAPI.ResetNoHalt();
            SEGGER_RTT_UnLockMutex(CommonData->AuxWidgets->IOHandle);
        break;
        case e_PIEButtonMAX:
        default:
        break;
    }
}
