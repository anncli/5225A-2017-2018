#pragma config(Sensor, in1,    autoPoti,       sensorPotentiometer)
#pragma config(Sensor, in2,    mobilePoti,     sensorPotentiometer)
#pragma config(Sensor, in5,    armPoti,        sensorPotentiometer)
#pragma config(Sensor, dgtl1,  trackL,         sensorQuadEncoder)
#pragma config(Sensor, dgtl3,  trackR,         sensorQuadEncoder)
#pragma config(Sensor, dgtl5,  trackB,         sensorQuadEncoder)
#pragma config(Sensor, dgtl7,  liftEnc,        sensorQuadEncoder)
#pragma config(Sensor, dgtl9,  limMobile,      sensorTouch)
#pragma config(Sensor, dgtl10, jmpSkills,      sensorDigitalIn)
#pragma config(Sensor, dgtl11, sonar,          sensorSONAR_raw)
#pragma config(Motor,  port2,           liftL,         tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port3,           driveL1,       tmotorVex393TurboSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port4,           driveL2,       tmotorVex393TurboSpeed_MC29, openLoop)
#pragma config(Motor,  port5,           arm,           tmotorVex393HighSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port6,           mobile,        tmotorVex393HighSpeed_MC29, openLoop)
#pragma config(Motor,  port7,           driveR2,       tmotorVex393TurboSpeed_MC29, openLoop, reversed)
#pragma config(Motor,  port8,           driveR1,       tmotorVex393TurboSpeed_MC29, openLoop)
#pragma config(Motor,  port9,           liftR,         tmotorVex393HighSpeed_MC29, openLoop)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

//#define CHECK_POTI_JUMPS

// Necessary definitions

bool TimedOut(unsigned long timeOut, const string description);

#define TASK_POOL_SIZE 19

// Year-independent libraries

#include "notify.h"
#include "task.h"
#include "async.h"
#include "timeout.h"
#include "motors.h"
#include "sensors.h"
#include "joysticks.h"
#include "cycle.h"
#include "utilities.h"
#include "pid.h"
#include "state.h"

#include "notify.c"
#include "task.c"
#include "async.c"
#include "timeout.c"
#include "motors.c"
#include "sensors.c"
#include "joysticks.c"
#include "cycle.c"
#include "utilities.c"
#include "pid.c"
#include "state.c"

#include "Vex_Competition_Includes_Custom.c"

#include "controls.h"

#include "auto.h"
#include "auto_simple.h"
#include "auto_runs.h"

//#define DEBUG_TRACKING
//#define TRACK_IN_DRIVER
#define SKILLS_RESET_AT_START
//#define ULTRASONIC_RESET

//#define LIFT_SLOW_DRIVE_THRESHOLD 1200

typedef struct _sSimpleConfig {
	long target;
	word mainPower;
	word brakePower;
} sSimpleConfig;

void configure(sSimpleConfig& config, long target, word mainPower, word brakePower)
{
	writeDebugStreamLine("configure %d %d %d", target, mainPower, brakePower);
	config.target = target;
	config.mainPower = mainPower;
	config.brakePower = brakePower;
}

unsigned long gOverAllTime = 0;
sCycleData gMainCycle;
int gNumCones = 0;
word gUserControlTaskId = -1;
bool gSetTimedOut = false;

bool gDriveManual;

/* Drive */
void setDrive(word left, word right, bool debug = false)
{
	if (debug)
		writeDebugStreamLine("DRIVE %d %d", left, right);
	gMotor[driveL1].power = gMotor[driveL2].power = left;
	gMotor[driveR1].power = gMotor[driveR2].power = right;
}

void handleDrive()
{
	if (gDriveManual)
	{
		//gJoy[JOY_TURN].deadzone = MAX(abs(gJoy[JOY_THROTTLE].cur) / 2, DZ_ARM);
		short y = gJoy[JOY_THROTTLE].cur;
		short a = gJoy[JOY_TURN].cur;

		if (!a && abs(gVelocity.a) > 0.5)
			a = -6 * sgn(gVelocity.a);

		word l = y + a;
		word r = y - a;

		//if (gSensor[liftEnc].value > LIFT_SLOW_DRIVE_THRESHOLD)
		//{
		//	LIM_TO_VAL_SET(l, 40);
		//	LIM_TO_VAL_SET(r, 40);
		//}
		setDrive(l, r);
	}
}


/* Lift */
typedef enum _tLiftStates {
	liftManaged,
	liftIdle,
	liftManual,
	liftToTarget,
	liftRaiseSimple,
	liftLowerSimple,
	liftStopping,
	liftHold,
	liftHoldDown,
	liftHoldUp,
	liftResetEncoder
} tLiftStates;

void setLift(word power,bool debug=false)
{
	if( debug )	writeDebugStreamLine("%06d Lift %4d", nPgmTime-gOverAllTime,power );
	gMotor[liftL].power = gMotor[liftR].power = power;
}

#define LIFT_TOP 36.6
#define LIFT_BOTTOM 5.8
#define LIFT_MID 20.25
#define LIFT_HOLD_DOWN_THRESHOLD 7.5
#define LIFT_HOLD_UP_THRESHOLD 35.5

#define LIFT_MID_POS 54
#define LIFT_ARM_LEN 9

#define LIFT_HEIGHT(pos) (LIFT_MID + 2 * LIFT_ARM_LEN * sin(((pos) - LIFT_MID_POS) * PI / 180))
#define LIFT_POS(height) (LIFT_MID_POS + asin(((height) - LIFT_MID) / (2 * LIFT_ARM_LEN)) * 180 / PI)

