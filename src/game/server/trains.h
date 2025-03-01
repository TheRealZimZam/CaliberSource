//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TRAINS_H
#define TRAINS_H
#ifdef _WIN32
#pragma once
#endif


#include "entityoutput.h"
#include "pathtrack.h"


// Spawnflags of CPathCorner
#define SF_CORNER_WAITFORTRIG	0x001
#define SF_CORNER_TELEPORT		0x002

//-----------------------------------------------------------------------------
// Purpose: CBasePlatTrain
//-----------------------------------------------------------------------------
#define SF_PLAT_TOGGLE				0x0001

class CBasePlatTrain : public CBaseToggle
{
	DECLARE_CLASS( CBasePlatTrain, CBaseToggle );

public:
	~CBasePlatTrain();
	bool KeyValue( const char *szKeyName, const char *szValue );
	void Precache( void );

	// This is done to fix spawn flag collisions between this class and a derived class
	virtual bool IsTogglePlat( void ) { return (m_spawnflags & SF_PLAT_TOGGLE) ? true : false; }

	DECLARE_DATADESC();

	void	PlayMovingSound();
	void	StopMovingSound();

	string_t	m_NoiseMoving;	// sound a plat makes while moving
	string_t	m_NoiseArrived;

	CSoundPatch *m_pMovementSound;
#ifdef HL1_DLL
	int			m_MoveSound;
	int			m_StopSound;
#endif

	float	m_volume;			// Sound volume
	float	m_flTWidth;
	float	m_flTLength;
};

// Tracktrain spawn flags
#define SF_TRACKTRAIN_NOPITCH					0x0001
#define SF_TRACKTRAIN_NOCONTROL					0x0002
#define SF_TRACKTRAIN_FORWARDONLY				0x0004
#define SF_TRACKTRAIN_PASSABLE					0x0008
#define SF_TRACKTRAIN_FIXED_ORIENTATION			0x0010
#define SF_TRACKTRAIN_HL1TRAIN					0x0080
#define SF_TRACKTRAIN_USE_MAXSPEED_FOR_PITCH	0x0100
#define SF_TRACKTRAIN_UNBLOCKABLE_BY_PLAYER		0x0200
#define SF_TRACKTRAIN_ALLOWROLL					0x0400

enum TrainVelocityType_t
{
        TrainVelocity_Instantaneous = 0,
        TrainVelocity_LinearBlend,
        TrainVelocity_EaseInEaseOut,
};

enum TrainOrientationType_t
{
        TrainOrientation_Fixed = 0,
        TrainOrientation_AtPathTracks,
        TrainOrientation_LinearBlend,
        TrainOrientation_EaseInEaseOut,
};

//-----------------------------------------------------------------------------
// Purpose: CFuncTrackTrain
//-----------------------------------------------------------------------------
class CFuncTrackTrain : public CBasePlatTrain
{
	DECLARE_CLASS( CFuncTrackTrain, CBasePlatTrain );
	DECLARE_SERVERCLASS();

public:
	CFuncTrackTrain();

	void Spawn( void );
	bool CreateVPhysics( void );
	void Precache( void );
	void UpdateOnRemove();
	void MoveDone();

	void Blocked( CBaseEntity *pOther );
	bool KeyValue( const char *szKeyName, const char *szValue );

	virtual int DrawDebugTextOverlays();
	void DrawDebugGeometryOverlays();

	void Next( void );
	void Find( void );
	void NearestPath( void );
	void DeadEnd( void );

	void SetTrack( CPathTrack *track ) { m_ppath = track->Nearest(GetLocalOrigin()); }
	void SetControls( CBaseEntity *pControls );
	bool OnControls( CBaseEntity *pControls );

	void SoundStop( void );
	void SoundUpdate( void );

	void Start( void );
	void Stop( void );

