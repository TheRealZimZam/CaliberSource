//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a sniper rifle weapon.
//			
//			Primary attack: fires a single high-powered shot, then reloads.
//			Secondary attack: cycles sniper scope through zoom levels.
//
// TODO: Circular mask around crosshairs when zoomed in.
// TODO: Finalize kickback.
// TODO: Use CSniperBullet - hitscanning with this thing is underwhelming
// When given to a npc (npc_combine_e), this is to be used mostly as a thematic device,
// it does represents the real sniper (proto_sniper), but without the laser 
// and long-range OP-ness of the said sniper.
// NOTENOTE; This can be used as a baseclass for weapons that have a zoom
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "weapon_sniperrifle.h"
#include "baseplayer_shared.h"
#include "gamerules.h"				// For g_pGameRules
#include "in_buttons.h"
#include "vstdlib/random.h"
#ifndef CLIENT_DLL
	#include "basecombatcharacter.h"
	#include "AI_BaseNPC.h"
	#include "soundent.h"
	#include "te_effect_dispatch.h"
	#include "proto_sniper.h"			// For SniperBullet
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SNIPER_ZOOM_RATE					0.2f			// Interval between zoom levels in seconds.

ConVar sk_sniper_zoomout_on_reload( "sk_sniper_zoomout_on_reload", "1", FCVAR_REPLICATED | FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Discrete zoom levels for the scope.
//-----------------------------------------------------------------------------
static int g_nZoomFOV[] =
{
	20,
	5
};

IMPLEMENT_NETWORKCLASS_ALIASED( HLSniperRifle, DT_HLSniperRifle )

BEGIN_NETWORK_TABLE( CHLSniperRifle, DT_HLSniperRifle )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_nZoomLevel ) ),
	RecvPropFloat( RECVINFO( m_fNextZoom ) ),
#else
	SendPropInt( SENDINFO( m_nZoomLevel ) ),
	SendPropFloat( SENDINFO( m_fNextZoom ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CHLSniperRifle )
	DEFINE_PRED_FIELD( m_nZoomLevel, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fNextZoom, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Maps base activities to weapons-specific ones so our characters do the right things.
//-----------------------------------------------------------------------------
acttable_t	CHLSniperRifle::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SNIPER_RIFLE,	true },
	{ ACT_RELOAD,					ACT_RELOAD_AR2,					true },
	{ ACT_IDLE,						ACT_IDLE_AR2,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_AR2,				true },

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },

	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SNIPER_RIFLE,	true },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_AR2,			true }
};

