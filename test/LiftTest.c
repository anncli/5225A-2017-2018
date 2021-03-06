#pragma config(Sensor, in4,    liftPoti,       sensorPotentiometer)
#pragma config(Motor,  port2,           left,          tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port9,           right,         tmotorVex393HighSpeed_MC29, openLoop)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

task showValues()
{
	while (true)
	{
		clearLCDLine(1);
		displayLCDNumber(1, 0, SensorValue[liftPoti]);
		sleep(10);
	}
}

task main()
{
	unsigned long time, diff;
	clearDebugStream();
	startTask(showValues);
	while (true)
	{
		while (!vexRT[Btn8R]) sleep(10);
		time = nPgmTime;
		motor[left] = motor[right] = 127;
		while (SensorValue[liftPoti] < 3200) sleep(10);
		motor[left] = motor[right] = 15;
		diff = nPgmTime - time;
		writeDebugStreamLine("Up:   %d.%d s", diff / 1000, diff % 1000);
		clearLCDLine(0);
		displayLCDNumber(0, 0, diff);
		while (vexRT[Btn8R]) sleep(10);

		while (!vexRT[Btn8R]) sleep(10);
		 time = nPgmTime;
		motor[left] = motor[right] = -127;
		while (SensorValue[liftPoti] > 1050) sleep(10);
		motor[left] = motor[right] = -15;
		diff = nPgmTime - time;
		writeDebugStreamLine("Down: %d.%d s", diff / 1000, diff % 1000);
		clearLCDLine(0);
		displayLCDNumber(0, 0, diff);
		while (vexRT[Btn8R]) sleep(10);
	}
}
