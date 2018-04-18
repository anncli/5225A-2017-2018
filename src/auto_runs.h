/* Enumerations */
typedef enum _tAlliance
{
	allianceBlue,
	allianceRed
} tAlliance;

/* Functions */
void selectAuto();
void runAuto();
void autoSkills(int segment = -1);

#if SKILLS_ROUTE == 0

void autoBlock();

#elif SKILLS_ROUTE < 0

void autoTest();

#endif

/* Variables */
int gCurAuto = 0;
tAlliance gAlliance = allianceBlue;

/* Defines */
#if SKILLS_ROUTE == 0
#define AUTO_OPTIONS_COUNT 11
#elif SKILLS_ROUTE == 1
#define AUTO_OPTIONS_COUNT 1
#define SKILLS_1_SAFE (gAlliance == allianceRed)
#else
#define AUTO_OPTIONS_COUNT 0
#endif
