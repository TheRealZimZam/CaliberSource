//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "AI_BaseNPC.h"
#include "trains.h"
#include "ndebugoverlay.h"
#include "EntityList.h"
#include "engine/IEngineSound.h"
#include "doors.h"
#include "soundent.h"
#include "hl1_basegrenade.h"
#include "shake.h"
#include "globalstate.h"
#include "soundscape.h"
#include "buttons.h"
#include "sprite.h"
#include "actanimating.h"
#include "npcevent.h"
#include "func_break.h"

//=========================================================
// Furniture - this is the cool comment I cut-and-pasted
//=========================================================
class CNPC_Furniture : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Furniture, CAI_BaseNPC );
public:
	void Spawn ( void );
	void Die( void );
	Class_T	 Classify ( void );
	float MaxYawSpeed( void ){ return 0; }
	virtual int	ObjectCaps( void ) { return (CAI_BaseNPC::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
};


LINK_ENTITY_TO_CLASS( monster_furniture, CNPC_Furniture );


//=========================================================
// Furniture is killed
//=========================================================
void CNPC_Furniture :: Die ( void )
{
	SetThink ( &CNPC_Furniture::SUB_Remove );
	SetNextThink( gpGlobals->curtime );
}

//=========================================================
// This used to have something to do with bees flying, but 
// now it only initializes moving furniture in scripted sequences
//=========================================================
void CNPC_Furniture::Spawn( )
{
	engine->PrecacheModel((char *)STRING(GetModelName()));
	SetModel( STRING(GetModelName()) );

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	m_iHealth		= 80000; //wow
	m_takedamage = DAMAGE_AIM;
	m_fEffects		= 0;
	SetSequence( 0 );
	m_flCycle		= 0;

//	pev->nextthink += 1.0;
//	SetThink (WalkMonsterDelay);

	ResetSequenceInfo( );
	m_flCycle = 0;
	NPCInit();
}

//=========================================================
// ID's Furniture as neutral (noone will attack it)
//=========================================================
Class_T CNPC_Furniture::Classify ( void )
{
	return	CLASS_NONE;
}


//=========================================================
// Dead HEV suit prop
//=========================================================
class CNPC_DeadHEV : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_DeadHEV, CAI_BaseNPC );
public:
	void Spawn( void );
	Class_T	Classify ( void ) { return	CLASS_HUMAN_MILITARY; }
	float MaxYawSpeed( void ) { return 8.0f; }

	bool KeyValue( const char *szKeyName, const char *szValue );

	int	m_iPose;// which sequence to display	-- temporary, don't need to save
	static char *m_szPoses[4];
};

char *CNPC_DeadHEV::m_szPoses[] = { "deadback", "deadsitting", "deadstomach", "deadtable" };

bool CNPC_DeadHEV::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "pose" ) )
		m_iPose = atoi( szValue );
	else 
		CAI_BaseNPC::KeyValue( szKeyName, szValue );

	return true;
}

LINK_ENTITY_TO_CLASS( monster_hevsuit_dead, CNPC_DeadHEV );

//=========================================================
// ********** DeadHEV SPAWN **********
//=========================================================
void CNPC_DeadHEV::Spawn( void )
{
	engine->PrecacheModel("models/player.mdl");
	SetModel( "models/player.mdl" );

	m_fEffects			= 0;
	SetSequence( 0 );
	m_nBody				= 1;
	m_bloodColor		= BLOOD_COLOR_RED;

	SetSequence( LookupSequence( m_szPoses[m_iPose] ) );

	if ( GetSequence() == -1 )
	{
		Msg ( "Dead hevsuit with bad pose\n" );
		SetSequence( 0 );
		m_fEffects = EF_BRIGHTLIGHT;
	}

	// Corpses have less health
	m_iHealth			= 8;

	NPCInitDead();
}



///////////////////////
//XEN!
//////////////////////

#define XEN_PLANT_GLOW_SPRITE		"sprites/flare3.spr"
#define XEN_PLANT_HIDE_TIME			5

