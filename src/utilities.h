/* Defines */
// Limit a variable to a value
#define LIM_TO_VAL(input, val) (abs(input) > (val) ? (val) * sgn(input) : (input))

// Limit a variable to a value and set that variable to the result
#define LIM_TO_VAL_SET(input, val) input = LIM_TO_VAL(input, val)

// The length of an array
#define ARR_LEN(array) (sizeof(array) / sizeof(array[0]))

// Swaps any integral type
#define SWAP(x, y) { x = x ^ y; y = x ^ y; x = x ^ y; }

// Checks the bit in a bitmap
#define CHK_BIT(bit, map) ((map & bit) == bit)

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define NORMAL_RAD(a) (fmod(a + PI, PI * 2) - PI)

#define STOP_TASK_NOT_CUR(t) if (t != nCurrentTask) stopTask(t)

/* Functions */
float fmod(float x, float y); // Floating point mod operation
float sq(float x); // Square a value
int sq(int x); // Square a value
float degToRad(float degrees); // Convert degrees to radians
float radToDeg(float radians); // Convert radians to degrees
void stopAllButCurrentTasks();