float gLiftTarget;
bool gLiftReset = false;

MAKE_MACHINE(lift, tLiftStates, liftIdle,
{
case liftIdle:
	setLift(0);
	break;
case liftToTarget:
{
	unsigned long begin = nPgmTime;
	float target = gLiftTarget;
	float last = LIFT_HEIGHT(gSensor[liftEnc].value);
	writeDebugStreamLine("liftToTarget %f -> %f", last, target);
	const float kP_vel = arg._long ? 0.006 : 0.01;
	const float kI_vel = 0.00001;
	bool up = target > last;
	float kP_pwr = up ? 2500.0 : 6000.0;
	float err;
	float vel;
	float integral = 0;
	float epsilon;
	int poti;
	sleep(20);
	sCycleData cycle;
	initCycle(cycle, 20, "liftToTarget");
	do
	{
		poti = gSensor[liftEnc].value;
		float cur = LIFT_HEIGHT(poti);
		vel = (cur - last) / cycle.period;
		err = target - cur;
		if (fabs(err) < 3.0)
			integral += err * cycle.period;
		else
			integral = 0;
		float power = kP_pwr * (kP_vel * err + kI_vel * integral - vel);
		if (sgn(power) == -sgn(vel)) power *= 0.05;
		LIM_TO_VAL_SET(power, 127);
		setLift((word)power);
		last = cur;
		epsilon = 20 * vel;
		LIM_TO_VAL_SET(epsilon, 4);
		if (fabs(epsilon) < 1.0) epsilon = sgn(vel);
		endCycle(cycle);
	} while (up ? err > epsilon : err < epsilon);
	writeDebugStreamLine("Lift at target: %f %f %f | %d | %d ms", target, LIFT_HEIGHT(gSensor[liftEnc].value), err, poti, nPgmTime - begin);
	arg._long = 0;
	NEXT_STATE((up ? target >= LIFT_HOLD_UP_THRESHOLD : target < LIFT_HOLD_DOWN_THRESHOLD) ? liftHold : liftStopping);
}
case liftStopping:
{
	const float kP = -5.0;
	velocityClear(liftEnc);
	sCycleData cycle;
	unsigned long end = nPgmTime + 200;
	writeDebugStreamLine("%d", nPgmTime);
	initCycle(cycle, 20, "liftStopping");
	while (nPgmTime < end)
	{
		velocityCheck(liftEnc);
		if (gSensor[liftEnc].velGood)
		{
			if (fabs(gSensor[liftEnc].velocity) < 2.0)
				break;
			float power = kP * gSensor[liftEnc].velocity;
			LIM_TO_VAL_SET(power, 12);
			setLift((word)power);
		}
		endCycle(cycle);
	}
	writeDebugStreamLine("%d", nPgmTime);
	NEXT_STATE(liftHold);
}
case liftRaiseSimple:
{
	sSimpleConfig &config = *(sSimpleConfig *)arg._ptr;
	writeDebugStreamLine("Raising lift to %d", config.target);
	setLift(config.mainPower);
	int pos;
	while ((pos = gSensor[liftEnc].value) < config.target) sleep(10);
	if (config.brakePower)
	{
		setLift(config.brakePower);
		sleep(200);
	}
	writeDebugStreamLine("Lift moved up to %d | %d", config.target, pos);
	arg._long = -1;
	NEXT_STATE(liftHold);
}
case liftLowerSimple:
{
	sSimpleConfig &config = *(sSimpleConfig *)arg._ptr;
	writeDebugStreamLine("Lowering lift to %d", config.target);
	setLift(config.mainPower);
	int pos;
	while ((pos = gSensor[liftEnc].value) > config.target) sleep(10);
	if (config.brakePower)
	{
		setLift(config.brakePower);
		sleep(200);
	}
	writeDebugStreamLine("Lift moved down to %d | %d", config.target, pos);
	arg._long = -1;
	NEXT_STATE(liftHold);
}
case liftHold:
{
	float target = arg._long ? LIFT_HEIGHT(gSensor[liftEnc].value) : gLiftTarget;
	if (target < LIFT_HOLD_DOWN_THRESHOLD)
		NEXT_STATE(liftHoldDown);
	if (target > LIFT_HOLD_UP_THRESHOLD)
		NEXT_STATE(liftHoldUp);
	setLift(8 + (word)(4 * cos((target - LIFT_MID) * PI / 180)));
	break;
}
case liftHoldDown:
	setLift(-15);
	break;
case liftHoldUp:
	setLift(15);
	break;
case liftResetEncoder:
	if (!gLiftReset)
	{
		setLift(-35);
		sleep(400);
		velocityClear(liftEnc);
		do {
			sleep(200);
			velocityCheck(liftEnc);
		} while (!gSensor[liftEnc].velGood || gSensor[liftEnc].velocity <= -0.01);
		sleep(400);
		resetQuadratureEncoder(liftEnc);
		setLift(0);
		gLiftReset = true;
	}
	NEXT_STATE(liftIdle);
})

