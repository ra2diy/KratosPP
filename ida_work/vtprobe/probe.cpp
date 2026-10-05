// Probe: ask MSVC for the real vtable layout of the YRpp hierarchy + emitted displacements.
#include <ObjectClass.h>
#include <TechnoClass.h>
#include <FootClass.h>

void probe(ObjectClass* o, TechnoClass* t, FootClass* f)
{
	o->GetCell();
	o->GetMapCoords();
	o->GetMapCoordsAgain();
	o->GetCellAgain();
	CellStruct cs{ 0, 0 };
	o->SetLocation(cs);
	o->GetHeight();
	o->SetHeight(0);
	o->GetZ();
	o->IsWarpingIn();
	o->IsNotWarping();
	o->IsSurfaced();
	o->GetTechnoType();

	t->QueueVoice(0);
	t->IsPowerOnline();
	t->EnterGrinder();
	int out = 0;
	t->IsRadarVisible(&out);
	t->VoiceMove();
	t->ClickedEvent(NetworkEvents::None);
	t->CanAttackOnTheMove();
	t->IsReadyToCloak();
	t->ShouldNotBeCloaked();
	t->Limbo();
	t->GetCellAgain();

	f->CanScatter();
}
