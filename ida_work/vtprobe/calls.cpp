// Exactly the call pattern used by the new ApplyVanillaGuardCommand.
#include <FootClass.h>
#include <ObjectClass.h>
#include <TechnoClass.h>

void ApplyVanillaGuardCommand(TechnoClass* pTechno)
{
	CellClass* pCell = pTechno->GetCellAgain();
	pTechno->ClickedMission(Mission::Area_Guard, reinterpret_cast<ObjectClass*>(pCell), nullptr, nullptr);
}