IMPLEMENT_ACTTABLE(CHLSniperRifle);


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CHLSniperRifle::CHLSniperRifle( )
{
	m_fNextZoom = gpGlobals->curtime;
	m_nZoomLevel = 0;

	m_bReloadsSingly = true;	//Reload one bullet at a time
	m_bBoltAction = false;	//Bolt-action style firing defaults off
	m_bUseProjectile = true;	//Projectile defaults on

	m_fMinRange1		= 64;
	m_fMinRange2		= 64;
	m_fMaxRange1		= 2048;
	m_fMaxRange2		= 2048;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLSniperRifle::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : virtual const Vector&
//-----------------------------------------------------------------------------
const Vector &CHLSniperRifle::GetBulletSpread( void )
{
	static Vector cone;
	cone = ( m_nZoomLevel != 0 ) ? SNIPER_CONE_PLAYER_ZOOMED : SNIPER_CONE_PLAYER;

	if ( GetOwner() && GetOwner()->IsNPC() )
		cone = SNIPER_CONE_NPC;

	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: Turns off the zoom when the rifle is holstered.
//-----------------------------------------------------------------------------
bool CHLSniperRifle::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (pPlayer != NULL)
	{
		if ( m_nZoomLevel != 0 )
		{
			if ( pPlayer->SetFOV( this, 0, 0.1f ) )
			{
				pPlayer->ShowViewModel(true);		
				m_nZoomLevel = 0;
			}
		}
	}
#endif

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Overloaded to handle the zoom functionality.
//-----------------------------------------------------------------------------
void CHLSniperRifle::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (pPlayer == NULL)
	{
		return;
	}

	if ((m_bInReload) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
	{
		FinishReload();
		m_bInReload = false;
	}

	if (pPlayer->m_nButtons & IN_ATTACK2)
	{
		if (m_fNextZoom <= gpGlobals->curtime)
		{
			Zoom();
			pPlayer->m_nButtons &= ~IN_ATTACK2;
		}
	}
	else if ((pPlayer->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
	{
		if ( (m_iClip1 == 0 && UsesClipsForAmmo1()) || ( !UsesClipsForAmmo1() && !pPlayer->GetAmmoCount(m_iPrimaryAmmoType) ) )
		{
			m_bFireOnEmpty = true;
		}

		// Fire underwater?
		if (pPlayer->GetWaterLevel() == 3 && m_bFiresUnderwater == false)
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
			return;
		}
		else
		{
			// If the firing button was just pressed, reset the firing time
			if ( pPlayer && pPlayer->m_afButtonPressed & IN_ATTACK )
			{
				 m_flNextPrimaryAttack = gpGlobals->curtime;
			}

			PrimaryAttack();
		}
	}

	// -----------------------
	//  Reload pressed / Clip Empty
	// -----------------------
	if ( pPlayer->m_nButtons & IN_RELOAD && UsesClipsForAmmo1() && !m_bInReload ) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}

	// -----------------------
	//  No buttons down
	// -----------------------
	if (!((pPlayer->m_nButtons & IN_ATTACK) || (pPlayer->m_nButtons & IN_ATTACK2) || (pPlayer->m_nButtons & IN_RELOAD)))
	{
		// no fire buttons down
		m_bFireOnEmpty = false;

		if ( !HasAnyAmmo() && m_flNextPrimaryAttack < gpGlobals->curtime ) 
		{
			// weapon isn't useable, switch.
			if ( !(GetWeaponFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && pPlayer->SwitchToNextBestWeapon( this ) )
			{
				m_flNextPrimaryAttack = gpGlobals->curtime + 0.3;
				return;
			}
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip1 == 0 && !(GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < gpGlobals->curtime )
			{
				Reload();
				return;
			}
		}

		WeaponIdle( );
		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Same as base reload but doesn't change the owner's next attack time. This
//			lets us zoom out while reloading. This hack is necessary because our
//			ItemPostFrame is only called when the owner's next attack time has
//			expired.
// Output : Returns true if the weapon was reloaded, false if no more ammo.
//-----------------------------------------------------------------------------
bool CHLSniperRifle::Reload( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if (!pOwner)
	{
		return false;
	}

	if ( m_nZoomLevel != 0 && sk_sniper_zoomout_on_reload.GetBool() )
	{
		m_nZoomLevel = 2;
		Zoom();
	}

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) > 0)
	{
		int primary		= min(GetMaxClip1() - m_iClip1, pOwner->GetAmmoCount(m_iPrimaryAmmoType));
		int secondary	= min(GetMaxClip2() - m_iClip2, pOwner->GetAmmoCount(m_iSecondaryAmmoType));

		if (primary > 0 || secondary > 0)
		{
			// Play reload on different channel as it happens after every fire
			// and otherwise steals channel away from fire sound
			WeaponSound(RELOAD);
			SendWeaponAnim( ACT_VM_RELOAD );
#ifndef CLIENT_DLL
			CSoundEnt::InsertSound( SOUND_WEAPON, GetAbsOrigin(), 256, 0.2, GetOwner() );
#endif
			// Play the player's reload animation
			if ( pOwner->IsPlayer() )
			{
				( ( CBasePlayer * )pOwner)->SetAnimation( PLAYER_RELOAD );
			}
			m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();

			m_bInReload = true;
		}
		return true;
	}

	return false;
}

bool CHLSniperRifle::CycleBolt( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	
	if ( pOwner == NULL )
		return false;

	SendWeaponAnim( ACT_VM_RELOAD2 );

	pOwner->m_flNextAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLSniperRifle::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast safely.
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
		return;

	// If my clip is empty (and I use clips) start reload
	if ( !m_iClip1 ) 
	{
		Reload();
		return;
	}

	// MUST call sound before removing a round from the clip of a CMachineGun dvs: does this apply to the sniper rifle? I don't know.
	WeaponSound(SINGLE);

	// Register a muzzleflash for the AI.
	pPlayer->DoMuzzleFlash();

	// player "shoot" animation
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
	pPlayer->SetAimTime( 2.0f );
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 600, 0.2 );

	// Fire the bullet
	Vector vecSrc	 = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );	
	if ( m_bUseProjectile )
	{
		//TODO; We need to do a trace here to grab the actual target for the bullet (vecaiming is temp!!)
		CSniperBullet *pBullet = (CSniperBullet *)Create( "sniperbullet", vecSrc, pPlayer->GetLocalAngles(), NULL );
		pBullet->SetOwnerEntity( pPlayer );
		pBullet->Start( vecSrc, vecAiming/*temp!!*/, pPlayer, true );
	}
	else
	{
		pPlayer->FireBullets( SNIPER_BULLET_COUNT_PLAYER, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, SNIPER_TRACER_FREQUENCY_PLAYER );
	}
#endif

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_iClip1 = m_iClip1 - 1;

	QAngle vecPunch(-SNIPER_KICKBACK, random->RandomFloat( -SNIPER_KICKBACK/2, SNIPER_KICKBACK/2 ), 0);
	if ( m_nZoomLevel != 0 )
	{
		QAngle vecPunch((-SNIPER_KICKBACK * 0.25), random->RandomFloat( (-SNIPER_KICKBACK * 0.25), (SNIPER_KICKBACK * 0.25) ), 0);
	}
	pPlayer->ViewPunch(vecPunch);

	// Indicate out of ammo condition if we run out of ammo.
	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}
}