class CXenPLight : public CActAnimating
{
	DECLARE_CLASS( CXenPLight, CActAnimating );

public:

	DECLARE_DATADESC();

	void		Spawn( void );
	void		Precache( void );
	void		Touch( CBaseEntity *pOther );
	void		Think( void );

	void		LightOn( void );
	void		LightOff( void );

	float		m_flDmgTime;
	

private:
	CSprite		*m_pGlow;
};

LINK_ENTITY_TO_CLASS( xen_plantlight, CXenPLight );

BEGIN_DATADESC( CXenPLight )
	DEFINE_FIELD( CXenPLight, m_pGlow, FIELD_CLASSPTR ),
	DEFINE_FIELD( CXenPLight, m_flDmgTime, FIELD_FLOAT ),
END_DATADESC()

void CXenPLight::Spawn( void )
{
	Precache();

	SetModel( "models/light.mdl" );

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID );

	UTIL_SetSize( this, Vector(-80,-80,0), Vector(80,80,32));
	SetActivity( ACT_IDLE );
	SetNextThink( gpGlobals->curtime + 0.1 );
	m_flCycle = random->RandomFloat(0,1);

	m_pGlow = CSprite::SpriteCreate( XEN_PLANT_GLOW_SPRITE, GetLocalOrigin() + Vector(0,0,(WorldAlignMins().z+WorldAlignMaxs().z)*0.5), FALSE );
	m_pGlow->SetTransparency( kRenderGlow, GetRenderColor().r, GetRenderColor().g, GetRenderColor().b, GetRenderColor().a, m_nRenderFX );
	m_pGlow->SetAttachment( this, 1 );
}


void CXenPLight::Precache( void )
{
	engine->PrecacheModel( "models/light.mdl" );
	engine->PrecacheModel( XEN_PLANT_GLOW_SPRITE );
}


void CXenPLight::Think( void )
{
	StudioFrameAdvance();
	SetNextThink( gpGlobals->curtime + 0.1 );

	switch( GetActivity() )
	{
	case ACT_CROUCH:
		if ( IsSequenceFinished() )
		{
			SetActivity( ACT_CROUCHIDLE );
			LightOff();
		}
		break;

	case ACT_CROUCHIDLE:
		if ( gpGlobals->curtime > m_flDmgTime )
		{
			SetActivity( ACT_STAND );
			LightOn();
		}
		break;

	case ACT_STAND:
		if ( IsSequenceFinished() )
			SetActivity( ACT_IDLE );
		break;

	case ACT_IDLE:
	default:
		break;
	}
}


void CXenPLight :: Touch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() )
	{
		m_flDmgTime = gpGlobals->curtime + XEN_PLANT_HIDE_TIME;
		if ( GetActivity() == ACT_IDLE || GetActivity() == ACT_STAND )
		{
			SetActivity( ACT_CROUCH );
		}
	}
}


void CXenPLight :: LightOn( void )
{
	FireTargets( STRING(m_target), this, this, USE_ON, 0 );

	if ( m_pGlow )
	     m_pGlow->m_fEffects &= ~EF_NODRAW;
}


void CXenPLight :: LightOff( void )
{
	FireTargets( STRING(m_target), this, this, USE_OFF, 0 );


	if ( m_pGlow )
		 m_pGlow->m_fEffects |= EF_NODRAW;
}


class CXenHair : public CActAnimating
{
	DECLARE_CLASS( CXenHair, CActAnimating );
public:
	void		Spawn( void );
	void		Precache( void );
	void		Think( void );
};

LINK_ENTITY_TO_CLASS( xen_hair, CXenHair );

#define SF_HAIR_SYNC		0x0001

void CXenHair::Spawn( void )
{
	Precache();
	SetModel( "models/hair.mdl" );
	UTIL_SetSize( this, Vector(-4,-4,0), Vector(4,4,32));
	SetSequence( 0 );
	
	if ( !HasSpawnFlags( SF_HAIR_SYNC ) )
	{
		m_flCycle = random->RandomFloat( 0,1);
		m_flPlaybackRate = random->RandomFloat( 0.7, 1.4 );
	}
	ResetSequenceInfo( );

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.1, 0.4 ) );	// Load balance these a bit
}


