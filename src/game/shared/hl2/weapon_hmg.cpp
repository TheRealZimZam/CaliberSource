//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: HMG1 - Jumbo Pistol
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon_shared.h"
#include "NPCevent.h"
#include "baseplayer_shared.h"
#include "in_buttons.h"
#include "rumble_shared.h"
#include "gamestats.h"
#ifndef CLIENT_DLL
	#include "game.h"
	#include "basecombatcharacter.h"
	#include "AI_BaseNPC.h"
	#include "AI_Memory.h"
	#include "soundent.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sv_funmode;

ConVar sk_weapon_hmg1_lerp( "sk_weapon_hmg1_lerp", "2.5" );


#ifdef CLIENT_DLL
#define CWeaponHMG1 C_WeaponHMG1
#endif
class CWeaponHMG1 : public CHLSelectFireMachineGun
{
public:
	DECLARE_CLASS( CWeaponHMG1, CHLSelectFireMachineGun );

	CWeaponHMG1();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_8DEGREES;
		if ( GetHSpread() != NULL )
			cone = Vector( GetHSpread(), (GetHSpread() / 2) + (GetVSpread() / 2), GetVSpread() );

		if( GetOwner() && GetOwner()->IsPlayer() && m_iFireMode == FIREMODE_3RNDBURST )
			cone = VECTOR_CONE_4DEGREES;

		return cone;
	}

	void			Precache( void );
	void			AddViewKick( void );

	const char *GetTracerType( void ) { return "BigTracer"; }
	float			GetFireRate( void ) { return BaseClass::GetCycleTime(); }
	virtual int		GetBurstSize( void ) { return 4; }

	int		GetMinBurst() { return 8; }
	int		GetMaxBurst() { return 10; }
	virtual float	GetMinRestTime() { return 0.5; }
	virtual float	GetMaxRestTime() { return 0.9; }

#ifndef CLIENT_DLL
	void			Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	int				CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
#endif

	DECLARE_ACTTABLE();

/*
private:
	float			m_flHeat;
	bool			m_bOverheating;
*/
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponHMG1, DT_WeaponHMG1 )

BEGIN_NETWORK_TABLE( CWeaponHMG1, DT_WeaponHMG1 )
#ifdef CLIENT_DLL
//	RecvPropFloat( RECVINFO( m_flHeat ) ),
//	RecvPropBool( RECVINFO( m_bOverheating ) ),
#else
//	SendPropFloat( SENDINFO( m_flHeat ) ),
//	SendPropBool( SENDINFO( m_bOverheating ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponHMG1 )
//	DEFINE_PRED_FIELD( m_flHeat, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_bOverheating, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_hmg1, CWeaponHMG1 );
PRECACHE_WEAPON_REGISTER(weapon_hmg1);

acttable_t	CWeaponHMG1::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,		ACT_RANGE_ATTACK_SMG1,		true },	// FIXME: hook to HMG unique
	{ ACT_RELOAD,				ACT_RELOAD_SMG1,			true },	// FIXME: hook to HMG unique
	{ ACT_IDLE,					ACT_IDLE_SMG1,				true },	// FIXME: hook to HMG unique
	{ ACT_IDLE_ANGRY,			ACT_IDLE_ANGRY_SMG1,		true },	// FIXME: hook to HMG unique

	{ ACT_WALK,					ACT_WALK_AIM_RIFLE,			true },	// FIXME: hook to HMG unique
	{ ACT_WALK_AIM,				ACT_WALK_AIM_RIFLE,			true },	// FIXME: hook to HMG unique
	{ ACT_RUN,					ACT_RUN_AIM_RIFLE,			true },	// FIXME: hook to HMG unique
	{ ACT_RUN_AIM,				ACT_RUN_AIM_RIFLE,			true },	// FIXME: hook to HMG unique

	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_HMG1,	true },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },	// FIXME: hook to HMG unique
};

IMPLEMENT_ACTTABLE(CWeaponHMG1);

//=========================================================
CWeaponHMG1::CWeaponHMG1( )
{
	m_fMinRange1		= 32;
	m_fMaxRange1		= 2000;
//	m_flHeat			= 0;
//	m_bOverheating		= true;

	if ( sv_funmode.GetBool() )
	{
		m_bCanJam			= true;
	}
	m_iFireMode			= FIREMODE_3RNDBURST;
}

void CWeaponHMG1::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponHMG1::AddViewKick( void )
{
	#define	EASY_DAMPEN			0.5f
	#define	MAX_VERTICAL_KICK	10.0f	//Degrees
	
	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
		return;

	DoMachineGunKick( pPlayer, EASY_DAMPEN, MAX_VERTICAL_KICK, m_fFireDuration, sk_weapon_hmg1_lerp.GetFloat() );
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponHMG1::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_HMG1:
		case EVENT_WEAPON_SMG1:
		{
			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition( );

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );
			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			WeaponSound(SINGLE_NPC);
			pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2 );
			pOperator->DoMuzzleFlash();
			m_iClip1 = m_iClip1 - 1;
		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}
#endif