void handleLift()
{
	if (liftState == liftManaged ) return;

	if (RISING(JOY_LIFT))
	{
		liftSet(liftManual);
	}
	if (FALLING(JOY_LIFT))
	{
		liftSet(liftHold, -1);
	}

	if (liftState == liftManual)
	{
		word value = gJoy[JOY_LIFT].cur * 2 - 128 * sgn(gJoy[JOY_LIFT].cur);
		if (LIFT_HEIGHT(gSensor[liftEnc].value) <= LIFT_BOTTOM && value < -10) value = -10;
		if (LIFT_HEIGHT(gSensor[liftEnc].value) >= LIFT_TOP && value > 10) value = 10;
		setLift(value);
	}
}


/* Arm */
typedef enum _tArmStates {
	armManaged,
	armIdle,
	armManual,
	armToTarget,
	armRaiseSimple,
	armLowerSimple,
	armStopping,
	armHold
} tArmStates;

#define ARM_TOP 3200
#define ARM_BOTTOM 550
#define ARM_PRESTACK 2300
#define ARM_CARRY 1500
#define ARM_STACK 2400
#define ARM_HORIZONTAL 1150

void setArm(word power, bool debug = false)
{
	if( debug ) writeDebugStreamLine("%06d Arm  %4d", nPgmTime-gOverAllTime, power);
	gMotor[arm].power = power;
	//	motor[arm]=power;
}

MAKE_MACHINE(arm, tArmStates, armIdle,
{
case armIdle:
	setArm (0);
	break;
case armToTarget:
	{
		if (arg._long != -1)
		{
			const float kP = 0.1;
			int err;
			sCycleData cycle;
			initCycle(cycle, 10, "armToTarget");
			do
			{
				err = arg._long - gSensor[armPoti].value;
				float power = kP * err;
				LIM_TO_VAL_SET(power, 127);
				setArm((word)power);
				endCycle(cycle);
			} while (abs(err) > 100);
		}
		writeDebugStreamLine("Arm at target: %d %d", arg, gSensor[armPoti].value);
		NEXT_STATE(armStopping);
	}
case armRaiseSimple:
{
	sSimpleConfig &config = *(sSimpleConfig *)arg._ptr;
	setArm(config.mainPower);
	int pos;
	while ((pos = gSensor[armPoti].value) < config.target) sleep(10);
	if (config.brakePower)
	{
		setArm(config.brakePower);
		sleep(200);
	}
	writeDebugStreamLine("Arm moved up to %d | %d", config.target, pos);
	arg._long = config.target;
	NEXT_STATE(armHold);
}
case armLowerSimple:
{
	sSimpleConfig &config = *(sSimpleConfig *)arg._ptr;
	setArm(config.mainPower);
	int pos;
	while ((pos = gSensor[armPoti].value) > config.target) sleep(10);
	if (config.brakePower)
	{
		setArm(config.brakePower);
		sleep(200);
	}
	writeDebugStreamLine("Arm moved down to %d | %d", config.target, pos);
	arg._long = config.target;
	NEXT_STATE(armHold);
}
case armStopping:
	velocityClear(armPoti);
	velocityCheck(armPoti);
	do
	{
		sleep(5);
		velocityCheck(armPoti);
	} while (!gSensor[armPoti].velGood);
	setArm(sgn(gSensor[armPoti].velocity) * -25);
	sleep(150);
	NEXT_STATE(armHold);
case armHold:
	{
		if (arg._long == -1) arg._long = gSensor[armPoti].value;
		const float kP = 0.4;
		sCycleData cycle;
		initCycle(cycle, 10, "armHold");
		while (true)
		{
			float power = (arg._long - gSensor[armPoti].value) * kP;
			LIM_TO_VAL_SET(power, 12);
			setArm((word)power);
			endCycle(cycle);
		}
	}
})

void handleArm()
{
	if (armState == armManaged ) return;

	if (RISING(JOY_ARM))
	{
		armSet(armManual);
	}
	if (FALLING(JOY_ARM) && armState == armManual)
	{
		armSet(armHold, -1);
	}

	if (RISING(BTN_ARM_BACK))
	{
		armSet(armToTarget, ARM_PRESTACK);
	}
	if (FALLING(BTN_ARM_BACK))
	{
		armSet(armToTarget, ARM_HORIZONTAL);
	}

	if (armState == armManual)
	{
		word value = gJoy[JOY_ARM].cur * 2 - 128 * sgn(gJoy[JOY_ARM].cur);
		if (gSensor[armPoti].value >= ARM_TOP && value > 10) value = 10;
		if (gSensor[armPoti].value <= ARM_BOTTOM && value < -10) value = -10;
		setArm(value);
	}
}


/* Mobile */

typedef enum _tMobileStates {
	mobileManaged,
	mobileIdle,
	mobileTop,
	mobileBottom,
	mobileBottomSlow,
	mobileUpToMiddle,
	mobileDownToMiddle,
	mobileMiddle
} tMobileStates;

#define MOBILE_TOP 2250
#define MOBILE_BOTTOM 500
#define MOBILE_MIDDLE_UP 600
#define MOBILE_MIDDLE_DOWN 1200
#define MOBILE_MIDDLE_THRESHOLD 1900
#define MOBILE_HALFWAY 1200

#define MOBILE_UP_POWER 127
#define MOBILE_DOWN_POWER -127
#define MOBILE_UP_HOLD_POWER 10
#define MOBILE_DOWN_HOLD_POWER -10
#define MOBILE_DOWN_SLOW_POWER_1 -60
#define MOBILE_DOWN_SLOW_POWER_2 6

#define MOBILE_LIFT_CHECK_THRESHOLD 1700
#define LIFT_MOBILE_THRESHOLD 18

#define MOBILE_SLOW_HOLD_TIMEOUT 250

