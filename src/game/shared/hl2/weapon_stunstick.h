//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Stunstick
//
//=============================================================================//

#ifndef WEAPON_STUNSTICK_H
#define WEAPON_STUNSTICK_H
#ifdef _WIN32
#pragma once
#endif

#include "basebludgeonweapon_shared.h"

#define	STUNSTICK_RANGE		75.0f
#define	STUNSTICK_REFIRE	0.6f

#ifdef CLIENT_DLL
#define CWeaponStunStick C_WeaponStunStick
#endif
class CWeaponStunStick : public CBaseBludgeonWeapon
{
	DECLARE_CLASS( CWeaponStunStick, CBaseBludgeonWeapon );
public:

	CWeaponStunStick();

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

#ifdef CLIENT_DLL
	virtual int				DrawModel( int flags );
	virtual void			ClientThink( void );
	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual RenderGroup_t	GetRenderGroup( void );
	virtual void			ViewModelDrawn( C_BaseViewModel *pBaseViewModel );
#endif

	virtual void Precache();

	void		Spawn();

	float		GetRange( void )		{ return STUNSTICK_RANGE; }
	float		GetFireRate( void )		{ return STUNSTICK_REFIRE; }

	bool		Deploy( void );
	bool		Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	
	void		Drop( const Vector &vecVelocity );
	void		ImpactEffect( trace_t &traceHit );
	void		SecondaryAttack( void )	{}
	void		SetStunState( bool state );
	bool		GetStunState( void );

	float		GetDamageForActivity( Activity hitActivity );
#ifndef CLIENT_DLL
	int			WeaponMeleeAttack1Condition( float flDot, float flDist );
	void		Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	bool		CanBePickedUpByNPCs( void ) { return false;	}		
#endif
private:

#ifdef CLIENT_DLL
	#define	NUM_BEAM_ATTACHMENTS	9
	struct stunstickBeamInfo_t
	{
		int IDs[2];		// 0 - top, 1 - bottom
	};

	stunstickBeamInfo_t		m_BeamAttachments[NUM_BEAM_ATTACHMENTS];	// Lookup for arc attachment points on the head of the stick
	int						m_BeamCenterAttachment;						// "Core" of the effect (center of the head)

	void	SetupAttachmentPoints( void );
	void	DrawFirstPersonEffects( void );
	void	DrawThirdPersonEffects( void );
	void	DrawEffects( void );
	bool	InSwing( void );

	bool	m_bSwungLastFrame;

	#define	FADE_DURATION	0.25f

	float	m_flFadeTime;

	//Tony; third person check thing, this has to be done for the local player if third person switches, so we can re-calc attachment points.
	virtual void			ThirdPersonSwitch( bool bThirdPerson );

#endif

	CNetworkVar( bool, m_bActive );
};

#endif // WEAPON_STUNSTICK_H
