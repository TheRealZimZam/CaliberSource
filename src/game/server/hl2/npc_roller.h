//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Rollermine - Robotic houndeye
//
//=============================================================================//

#ifndef NPC_ROLLER_H
#define NPC_ROLLER_H
#ifdef _WIN32
#pragma once
#endif

//------------------------------------
// Spawnflags
//------------------------------------
#define SF_ROLLERMINE_FRIENDLY		(1 << 16)
#define SF_ROLLERMINE_PROP_COLLISION		(1 << 17)

//-----------------------------------------------------------------------------
// CRollerController implementation
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: This class only implements the IMotionEvent-specific behavior
//			It keeps track of the forces so they can be integrated
//-----------------------------------------------------------------------------
class CRollerController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:
	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

	AngularImpulse	m_vecAngular;
	Vector			m_vecLinear;

	void Off( void ) { m_fIsStopped = true; }
	void On( void ) { m_fIsStopped = false; }

	bool IsOn( void ) { return !m_fIsStopped; }

private:
	bool	m_fIsStopped;
};

//-----------------------------------------------------------------------------

bool NPC_Rollermine_IsRollermine( CBaseEntity *pEntity );
CBaseEntity *NPC_Rollermine_DropFromPoint( const Vector &originStart, CBaseEntity *pOwner, const char *pszTemplate );

/*
//=========================================================
class CNPC_Roller : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Roller, CAI_BaseNPC );

public:
	~CNPC_Roller( void );
	void	Precache( void );
	void	Spawn( void );
	Class_T Classify( void );

	virtual int TranslateSchedule( int scheduleType );
	virtual int SelectSchedule( void );

	DECLARE_DATADESC();

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );

	bool ShouldSavePhysics()	{ return true; }
	bool IsActivityFinished(void) { return true; }
	void SetActivity( Activity NewActivity ) { };
	virtual void PrescheduleThink( void );

	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );

	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	int VPhysicsTakeDamage( const CTakeDamageInfo &info );

	bool FInViewCone( CBaseEntity *pEntity );

	bool OverrideMove( float flInterval = 0.1 );

	void TaskFail( AI_TaskFailureCode_t code );
	void TaskFail( const char *pszGeneralFailText )	{ TaskFail( MakeFailCode( pszGeneralFailText ) ); }

	void Unstick( void );
	void RemainUpright( void );

	CRollerController			m_RollerController;
	IPhysicsMotionController	*m_pMotionController;

	bool FValidateHintType(CAI_Hint *pHint);

	bool IsBlockedForward( void );
	bool IsBlockedBackward( void );
	bool IsBlocked( const Vector &vecCheck );
	bool m_fHACKJustSpawned;

	void MarcoSound( void );
	void PoloSound( void );

	virtual void PowerOn( void );
	virtual void PowerOff( void );

	virtual void PowerOnSound( void );
	virtual void PowerOffSound( void );

	virtual int RollerPhysicsDamageMask( void );

	Vector m_vecUnstickDirection;

	float m_flLastZPos;
	float m_flTimeMarcoSound;
	float m_flTimePoloSound;
	int m_iFail;
	float m_flForwardSpeed;
	unsigned char m_iAccessCode[ ROLLER_CODE_DIGITS ];
	int m_iCodeProgress;
	CSoundPatch *m_pRollSound;

	DEFINE_CUSTOM_AI;
};
*/
#endif // NPC_ROLLER_H