bool gMobileCheckLift;
bool gMobileSlow = false;

void setMobile(word power, bool debug = false)
{
	writeDebugStreamLine("MOBILE %d", power);
	gMotor[mobile].power = power;
}

void mobileClearLift()
{
	writeDebugStreamLine("mobileClearLift");
	if (gMobileCheckLift && gSensor[liftEnc].value < LIFT_MOBILE_THRESHOLD)
	{
		sSimpleConfig config;
		configure(config, LIFT_MOBILE_THRESHOLD + 1, 80, -15);
		liftSet(liftRaiseSimple, &config);
		unsigned long timeout = nPgmTime + 1000;
		liftTimeoutWhile(liftRaiseSimple, timeout);
	}
}

byte detachIntakeAsync(tMobileStates arg0);

MAKE_MACHINE(mobile, tMobileStates, mobileIdle,
{
case mobileIdle:
	setMobile(0);
	break;
case mobileTop:
	{
		if (arg._long)
			mobileClearLift();
		setMobile(MOBILE_UP_POWER);
		unsigned long timeout = nPgmTime + 2000;
		while (gSensor[mobilePoti].value < MOBILE_TOP - 600 && !TimedOut(timeout, "mobileTop 1")) sleep(10);
		setMobile(15);
		while (gSensor[mobilePoti].value < MOBILE_TOP - 600 && !TimedOut(timeout, "mobileTop 1")) sleep(10);
		setMobile(MOBILE_UP_HOLD_POWER);
		break;
	}
case mobileBottom:
	{
		gNumCones = 0;
		if (gMobileSlow)
			NEXT_STATE(mobileBottomSlow)
		if (arg._long && gSensor[mobilePoti].value > MOBILE_LIFT_CHECK_THRESHOLD)
			mobileClearLift();
		setMobile(MOBILE_DOWN_POWER);
		unsigned long timeout = nPgmTime + 2000;
		while (gSensor[mobilePoti].value > MOBILE_BOTTOM && !TimedOut(timeout, "mobileBottom")) sleep(10);
		setMobile(MOBILE_DOWN_HOLD_POWER);
		break;
	}
case mobileBottomSlow:
	{
		gMobileSlow = false;
		if (arg._long && gSensor[mobilePoti].value > MOBILE_LIFT_CHECK_THRESHOLD)
			mobileClearLift();
		//sPID pid;
		//pidInit(pid, 0.04, 0, 3.5, -1, -1, -1, 60);
		velocityClear(mobilePoti);
		unsigned long timeout = nPgmTime + 3000;
		setMobile(-60);
		while (gSensor[mobilePoti].value > MOBILE_TOP - 600 && !TimedOut(timeout, "mobileBottomSlow 1")) sleep(10);
		sCycleData cycle;
		initCycle(cycle, 10, "mobileBottomSlow");
		//const float kP = 0.02;
		const float kP_vel = 0.001;
		const float kP_pwr = 3.0;
		while (gSensor[mobilePoti].value > MOBILE_BOTTOM + 200 && !TimedOut(timeout, "mobileBottomSlow 2"))
		{
			//setMobile((MOBILE_BOTTOM - gSensor[mobilePoti].value) * kP);
			//pidCalculate(pid, MOBILE_BOTTOM, gSensor[mobilePoti].value);
			//setMobile((word)pid.output);
			velocityCheck(mobilePoti);
			if (gSensor[mobilePoti].velGood)
			{
				float power = ((MOBILE_BOTTOM + 200 - gSensor[mobilePoti].value) * kP_vel - gSensor[mobilePoti].velocity) * kP_pwr;
				LIM_TO_VAL_SET(power, 10);
				setMobile((word)power);
			}
			endCycle(cycle);
		}
		setMobile(0);
		while (gSensor[mobilePoti].value > MOBILE_BOTTOM && !TimedOut(timeout, "mobileBottomSlow 3")) sleep(10);
		arg._long = 0;
		NEXT_STATE(mobileBottom)
	}
case mobileUpToMiddle:
	{
		setMobile(MOBILE_UP_POWER);
		unsigned long timeout = nPgmTime + 1000;
		while (gSensor[mobilePoti].value < MOBILE_MIDDLE_UP && !TimedOut(timeout, "mobileUpToMiddle")) sleep(10);
		setMobile(15);
		arg._long = -1;
		NEXT_STATE(mobileMiddle)
	}
case mobileDownToMiddle:
	{
		if (arg._long)
			mobileClearLift();
		setMobile(MOBILE_DOWN_POWER);
		unsigned long timeout = nPgmTime + 1000;
		while (gSensor[mobilePoti].value > MOBILE_MIDDLE_DOWN && !TimedOut(timeout, "mobileUpToMiddle")) sleep(10);
		setMobile(15);
		arg._long = -1;
		NEXT_STATE(mobileMiddle)
	}
case mobileMiddle:
	while (gSensor[mobilePoti].value < MOBILE_MIDDLE_THRESHOLD) sleep(10);
		arg._long = -1;
	NEXT_STATE(mobileTop)
})

void mobileWaitForSlowHold(TVexJoysticks btn)
{
	unsigned long timeout = nPgmTime + MOBILE_SLOW_HOLD_TIMEOUT;
	while (nPgmTime < timeout)
	{
		if (!gJoy[btn].cur) return;
		sleep(10);
	}
	gMobileSlow = true;
	if (mobileState == mobileBottom)
		mobileSet(mobileBottomSlow);
	writeDebugStreamLine("mobileBottomSlow activated");
}

