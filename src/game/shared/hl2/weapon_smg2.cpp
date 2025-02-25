//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Ol' Reliable - SMG
//
//	Primary attack: Shooty thingy.
//	Secondary attack: Grenade launcher.
// TODO's; SMG1 Fires normal ar2 grenade, SMG2 fires airburst
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
	#include "grenade_ar2.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	COMBINE_MIN_GRENADE_CLEAR_DIST 250
#define	SMG2_EASY_DAMPEN 0.5f

#ifndef CLIENT_DLL
//ConVar sk_weapon_smg1_lerp( "sk_weapon_smg1_lerp", "6.0" );
//ConVar sk_weapon_smg2_lerp( "sk_weapon_smg2_lerp", "4.0" );
ConVar sk_weapon_smg2_grenade_launchspeed( "sk_weapon_smg2_grenade_launchspeed", "1000" );

extern ConVar sk_plr_dmg_smg_grenade;
extern ConVar sk_npc_dmg_smg_grenade;
#endif
extern ConVar sv_funmode;

//=========================================================
//	>> CHLGrenadeLauncher
//=========================================================
#ifdef CLIENT_DLL
#define CHLGrenadeLauncher C_HLGrenadeLauncher
#endif
class CHLGrenadeLauncher : public CHLMachineGun
{
	DECLARE_CLASS( CHLGrenadeLauncher, CHLMachineGun );
public:
	CHLGrenadeLauncher();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	void	Precache( void );
	void	ItemPostFrame( void );
	void	SecondaryAttack( void );

	bool	Reload( void );

	virtual float GetFireRate( void ) 
	{
#if 0
		if ( GetOwner() && GetOwner()->IsNPC() )
		{
			// NPC value
			return BaseClass::GetCycleTime() + 0.025f;	//assuming default, addon 0.025 time (0.1f)
		}
#endif
		// Player(s) value
		return BaseClass::GetCycleTime();	//0.075f - 13.3hz
	}
	
	const WeaponProficiencyInfo_t *GetProficiencyValues();

#ifndef CLIENT_DLL
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }	//bits_CAP_WEAPON_RANGE_ATTACK2
	int		WeaponRangeAttack2Condition( float flDot, float flDist );

	void FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir );
	void Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

	DECLARE_ACTTABLE();

protected:
	void	FireGrenade( void );
	bool	m_bUseAirburstGrenade;
	CNetworkVar( Vector, m_vecTossVelocity );
	CNetworkVar( float, m_flNextGrenadeCheck );
	int		m_lastGrenadeCondition;

};

IMPLEMENT_NETWORKCLASS_ALIASED( HLGrenadeLauncher, DT_HLGrenadeLauncher )

BEGIN_NETWORK_TABLE( CHLGrenadeLauncher, DT_HLGrenadeLauncher )
#ifdef CLIENT_DLL
	RecvPropVector( RECVINFO( m_vecTossVelocity ) ),
	RecvPropTime( RECVINFO( m_flNextGrenadeCheck ) ),
#else
	SendPropVector( SENDINFO( m_vecTossVelocity ) ),
	SendPropTime( SENDINFO( m_flNextGrenadeCheck ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CHLGrenadeLauncher )
	DEFINE_PRED_FIELD( m_vecTossVelocity, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flNextGrenadeCheck, FIELD_TIME, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

acttable_t	CHLGrenadeLauncher::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SMG1,			true },
	{ ACT_RANGE_ATTACK2, 			ACT_RANGE_ATTACK_AR2_GRENADE, 	true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_SPRINT,					ACT_RUN_RIFLE_STIMULATED,		true },
	
// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SMG1,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },
//	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_RIFLE_LOW,		false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
};

IMPLEMENT_ACTTABLE(CHLGrenadeLauncher);

