//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "weapon_citizenpackage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Old Man Harpoon - Lost Coast.
//-----------------------------------------------------------------------------
class CWeaponOldManHarpoon : public CWeaponCitizenPackage
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CWeaponOldManHarpoon, CWeaponCitizenPackage );

	CWeaponOldManHarpoon();

	DECLARE_SERVERCLASS();
	
	DECLARE_ACTTABLE();
};

//IMPLEMENT_SERVERCLASS_ST( CWeaponOldManHarpoon, DT_WeaponOldManHarpoon )
IMPLEMENT_SERVERCLASS_ST(CWeaponOldManHarpoon, DT_WeaponOldManHarpoon)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_oldmanharpoon, CWeaponOldManHarpoon );
PRECACHE_WEAPON_REGISTER( weapon_oldmanharpoon );

BEGIN_DATADESC( CWeaponOldManHarpoon )
END_DATADESC()

acttable_t	CWeaponOldManHarpoon::m_acttable[] = 
{
	{ ACT_IDLE,						ACT_IDLE_SUITCASE,					false },
	{ ACT_WALK,						ACT_WALK_SUITCASE,					false },
};
IMPLEMENT_ACTTABLE( CWeaponOldManHarpoon );

//=========================================================
CWeaponOldManHarpoon::CWeaponOldManHarpoon( )
{

}