	bool IsDirForward();
	void SetDirForward( bool bForward );
	void SetSpeed( float flSpeed, bool bAccel = false );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	// Input handlers
	void InputSetSpeed( inputdata_t &inputdata );
	void InputSetSpeedDir( inputdata_t &inputdata );
	void InputSetSpeedReal( inputdata_t &inputdata );
	void InputSetMaxSpeed( inputdata_t &inputdata );
	void InputStop( inputdata_t &inputdata );
	void InputStart( inputdata_t &inputdata );
	void InputResume( inputdata_t &inputdata );
	void InputReverse( inputdata_t &inputdata );
	void InputStartForward( inputdata_t &inputdata );
	void InputStartBackward( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );
	void InputSetSpeedDirAccel( inputdata_t &inputdata );
	void InputSetSpeedForwardModifier( inputdata_t &inputdata );
	void InputTeleportToPathTrack( inputdata_t &inputdata );

	static CFuncTrackTrain *Instance( edict_t *pent );

	DECLARE_DATADESC();

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DIRECTIONAL_USE | FCAP_USE_ONGROUND; }

	virtual void	OnRestore( void );

	float GetMaxSpeed() const { return m_maxSpeed; }
	float GetCurrentSpeed() const { return m_flSpeed; }
	float GetDesiredSpeed() const { return m_flDesiredSpeed;}

	virtual bool IsBaseTrain( void ) const { return true; }

	void SetSpeedForwardModifier( float flModifier );
	void SetBlockDamage( float flDamage ) { m_flBlockDamage = flDamage; }
	float GetBlockDamage( void ) {return m_flBlockDamage;}

private:

	void ArriveAtNode( CPathTrack *pNode );
	void FirePassInputs( CPathTrack *pStart, CPathTrack *pEnd, bool forward );

public:

	// UNDONE: Add accessors?
	CPathTrack	*m_ppath;
	float		m_length;
	
#ifdef HL1_DLL	
	bool		m_bOnTrackChange;		// we don't want to find a new node if we restore while 
										// riding on a func_trackchange
#endif

private:

	TrainVelocityType_t GetTrainVelocityType();
	void UpdateTrainVelocity( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );

	TrainOrientationType_t GetTrainOrientationType();
	void UpdateTrainOrientation( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateOrientationAtPathTracks( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateOrientationBlend( TrainOrientationType_t eOrientationType, CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval );
	void DoUpdateOrientation( const QAngle &curAngles, const QAngle &angles, float flInterval );

	void TeleportToPathTrack( CPathTrack *pTeleport );


	Vector		m_controlMins;
	Vector		m_controlMaxs;
	Vector		m_lastBlockPos;				// These are used to build a heuristic decision about being temporarily blocked by physics objects
	int			m_lastBlockTick;			// ^^^^^^^
//	float		m_flVolume;
	float		m_flBank;
	float		m_oldSpeed;
	float		m_flBlockDamage;			// Damage to inflict when blocked.
	float		m_height;
	float		m_maxSpeed;
	float		m_dir;


	string_t	m_iszSoundMove;				// Looping sound to play while moving. Pitch shifted based on speed.
	string_t	m_iszSoundMovePing;			// Ping sound to play while moving. Interval decreased based on speed.
	string_t	m_iszSoundStart;			// Sound to play when starting to move.
	string_t	m_iszSoundStop;				// Sound to play when stopping.

	float		m_flMoveSoundMinTime;		// The most often to play the move 'ping' sound (used at max speed)
	float		m_flMoveSoundMaxTime;		// The least often to play the move 'ping' sound (used approaching zero speed)
	float		m_flNextMoveSoundTime;

	int			m_nMoveSoundMinPitch;		// The sound pitch to approach as we come to a stop
	int			m_nMoveSoundMaxPitch;		// The sound pitch to approach as we approach our max speed (actually, it's hardcoded to 1000 in/sec)

	TrainOrientationType_t m_eOrientationType;
	TrainVelocityType_t m_eVelocityType;
	bool		m_bSoundPlaying;

	COutputEvent m_OnStart,m_OnStop,m_OnNext; 

	bool		m_bManualSpeedChanges;		// set when we want to send entity IO to govern speed and obey our TrainVelocityType_t
	float		m_flDesiredSpeed;			// target speed, when m_bManualSpeedChanges is set
	float		m_flSpeedChangeTime;
	float		m_flAccelSpeed;
	float		m_flDecelSpeed;
	bool		m_bAccelToSpeed;

	float		m_flNextMPSoundTime;
	float		m_flSpeedForwardModifier;
	float		m_flUnmodifiedDesiredSpeed;
};


#endif // TRAINS_H