//=========================================================
CHLGrenadeLauncher::CHLGrenadeLauncher( )
{
	m_fMinRange1		= 32;
	m_fMaxRange1		= 1536;

	m_fMinRange2		= 224;
	m_fMaxRange2		= 1280;

	m_bAltFiresUnderwater = false;
	m_bUseAirburstGrenade = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLGrenadeLauncher::Precache( void )
{
#ifndef CLIENT_DLL
	UTIL_PrecacheOther("grenade_ar2");
#endif

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Handle grenade detonate in-air (even when no ammo is left)
//-----------------------------------------------------------------------------
void CHLGrenadeLauncher::ItemPostFrame( void )
{
#ifndef CLIENT_DLL
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	// If its an airburst grenade, detonate it
	if ( (pOwner->m_afButtonPressed & IN_ATTACK2) && m_bUseAirburstGrenade )
	{
		CBaseEntity *pEntity = NULL;
		while ((pEntity = gEntList.FindEntityByClassname( pEntity, "grenade_ar2_airburst" )) != NULL)
		{
			CGrenadeAR2Airburst *pGrenade = dynamic_cast<CGrenadeAR2Airburst *>(pEntity);
			if (pGrenade->GetThrower() && GetOwner() && pGrenade->GetThrower() == GetOwner())
			{
				pGrenade->ManualDetonate();	// Go boom as soon as you can
			}
		}
	}
#endif

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHLGrenadeLauncher::Reload( void )
{
	//Seperate reload anim for secondary
#if 0	//NO ANIMS YET
	bool Ammo2;
	Ammo2 = DefaultReload( 0, GetMaxClip2(), ACT_VM_RELOAD2 );

	if ( UsesClipsForAmmo2() && m_iClip1 != 0 && m_iClip2 == 0 ) 
	{
		WeaponSound( SPECIAL1 );
		return Ammo2;
	}
#endif

	bool Ammo1;
	Ammo1 = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	float fCacheTime = m_flNextSecondaryAttack;

	if ( Ammo1 )
	{
		// Undo whatever the reload process has done to our secondary
		// attack timer. We allow you to interrupt reloading to fire
		// a grenade.
		m_flNextSecondaryAttack = GetOwner()->m_flNextAttack = fCacheTime;
		WeaponSound( RELOAD );
	}

	return Ammo1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLGrenadeLauncher::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	//Must have ammo
	if ( UsesClipsForAmmo2() && !m_iClip2 ) 
	{
		Reload();
		return;
	}
	else if ( ( pPlayer->GetAmmoCount( m_iSecondaryAmmoType ) <= 0 ) || ( pPlayer->GetWaterLevel() == 3 ) )
	{
		DryFire();
		return;
	}

	if( m_bInReload )
		m_bInReload = false;

	//Shoot the grenade out
	FireGrenade();

	//Decrease ammo
	if ( UsesClipsForAmmo2() )
	{
		m_iClip2--;
	}
	else
	{
		pPlayer->RemoveAmmo( 1, m_iSecondaryAmmoType );
	}
	m_iSecondaryAttacks++;
}

//NOTENOTE; If its actually a proper grenade-launcher, then route primary attack to this
void CHLGrenadeLauncher::FireGrenade( void )
{
	CBaseCombatCharacter *pOwner = ToBaseCombatCharacter( GetOwner() );

	//Create the grenade
#ifndef CLIENT_DLL
	Vector vecSrc = pOwner->Weapon_ShootPosition();
	Vector vecThrow;
	// Don't autoaim on grenade tosses
	AngleVectors( pOwner->EyeAngles(), &vecThrow );
	VectorScale( vecThrow, sk_weapon_smg2_grenade_launchspeed.GetFloat(), vecThrow );	//Quake is around 800

	QAngle angles;
	VectorAngles( vecThrow, angles );

	if ( m_bUseAirburstGrenade )
	{
		CGrenadeAR2 *aGrenade;
		aGrenade = (CGrenadeAR2Airburst*)Create( "grenade_ar2_airburst", vecSrc, angles, pOwner );
		aGrenade->SetLocalAngularVelocity( RandomAngle( -1, 1 ) );	//Dont tumble, stay straight
		aGrenade->SetAbsVelocity( vecThrow );
		aGrenade->SetThrower( GetOwner() );
	}
	else
	{
		CGrenadeAR2 *sGrenade;
		sGrenade = (CGrenadeAR2*)Create( "grenade_ar2", vecSrc, angles, pOwner );
		sGrenade->SetLocalAngularVelocity( RandomAngle( -400, 400 ) );
		sGrenade->SetAbsVelocity( vecThrow );
		sGrenade->SetThrower( GetOwner() );
	}

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 1000, 0.2, GetOwner(), SOUNDENT_CHANNEL_WEAPON );
#endif

	// MUST call sound before removing a round from the clip of a CMachineGun
	BaseClass::WeaponSound( WPN_DOUBLE );

	//Factor in the view kick
	m_fFireDuration = m_fFireDuration + 1.0f;	// Increase the view kick for the primary fire aswell
	AddViewKick();

	if (pOwner->IsPlayer())
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );
		//player "shoot" animation
		pPlayer->SetAnimation( PLAYER_ATTACK1 );
		//Disorient the player
		pPlayer->ViewPunch( QAngle( -3, random->RandomFloat( -1, 1 ), 0 ) );
#ifdef GAME_DLL
		pPlayer->RumbleEffect( RUMBLE_357, 0, RUMBLE_FLAGS_NONE );
		//Register a muzzleflash for the AI.
		pPlayer->SetAimTime( 2.0f );
		pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );	
		gamestats->Event_WeaponFired( pPlayer, false, GetClassname() );
#endif
	}

	//Can blow up after a short delay (so have time to release mouse button)
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLGrenadeLauncher::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir )
{
	// FIXME: use the returned number of bullets to account for >10hz firerate
//	WeaponSoundRealtime( SINGLE_NPC );
	WeaponSound( SINGLE_NPC );

	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;

	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );
	pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, pOperator->GetAttackSpread(this),
		MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2, entindex(), 0 );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLGrenadeLauncher::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	Vector vecShootOrigin, vecShootDir;
	QAngle	angShootDir;
	GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
	AngleVectors( angShootDir, &vecShootDir );
	FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHLGrenadeLauncher::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
	case EVENT_WEAPON_SMG1:
	case EVENT_WEAPON_SMG2:
		{
			Vector vecShootOrigin, vecShootDir;
			QAngle angDiscard;

			// Support old style attachment point firing
			if ((pEvent->options == NULL) || (pEvent->options[0] == '\0') || (!pOperator->GetAttachment(pEvent->options, vecShootOrigin, angDiscard)))
			{
				vecShootOrigin = pOperator->Weapon_ShootPosition();
			}

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );

			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
		}
		break;

	//FIXME:
	case EVENT_WEAPON_AR2_GRENADE:
	case EVENT_WEAPON_AR2_ALTFIRE:
		{
			CAI_BaseNPC *npc = pOperator->MyNPCPointer();

			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition();
			vecShootDir = npc->GetShootEnemyDir( vecShootOrigin );

			Vector vecThrow = m_vecTossVelocity;

			WeaponSound( DOUBLE_NPC );

			CGrenadeAR2 *pGrenade = (CGrenadeAR2*)Create( "grenade_ar2", vecShootOrigin, vec3_angle, npc );
			pGrenade->SetAbsVelocity( vecThrow );
//			pGrenade->SetLocalAngularVelocity( QAngle( 0, 400, 0 ) );
			pGrenade->SetLocalAngularVelocity( RandomAngle(-400, 400) );
			pGrenade->SetThrower( GetOwner() );
			pGrenade->SetDamage(sk_npc_dmg_smg_grenade.GetFloat());

			m_lastGrenadeCondition = COND_NONE;
			// FIXME: Shouldnt this just take the variables from the user ai???
			if ( g_pGameRules->IsSkillLevel(SKILL_HARD) )
				m_flNextGrenadeCheck = gpGlobals->curtime + random->RandomFloat( 5, 8 );// wait a random amount of time before shooting again
			else
				m_flNextGrenadeCheck = gpGlobals->curtime + 8;// wait eight seconds before even looking again to see if a grenade can be launched

			m_iClip2--;
		}
		break;


	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flDot - 
