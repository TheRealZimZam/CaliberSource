//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements breakables and pushables. func_breakable is a bmodel
//			that breaks into pieces after taking damage.
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "filters.h"
#include "func_break.h"
#include "soundent.h"
#include "decals.h"
#include "explode.h"
#include "in_buttons.h"
#include "physics.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "globals.h"
#include "util.h"
#include "physics_impact_damage.h"
#include "tier0/icommandline.h"

#ifdef PORTAL
	#include "portal_shareddefs.h"
	#include "portal_util_shared.h"
	#include "prop_portal_shared.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar func_break_max_pieces( "func_break_max_pieces", "15", FCVAR_ARCHIVE | FCVAR_REPLICATED );
ConVar func_break_reduction_factor( "func_break_reduction_factor", ".5" );

extern void PlayerPickupObject( CBasePlayer *pPlayer, CBaseEntity *pObject );

extern Vector		g_vecAttackDir;

// Just add more items to the bottom of this array and they will automagically be supported
// This is done instead of just a classname/string in the FGD because its extremely stupid and cant be
// easily updated by mappers...

#ifndef HL1_DLL
	const char *CBreakable::pSpawnObjects[] =
	{
		NULL,						// 0
		"item_battery",				// 1
		"item_healthkit",			// 2
		"item_box_srounds"			// 3
		"item_large_box_srounds"	// 4
		"item_box_mrounds"			// 5
		"item_large_box_mrounds"	// 6
		"item_box_lrounds"			// 7
		"item_large_box_lrounds"	// 8
		"item_box_buckshot"			// 9
		"item_flare_round"			// 10
		"item_box_flare_rounds"		// 11
		"item_ml_grenade"			// 12
		"item_ar2_grenade"			// 13
		"item_box_sniper_rounds"	// 14
		"weapon_iceaxe"				// 15
		"weapon_stunstick"			// 16
		"weapon_ar1"				// 17
		"weapon_ar2"				// 18
		"weapon_hmg1"				// 19
		"weapon_rpg"				// 20
		"weapon_smg1"				// 21
		"weapon_smg2"				// 22
		"weapon_slam"				// 23
		"weapon_shotgun"			// 24
		"weapon_molotov"			// 25
		"item_dynamic_resupply",	// 26
	};
#else
	// Half-Life 1 spawn objects!
	const char *CBreakable::pSpawnObjects[] =
	{
		NULL,				// 0
		"item_battery",		// 1
		"item_healthkit",	// 2
		"weapon_glock",		// 3
		"ammo_9mmclip",		// 4
		"weapon_mp5",		// 5
		"ammo_9mmAR",		// 6
		"ammo_ARgrenades",	// 7
		"weapon_shotgun",	// 8
		"ammo_buckshot",	// 9
		"weapon_crossbow",	// 10
		"ammo_crossbow",	// 11
		"weapon_357",		// 12
		"ammo_357",			// 13
		"weapon_rpg",		// 14
		"ammo_rpgclip",		// 15
		"ammo_gaussclip",	// 16
		"weapon_handgrenade",// 17
		"weapon_tripmine",	// 18
		"weapon_satchel",	// 19
		"weapon_snark",		// 20
		"weapon_hornetgun",	// 21
	};
#endif

const char *pFGDPropData[] =
{
	// 3/10/24 rolled back -
	// Original list is crap and disordered, but i cant really change it or compatibility with
	// old maps is broke... I'll just try to make it look nice FGD wise -MM
	NULL,
	"Wooden.Tiny",		// 1
	"Wooden.Small",		// 2
	"Wooden.Medium",	// 3
	"Wooden.Large",		// 4
	"Wooden.Huge",		// 5
	"Metal.Small",		// 6
	"Metal.Medium",		// 7
	"Metal.Large",		// 8
	"Cardboard.Small",	// 9
	"Cardboard.Medium",	// 10
	"Cardboard.Large",	// 11
	"Stone.Small",		// 12
	"Stone.Medium",		// 13
	"Stone.Large",		// 14
	"Stone.Huge",		// 15
	"Glass.Small",		// 16
	"Plastic.Small",	// 17
	"Plastic.Medium",	// 18
	"Plastic.Large",	// 19
	"Pottery.Small",	// 20
	"Pottery.Medium",	// 21
	"Pottery.Large",	// 22
	"Pottery.Huge",		// 23
	"Glass.Window",		// 24

	"ClubOnly",			// 25
	"ExplosiveOnly",	// 26

#ifdef CALIBER_DLL
	"Wooden_Unbreakable.Huge",	// 27
	"Cloth.Small",				// 28
	"Cloth.Medium",				// 29
	"Cloth.Large",				// 30
	"Metal.Barrel",				// 31
	"Metal.ExplosiveBarrel",	// 32
	"Metal.ExplosiveContainer",	// 33
	"Glass.Large",				// 34
	"Glass.Window_Bulletproof",	// 35
	"Flesh.Tiny",				// 36
	"Flesh.Small",				// 37
	"Flesh.Medium",				// 38
	"Flesh.Large",				// 39
#endif
};