void CXenHair::Think( void )
{
	StudioFrameAdvance();
	SetNextThink( gpGlobals->curtime + 0.5 );
}


void CXenHair::Precache( void )
{
	engine->PrecacheModel( "models/hair.mdl" );
}

class CXenTreeTrigger : public CBaseEntity
{
	DECLARE_CLASS( CXenTreeTrigger, CBaseEntity );
public:
	void		Touch( CBaseEntity *pOther );
	static CXenTreeTrigger *TriggerCreate( CBaseEntity *pOwner, const Vector &position );
};
LINK_ENTITY_TO_CLASS( xen_ttrigger, CXenTreeTrigger );

CXenTreeTrigger *CXenTreeTrigger :: TriggerCreate( CBaseEntity *pOwner, const Vector &position )
{
	CXenTreeTrigger *pTrigger = CREATE_ENTITY( CXenTreeTrigger, "xen_ttrigger" ); 
	pTrigger->SetAbsOrigin( position );

	pTrigger->SetSolid( SOLID_BBOX );
	pTrigger->AddSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID );
	pTrigger->SetMoveType( MOVETYPE_NONE );
	pTrigger->SetOwnerEntity( pOwner );

	return pTrigger;
}


void CXenTreeTrigger::Touch( CBaseEntity *pOther )
{
	if ( GetOwnerEntity() )
	{
		GetOwnerEntity()->Touch( pOther );
	}
}

#define TREE_AE_ATTACK		1

class CXenTree : public CActAnimating
{
	DECLARE_CLASS( CXenTree, CActAnimating );
public:
	void		Spawn( void );
	void		Precache( void );
	void		Touch( CBaseEntity *pOther );
	void		Think( void );
	int			OnTakeDamage( const CTakeDamageInfo &info ) { Attack(); return 0; }
	void		HandleAnimEvent( animevent_t *pEvent );
	void		Attack( void );	
	Class_T			Classify( void ) { return CLASS_BARNACLE; }

	DECLARE_DATADESC();

	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];

private:
	CXenTreeTrigger	*m_pTrigger;
};

LINK_ENTITY_TO_CLASS( xen_tree, CXenTree );

BEGIN_DATADESC( CXenTree )
	DEFINE_FIELD( CXenTree, m_pTrigger, FIELD_CLASSPTR ),
END_DATADESC()

void CXenTree::Spawn( void )
{
	Precache();

	SetModel( "models/tree.mdl" );
	SetMoveType( MOVETYPE_NONE );
	SetSolid ( SOLID_BBOX );

	m_takedamage = DAMAGE_YES;

	UTIL_SetSize( this, Vector(-30,-30,0), Vector(30,30,188));
	SetActivity( ACT_IDLE );
	SetNextThink( gpGlobals->curtime + 0.1 );
	m_flCycle = random->RandomFloat( 0,1 );
	m_flPlaybackRate = random->RandomFloat( 0.7, 1.4 );

	Vector triggerPosition, vForward;

	AngleVectors( GetAbsAngles(), &vForward );
	triggerPosition = GetAbsOrigin() + (vForward * 64);
	
	// Create the trigger
	m_pTrigger = CXenTreeTrigger::TriggerCreate( this, triggerPosition );
	UTIL_SetSize( m_pTrigger, Vector( -24, -24, 0 ), Vector( 24, 24, 128 ) );
}

const char *CXenTree::pAttackHitSounds[] = 
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CXenTree::pAttackMissSounds[] = 
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

void CXenTree::Precache( void )
{
	engine->PrecacheModel( "models/tree.mdl" );
	engine->PrecacheModel( XEN_PLANT_GLOW_SPRITE );
	PRECACHE_SOUND_ARRAY( pAttackHitSounds );
	PRECACHE_SOUND_ARRAY( pAttackMissSounds );
}


void CXenTree::Touch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() && FClassnameIs( pOther, "monster_bigmomma" ) )
		return;

	Attack();
}