//			flDist - 
// Output : int
//-----------------------------------------------------------------------------
int CHLGrenadeLauncher::WeaponRangeAttack2Condition( float flDot, float flDist )
{
	CAI_BaseNPC *npcOwner = GetOwner()->MyNPCPointer();

	if( flDist > m_fMaxRange2 || flDist < m_fMinRange2 )
	{
		// Too close or too far!
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return COND_NONE;	//COND_TOO_FAR_TO_ATTACK
	}

#if 0
	// --------------------------------------------------------
	// Assume things haven't changed too much since last time
	// --------------------------------------------------------
	if (gpGlobals->curtime < m_flNextGrenadeCheck )
		return m_lastGrenadeCondition;
#endif

	// -----------------------
	// If moving, don't check.
	// -----------------------
	if ( m_flGroundSpeed != 0 )
		return COND_NONE;

	CBaseEntity *pEnemy = npcOwner->GetEnemy();
	if (!pEnemy)
		return COND_NONE;


	Vector vecEnemyLKP = npcOwner->GetEnemyLKP();
#if 0
	if ( !( pEnemy->GetFlags() & FL_ONGROUND ) && pEnemy->GetWaterLevel() == 0 && vecEnemyLKP.z > (GetAbsOrigin().z + WorldAlignMaxs().z) )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		return COND_NONE;
	}