NEW_ASYNC_VOID_1(mobileWaitForSlowHold, TVexJoysticks);

void handleMobile()
{
	if (mobileState == mobileManaged)
		return;

	if (mobileState == mobileUpToMiddle || mobileState == mobileDownToMiddle || mobileState == mobileMiddle)
	{
		if (RISING(BTN_MOBILE_MIDDLE))
			mobileSet(mobileTop, -1);
		if (RISING(BTN_MOBILE_TOGGLE))
		{
			gMobileSlow = false;
			mobileSet(mobileManaged, -1);
			detachIntakeAsync(mobileBottom);
			mobileWaitForSlowHoldAsync(BTN_MOBILE_MIDDLE);
		}
	}
	else
	{
		if (RISING(BTN_MOBILE_TOGGLE))
		{
			if (gSensor[mobilePoti].value > MOBILE_HALFWAY)
			{
				gMobileSlow = false;
				mobileSet(mobileManaged, -1);
				detachIntakeAsync(mobileBottom);
				mobileWaitForSlowHoldAsync(BTN_MOBILE_TOGGLE);
			}
			else
				mobileSet(mobileTop, -1);
		}
		if (RISING(BTN_MOBILE_MIDDLE))
			mobileSet(gSensor[mobilePoti].value > MOBILE_HALFWAY ? mobileDownToMiddle : mobileUpToMiddle);
	}
}


/* Macros + Autonomous */

bool gLiftAsyncDone;
bool gContinueLoader = false;
bool gLiftTargetReached;



byte stackAsync(bool arg0, bool arg1);
byte dropArmAsync();

bool gKillDriveOnTimeout = false;

bool TimedOut(unsigned long timeOut, const string description)
{
	if (nPgmTime > timeOut)
	{
		tHog();
		writeDebugStreamLine("%06d EXCEEDED TIME %d - %s", nPgmTime - gOverAllTime, timeOut - gOverAllTime, description);
		int current = nCurrentTask;
		if (current == gUserControlTaskId || current == main)
		{
			int child = tEls[current].child;
			while (child != -1)
			{
				tStopAll(child);
				child = tEls[child].next;
			}
		}
		else
		{
			while (true)
			{
				int next = tEls[current].parent;
				if (next == -1 || next == gUserControlTaskId || next == main) break;
				current = next;
			}
		}
		//setLift(0, true);
		//setArm(0, true);
		//setMobile(0, true);
		//if (gKillDriveOnTimeout) setDrive(0, 0, true);
		//updateMotors();
		gTimedOut = gSetTimedOut;
		for (tMotor x = port1; x <= port10; ++x)
			gMotor[x].power = motor[x] = 0;
		armReset();
		liftReset();
		mobileReset();
		gDriveManual = true;
		if (nCurrentTask == gUserControlTaskId || current == main)
		{
			tRelease();
			startTaskID(current);
		}
		else
			tStopAll(current);
		return true;
	}
	else
		return false;
}

// STACKING ON                     0   1   2   3   4   5   6   7   8   9    10
const int gLiftRaiseTarget[11] = { 18, 25, 37, 45, 53, 64, 72, 81, 92, 102, 120 };
const int gLiftPlaceTarget[11] = { 1,  13, 20, 25, 34, 42, 51, 60, 70, 76,  90 };

void clearArm()
{
	writeDebugStreamLine("clearArm");
	sSimpleConfig config;
	configure(config, gNumCones == 11 ? LIFT_POS(LIFT_TOP) : gLiftRaiseTarget[gNumCones], 127, 0);
	liftSet(liftRaiseSimple, &config);
	unsigned long timeout = nPgmTime + 1500;
	liftTimeoutWhile(liftRaiseSimple, timeout);

	configure(config, ARM_TOP, 127, -15);
	armSet(armRaiseSimple, &config);
	timeout = nPgmTime + 1000;
	armTimeoutWhile(armRaiseSimple, timeout);
}

NEW_ASYNC_VOID_0(clearArm);

void detachIntake(tMobileStates nextMobileState)
{
	if (gSensor[liftEnc].value < (gNumCones == 11 ? LIFT_POS(LIFT_TOP) : gLiftRaiseTarget[gNumCones]) && gNumCones > 0)
	{
		//goto skip;
		////lower lift
		//sSimpleConfig liftConfig;
		//configure(liftConfig, gLiftPlaceTarget[MIN(gNumCones, 10)], -127, 10);
		//liftSet(liftLowerSimple, &liftConfig);
		//unsigned long liftTimeOut = nPgmTime + 800;
		//liftTimeoutWhile(liftLowerSimple, liftTimeOut);

		//lower arm
		sSimpleConfig armConfig;
		configure(armConfig, ARM_PRESTACK - 100, -127, 0);
		armSet(armLowerSimple, &armConfig);
		unsigned long armTimeOut = nPgmTime + 800;
		armTimeoutWhile(armLowerSimple, armTimeOut);
		clearArm();

//skip:
//		clearArm();
	}

	mobileSet(nextMobileState);
}

NEW_ASYNC_VOID_1(detachIntake, tMobileStates);

