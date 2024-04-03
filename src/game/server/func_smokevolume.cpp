//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "baseparticleentity.h"
#include "sendproxy.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFuncSmokeVolume : public CBaseParticleEntity
{
public:
	DECLARE_CLASS( CFuncSmokeVolume, CBaseParticleEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CFuncSmokeVolume();
	void Spawn();
	void Activate( void );

	// Set the times it fades out at.
	void SetDensity( float density );

	// Inputs
	void InputDisable( inputdata_t &inputdata );
	void InputEnable( inputdata_t &inputdata );

private:
	CNetworkVar( color32, m_Color1 );
	CNetworkVar( color32, m_Color2 );
	CNetworkString( m_MaterialName, 255 );
	string_t m_String_tMaterialName;
	CNetworkVar( float, m_ParticleDrawWidth );
	CNetworkVar( float, m_ParticleSpacingDistance );
	CNetworkVar( float, m_DensityRampSpeed );
	CNetworkVar( float, m_RotationSpeed );
	CNetworkVar( float, m_MovementSpeed );
	CNetworkVar( float, m_DesiredDensity );
	CNetworkVar( bool, m_bStartDisabled );
	float m_BaseDensity;	//Store the base density on the server for enable/disable
};

BEGIN_DATADESC( CFuncSmokeVolume )

	DEFINE_KEYFIELD( m_bStartDisabled,	FIELD_BOOLEAN,	"StartDisabled" ),

	// Save/restore Keyvalue fields
	DEFINE_KEYFIELD( m_Color1, FIELD_COLOR32, "Color1" ),
	DEFINE_KEYFIELD( m_Color2, FIELD_COLOR32, "Color2" ),
//	DEFINE_ARRAY( m_MaterialName, FIELD_STRING, 255 ),
	DEFINE_KEYFIELD( m_String_tMaterialName, FIELD_STRING, "Material" ),
	DEFINE_KEYFIELD( m_ParticleDrawWidth, FIELD_FLOAT, "ParticleDrawWidth" ),
	DEFINE_KEYFIELD( m_ParticleSpacingDistance, FIELD_FLOAT, "ParticleSpacingDistance" ),
	DEFINE_KEYFIELD( m_DensityRampSpeed, FIELD_FLOAT, "DensityRampSpeed" ),
	DEFINE_KEYFIELD( m_RotationSpeed, FIELD_FLOAT, "RotationSpeed" ),
	DEFINE_KEYFIELD( m_MovementSpeed, FIELD_FLOAT, "MovementSpeed" ),
	DEFINE_KEYFIELD( m_DesiredDensity, FIELD_FLOAT, "Density" ),
	DEFINE_FIELD( m_BaseDensity, FIELD_FLOAT ),
	// inputs
	DEFINE_INPUT( m_RotationSpeed, FIELD_FLOAT, "SetRotationSpeed"),
	DEFINE_INPUT( m_MovementSpeed, FIELD_FLOAT, "SetMovementSpeed"),
	DEFINE_INPUT( m_DesiredDensity, FIELD_FLOAT, "SetDensity"),

	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),

END_DATADESC()



IMPLEMENT_SERVERCLASS_ST( CFuncSmokeVolume, DT_FuncSmokeVolume )
	SendPropInt( SENDINFO( m_Color1 ), 32, SPROP_UNSIGNED, SendProxy_Color32ToInt ),
	SendPropInt( SENDINFO( m_Color2 ), 32, SPROP_UNSIGNED, SendProxy_Color32ToInt ),
	SendPropString( SENDINFO( m_MaterialName ) ),
	SendPropFloat( SENDINFO( m_ParticleDrawWidth ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_ParticleSpacingDistance ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_DensityRampSpeed ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_RotationSpeed ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_MovementSpeed ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_DesiredDensity ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO(m_spawnflags), 8, SPROP_UNSIGNED )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( func_smokevolume, CFuncSmokeVolume );

CFuncSmokeVolume::CFuncSmokeVolume()
{
	m_DesiredDensity = 1.0f;
	m_BaseDensity = m_DesiredDensity;
}

// -------------------------------------------------------------------------
void CFuncSmokeVolume::Spawn()
{
	memset( m_MaterialName.GetForModify(), 0, sizeof( m_MaterialName ) );

	// Bind to our bmodel.
	SetModel( STRING( GetModelName() ) );

	BaseClass::Spawn();
}

void CFuncSmokeVolume::Activate( void )
{
	BaseClass::Activate();
	Q_strncpy( m_MaterialName.GetForModify(), STRING( m_String_tMaterialName ), 255 );
}

// -------------------------------------------------------------------------
void CFuncSmokeVolume::SetDensity( float density )
{
	m_DesiredDensity = density;
	m_BaseDensity = m_DesiredDensity;
}

void CFuncSmokeVolume::InputDisable( inputdata_t &inputdata )
{
	// Put density to 0
	m_DesiredDensity = 0;
}

void CFuncSmokeVolume::InputEnable( inputdata_t &inputdata )
{
	// Put density back to the base
	m_DesiredDensity = m_BaseDensity;
}
