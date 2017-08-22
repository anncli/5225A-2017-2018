//////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                       VEX Competition Control Include File
//
// This file provides control over a VEX Competition Match. It should be included in the user's
// program with the following line located near the start of the user's program
//        #include "VEX_Competition_Includes_Custom.h"
// The above statement will cause this program to be included in the user's program. There's no
// need to modify this program.
//
// The program displays status information on the new VEX LCD about the competition state. You don't
// need the LCD, the program will work fine whether or not the LCD is actually provisioned.
//
// The status information is still useful without the LCD. The ROBOTC IDE debugger has a "remote screen"
// that contains a copy of the status information on the LCD. You can use this to get a view of the
// status of your program. The remote screen is shown with the menu command
//   "Robot -> Debugger Windows -> VEX Remote Screen"
//
//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef FORCE_AUTO
#define AUTONOMOUS true
#else
#define AUTONOMOUS bIfiAutonomousMode
#endif

#ifdef IGNORE_DISABLE
#define DISABLED false
#else
#define DISABLED bIfiRobotDisabled
#endif

void allMotorsOff();
void allTasksStop();
void disabled();
void startup();
task autonomous();
task usercontrol();

bool bStopTasksBetweenModes = true;

task main()
{
	// Master CPU will not let competition start until powered on for at least 2-seconds
	clearLCDLine(0);
	clearLCDLine(1);
	displayLCDPos(0, 0);
	displayNextLCDString("Startup");
	wait1Msec(2000);


	startup();

	//wait1Msec(500);


	while (true)
	{
		while (DISABLED) { disabled(); wait1Msec(25); }

		if (AUTONOMOUS)
		{
			startTask(autonomous);

			// Waiting for autonomous phase to end
			while (AUTONOMOUS && !DISABLED)
			{
				if (!bVEXNETActive)
				{
					if (nVexRCReceiveState == vrNoXmiters) // the transmitters are powered off!!
						allMotorsOff();
				}
				wait1Msec(25); // Waiting for autonomous phase to end
			}
			allMotorsOff();
			if(bStopTasksBetweenModes)
			{
				allTasksStop();
			}
		}

		else
		{
			startTask(usercontrol);

			// Here we repeat loop waiting for user control to end and (optionally) start
			// of a new competition run
			while (!AUTONOMOUS && !DISABLED)
			{
				if (nVexRCReceiveState == vrNoXmiters) // the transmitters are powered off!!
					allMotorsOff();
				wait1Msec(25);
			}
			allMotorsOff();
			if(bStopTasksBetweenModes)
			{
				allTasksStop();
			}
		}
	}
}

void allMotorsOff()
{
	motor[port1] = 0;
	motor[port2] = 0;
	motor[port3] = 0;
	motor[port4] = 0;
	motor[port5] = 0;
	motor[port6] = 0;
	motor[port7] = 0;
	motor[port8] = 0;
#if defined(VEX2)
	motor[port9] = 0;
	motor[port10] = 0;
#endif
}

void allTasksStop()
{
	stopTask(1);
	stopTask(2);
	stopTask(3);
	stopTask(4);
#if defined(VEX2)
	stopTask(5);
	stopTask(6);
	stopTask(7);
	stopTask(8);
	stopTask(9);
	stopTask(10);
	stopTask(11);
	stopTask(12);
	stopTask(13);
	stopTask(14);
	stopTask(15);
	stopTask(16);
	stopTask(17);
	stopTask(18);
	stopTask(19);
#endif
}