void stack(bool pickup, bool downAfter)
{
	writeDebugStreamLine(" STACKING on %d", gNumCones);

	unsigned long armTimeOut;
	unsigned long liftTimeOut;

	sSimpleConfig armConfig, liftConfig;

	if (pickup)
	{
		armSet(armToTarget, ARM_HORIZONTAL);
		armTimeOut = nPgmTime + 1000;
		timeoutWhileGreaterThanL(&gSensor[armPoti].value, ARM_PRESTACK, armTimeOut);

		configure(liftConfig, LIFT_POS(LIFT_BOTTOM), -127, 0);
		liftSet(liftLowerSimple, &liftConfig);
		liftTimeOut = nPgmTime + 1200;
		timeoutWhileGreaterThanL(&gSensor[liftEnc].value, 5, liftTimeOut);

		configure(armConfig, ARM_BOTTOM + 250, -127, 0);
		armSet(armLowerSimple, &armConfig);
		armTimeOut = nPgmTime + 800;
		armTimeoutWhile(armLowerSimple, armTimeOut);
		liftTimeoutWhile(liftLowerSimple, liftTimeOut);
	}

	configure(liftConfig, gLiftRaiseTarget[gNumCones], 80, -15);
	liftSet(liftRaiseSimple, &liftConfig);
	liftTimeOut = nPgmTime + 1500;
	timeoutWhileLessThanL(&gSensor[liftEnc].value, gLiftPlaceTarget[gNumCones], liftTimeOut);

	configure(armConfig, ARM_STACK, 127, -12);
	armSet(armRaiseSimple, &armConfig);
	armTimeOut = nPgmTime + 1000;
	liftTimeoutWhile(liftRaiseSimple, liftTimeOut);
	armTimeoutWhile(armRaiseSimple, armTimeOut);

	configure(liftConfig, gLiftPlaceTarget[gNumCones], -70, 0);
	liftSet(liftLowerSimple, &liftConfig);
	liftTimeOut = nPgmTime + 800;
	liftTimeoutWhile(liftLowerSimple, liftTimeOut);

	++gNumCones;

	if (downAfter)
	{
		configure(armConfig, ARM_HORIZONTAL, -127, 25);
		armSet(armLowerSimple, &armConfig);
		armTimeOut = nPgmTime + 1000;
		timeoutWhileGreaterThanL(&gSensor[armPoti].value, ARM_PRESTACK, armTimeOut);

		if (gNumCones <= 4)
		{
			configure(liftConfig, 28, 80, -25);
			liftSet(liftRaiseSimple, &liftConfig);
			liftTimeOut = nPgmTime + 800;
			liftTimeoutWhile(liftRaiseSimple, liftTimeOut);
		}
		else
		{
			configure(liftConfig, 44, -80, 25);
			liftSet(liftLowerSimple, &liftConfig);
			liftTimeOut = nPgmTime + 1000;
			liftTimeoutWhile(liftLowerSimple, liftTimeOut);
		}

		armTimeoutWhile(armLowerSimple, armTimeOut);
	}
}

NEW_ASYNC_VOID_2(stack, bool, bool);

bool stackRunning()
{
	for (int i = 0; i < TASK_POOL_SIZE; ++i)
	{
		if (gAsyncTaskData[i].id == &stackDummy && tEls[threadPoolTask0 + i].parent != -1)
			return true;
	}
	return false;
}

float gLoaderOffset[12] = { 5.5, 4.5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 };
sNotifier gStackFromLoaderNotifier;

void stackFromLoader(int max, bool wait, bool onMobile)
{
	//if (max > 7) max = 7;
	//gLiftState = liftManaged;
	//gArmState = armManaged;
	//gClawStart = clawManaged;
	//gDriveManual = false;
	//EndTimeSlice();
	//float sy = onMobile ? gLoaderOffset[gNumCones] : 2;
	//resetPositionFull(gPosition, sy, 0, 0);
	//trackPositionTaskAsync();
	//byte async = moveToTargetAsync(0.5, 0, sy, 0, -25, 3, 0.5, 0.5, true, false);
	//unsigned long driveTimeout = nPgmTime + 2000;
	////setClaw(CLAW_OPEN_POWER, true);
	//unsigned long coneTimeout = nPgmTime + 1000;
	////while (gSensor[clawPoti].value < CLAW_OPEN && !TimedOut(coneTimeout, "loader 1")) sleep(10);
	//setClaw(CLAW_OPEN_HOLD_POWER);
	//await(async, driveTimeout, "loader 1");
	//trackPositionTaskKill();
	//gDriveManual = true;
	//if (wait && waitOn(gStackFromLoaderNotifier, nPgmTime + 60_000, "loader 2") == -1) goto end;
	//while (gNumCones < max)
	//{
	//	gLiftState = liftManaged;
	//	gArmState = armManaged;
	//	gClawStart = clawManaged;
	//	if (gSensor[armPoti].value < 1400)
	//	{
	//		setArm(60);
	//		while (gSensor[armPoti].value < 1400) sleep(10);
	//		setArm(-7);
	//	}
	//	else if (gSensor[armPoti].value > 1500)
	//	{
	//		if (gSensor[armPoti].value > 1700)
	//		{
	//			setArm(-80);
	//			while (gSensor[armPoti].value > 1700) sleep(10);
	//			setArm(-50);
	//		}
	//		while (gSensor[armPoti].value > 1500) sleep(10);
	//		setArm(5);
	//	}
	//	else setArm(-10);
	//	setLift(-80);
	//	coneTimeout = nPgmTime + 1000;
	//	while (gSensor[liftEnc].value > LIFT_BOTTOM + 500 && !TimedOut(coneTimeout, "loader 2")) sleep(10);
	//	setLift(-60);
	//	while (!gSensor[limBottom].value && !TimedOut(coneTimeout, "loader 3")) sleep(10);
	//	setLift(-10);
	//	sleep(200);
	//	setArm(-60);
	//	coneTimeout = nPgmTime + 500;
	//	while (gSensor[armPoti].value > 1500 && !TimedOut(coneTimeout, "loader 4")) sleep(10);
	//	setArm(-10);
	//	sleep(300);
	//	stack(false);
	//}
	//end:
	//gLiftState = liftIdle;
	//gArmState = armIdle;
	//gClawState = clawIdle;
	//writeDebugStreamLine("Done stacking from loader");
}

