/* Enumerations */
typedef enum _stopType
{
	stopNone =		0b00000000,
	stopSoft =		0b00000001,
	stopHarsh =		0b00000010
} tStopType;

/* Structures */
typedef struct _turnState
{
	float target;
	float power;
	float error;
	float lstError;
	float integral;
	float input;
	unsigned long time;
	unsigned long lstTime;
	unsigned long nextDebug;
} sTurnState;

/* Variables */
sVector gTargetLast;

/* Functions */
void moveToTargetSimple(float y, float x, float ys, float xs, byte power, float dropEarly = 0, tStopType stopType = stopSoft | stopHarsh, bool slow = true);
void moveToTargetSimple(float y, float x, byte power, float dropEarly = 0, tStopType stopType = stopSoft | stopHarsh, bool slow = true);
void moveToTargetDisSimple(float a, float d, float ys, float xs, byte power, float dropEarly = 0, tStopType stopType = stopSoft | stopHarsh, bool slow = true);
void moveToTargetDisSimple(float a, float d, byte power, float dropEarly = 0, tStopType stopType = stopSoft | stopHarsh, bool slow = true);
void turnToAngleRadSimple(float a, tTurnDir turnDir, byte left, byte right);
void turnToAngleSimple(float a, tTurnDir turnDir, byte left, byte right);
void turnToTargetSimple(float y, float x, tTurnDir turnDir, byte left, byte right, float offset = 0);
void turnSimpleInternalCw(float a, sTurnState& state);
void turnSimpleInternalCcw(float a, sTurnState& state);
void turnToAngleStupidCw(float a);
void turnToAngleStupidCcw(float a);
void turnToTargetStupidCw(float y, float x, float offset);
void turnToTargetStupidCcw(float y, float x, float offset);

/* Async Functions */
NEW_ASYNC_VOID_8(moveToTargetSimple, float, float, float, float, byte, float, tStopType, bool);
NEW_ASYNC_VOID_8(moveToTargetDisSimple, float, float, float, float, byte, float, tStopType, bool);
NEW_ASYNC_VOID_4(turnToAngleSimple, float, tTurnDir, byte, byte);
NEW_ASYNC_VOID_6(turnToTargetSimple, float, float, tTurnDir, byte, byte, float);
NEW_ASYNC_VOID_1(turnToAngleStupidCw, float);
NEW_ASYNC_VOID_1(turnToAngleStupidCcw, float);
NEW_ASYNC_VOID_3(turnToTargetStupidCw, float, float, float);
NEW_ASYNC_VOID_3(turnToTargetStupidCcw, float, float, float);