void CXenTree::Attack( void )
{
	if ( GetActivity() == ACT_IDLE )
	{
		SetActivity( ACT_MELEE_ATTACK1 );
		m_flPlaybackRate = random->RandomFloat( 1.0, 1.4 );

		CPASAttenuationFilter filter( this );
		enginesound->EmitSound( filter, entindex(), CHAN_WEAPON, pAttackMissSounds[ random->RandomInt( 0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, PITCH_NORM );
	}
}


void CXenTree::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
		case TREE_AE_ATTACK:
		{
			CBaseEntity *pList[8];
			BOOL sound = FALSE;
			int count = UTIL_EntitiesInBox( pList, 8, m_pTrigger->GetAbsOrigin() + m_pTrigger->WorldAlignMins(), m_pTrigger->GetAbsOrigin() +  m_pTrigger->WorldAlignMaxs(), FL_NPC|FL_CLIENT );

			Vector forward;
			AngleVectors( GetAbsAngles(), &forward );

			for ( int i = 0; i < count; i++ )
			{
				if ( pList[i] != this )
				{
					if ( pList[i]->GetOwnerEntity() != this )
					{
						sound = true;
						pList[i]->TakeDamage( CTakeDamageInfo(this, this, 25, DMG_CRUSH | DMG_SLASH ) );
						pList[i]->ViewPunch( QAngle( 15, 0, 18 ) );

						pList[i]->SetAbsVelocity( pList[i]->GetAbsVelocity() + forward * 100 );
					}
				}
			}
					
			if ( sound )
			{
				CPASAttenuationFilter filter( this );
				enginesound->EmitSound( filter, entindex(), CHAN_WEAPON, pAttackHitSounds[ random->RandomInt( 0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, PITCH_NORM );
			}
		}
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

void CXenTree :: Think( void )
{
	StudioFrameAdvance();
	SetNextThink( gpGlobals->curtime + 0.1 );
	DispatchAnimEvents( this );

	switch( GetActivity() )
	{
	case ACT_MELEE_ATTACK1:
		if ( IsSequenceFinished() )
		{
			SetActivity( ACT_IDLE );
			m_flPlaybackRate = random->RandomFloat( 0.6f, 1.4f );
		}
		break;

	default:
	case ACT_IDLE:
		break;

	}
}

class CXenSpore : public CActAnimating
{
	DECLARE_CLASS( CXenSpore, CActAnimating );
public:
	void		Spawn( void );
	void		Precache( void );
	void		Touch( CBaseEntity *pOther );
//	void		HandleAnimEvent( MonsterEvent_t *pEvent );
	void		Attack( void ) {}

	static const char *pModelNames[];
};

class CXenSporeSmall : public CXenSpore
{
	DECLARE_CLASS( CXenSporeSmall, CXenSpore );
	void		Spawn( void );
};

class CXenSporeMed : public CXenSpore
{
	DECLARE_CLASS( CXenSporeMed, CXenSpore );
	void		Spawn( void );
};

class CXenSporeLarge : public CXenSpore
{
	DECLARE_CLASS( CXenSporeLarge, CXenSpore );
	void		Spawn( void );

	static const Vector m_hullSizes[];
};

// Fake collision box for big spores
class CXenHull : public CPointEntity
{
	DECLARE_CLASS( CXenHull, CPointEntity );
public:
	static CXenHull	*CreateHull( CBaseEntity *source, const Vector &mins, const Vector &maxs, const Vector &offset );
	Class_T			Classify( void ) { return CLASS_BARNACLE; }
};

CXenHull *CXenHull::CreateHull( CBaseEntity *source, const Vector &mins, const Vector &maxs, const Vector &offset )
{
	CXenHull *pHull = CREATE_ENTITY( CXenHull, "xen_hull" ); 

	UTIL_SetOrigin( pHull, source->GetAbsOrigin() + offset );
	pHull->SetSolid( SOLID_BBOX );
	pHull->SetMoveType( MOVETYPE_NONE );
	pHull->SetOwnerEntity( source );
	UTIL_SetSize( pHull, mins, maxs );
	pHull->SetRenderColorA( 0 );
	pHull->m_nRenderMode  = kRenderTransTexture;

	pHull->Relink();
	
	return pHull;
}


LINK_ENTITY_TO_CLASS( xen_spore_small, CXenSporeSmall );
LINK_ENTITY_TO_CLASS( xen_spore_medium, CXenSporeMed );
LINK_ENTITY_TO_CLASS( xen_spore_large, CXenSporeLarge );
LINK_ENTITY_TO_CLASS( xen_hull, CXenHull );

void CXenSporeSmall::Spawn( void )
{
	m_nSkin = 0;
	CXenSpore::Spawn();
	UTIL_SetSize( this, Vector(-16,-16,0), Vector(16,16,64));
}
void CXenSporeMed::Spawn( void )
{
	m_nSkin = 1;
	CXenSpore::Spawn();
	UTIL_SetSize( this, Vector(-40,-40,0), Vector(40,40,120));
}


// I just eyeballed these -- fill in hulls for the legs
const Vector CXenSporeLarge::m_hullSizes[] = 
{
	Vector( 90, -25, 0 ),
	Vector( 25, 75, 0 ),
	Vector( -15, -100, 0 ),
	Vector( -90, -35, 0 ),
	Vector( -90, 60, 0 ),
};

void CXenSporeLarge::Spawn( void )
{
	m_nSkin = 2;
	CXenSpore::Spawn();
	UTIL_SetSize( this, Vector(-48,-48,110), Vector(48,48,240));
	
	Vector forward, right;

	AngleVectors( GetAbsAngles(), &forward, &right, NULL );

	// Rotate the leg hulls into position
	for ( int i = 0; i < ARRAYSIZE(m_hullSizes); i++ )
	{
		CXenHull::CreateHull( this, Vector(-12, -12, 0 ), Vector( 12, 12, 120 ), (m_hullSizes[i].x * forward) + (m_hullSizes[i].y * right) );
	}
}

void CXenSpore :: Spawn( void )
{
	Precache();

	SetModel( pModelNames[m_nSkin] );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	m_takedamage = DAMAGE_NO;

//	SetActivity( ACT_IDLE );
	SetSequence( 0 );
	m_flCycle = random->RandomFloat( 0.0f, 1.0f);
	m_flPlaybackRate = random->RandomFloat( 0.7f, 1.4f );
	ResetSequenceInfo( );
	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.1f, 0.4f ) );	// Load balance these a bit
}

const char *CXenSpore::pModelNames[] = 
{
	"models/fungus(small).mdl",
	"models/fungus.mdl",
	"models/fungus(large).mdl",
};

void CXenSpore::Precache( void )
{
	engine->PrecacheModel( (char *)pModelNames[m_nSkin] );
}

void CXenSpore :: Touch( CBaseEntity *pOther )
{
}


//=========================================================
// HL1GIB
//=========================================================
class CHL1Gib : public CBaseEntity
{
	DECLARE_CLASS( CHL1Gib, CBaseEntity );

public:
	void Spawn( const char *szGibModel );
	void BounceGibTouch ( CBaseEntity *pOther );
	void StickyGibTouch ( CBaseEntity *pOther );
	void WaitTillLand( void );
	void LimitVelocity( void );

	virtual int	ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }
	static	void SpawnHeadGib( CBaseEntity *pVictim );
	static	void SpawnRandomGibs( CBaseEntity *pVictim, int cGibs, int human );
	static  void SpawnStickyGibs( CBaseEntity *pVictim, Vector vecOrigin, int cGibs );

	int		m_bloodColor;
	int		m_cBloodDecals;
	int		m_material;
	float	m_lifeTime;

	DECLARE_DATADESC();
};

void CHL1Gib::WaitTillLand ( void )
{
	if ( !IsInWorld() )
	{
		UTIL_Remove( this );
		return;
	}

	if ( GetAbsVelocity() == vec3_origin )
	{
	/*	SetRenderColorA( 255 );
		m_nRenderMode = kRenderTransTexture;
		AddSolidFlags( FSOLID_NOT_SOLID );*/
		
		SetNextThink( gpGlobals->curtime + m_lifeTime );
		SetThink ( SUB_FadeOut );

		// If you bleed, you stink!
	/*	if ( m_bloodColor != DONT_BLEED )
		{
			// ok, start stinkin!
			CSoundEnt::InsertSound ( bits_SOUND_MEAT, pev->origin, 384, 25 );
		}*/
	}
	else
	{
		// wait and check again in another half second.
		SetNextThink( gpGlobals->curtime + 0.5 );
	}
}

//
// Gib bounces on the ground or wall, sponges some blood down, too!
//
void CHL1Gib::BounceGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	trace_t	tr;
	
	if ( GetFlags() & FL_ONGROUND)
	{
		SetAbsVelocity( GetAbsVelocity() * 0.9 );

		SetAbsAngles( QAngle( 0, GetAbsAngles().y, 0 ) );
		SetLocalAngularVelocity( QAngle( 0, GetLocalAngularVelocity().y, 0 ) );
	}
	else
	{
		if ( m_cBloodDecals > 0 && m_bloodColor != DONT_BLEED )
		{
			vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 8 );//move up a bit, and trace down.
			UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -24 ),  MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

			UTIL_BloodDecalTrace( &tr, m_bloodColor );

			m_cBloodDecals--; 
		}

		if ( m_material != matNone && random->RandomInt( 0, 2 ) == 0 )
		{
			float volume;
			float zvel = fabs( GetAbsVelocity().z );
		
			volume = 0.8 * min(1.0, ((float)zvel) / 450.0);

			CBreakable::MaterialSoundRandom( entindex(), (Materials)m_material, volume );
		}
	}
}