#endif

	// --------------------------------------
	//  Get target vector
	// --------------------------------------
	Vector vecTarget = vecEnemyLKP;	// Launch it to where you last saw them
	if ( random->RandomInt( 0, 1 ) )
	{
		// Guess where they are
		vecTarget = vecEnemyLKP + pEnemy->GetAbsVelocity();
	}

	if ( ( vecTarget - npcOwner->GetLocalOrigin() ).Length2D() <= COMBINE_MIN_GRENADE_CLEAR_DIST )
	{
		// crap, I don't want to blow myself up
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return COND_NONE;
	}

	// ---------------------------------------------------------------------
	// Are any friendlies near the intended grenade impact area?
	// ---------------------------------------------------------------------
	CBaseEntity *pTarget = NULL;

	while ( ( pTarget = gEntList.FindEntityInSphere( pTarget, vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST ) ) != NULL )
	{
		//Check to see if the default relationship is hatred, and if so intensify that
		if ( npcOwner->IRelationType( pTarget ) == D_LI )
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
			// Tell my squad members to clear out so I can get a grenade in
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY | SOUND_CONTEXT_COMBINE_ONLY, vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST, 0.1 );
			return COND_WEAPON_BLOCKED_BY_FRIEND;
		}
	}

	// ---------------------------------------------------------------------
	// Check that throw is legal and clear
	// ---------------------------------------------------------------------
	// FIXME: speed is based on difficulty...

	Vector vecToss = VecCheckThrow( this, npcOwner->GetLocalOrigin() + Vector(0,0,60), vecTarget, 650.0, 0.5 );
	if ( vecToss != vec3_origin )
	{
		m_vecTossVelocity = vecToss;

		// don't check again for a while.
		// JAY: HL1 keeps checking - test?
		//m_flNextGrenadeCheck = gpGlobals->curtime;
		m_flNextGrenadeCheck = gpGlobals->curtime + 0.3; // 1/3 second.
		return COND_CAN_RANGE_ATTACK2;
	}
	else
	{
		// don't check again for a while.
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return COND_WEAPON_SIGHT_OCCLUDED;
	}
}
#endif

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CHLGrenadeLauncher::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 7.0,		0.75	},
		{ 7.0,		0.75	},
		{ 5.00,		0.75	},
		{ 10.0/3.0, 0.75	},
		{ 5.0/3.0,	0.75	},
		{ 1.00,		1.0		},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return proficiencyTable;
}

//-----------------------------------------------------------------------------
// SMG2
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
#define CWeaponSMG2 C_WeaponSMG2
#endif
class CWeaponSMG2 : public CHLGrenadeLauncher
{
public:
	DECLARE_CLASS( CWeaponSMG2, CHLGrenadeLauncher );

	CWeaponSMG2();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	void	Precache( void );
	void	AddViewKick( void );

	// AI fires in burst or auto - dont choose random burst sizes
	int		GetMinBurst() { return 4; }
	int		GetMaxBurst() { return 4; }
	virtual int		GetBurstSize( void ) { return 4; }
#ifndef CLIENT_DLL
	virtual float	GetMinRestTime();
	virtual float	GetMaxRestTime();
#endif
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSMG2, DT_WeaponSMG2 )

BEGIN_NETWORK_TABLE( CWeaponSMG2, DT_WeaponSMG2 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponSMG2 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_smg2, CWeaponSMG2 );
PRECACHE_WEAPON_REGISTER(weapon_smg2);


//=========================================================
CWeaponSMG2::CWeaponSMG2( )
{
	m_fMinRange1		= 32;
	m_fMaxRange1		= 1792;

	m_fMinRange2		= 224;
	m_fMaxRange2		= 1280;

	if ( sv_funmode.GetBool() )
	{
		m_bReloadsFullClip	= true;
	}

	m_bAltFiresUnderwater = false;
	m_bUseAirburstGrenade = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSMG2::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSMG2::AddViewKick( void )
{
	#define	SMG2_MAX_VERTICAL_KICK		2.0f	//Degrees TODO; Convar
	#define	SMG2_SLIDE_LIMIT			1.0f	//Seconds TODO; Convar

	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
		return;

	DoMachineGunKick( pPlayer, SMG2_EASY_DAMPEN, SMG2_MAX_VERTICAL_KICK, m_fFireDuration, SMG2_SLIDE_LIMIT );
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Adjust time between bursts for different npc types.
//-----------------------------------------------------------------------------
float CWeaponSMG2::GetMinRestTime()
{
	//Default is 0.3
	Class_T OwnerClass = GetOwner()->Classify();

	if ( OwnerClass == CLASS_CITIZEN_PASSIVE	||
		 OwnerClass == CLASS_CITIZEN_REBEL	||
		 OwnerClass == CLASS_PLAYER_ALLY	||
		 OwnerClass == CLASS_METROPOLICE	)
		 return 0.4f;

	return BaseClass::GetMinRestTime();
}

//-----------------------------------------------------------------------------
float CWeaponSMG2::GetMaxRestTime()
{
	// Default is 0.6
	Class_T OwnerClass = GetOwner()->Classify();

	if ( OwnerClass == CLASS_PLAYER_ALLY	||
		 OwnerClass == CLASS_METROPOLICE	)
		 return 0.7f;

	if ( OwnerClass == CLASS_CITIZEN_PASSIVE	||
		 OwnerClass == CLASS_CITIZEN_REBEL	)
		 return 0.8f;

	return BaseClass::GetMaxRestTime();
}
#endif