//-----------------------------------------------------------------------------
// Purpose: Zooms in using the sniper rifle scope.
//-----------------------------------------------------------------------------
void CHLSniperRifle::Zoom( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
	{
		return;
	}

	if (m_nZoomLevel >= sizeof(g_nZoomFOV) / sizeof(g_nZoomFOV[0]))
	{
		if ( pPlayer->SetFOV( this, 0, SNIPER_ZOOM_RATE ) )
		{
#ifndef CLIENT_DLL
			pPlayer->ShowViewModel(true);
#endif
			// Zoom out to the default zoom level
			WeaponSound(SPECIAL2);	
			m_nZoomLevel = 0;
		}
	}
	else
	{
		if ( pPlayer->SetFOV( this, g_nZoomFOV[m_nZoomLevel], SNIPER_ZOOM_RATE ) )
		{
#ifndef CLIENT_DLL
			if (m_nZoomLevel == 0)
				pPlayer->ShowViewModel(false);
#endif
			WeaponSound(SPECIAL1);
			m_nZoomLevel++;
		}
	}

	m_fNextZoom = gpGlobals->curtime + SNIPER_ZOOM_RATE;
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CHLSniperRifle::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch ( pEvent->event )
	{
		case EVENT_WEAPON_SNIPER_RIFLE_FIRE:
		{
			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition();

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			Vector vecSpread;
			if (npc)
			{
				vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );
				vecSpread = VECTOR_CONE_PRECALCULATED;
			}
			else
			{
				AngleVectors( pOperator->GetLocalAngles(), &vecShootDir );
				vecSpread = GetBulletSpread();
			}

			WeaponSound( SINGLE_NPC );

			// Fire the bullet
			if ( m_bUseProjectile )
			{
				CSniperBullet *pBullet = (CSniperBullet *)Create( "sniperbullet", vecShootOrigin, pOperator->GetLocalAngles(), NULL );
				pBullet->SetOwnerEntity( pOperator );
				pBullet->Start( vecShootOrigin, GetEnemy()->WorldSpaceCenter(), pOperator, true );
			}
			else
			{
				pOperator->FireBullets( SNIPER_BULLET_COUNT_NPC, vecShootOrigin, vecShootDir, vecSpread, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, SNIPER_TRACER_FREQUENCY_NPC );
			}

			pOperator->DoMuzzleFlash();
			break;
		}

		case EVENT_WEAPON_RELOAD:
		{
			CEffectData data;

			// Emit a shell
			for ( int i = 0; i < GetDefaultClip1(); i++ )
			{
				data.m_vOrigin = pOperator->WorldSpaceCenter() + RandomVector( -4, 4 );
				data.m_vAngles = QAngle( 90, random->RandomInt( 0, 360 ), 0 );
				data.m_nEntIndex = entindex();

				DispatchEffect( "RifleShellEject", data );
			}
			break;
		}

		default:
		{
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// SniperRifle
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
#define CWeaponSniperRifle C_WeaponSniperRifle
#endif
class CWeaponSniperRifle : public CHLSniperRifle
{
public:
	DECLARE_CLASS( CWeaponSniperRifle, CHLSniperRifle );

	CWeaponSniperRifle();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	void	Precache( void );

	virtual float	GetMinRestTime() { return 0.8; }
	virtual float	GetMaxRestTime() { return 1.0; }
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSniperRifle, DT_WeaponSniperRifle )

BEGIN_NETWORK_TABLE( CWeaponSniperRifle, DT_WeaponSniperRifle )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponSniperRifle )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_sniperrifle, CWeaponSniperRifle );
PRECACHE_WEAPON_REGISTER(weapon_sniperrifle);


//=========================================================
CWeaponSniperRifle::CWeaponSniperRifle( )
{
	m_bReloadsSingly = true;
	m_bBoltAction = true;
	m_bUseProjectile = false;

	m_fMinRange1		= 64;
	m_fMaxRange1		= 2048;

	m_fMinRange2		= 224;
	m_fMaxRange2		= 1280;
}

void CWeaponSniperRifle::Precache( void )
{
	BaseClass::Precache();
}
