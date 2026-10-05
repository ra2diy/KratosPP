#include <TechnoClass.h>
#include <cstdio>
int main() {
    printf("AngleRotatedSideways=+0x%X\n", offsetof(TechnoClass, AngleRotatedSideways));
    printf("AngleRotatedForwards=+0x%X\n", offsetof(TechnoClass, AngleRotatedForwards));
    printf("RockingSidewaysPerFrame=+0x%X\n", offsetof(TechnoClass, RockingSidewaysPerFrame));
    printf("PrimaryFacing=+0x%X\n", offsetof(TechnoClass, PrimaryFacing));
    printf("CountedAsOwned=+0x%X\n", offsetof(TechnoClass, CountedAsOwned));
    printf("IsSinking=+0x%X\n", offsetof(TechnoClass, IsSinking));
    printf("WasSinkingAlready=+0x%X\n", offsetof(TechnoClass, WasSinkingAlready));
    printf("IsCrashing=+0x%X\n", offsetof(TechnoClass, IsCrashing));
    return 0;
}