LINK_ENTITY_TO_CLASS( func_breakable, CBreakable );
BEGIN_DATADESC( CBreakable )

	DEFINE_FIELD( m_Material, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_Explosion, FIELD_INTEGER, "explosion" ),
	DEFINE_KEYFIELD( m_GibDir, FIELD_VECTOR, "gibdir" ),
	DEFINE_FIELD( m_hBreaker, FIELD_EHANDLE ),

	// Don't need to save/restore these because we precache after restore
	//DEFINE_FIELD( m_idShard, FIELD_INTEGER ),
	DEFINE_FIELD( m_angle, FIELD_FLOAT ),
	DEFINE_FIELD( m_iszGibModel, FIELD_STRING ),
	DEFINE_FIELD( m_iszSpawnObject, FIELD_STRING ),
	DEFINE_KEYFIELD( m_ExplosionMagnitude, FIELD_INTEGER, "explodemagnitude" ),
	DEFINE_KEYFIELD( m_flPressureDelay, FIELD_FLOAT, "PressureDelay" ),
	DEFINE_KEYFIELD( m_iMinHealthDmg, FIELD_INTEGER, "minhealthdmg" ),
	DEFINE_FIELD( m_bTookPhysicsDamage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bUseDeadModel, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iszPropData, FIELD_STRING ),
	DEFINE_INPUT( m_impactEnergyScale, FIELD_FLOAT, "physdamagescale" ),
	DEFINE_KEYFIELD( m_iExtraImpactFX, FIELD_INTEGER, "extraimpactfx" ),
	DEFINE_KEYFIELD( m_PerformanceMode, FIELD_INTEGER, "PerformanceMode" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Break", InputBreak ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetHealth", InputSetHealth ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddHealth", InputAddHealth ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "RemoveHealth", InputRemoveHealth ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetMass", InputSetMass ),

	// Function Pointers
	DEFINE_ENTITYFUNC( BreakTouch ),
	DEFINE_THINKFUNC( Die ),

	// Outputs
	DEFINE_OUTPUT(m_OnBreak, "OnBreak"),
	DEFINE_OUTPUT(m_OnHealthChanged, "OnHealthChanged"),

	DEFINE_FIELD( m_flDmgModBullet, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDmgModClub, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDmgModExplosive, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDmgModFire, FIELD_FLOAT ),
	DEFINE_FIELD( m_iszPhysicsDamageTableName, FIELD_STRING ),
	DEFINE_FIELD( m_iszBreakableModel, FIELD_STRING ),
	DEFINE_FIELD( m_iBreakableSkin, FIELD_INTEGER ),
	DEFINE_FIELD( m_iBreakableCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_iMaxBreakableSize, FIELD_INTEGER ),
	DEFINE_FIELD( m_iszBasePropData, FIELD_STRING ),
	DEFINE_FIELD( m_iInteractions, FIELD_INTEGER ),
	DEFINE_FIELD( m_explodeRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_iszModelName, FIELD_STRING ),
	
	// Physics Influence
	DEFINE_FIELD( m_hPhysicsAttacker, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flLastPhysicsInfluenceTime, FIELD_TIME ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBreakable::KeyValue( const char *szKeyName, const char *szValue )
{
	// UNDONE_WC: explicitly ignoring these fields, but they shouldn't be in the map file!
	if (FStrEq(szKeyName, "material"))
	{
		int i = atoi( szValue);

		// 0:glass, 1:metal, 2:flesh, 3:wood
		if ((i < 0) || (i >= matLastMaterial))
			m_Material = matWood;
		else
			m_Material = (Materials)i;
	}
	else if (FStrEq(szKeyName, "deadmodel"))
	{
		// Deadmodel - instead of having to create a disabled brush or prop,
		// you can just specifiy a model here and things will be taken care of.
//		m_iszDeadModelName = AllocPooledString(szValue);
		m_bUseDeadModel = (atoi(szValue) != 0);
	}
	else if (FStrEq(szKeyName, "shards"))
	{
		// OBSOLETE - See func_break_max_pieces
//		m_iShards = atof(szValue);
		Warning("%s (%s) using obsolete shard parameters. Replace with a func_breakable_surf!\n", GetClassname(), GetDebugName() );
	}
	else if (FStrEq(szKeyName, "gibmodel") )
	{
		m_iszGibModel = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "spawnobject") )
	{
		// Default to NULL
		m_iszSpawnObject = NULL_STRING;

		// TODO; If its a obsoleet number use the old preset list, else assume its a string
		int object = atoi( szValue );
		if ( object > 0 && object < ARRAYSIZE(pSpawnObjects) )
			m_iszSpawnObject = MAKE_STRING( pSpawnObjects[object] );
		else if ( object != 0 )
			m_iszSpawnObject = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "propdata") )
	{
		int pdata = atoi( szValue );
		if ( pdata > 0 && pdata < ARRAYSIZE(pFGDPropData) )
		{
			m_iszPropData = MAKE_STRING( pFGDPropData[pdata] );
		}
		else if ( pdata )
		{
			// If you've hit this warning, it's probably because someone's added a new
			// propdata field to func_breakables in the .fgd, and not added it to the
			// pFGDPropData list.
			Warning("func_breakable (%s) with invalid propdata %d.\n", GetDebugName(), pdata );
		}
	}
	else if (FStrEq(szKeyName, "lip") )
	{
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBreakable::Spawn( void )
{
	// Initialize damage modifiers. Must be done before baseclass spawn.
	m_flDmgModBullet = func_breakdmg_bullet.GetFloat();
	m_flDmgModClub = func_breakdmg_club.GetFloat();
	m_flDmgModExplosive = func_breakdmg_explosive.GetFloat();
	m_flDmgModFire = func_breakdmg_fire.GetFloat();

	ParsePropData();

    Precache( );    

	if ( !m_iHealth || FBitSet( m_spawnflags, SF_BREAK_TRIGGER_ONLY ) )
	{
		// This allows people to shoot at the glass (since it's penetrable)
		if ( m_Material == matGlass )
		{
			m_iHealth = 1;
		}

		m_takedamage = DAMAGE_NO;
	}
	else
	{
		m_takedamage = DAMAGE_YES;
	}

	m_iMaxHealth = ( m_iHealth > 0 ) ? m_iHealth : 1;
  
	SetSolid( SOLID_BSP );
    SetMoveType( MOVETYPE_PUSH );
	
	// this is a hack to shoot the gibs in a specific yaw/direction
	m_angle = GetLocalAngles().y;
	SetLocalAngles( vec3_angle );
	
	SetModel( STRING( GetModelName() ) );//set size and link into world.

	SetTouch( &CBreakable::BreakTouch );
	if ( FBitSet( m_spawnflags, SF_BREAK_TRIGGER_ONLY ) )		// Only break on trigger
	{
		SetTouch( NULL );
	}

	// Flag unbreakable glass as "worldbrush" so it will block ALL tracelines
	if ( !IsBreakable() && m_nRenderMode != kRenderNormal )
		AddFlag( FL_WORLDBRUSH );

	if ( m_impactEnergyScale == 0 )
	{
		m_impactEnergyScale = 1.0;
	}

	// If this is set to 0 this is a old breakable that hasnt specified, so put it to true
	if ( m_iExtraImpactFX == NULL )
	{
		m_iExtraImpactFX = 2;
	}

	CreateVPhysics();
}

//-----------------------------------------------------------------------------
// Purpose: Parse this prop's data, if it has a keyvalues section.
//			Returns true only if this prop is using a model that has a prop_data section that's invalid.
//-----------------------------------------------------------------------------
void CBreakable::ParsePropData( void )
{
	if ( m_iszPropData == NULL_STRING )
		return;

	if ( !Q_strncmp( STRING(m_iszPropData), "None", 4 ) )
		return;

	g_PropDataSystem.ParsePropFromBase( this, STRING(m_iszPropData) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBreakable::CreateVPhysics( void )
{
	VPhysicsInitStatic();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CBreakable::MaterialSound( Materials precacheMaterial )
{
    switch ( precacheMaterial ) 
	{
	case matWood:
		return "Breakable.MatWood";
	case matFlesh:
	case matWeb:
		return "Breakable.MatFlesh";
	case matComputer:
		return "Breakable.Computer";
	case matUnbreakableGlass:
	case matGlass:
		return "Breakable.MatGlass";
	case matMetal:
		return "Breakable.MatMetal";
	case matCinderBlock:
	case matRocks:
		return "Breakable.MatConcrete";
	case matCeilingTile:
	case matDirt:
	case matPlaster:
	case matPlastic:
	case matNone:
	default:
		break;
	}

	return NULL;
}


void CBreakable::MaterialSoundRandom( int entindex, Materials soundMaterial, float volume )
{
	const char	*soundname;
	soundname = MaterialSound( soundMaterial );
	if ( !soundname )
		return;

	CSoundParameters params;
	if ( !GetParametersForSound( soundname, params, NULL ) )
		return;

	CPASAttenuationFilter filter( CBaseEntity::Instance( entindex ), params.soundlevel );


	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = volume;
	ep.m_SoundLevel = params.soundlevel;

	EmitSound( filter, entindex, ep );
}


void CBreakable::Precache( void )
{
	const char *pGibName = "WoodChunks";

    switch (m_Material) 
	{
	case matWood:
		pGibName = "WoodChunks";
		break;

	case matUnbreakableGlass:
	case matGlass:
		pGibName = "GlassChunks";
		break;

	case matMetal:
		pGibName = "MetalChunks";
		break;

	case matRocks:
		pGibName = "ConcreteChunks";
		break;

#ifdef HL1_DLL
	case matComputer:
		pGibName = "ComputerGibs";
		break;
#else
	case matComputer:
		pGibName = "ComputerChunks";
		break;
#endif

	case matCeilingTile:
		pGibName = "CeilingTile";
		break;

	case matCinderBlock:
		pGibName = "CinderBlocks";
		break;
		
	case matFlesh:
		pGibName = "FleshGibs";
		break;

	case matWeb:
		pGibName = "WebGibs";
		break;

	case matDirt:
		pGibName = "DirtChunks";
		break;

	case matPlaster:
		pGibName = "PlasterChunks";
		break;

	case matPlastic:
		pGibName = "PlasticChunks";
		break;

	case matNone:
		pGibName = "";
		break;

	default:
		Warning("%s (%s) at (%.3f %.3f %.3f) using obsolete or unknown material type.\n", GetClassname(), GetDebugName(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		pGibName = "WoodChunks";
		break;
	}

	if ( m_iszGibModel != NULL_STRING )
	{
		pGibName = STRING(m_iszGibModel);
		PrecacheModel( pGibName );
	}

	m_iszModelName = MAKE_STRING( pGibName );

	// Precache the spawn item's data
	if ( !CommandLine()->CheckParm("-makereslists"))
	{
		if ( m_iszSpawnObject != NULL_STRING )
		{
			UTIL_PrecacheOther( STRING( m_iszSpawnObject ) );
		}
	}
	else
	{
		// Actually, precache all possible objects...
		for ( int i = 0; i < ARRAYSIZE(pSpawnObjects) ; ++i )
		{
			if ( !pSpawnObjects[ i ] )
				continue;

			if ( !Q_strnicmp( pSpawnObjects[ i ], "unused", Q_strlen( "unused" ) ) )
				continue;

			UTIL_PrecacheOther( pSpawnObjects[ i ] );
		}
	}

	PrecacheScriptSound( "Breakable.MatGlass" );
	PrecacheScriptSound( "Breakable.MatWood" );
	PrecacheScriptSound( "Breakable.MatMetal" );
	PrecacheScriptSound( "Breakable.MatFlesh" );
	PrecacheScriptSound( "Breakable.MatConcrete" );
	PrecacheScriptSound( "Breakable.Computer" );
	PrecacheScriptSound( "Breakable.Crate" );
	PrecacheScriptSound( "Breakable.Glass" );
	PrecacheScriptSound( "Breakable.Metal" );
	PrecacheScriptSound( "Breakable.Flesh" );
	PrecacheScriptSound( "Breakable.Concrete" );
	PrecacheScriptSound( "Breakable.Ceiling" );
}


// play shard sound when func_breakable takes damage.
// the more damage, the louder the shard sound.
void CBreakable::DamageSound( void )
{
	int pitch;
	float fvol;
	char *soundname = NULL;
	int material = m_Material;

	if (random->RandomInt(0,2))
	{
		pitch = PITCH_NORM;
	}
	else
	{
		pitch = 95 + random->RandomInt(0,34);
	}

	fvol = random->RandomFloat(0.75, 1.0);

	if (material == matComputer && random->RandomInt(0,1))
	{
		material = matMetal;
	}

	switch (material)
	{
	case matGlass:
	case matUnbreakableGlass:
		soundname = "Breakable.MatGlass";
		break;

	case matWood:
		soundname = "Breakable.MatWood";
		break;

	case matMetal:
		soundname = "Breakable.MatMetal";
		break;

	case matRocks:
	case matCinderBlock:
		soundname = "Breakable.MatConcrete";
		break;

	case matComputer:
		soundname = "Breakable.Computer";
		break;

	default:
		break;
	}

	if ( soundname )
	{
		CSoundParameters params;
		if ( GetParametersForSound( soundname, params, NULL ) )
		{
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = params.channel;
			ep.m_pSoundName = params.soundname;
			ep.m_flVolume = fvol;
			ep.m_SoundLevel = params.soundlevel;
			ep.m_nPitch = pitch;

			EmitSound( filter, entindex(), ep );
		}
	}
}

void CBreakable::BreakTouch( CBaseEntity *pOther )
{
	// only players can break these right now
	if ( !pOther->IsPlayer() || !IsBreakable() )
	{
        return;
	}

	float flDamage;

	// can I be broken when run into?
	if ( HasSpawnFlags( SF_BREAK_TOUCH ) )
	{
		flDamage = pOther->GetSmoothedVelocity().Length() * 0.01;

		if (flDamage >= m_iHealth)
		{
			m_takedamage = DAMAGE_YES;

			SetTouch( NULL );
			OnTakeDamage( CTakeDamageInfo( pOther, pOther, flDamage, DMG_CRUSH ) );

			// do a little damage to player if we broke glass or computer
			CTakeDamageInfo info( pOther, pOther, flDamage/4, DMG_SLASH );
			CalculateMeleeDamageForce( &info, (pOther->GetAbsOrigin() - GetAbsOrigin()), GetAbsOrigin() );
			pOther->TakeDamage( info );
		}
	}

	// can I be broken when stood upon?
	if ( HasSpawnFlags( SF_BREAK_PRESSURE ) && pOther->GetGroundEntity() == this )
	{
		// play creaking sound here.
		DamageSound();

		m_hBreaker = pOther;

		SetThink( &CBreakable::Die );
		SetTouch( NULL );
		
		// Add optional delay 
		SetNextThink( gpGlobals->curtime + m_flPressureDelay );
	}
}

//-----------------------------------------------------------------------------
// Break when triggered
//-----------------------------------------------------------------------------
void CBreakable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || pActivator->IsPlayer() )
		return;

	Break( pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for adding to the breakable's health.
// Input  : Integer health points to add.
//-----------------------------------------------------------------------------
void CBreakable::InputAddHealth( inputdata_t &inputdata )
{
	UpdateHealth( m_iHealth + inputdata.value.Int(), inputdata.pActivator );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for breaking the breakable immediately.
//-----------------------------------------------------------------------------
void CBreakable::InputBreak( inputdata_t &inputdata )
{
	Break( inputdata.pActivator );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for removing health from the breakable.
// Input  : Integer health points to remove.
//-----------------------------------------------------------------------------
void CBreakable::InputRemoveHealth( inputdata_t &inputdata )
{
	UpdateHealth( m_iHealth - inputdata.value.Int(), inputdata.pActivator );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the breakable's health.
//-----------------------------------------------------------------------------
void CBreakable::InputSetHealth( inputdata_t &inputdata )
{
	UpdateHealth( inputdata.value.Int(), inputdata.pActivator );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the breakable's mass.
//-----------------------------------------------------------------------------
void CBreakable::InputSetMass( inputdata_t &inputdata )
{
	IPhysicsObject * vPhys = VPhysicsGetObject();
	if ( vPhys )
	{
		float toMass = inputdata.value.Float();
		Assert(toMass > 0);
		vPhys->SetMass( toMass );
	}
	else
	{
		Warning( "Tried to call SetMass() on %s but it has no physics.\n", GetEntityName().ToCStr() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Choke point for changes to breakable health. Ensures outputs are fired.
// Input  : iNewHealth - 
//			pActivator - 
// Output : Returns true if the breakable survived, false if it died (broke).
//-----------------------------------------------------------------------------
bool CBreakable::UpdateHealth( int iNewHealth, CBaseEntity *pActivator )
{
	if ( iNewHealth != m_iHealth )
	{
		m_iHealth = iNewHealth;

		if ( m_iMaxHealth == 0 )
		{
			Assert( false );
			m_iMaxHealth = 1;
		}

		// Output the new health as a percentage of max health [0..1]
		float flRatio = clamp( (float)m_iHealth / (float)m_iMaxHealth, 0, 1 );
		m_OnHealthChanged.Set( flRatio, pActivator, this );

		if ( m_iHealth <= 0 )
		{
			Break( pActivator );
			return false;
		}
		else
		{
			if ( FBitSet( m_spawnflags, SF_BREAK_TRIGGER_ONLY ) )
			{
				m_takedamage = DAMAGE_NO;
			}
			else
			{
				m_takedamage = DAMAGE_YES;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Breaks the breakable if it can be broken.
// Input  : pBreaker - The entity that caused us to break, either via an input,
//				by shooting us, or by touching us.
//-----------------------------------------------------------------------------
void CBreakable::Break( CBaseEntity *pBreaker )
{
	if ( IsBreakable() )
	{
		QAngle angles = GetLocalAngles();
		angles.y = m_angle;
		SetLocalAngles( angles );
		m_hBreaker = pBreaker;
		Die();
	}
	else
	{
		DamageSound();
	}
}


void CBreakable::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
#ifndef HL1_DLL
	if ( m_iExtraImpactFX == 2 && random->RandomInt(0,1) )
	{
		switch( m_Material )
		{
			// random spark if this is a 'computer' object
			case matComputer:
			{
				g_pEffects->Sparks( ptr->endpos );

				EmitSound( "Breakable.Computer" );
			}
			break;

			// Send some smoke and embers - actual ignition is level-based
			case matWood:
			{
				if ( info.GetDamageType() & (DMG_BURN|DMG_PLASMA|DMG_SHOCK) )
				{
					UTIL_Smoke( info.GetDamagePosition(), random->RandomInt( 10, 15 ), 10 );
				}
			}
			break;

			// Ricochet spark
			case matMetal:
			case matUnbreakableGlass:
				if ( info.GetDamageType() & DMG_BULLET )
				{
					g_pEffects->Ricochet( ptr->endpos, (vecDir*-1.0f) );
				}
			break;

			// Shoot some extra blood
			case matFlesh:
			{
				//!!TODO; its not always red - figure something out here
				UTIL_BloodImpact( ptr->endpos, vecDir, BLOOD_COLOR_RED, 1 );
			}
			break;
		}
	}
#else
	if ( random->RandomInt(0,1) )
	{
		// random spark if this is a 'computer' object
		switch( m_Material )
		{
			case matComputer:
			{
				g_pEffects->Sparks( ptr->endpos );

				EmitSound( "Breakable.Computer" );
			}
			break;
			
			case matMetal:
			case matUnbreakableGlass:
				if ( info.GetDamageType() & DMG_BULLET )
				{
					g_pEffects->Ricochet( ptr->endpos, (vecDir*-1.0f) );
				}
			break;
		}
	}
#endif

	BaseClass::TraceAttack( info, vecDir, ptr );
}


//-----------------------------------------------------------------------------
// Purpose: Allows us to take damage from physics objects
//-----------------------------------------------------------------------------
void CBreakable::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision( index, pEvent );

	Vector damagePos;
	pEvent->pInternalData->GetContactPoint( damagePos );
	Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
	if ( damageForce == vec3_origin )
	{
		// This can happen if this entity is a func_breakable, and can't move.
		// Use the velocity of the entity that hit us instead.
		damageForce = pEvent->postVelocity[!index] * pEvent->pObjects[!index]->GetMass();
	}

	// If we're supposed to explode on collision, do so
	if ( HasSpawnFlags( SF_BREAK_PHYSICS_BREAK_IMMEDIATELY ) )
	{
		// We're toast
		m_bTookPhysicsDamage = true;
		CBaseEntity *pHitEntity = pEvent->pEntities[!index];

		// HACKHACK: Reset mass to get correct collision response for the object breaking this glass
		if ( m_Material == matGlass )
		{
			pEvent->pObjects[index]->SetMass( 2.0f );
		}
		CTakeDamageInfo dmgInfo( pHitEntity, pHitEntity, damageForce, damagePos, (m_iHealth + 1), DMG_CRUSH );
		PhysCallbackDamage( this, dmgInfo, *pEvent, index );
	}
	else if ( !HasSpawnFlags( SF_BREAK_DONT_TAKE_PHYSICS_DAMAGE ) )
	{
		int otherIndex = !index;
		CBaseEntity *pOther = pEvent->pEntities[otherIndex];

		// We're to take normal damage from this
		int damageType;
		IBreakableWithPropData *pBreakableInterface = assert_cast<IBreakableWithPropData*>(this);
		float damage = CalculateDefaultPhysicsDamage( index, pEvent, m_impactEnergyScale, true, damageType, pBreakableInterface->GetPhysicsDamageTable() );
		if ( damage > 0 )
		{
			// HACKHACK: Reset mass to get correct collision response for the object breaking this glass
			if ( m_Material == matGlass )
			{
				pEvent->pObjects[index]->SetMass( 2.0f );
			}
			CTakeDamageInfo dmgInfo( pOther, pOther, damageForce, damagePos, damage, damageType );
			PhysCallbackDamage( this, dmgInfo, *pEvent, index );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allows us to make damage exceptions that are breakable-specific.
//-----------------------------------------------------------------------------
int CBreakable::OnTakeDamage( const CTakeDamageInfo &info )
{
	Vector	vecTemp;

	CTakeDamageInfo subInfo = info;

	// If attacker can't do at least the min required damage to us, don't take any damage from them
	if ( m_takedamage == DAMAGE_NO || info.GetDamage() < m_iMinHealthDmg )
		return 0;

	// Check our damage filter
	if ( !PassesDamageFilter(subInfo) )
	{
		m_bTookPhysicsDamage = false;
		return 1;
	}

	vecTemp = subInfo.GetInflictor()->GetAbsOrigin() - WorldSpaceCenter();

	if (!IsBreakable())
		return 0;

	float flPropDamage = GetBreakableDamage( subInfo, assert_cast<IBreakableWithPropData*>(this) );
	subInfo.SetDamage( flPropDamage );
	
	int iPrevHealth = m_iHealth;
	BaseClass::OnTakeDamage( subInfo );

	// HACK: slam health back to what it was so UpdateHealth can do its thing
	int iNewHealth = m_iHealth;
	m_iHealth = iPrevHealth;
	if ( !UpdateHealth( iNewHealth, info.GetAttacker() ) )
		return 1;

	// Make a shard noise each time func breakable is hit, if it's capable of taking damage
	if ( m_takedamage == DAMAGE_YES )
	{
		// Don't play shard noise if being burned.
		// Don't play shard noise if cbreakable actually died.
		if ( subInfo.GetDamageType() & (DMG_SLASH|DMG_CLUB|DMG_VEHICLE) )
		{
			DamageSound();
		}
		// If you're about to explode, send out a danger sound
		if ( Explodable() && ((GetFlags() & FL_ONFIRE) != 0 || m_iHealth < GetMaxHealth() / 2) )	//IsOnFire() is not working... i dunno
		{
			CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), GetExplosiveRadius(), 0.2, this );
		}
	}

	return 1;
}

//------------------------------------------------------------------------------
// Purpose : Reset the OnGround flags for any entities that may have been
//			 resting on me
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CBreakable::ResetOnGroundFlags(void)
{
	// !!! HACK  This should work!
	// Build a box above the entity that looks like an 9 inch high sheet
	Vector mins, maxs;
	CollisionProp()->WorldSpaceAABB( &mins, &maxs );
	mins.z -= 1;
	maxs.z += 8;

	// BUGBUG -- can only find 256 entities on a breakable -- should be enough
	CBaseEntity *pList[256];
	int count = UTIL_EntitiesInBox( pList, 256, mins, maxs, FL_ONGROUND );
	if ( count )
	{
		for ( int i = 0; i < count; i++ )
		{
			pList[i]->SetGroundEntity( (CBaseEntity *)NULL );
		}
	}

#ifdef PORTAL
	// !!! HACK  This should work!
	// Tell touching portals to fizzle
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		Vector vMin, vMax;
		CollisionProp()->WorldSpaceAABB( &vMin, &vMax );

		Vector vBoxCenter = ( vMin + vMax ) * 0.5f;
		Vector vBoxExtents = ( vMax - vMin ) * 0.5f;

		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( UTIL_IsBoxIntersectingPortal( vBoxCenter, vBoxExtents, pTempPortal ) )
			{
				pTempPortal->DoFizzleEffect( PORTAL_FIZZLE_KILLED, false );
				pTempPortal->Fizzle();
			}
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Breaks the breakable. m_hBreaker is the entity that caused us to break.
//-----------------------------------------------------------------------------
void CBreakable::Die( void )
{
	Vector vecVelocity;// shard velocity
	char cFlag = 0;	//Used for legacy tempents - only glass does anything for now
	int pitch;
	float fvol;

	pitch = 95 + random->RandomInt(0,29);

	if (pitch > 97 && pitch < 103)
	{
		pitch = 100;
	}

	// The more negative m_iHealth, the louder
	// the sound should be.

	fvol = random->RandomFloat(0.80, 0.95) + (abs(m_iHealth) / 100.0);
	if (fvol > 1.0)
	{
		fvol = 1.0;
	}

	const char *soundname = NULL;

	switch (m_Material)
	{
	default:
		break;

	case matGlass:
		soundname = "Breakable.Glass";
		cFlag = BREAK_GLASS;
		break;

	case matWood:
		soundname = "Breakable.Crate";
		cFlag = BREAK_WOOD;
		break;

	case matMetal:
		soundname = "Breakable.Metal";
		cFlag = BREAK_METAL;
		break;

	case matRocks:
	case matCinderBlock:
		soundname = "Breakable.Concrete";
		cFlag = BREAK_CONCRETE;
		break;

	case matComputer:
		soundname = "Breakable.Computer";
		cFlag = BREAK_METAL;
		break;

	case matFlesh:
	case matWeb:
		soundname = "Breakable.Flesh";
		cFlag = BREAK_FLESH;
		break;

	case matCeilingTile:
		soundname = "Breakable.Ceiling";
		break;

	case matDirt:
		soundname = "Breakable.Dirt";
		break;

	case matPlaster:
		soundname = "Breakable.Plaster";
		break;

	case matPlastic:
		soundname = "Breakable.Plastic";
		break;
	}
    
	if ( soundname )
	{
		if ( m_hBreaker && m_hBreaker->IsPlayer() )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "break_breakable" );
			if ( event )
			{
				event->SetInt( "userid", ToBasePlayer( m_hBreaker )->GetUserID() );
				event->SetInt( "entindex", entindex() );
				event->SetInt( "material", cFlag );
				gameeventmanager->FireEvent( event );
			}
		}

		CSoundParameters params;
		if ( GetParametersForSound( soundname, params, NULL ) )
		{
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = params.channel;
			ep.m_pSoundName = params.soundname;
			ep.m_flVolume = fvol;
			ep.m_SoundLevel = params.soundlevel;
			ep.m_nPitch = pitch;

			EmitSound( filter, entindex(), ep );	
		}

#ifndef HL1_DLL
		CSoundEnt::InsertSound( SOUND_WORLD, GetAbsOrigin(), (512 * fvol), 0.2, this );
#endif
	}
		
	switch( m_Explosion )
	{
	case expDirected:
		vecVelocity = g_vecAttackDir * -200;
		break;

	case expUsePrecise:
		{
			AngleVectors( m_GibDir, &vecVelocity, NULL, NULL );
			vecVelocity *= 200;
		}
		break;

	case expRandom:
		vecVelocity.x = 0;
		vecVelocity.y = 0;
		vecVelocity.z = 0;
		break;

	default:
		DevMsg("**ERROR - Unspecified gib dir method in func_breakable!\n");
		break;
	}

	Vector vecSpot = WorldSpaceCenter();
	CPVSFilter filter2( vecSpot );

	int iModelIndex = 0;
	CCollisionProperty *pCollisionProp = CollisionProp();

	Vector vSize = pCollisionProp->OBBSize();
	int iCount = ( vSize[0] * vSize[1] + vSize[1] * vSize[2] + vSize[2] * vSize[0] ) / ( 3 * 12 * 12 );

	if ( iCount > func_break_max_pieces.GetInt() )
	{
		iCount = func_break_max_pieces.GetInt();
	}

	ConVarRef breakable_disable_gib_limit( "breakable_disable_gib_limit" );
	if ( !breakable_disable_gib_limit.GetBool() && iCount )
	{
		if ( m_PerformanceMode == PM_NO_GIBS )
		{
			iCount = 0;
		}
		else if ( m_PerformanceMode == PM_REDUCED_GIBS )
		{
			int iNewCount = iCount * func_break_reduction_factor.GetFloat();
			iCount = max( iNewCount, 1 );
		}
	}

	float fadetime = 2.5;
	if ( m_iszModelName != NULL_STRING )
	{
		for ( int i = 0; i < iCount; i++ )
		{
			// Use the passed model instead of the propdata type
			const char *modelName = STRING( m_iszModelName );
			
			// if the map specifies a model by name
			if( strstr( modelName, ".mdl" ) != NULL )
				iModelIndex = modelinfo->GetModelIndex( modelName );
			else
				iModelIndex = modelinfo->GetModelIndex( g_PropDataSystem.GetRandomChunkModel( STRING( m_iszModelName ) ) );

			// All objects except the first one in this run are marked as slaves...
			int slaveFlag = 0;
			if ( i != 0 )
			{
				slaveFlag = BREAK_SLAVE;
			}

			te->BreakModel( filter2, 0.0, 
				vecSpot, pCollisionProp->GetCollisionAngles(), vSize, 
				vecVelocity, iModelIndex, 100, 1, fadetime, cFlag | slaveFlag );
		}
	}

	ResetOnGroundFlags();

	// Don't fire something that could fire myself
	SetName( NULL_STRING );

	AddSolidFlags( FSOLID_NOT_SOLID );

	// Fire targets on break
	m_OnBreak.FireOutput( m_hBreaker, this );

	if ( m_iszSpawnObject != NULL_STRING )
		CBaseEntity::Create( STRING(m_iszSpawnObject), vecSpot, pCollisionProp->GetCollisionAngles(), this );

	if ( Explodable() )
		ExplosionCreate( vecSpot, pCollisionProp->GetCollisionAngles(), this, GetExplosiveDamage(), GetExplosiveRadius(), true );

	VPhysicsDestroyObject();

	if ( m_bUseDeadModel )
	{
		// Disable collisions, Update texture
		SetTextureFrameIndex( 1 );
		SetUse( NULL );
		SetTouch( NULL );
		m_takedamage = DAMAGE_NO;

		// TODO; Need to count the number of breakables here...
		// It'd be nice to keep broken windows and such around, but breakable entities
		// are expected to be deleted when they are killed - if logics arent setup when entering
		// new areas to delete the old area's breakables, the entlist could get clogged very quickly.
		// For now, im going to assume the level designer hasnt done that and just fade these out here
		// like its a ps1 game - MM

		// Reuse the pressure delay time
		if ( m_flPressureDelay > 0 )
			fadetime = m_flPressureDelay;

		SUB_StartFadeOut(fadetime, true);
	}
	else
	{
		SetThink( &CBreakable::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether this object can be broken.
//-----------------------------------------------------------------------------
bool CBreakable::IsBreakable( void ) 
{ 
	return m_Material != matUnbreakableGlass;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char const *CBreakable::DamageDecal( int bitsDamageType, int gameMaterial )
{
	if ( m_Material == matGlass )
		return "GlassBreak";

	if ( m_Material == matUnbreakableGlass )
		return "BulletProof";

	return BaseClass::DamageDecal( bitsDamageType, gameMaterial );
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CBreakable::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		if ( GetMaxHealth() )
		{
			char tempstr[512];
			Q_snprintf(tempstr,sizeof(tempstr),"Health: %i",GetHealth());
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

		if ( m_iszBasePropData != NULL_STRING )
		{
			char tempstr[512];
			Q_snprintf(tempstr, sizeof(tempstr),"Base PropData: %s", STRING(m_iszBasePropData) );
			EntityText( text_offset, tempstr, 0);
			text_offset++;
		}
	}

	return text_offset;
}


//-----------------------------------------------------------------------------
// Purpose: Keep track of physgun influence
//-----------------------------------------------------------------------------

void CBreakable::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;
}

void CBreakable::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;
}

CBasePlayer *CBreakable::HasPhysicsAttacker( float dt )
{
	if (gpGlobals->curtime - dt <= m_flLastPhysicsInfluenceTime)
	{
		return m_hPhysicsAttacker;
	}
	return NULL;
}


//=============================================================================================================================
// PUSHABLE
//=============================================================================================================================
class CPushable : public CBreakable
{
public:
	DECLARE_CLASS( CPushable, CBreakable );

	void	Spawn ( void );
	bool	CreateVPhysics( void );
	void	Move( CBaseEntity *pMover, bool push );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_ONOFF_USE; }

	virtual void VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	unsigned int PhysicsSolidMaskForEntity( void ) const { return MASK_PLAYERSOLID; }
#if 0
private:
	int			m_iMassOverride;
#endif
};

LINK_ENTITY_TO_CLASS( func_pushable, CPushable );

#if 0
BEGIN_DATADESC( CPushable )
	DEFINE_INPUT( m_iMassOverride, FIELD_INTEGER, "mass" ),
END_DATADESC()
#endif

void CPushable::Spawn( void )
{
	BaseClass::Spawn();

	SetSolid( SOLID_VPHYSICS );

	SetMoveType( MOVETYPE_PUSH );
	SetModel( STRING( GetModelName() ) );

	CreateVPhysics();

	// Force Pushables to stay axially aligned.
	VPhysicsGetObject()->SetInertia( Vector( 1e30, 1e30, 1e30 ) );
}

bool CPushable::CreateVPhysics( void )
{
	VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );

#if 0
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if ( pPhysObj )
	{
		if ( m_iMassOverride )
		{
			pPhysObj->SetMass( m_iMassOverride );
		}
#ifdef HL1_DLL
		Vector vecInertia = Vector(800, 800, 800);
		pPhysObj->SetInertia( vecInertia );
#endif
	}
#endif

	return true;
}

// Pull the func_pushable
void CPushable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( m_spawnflags & SF_PUSH_NO_USE )
		return;

	// Allow pushables to be dragged by player
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if ( pPlayer )
	{
		if ( useType == USE_ON )
		{
// HLS uses ropes, otherwise use legacy code
#ifdef HL1_DLL
			PlayerPickupObject( pPlayer, this );
#else
			// TODO; Pull by default, pull if the player is going forward/holding W
			Move( pActivator, 0 );
#endif
		}
	}

	BaseClass::Use( pActivator, pCaller, useType, value );
}

// OBSOLETE
#if 0
void CPushable :: Touch( CBaseEntity *pOther )
{
	if ( FClassnameIs( pOther->pev, "worldspawn" ) )
		return;

	Move( pOther, 1 );
}
#endif

void CPushable::Move( CBaseEntity *pOther, bool push )
{
	// TODO; This might need to be re-done

	Vector vVelocity;
	int playerTouch = 0;

	// Is entity standing on this pushable ?
	if ( FBitSet(pOther->GetFlags(),FL_ONGROUND) && pOther->GetGroundEntity() == this )
	{
		// Only push if floating
		if ( GetWaterLevel() > 0 )
			vVelocity.z = pOther->GetAbsVelocity().z * 1;

		return;
	}

	if ( pOther->IsPlayer() )
	{
//		if ( push && !(pOther->m_nButtons & (IN_FORWARD|IN_USE)) )	// Don't push unless the player is pushing forward and NOT use (pull)
//			return;
		playerTouch = 1;
	}

	int factor;
	if ( playerTouch )
	{
		if ( !(pOther->GetFlags() & FL_ONGROUND) )	// Don't push away from jumping/falling players unless in water
		{
			if ( GetWaterLevel() < 1 )
				return;
			else 
				factor = 10;
		}
		else
			factor = push ? 75 : 50; //Pulling is a little harder than pushing
	}
	else
		factor = 25;

	vVelocity.x = pOther->GetAbsVelocity().x * factor;
	vVelocity.y = pOther->GetAbsVelocity().y * factor;

	ApplyAbsVelocityImpulse( vVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: Allows us to take damage from physics objects
//-----------------------------------------------------------------------------
void CPushable::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
#ifdef HL1_DLL
	int otherIndex = !index;
	CBaseEntity *pOther = pEvent->pEntities[otherIndex];
	if ( pOther->IsPlayer() )
	{
		// Pushables don't take damage from impacts with the player
		// We call all the way back to the baseclass to get the physics effects.
		CBaseEntity::VPhysicsCollision( index, pEvent );
		return;
	}
#endif

	BaseClass::VPhysicsCollision( index, pEvent );
}

