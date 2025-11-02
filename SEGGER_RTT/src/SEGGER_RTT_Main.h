/*******************************************************************************
 * FILENAME: SEGGER_RTT_Main.h
 * 
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    .h file
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
#ifndef __SEGGER_RTT_MAIN_H_
#define __SEGGER_RTT_MAIN_H_

/***  HEADER FILES TO INCLUDE          ***/
#include "PluginSDK/Plugin.h"

/***  DEFINES                          ***/

/***  MACROS                           ***/

/***  TYPE DEFINITIONS                 ***/

/***  CLASS DEFINITIONS                ***/

/***  GLOBAL VARIABLE DEFINITIONS      ***/
extern const struct IOS_API *g_SRTT_IOSystem;
extern const struct PI_UIAPI *g_SRTT_UI;
extern const struct PI_SystemAPI *g_SRTT_System;

/***  EXTERNAL FUNCTION PROTOTYPES     ***/
void SEGGER_RTT_RegisterPlugin(const struct PI_SystemAPI *SysAPI);

#endif   /* end of "#ifndef __SEGGER_RTT_MAIN_H_" */
