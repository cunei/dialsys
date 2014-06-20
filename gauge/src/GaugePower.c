/**********************************************************************************************************************
 *                                                                                                                    *
 *  G A U G E  P O W E R . C                                                                                          *
 *  ========================                                                                                          *
 *                                                                                                                    *
 *  This is free software; you can redistribute it and/or modify it under the terms of the GNU General Public         *
 *  License version 2 as published by the Free Software Foundation.  Note that I am not granting permission to        *
 *  redistribute or modify this under the terms of any later version of the General Public License.                   *
 *                                                                                                                    *
 *  This is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the                *
 *  impliedwarranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   *
 *  more details.                                                                                                     *
 *                                                                                                                    *
 *  You should have received a copy of the GNU General Public License along with this program (in the file            *
 *  "COPYING"); if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111,   *
 *  USA.                                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @file
 *  @brief Routines to display the powermeter information.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "config.h"
#include "socket.h"
#include "GaugeDisp.h"

#ifdef GAUGE_HAS_POWER

extern FACE_SETTINGS *faceSettings[];
extern MENU_DESC gaugeMenuDesc[];
extern char powerServer[];
extern int powerPort;

double myPowerReading[8];

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  P O W E R  M E T E R  I N I T                                                                            *
 *  ======================================                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Called once at the program start to find the devices.
 *  @result None 0 all is OK.
 */
void readPowerMeterInit (void)
{
	char addr[20];
	int clientSock = -1;
	
	if (GetAddressFromName (powerServer, addr))
	{
		clientSock = ConnectClientSocket (addr, powerPort);
	}
	if (SocketValid (clientSock))
	{
		gaugeMenuDesc[MENU_GAUGE_POWER].disable = 0;
		CloseSocket (&clientSock);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  P O W E R  K E Y                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Process each of the fields in the XML.
 *  @param readLevel 0 current, 1 today, 2 tomorrow.
 *  @param name Name of the field.
 *  @param value Value of the field.
 *  @result None.
 */
static void processPowerKey (int readLevel, const char *name, char *value)
{
	if (readLevel == 1 && strcmp (name, "now") == 0)
		myPowerReading[0] = atof (value);
	if (readLevel == 1 && strcmp (name, "max") == 0)
		myPowerReading[1] = atof (value);
	if (readLevel == 1 && strcmp (name, "min") == 0)
		myPowerReading[2] = atof (value);
	if (readLevel == 1 && strcmp (name, "minavg") == 0)
		myPowerReading[3] = atof (value);
	if (readLevel == 1 && strcmp (name, "houravg") == 0)
		myPowerReading[4] = atof (value);
	if (readLevel == 1 && strcmp (name, "dayavg") == 0)
		myPowerReading[5] = atof (value);
	if (readLevel == 1 && strcmp (name, "monthavg") == 0)
		myPowerReading[6] = atof (value);		
	if (readLevel == 1 && strcmp (name, "yearavg") == 0)
		myPowerReading[7] = atof (value);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  E L E M E N T  N A M E S                                                                           *
 *  =======================================                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Process each of the elements in the file.
 *  @param doc Document to read.
 *  @param aNode Current node.
 *  @param readLevel 0 current, 1 today, 2 tomorrow.
 *  @result None.
 */
static void processElementNames (xmlDoc *doc, xmlNode * aNode, int readLevel)
{
	xmlChar *key;
    xmlNode *curNode = NULL;

    for (curNode = aNode; curNode; curNode = curNode->next) 
    {
        if (curNode->type == XML_ELEMENT_NODE) 
        {
			if ((!xmlStrcmp (curNode -> name, (const xmlChar *)"power"))) 
			{
				++readLevel;
			}
			else
			{
				key = xmlNodeListGetString (doc, curNode -> xmlChildrenNode, 1);
				processPowerKey (readLevel, (const char *)curNode -> name, (char *)key);
		    	xmlFree (key);
		    }
        }
        processElementNames (doc, curNode->children, readLevel);
    }
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  B U F F E R                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Process the down loaded buffer.
 *  @param buffer Buffer to process.
 *  @param size Size of the buffer.
 *  @result None.
 */
static void processBuffer (char *buffer, size_t size)
{
	xmlDoc *doc = NULL;
	xmlNode *rootElement = NULL;
	xmlChar *xmlBuffer = NULL;

	xmlBuffer = xmlCharStrndup (buffer, size);
	doc = xmlParseDoc (xmlBuffer);

	if (doc != NULL)
	{
		rootElement = xmlDocGetRootElement (doc);
		processElementNames (doc, rootElement, 0);
		xmlFreeDoc (doc);
	}
	else
	{
		printf ("error: could not parse memory\n");
		printf ("BUFF[%d] [[[%s]]]\n", size, buffer);
	}
	xmlCleanupParser ();
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  P O W E R  M E T E R  I N F O                                                                            *
 *  ======================================                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Read the current tempature from the powermeter.
 *  @result None.
 */
void readPowerMeterInfo ()
{
	char buffer[512] = "", addr[20];
	int clientSock = -1, bytesRead = 0;
	
	if (GetAddressFromName (powerServer, addr))
	{
		clientSock = ConnectClientSocket (addr, powerPort);
	}
	if (SocketValid (clientSock))
	{
		bytesRead = RecvSocket (clientSock, buffer, 511);
		CloseSocket (&clientSock);
	}
	if (bytesRead)
	{
		processBuffer (buffer, bytesRead);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  P O W E R  M E T E R  V A L U E S                                                                        *
 *  ==========================================                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Calculate the value to show on the face.
 *  @param face Which face to display on.
 *  @result None zero if the face should be re-shown.
 */
void readPowerMeterValues (int face)
{
	FACE_SETTINGS *faceSetting = faceSettings[face];

	if (faceSetting -> faceFlags & FACE_REDRAW)
	{
		;
	}
	else if (faceSetting -> nextUpdate)
	{
		faceSetting -> nextUpdate -= 1;
		return;
	}
	else if (!faceSetting -> nextUpdate)
	{
		readPowerMeterInfo ();
		faceSetting -> nextUpdate = 30;
	}

	setFaceString (faceSetting, FACESTR_TOP, 0, "Power Meter");
	setFaceString (faceSetting, FACESTR_TIP, 0, _("<b>Current</b>: %0.0fW\n<b>Maximum</b>: %0.0fW\n"
			"<b>Minimum</b>: %0.0fW\n<b>Hour Average</b>: %0.0fW\n<b>Day Average</b>: %0.0fW"), 
			myPowerReading[0], myPowerReading[1], myPowerReading[2], myPowerReading[3], myPowerReading[4]);
	setFaceString (faceSetting, FACESTR_WIN, 0, _("Current: %0.0fW, Day Average: %0.0fW"),
			myPowerReading[0], myPowerReading[4]);
	setFaceString (faceSetting, FACESTR_BOT, 0, "%0.0fW\n(%0.0fW)", myPowerReading[0], myPowerReading[4]);
	faceSetting -> firstValue = myPowerReading[0] / 1000;
	faceSetting -> secondValue = myPowerReading[4] / 1000;
}

#endif