NEW_ASYNC_VOID_3(stackFromLoader, int, bool, bool);

bool cancel()
{
	if (stackKill() || stackFromLoaderKill())
	{
		liftReset();
		armReset();
		gDriveManual = true;
		writeDebugStreamLine("Stack cancelled");
		return true;
	}
	return false;
}

bool gStack = false;

void handleMacros()
{
	if (RISING(BTN_MACRO_STACK))
	{
		gStack = true;
	}
	if (RISING(BTN_MACRO_CLEAR))
	{
		writeDebugStreamLine("Clearing lift and arm");
		stackKill();
		clearArmAsync();
	}

	if (gStack == true && gNumCones < 11 )
	{
		if (!stackRunning())
		{
			writeDebugStreamLine("Stacking");
			stackAsync(true, gNumCones < 10);
			gStack = false;
		}
	}

	//if (RISING(BTN_MACRO_LOADER))
	//{
	//	if (!cancel())
	//	{
	//		writeDebugStreamLine("Stacking from loader");
	//		stackFromLoaderAsync(11, true, gMobileState == mobileHold && gMobileTarget == MOBILE_TOP);
	//		playSound(soundUpwardTones);
	//	}
	//}

	//if (FALLING(BTN_MACRO_LOADER)) notify(gStackFromLoaderNotifier);

	if (RISING(BTN_MACRO_CANCEL)) cancel();

	if (FALLING(BTN_MACRO_INC))
		writeDebugStreamLine("%06d MAcro_INC Released",nPgmTime,gNumCones);
	if (RISING(BTN_MACRO_INC) && gNumCones < 11) {
		++gNumCones;
		writeDebugStreamLine("%06d gNumCones= %d",nPgmTime,gNumCones);
	}

	if (RISING(BTN_MACRO_DEC) && gNumCones > 0) {
		--gNumCones;
		writeDebugStreamLine("%06d gNumCones= %d",nPgmTime,gNumCones);
	}

	if (FALLING(BTN_MACRO_ZERO))	writeDebugStreamLine("%06d MACRO_ZERO Released",nPgmTime,gNumCones);

	if (RISING(BTN_MACRO_ZERO)) {

		gNumCones = 0;
		writeDebugStreamLine("%06d gNumCones= %d",nPgmTime,gNumCones);
	}
}

#include "auto.c"
#include "auto_simple.c"
#include "auto_runs.c"


/* LCD */

void handleLcd()
{
	string line;

#ifdef DEBUG_TRACKING
	sprintf(line, "%3.2f %3.2f", gPosition.y, gPosition.x);
	clearLCDLine(0);
	displayLCDString(0, 0, line);

	sprintf(line, "%3.2f", radToDeg(gPosition.a));
	clearLCDLine(1);
	displayLCDString(1, 0, line);

	if (nLCDButtons) resetPositionFull(gPosition, 0, 0, 0);
#else
	sprintf(line, "%4d %2.1f %2d", gSensor[armPoti].value, LIFT_HEIGHT(gSensor[liftEnc].value), gNumCones);
	clearLCDLine(0);
	displayLCDString(0, 0, line);

	velocityCheck(trackL);
	velocityCheck(trackR);

	sprintf(line, "%2.1f %2.1f %s%c", gSensor[trackL].velocity, gSensor[trackR].velocity, gAlliance == allianceRed ? "Red  " : "Blue ", '0' + gCurAuto);
	clearLCDLine(1);
	displayLCDString(1, 0, line);
#endif
}

#ifndef SKILLS_RESET_AT_START
void waitForSkillsReset()
{
	while (!gSensor[btnSetPosition].value)
	{
		updateSensorInput(btnSetPosition);
		sleep(10);
	}
	autoMotorSensorUpdateTaskAsync();
	trackPositionTaskAsync();
	resetPositionFull(gPosition, 8.25, 61.5, 0);
	writeDebugStreamLine("RESET");
}

NEW_ASYNC_VOID_0(waitForSkillsReset);
#endif

// This function gets called 2 seconds after power on of the cortex and is the first bit of code that is run
void startup()
{
	clearDebugStream();
	writeDebugStreamLine("Code start");

	// Setup and initilize the necessary libraries
	setupMotors();
	setupSensors();
	setupJoysticks();
	tInit();
	asyncInit();

	mobileSetup();
	liftSetup();

	setupInvertedSen(jmpSkills);

	velocityClear(trackL);
	velocityClear(trackR);

#ifndef SKILLS_RESET_AT_START
	waitForSkillsResetAsync();
#endif

	gJoy[JOY_TURN].deadzone = DZ_TURN;
	gJoy[JOY_THROTTLE].deadzone = DZ_THROTTLE;
	gJoy[JOY_LIFT].deadzone = DZ_LIFT;
	gJoy[JOY_ARM].deadzone = DZ_ARM;

	enableJoystick(JOY_TURN);
	enableJoystick(JOY_THROTTLE);
	enableJoystick(JOY_LIFT);
	enableJoystick(JOY_ARM);
	enableJoystick(BTN_ARM_BACK);
	enableJoystick(BTN_MOBILE_TOGGLE);
	enableJoystick(BTN_MOBILE_MIDDLE);
	enableJoystick(BTN_MACRO_ZERO);
	enableJoystick(BTN_MACRO_CLEAR);
	enableJoystick(BTN_MACRO_STACK);
	enableJoystick(BTN_MACRO_LOADER);
	enableJoystick(BTN_MACRO_STATIONARY);
	enableJoystick(BTN_MACRO_CANCEL);
	enableJoystick(BTN_MACRO_RIGHT);
	enableJoystick(BTN_MACRO_INC);
	enableJoystick(BTN_MACRO_DEC);
}

