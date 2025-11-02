/*******************************************************************************
 * FILENAME: SEGGER_RTT_AuxWidgets.h
 * 
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    .h file
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
 * HISTORY:
 *    Paul Hutchinson (26 May 2025)
 *       Created
 *
 *******************************************************************************/
#ifndef __SEGGER_RTT_AUXWIDGETS_H_
#define __SEGGER_RTT_AUXWIDGETS_H_

/***  HEADER FILES TO INCLUDE          ***/
#include "SEGGER_RTT_Common.h"
#include "PluginSDK/Plugin.h"

/***  DEFINES                          ***/

/***  MACROS                           ***/

/***  TYPE DEFINITIONS                 ***/

/***  CLASS DEFINITIONS                ***/

/***  GLOBAL VARIABLE DEFINITIONS      ***/

/***  EXTERNAL FUNCTION PROTOTYPES     ***/
t_ConnectionWidgetsType *SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_AllocWidgets(t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle,struct SEGGER_RTT_Common *CommonData);
void SEGGER_RTT_Common_ConnectionAuxCtrlWidgets_FreeWidgets(t_DriverIOHandleType *DriverIO,t_WidgetSysHandle *WidgetHandle,t_ConnectionWidgetsType *ConAuxCtrls,struct SEGGER_RTT_Common *CommonData);

#endif   /* end of "#ifndef __SEGGER_RTT_AUXWIDGETS_H_" */
