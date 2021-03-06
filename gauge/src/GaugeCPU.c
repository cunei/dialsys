/**********************************************************************************************************************
 *                                                                                                                    *
 *  G A U G E  C P U . C                                                                                              *
 *  ====================                                                                                              *
 *                                                                                                                    *
 *  This is free software; you can redistribute it and/or modify it under the terms of the GNU General Public         *
 *  License version 2 as published by the Free Software Foundation.  Note that I am not granting permission to        *
 *  redistribute or modify this under the terms of any later version of the General Public License.                   *
 *                                                                                                                    *
 *  This is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied        *
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more     *
 *  details.                                                                                                          *
 *                                                                                                                    *
 *  You should have received a copy of the GNU General Public License along with this program (in the file            *
 *  "COPYING"); if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111,   *
 *  USA.                                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \file
 *  \brief Calculate the CPU usage for the gauge.
 */
#include <stdio.h>
#include <string.h>
#include "GaugeDisp.h"

#define CPU_COUNT 32

extern FACE_SETTINGS *faceSettings[];
extern MENU_DESC gaugeMenuDesc[];
extern MENU_DESC pickCPUMenuDesc[];
extern GAUGE_ENABLED gaugeEnabled[];
extern int sysUpdateID;

void readCPULoad (int procNumber);
int readStats (unsigned long long *stats, int procNumber);
int readAverage (float stats[]);
int readClockRates (void);

static int readCount[3];
static float loadAverages[3];
static int clockRates[CPU_COUNT + 1];
static int myUpdateID[CPU_COUNT + 1];
static int loadValues[CPU_COUNT + 1][8];
static unsigned long long startStats[CPU_COUNT + 1][10];
static unsigned long long endStats[CPU_COUNT + 1][10];
static int cpuCount = 0;
char *name[8] =
{
	__("Total"),
	__("User"),
	__("Nice"),
	__("System"),
	__("Idle"),
	__("ioWait"),
	__("Irq"),
	__("softIrq")
};

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  C P U I N I T                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Initalise the CPU tables.
 *  \result None.
 */