// This function gets called every 25ms during disabled (DO NOT PUT BLOCKING CODE IN HERE)
void disabled()
{
	updateSensorInput(autoPoti);
	selectAuto();
	handleLcd();
}

// This task gets started at the begining of the autonomous period
void autonomous()
{
	gAutoTime = nPgmTime;
	writeDebugStreamLine("Auto start %d", gAutoTime);

	gUserControlTaskId = -1;

	startSensors(); // Initilize the sensors

	gKillDriveOnTimeout = true;
	gSetTimedOut = true;
	gTimedOut = false;

	//resetPosition(gPosition);
	//resetQuadratureEncoder(trackL);
	//resetQuadratureEncoder(trackR);
	//resetQuadratureEncoder(trackB);

	autoMotorSensorUpdateTaskAsync();
	trackPositionTaskAsync();

	runAuto();

	writeDebugStreamLine("Auto: %d ms", nPgmTime - gAutoTime);

	return_t;
}

// This task gets started at the beginning of the usercontrol period
void usercontrol()
{
	gUserControlTaskId = nCurrentTask;
	gSetTimedOut = false;
	gTimedOut = false;

	startSensors(); // Initilize the sensors
#if defined(DEBUG_TRACKING) || defined(TRACK_IN_DRIVER)
	initCycle(gMainCycle, 15, "main");
#else
	initCycle(gMainCycle, 10, "main");
#endif

	updateSensorInput(jmpSkills);

#if defined(DEBUG_TRACKING) || defined(TRACK_IN_DRIVER)
	trackPositionTaskAsync();
#endif

	liftSet(liftResetEncoder);

	if (gSensor[jmpSkills].value)
	{
		autoMotorSensorUpdateTaskAsync();
		trackPositionTaskAsync();

		driverSkillsStart();

		trackPositionTaskKill();
		autoMotorSensorUpdateTaskKill();

		mobileSet(mobileTop);

		armSet(armHold);
	}

	armReset();
	mobileReset();

	gKillDriveOnTimeout = false;
	gDriveManual = true;
	gMobileCheckLift = true;

	while (true)
	{
		updateSensorInputs();
		updateJoysticks();

		selectAuto();

		handleDrive();
		handleLift();
		handleArm();
		handleMobile();
		handleMacros();

		if (RISING(BTN_MACRO_CANCEL))
			writeDebugStreamLine("%f %f %f", gPosition.y, gPosition.x, gPosition.a);

		handleLcd();

		if (RISING(BTN_MACRO_CANCEL))
		{
			writeDebugStreamLine("%f", abs((2.785 * (gSensor[trackL].value - gSensor[trackR].value)) / (360 * 4)));
			resetQuadratureEncoder(trackL);
			resetQuadratureEncoder(trackR);
		}

		updateSensorOutputs();
		updateMotors();
		endCycle(gMainCycle);
	}

	return_t;
}

ASYNC_ROUTINES
(
USE_ASYNC(timeoutWhileEqual)
USE_ASYNC(timeoutWhileNotEqual)
USE_ASYNC(timeoutWhileLessThanS)
USE_ASYNC(timeoutWhileGreaterThanS)
USE_ASYNC(timeoutWhileLessThanL)
USE_ASYNC(timeoutWhileGreaterThanL)
USE_ASYNC(timeoutWhileLessThanF)
USE_ASYNC(timeoutWhileGreaterThanF)
USE_ASYNC(autonomous)
USE_ASYNC(usercontrol)
USE_ASYNC(mobileWaitForSlowHold)
USE_ASYNC(clearArm)
USE_ASYNC(detachIntake)
USE_ASYNC(stack)
USE_ASYNC(stackFromLoader)
USE_ASYNC(trackPositionTask)
USE_ASYNC(autoMotorSensorUpdateTask)
//USE_ASYNC(autoSafetyTask)
//USE_ASYNC(moveToTarget)
//USE_ASYNC(turnToAngle)
//USE_ASYNC(turnToTarget)
USE_ASYNC(moveToTargetSimple)
USE_ASYNC(moveToTargetDisSimple)
USE_ASYNC(turnToAngleSimple)
USE_ASYNC(turnToTargetSimple)
USE_ASYNC(turnToAngleStupid)
USE_ASYNC(turnToTargetStupid)
USE_ASYNC(turnToAngleCustom)
USE_ASYNC(turnToTargetCustom)
USE_ASYNC(turnToAngleNew)
USE_ASYNC(turnToTargetNew)
//USE_ASYNC(waitForSkillsReset)
USE_ASYNC(resetBlueRight)
USE_MACHINE(lift)
USE_MACHINE(arm)
USE_MACHINE(mobile)
)