//
// Sticky gib puts blood on the wall and stays put. 
//
void CHL1Gib::StickyGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	trace_t	tr;
	
	SetThink ( &CHL1Gib::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 10 );

	if ( !FClassnameIs( pOther, "worldspawn" ) )
	{
		SetNextThink( gpGlobals->curtime );
		return;
	}

	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + GetAbsVelocity() * 32,  MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

	UTIL_BloodDecalTrace( &tr, m_bloodColor );

	SetAbsVelocity( tr.plane.normal * -1 );

	QAngle qAngle;

	VectorAngles( GetAbsVelocity(), qAngle );
	SetAbsAngles( qAngle );

	SetAbsVelocity( vec3_origin );
	SetLocalAngularVelocity( QAngle( 0, 0, 0 ) );
	SetMoveType( MOVETYPE_NONE );
}

//
// Throw a chunk
//
void CHL1Gib::Spawn( const char *szGibModel )
{
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	SetFriction( 0.55 ); // deading the bounce a bit
	
	// sometimes an entity inherits the edict from a former piece of glass,
	// and will spawn using the same render FX or rendermode! bad!
	SetRenderColorA( 255 );
	m_nRenderMode = kRenderNormal;
	m_nRenderFX = kRenderFxNone;
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	pev->classname = MAKE_STRING("gib");

	SetModel( szGibModel );
	UTIL_SetSize( this, Vector( 0, 0, 0), Vector(0, 0, 0));

	SetNextThink( gpGlobals->curtime + 4 );

	m_lifeTime = 25;

	SetThink ( &CHL1Gib::WaitTillLand );
	SetTouch ( &CHL1Gib::BounceGibTouch );

	m_material = matNone;
	m_cBloodDecals = 5;// how many blood decals this gib can place (1 per bounce until none remain). 
}

LINK_ENTITY_TO_CLASS( hl1gib, CHL1Gib );

BEGIN_DATADESC( CHL1Gib )
	// Function Pointers
	DEFINE_FUNCTION( CHL1Gib, BounceGibTouch ),
	DEFINE_FUNCTION( CHL1Gib, StickyGibTouch ),
	DEFINE_FUNCTION( CHL1Gib, WaitTillLand ),
END_DATADESC()