void readCPUInit (void)
{
	if (gaugeEnabled[FACE_TYPE_CPU_LOAD].enabled)
	{
		int i = 0;
		for (i = 0; i < CPU_COUNT; ++i)
		{
			myUpdateID[i] = 100;
			readStats (&startStats[i][0], i);
		}
		gaugeMenuDesc[MENU_GAUGE_LOAD].disable = 0;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  C P U V A L U E S                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Read the current CPU value.
 *  \param face Which face is it for.
 *  \result 1 if the display has been updated.
 */
void readCPUValues (int face)
{
	if (gaugeEnabled[FACE_TYPE_CPU_LOAD].enabled)
	{
		FACE_SETTINGS *faceSetting = faceSettings[face];
		int i, update = 0;
		int faceType = (faceSetting -> faceSubType >> 8) & 0x000F;
		int procNumber = faceSetting -> faceSubType & 0x00FF;
		unsigned int step, newVal = 0;
		char cpuName[10];

		if (faceSetting -> faceFlags & FACE_REDRAW)
		{
			faceSetting -> nextUpdate = 5;
			faceSetting -> firstValue = newVal;
			update = 1;
		}
		if (faceType != 0x0F)
		{
			if (!update && sysUpdateID % 2 != 0)
				return;

			strcpy (cpuName, _("CPU"));
			if (procNumber)
			{
				sprintf (&cpuName[3], "%d", procNumber);
			}
			if (myUpdateID[procNumber] != sysUpdateID)
			{
				readCPULoad (procNumber);
				myUpdateID[procNumber] = sysUpdateID;
			}
			newVal = loadValues[procNumber][faceType];
			if (!update)
			{
				step = abs (faceSetting -> firstValue - newVal);
				if (--faceSetting -> nextUpdate <= 0 || step > 10)
				{
					faceSetting -> nextUpdate = (5 - (step / 2));
					faceSetting -> firstValue = newVal;
					update = 1;
				}
			}
			if (update)
			{
				int percent = faceSetting -> firstValue;
				readClockRates ();
				setFaceString (faceSetting, FACESTR_TOP, 0, _("%s\n(%s)"), cpuName, name[faceType]);
				setFaceString (faceSetting, FACESTR_WIN, 0, _("%s %s - Gauge"), cpuName, name[faceType]);
				setFaceString (faceSetting, FACESTR_BOT, 0, _("%0.2f MHz\n%d%%"), (float)clockRates[faceType] / 1000, percent);
				setFaceString (faceSetting, FACESTR_TIP, 0, _("<b>%s %s</b>: %d%%\n<b>CPU Count</b>: %d\n<b>Clock</b>: %0.2f MHz"), 
						cpuName, name[faceType], percent, cpuCount, (float)clockRates[faceType] / 1000);
				for (i = 0; i < 8; i++)
					startStats[procNumber][i] = endStats[procNumber][i];
			}
		}
		else
		{
			if (!update && sysUpdateID % 5 != 0)
				return;

			setFaceString (faceSetting, FACESTR_TOP, 0, _("Load\nAverage"));
			setFaceString (faceSetting, FACESTR_WIN, 0, _("Load average - Gauge"));
			if (readAverage (loadAverages) == 6)
			{
				setFaceString (faceSetting, FACESTR_BOT, 0, _("%0.2f\n(%0.2f)"),
						loadAverages[0], loadAverages[1]);
				setFaceString (faceSetting, FACESTR_TIP, 0, _(
						"<b>1 min. average</b>: %0.2f\n<b>5 min. average</b>: %0.2f\n<b>15 min. average</b>: %0.2f\n"
						"<b>Processes</b>: %d\n<b>Running</b>: %d\n<b>Last PID</b>: %d"),
						loadAverages[0], loadAverages[1], loadAverages[2],
						readCount[1], readCount[0], readCount[2]);
				faceSetting -> firstValue = loadAverages[0];
				faceSetting -> secondValue = loadAverages[1];
				while (faceSetting -> firstValue > faceSetting -> faceScaleMax)
				{
					faceSetting -> faceScaleMax *= 2;
					maxMinReset (&faceSetting -> savedMaxMin, 5, 1);
				}
			}
			else
			{
				setFaceString (faceSetting, FACESTR_BOT, 0, _("0.00\n(0.00)"));
				setFaceString (faceSetting, FACESTR_TIP, 0, _("Missing load average"));
				faceSetting -> firstValue = faceSetting -> secondValue = 0;
			}
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  C P U L O A D                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Which processor to read from.
 *  \param procNumber Which processor to read from.
 *  \result none.
 */
void readCPULoad (int procNumber)
{
	int i, totalTicks = 0;

	readStats (&endStats[procNumber][0], procNumber);
	for (i = 1; i < 8; i++)
		totalTicks += (endStats[procNumber][i] - startStats[procNumber][i]);
	if (totalTicks == 0)
		return;
	for (i = 0; i < 8; i++)
	{
		loadValues[procNumber][i] = ((endStats[procNumber][i] - startStats[procNumber][i]) * 100) + (totalTicks / 2);
		loadValues[procNumber][i] /= totalTicks;
	}
	return;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  S T A T S                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Open the /proc/stat file and read the CPU status.
 *  \param stats Where to read the stats to.
 *  \param procNumber Which processor to read from.
 *  \result Number of words read.
 */
int readStats (unsigned long long *stats, int procNumber)
{
	int i, j, n = 0, found = 0;
	char readBuff[1025], word[254], procName[41];
	FILE *inCPUFile = fopen ("/proc/stat", "r");

	strcpy (procName, "cpu");
	if (procNumber) sprintf (&procName[3], "%d", procNumber - 1);

	while (inCPUFile != NULL && !found)
	{
		word[i = j = n = 0] = 0;
		if (!fgets (readBuff, 1024, inCPUFile))
			break;

		while (n < 9)
		{
			if (readBuff[i] == ' ' || readBuff[i] == 0)
			{
				if (word[0] != 0)
				{
					if (n == 0)
					{
						if (strcmp (word, procName) != 0)
							break;
						pickCPUMenuDesc[procNumber].disable = 0;
						stats[0] = 0;
						found = 1;
					}
					else
					{
						stats[n] = atoll (word);
						if (n != 4)					/* Ignore Idle ticks */
							stats[0] += stats[n];	/* Count all other ticks */
					}
					word[j = 0] = 0;
					n ++;
				}
				if (readBuff[i] == 0)
					break;
			}
			else
			{
				word[j++] = readBuff[i];
				word[j] = 0;
			}
			i ++;
		}
	}
	if (inCPUFile != NULL)
	{
		fclose (inCPUFile);
	}
	return n;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  A V E R A G E                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Read the load averages from the proc directory.
 *  \param readAvs Save the read averages here.
 *  \result Number of values read.
 */
int readAverage (float readAvs[])
{
	int retn = 0;
	FILE *readFile = fopen ("/proc/loadavg", "r");

	if (readFile != NULL)
	{
		retn = fscanf (readFile, "%f %f %f %d/%d %d",
				&readAvs[0], &readAvs[1], &readAvs[2],
				&readCount[0], &readCount[1], &readCount[2]);
		fclose (readFile);
	}
	return retn;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  C L O C K  R A T E S                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Read the cpuinfo clock speed.
 *  \result None.
 */
int readClockRates ()
{
	int i = 0, retn = 0;
	char readBuff[512];
	FILE *readFile;

	cpuCount = 0;
	clockRates[0] = 0;
	for (i = 0; i < CPU_COUNT; ++i)
	{
		sprintf (readBuff, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
		if ((readFile = fopen (readBuff, "r")) != NULL)
		{
			++cpuCount;
			if (fgets(readBuff, 510, readFile))
			{
				clockRates[++retn] = atoi (readBuff) / 1000;
				clockRates[0] += clockRates[retn];
			}
			fclose (readFile);
			continue;
		}
		break;
	}
	if (retn)
	{
		clockRates[0] /= retn;
	}
	return retn;
}

