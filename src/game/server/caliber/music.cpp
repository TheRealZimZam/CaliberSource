//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Basic entity that controls music better than ambient_generic.
// Can randomly play up to 5 tracks, looping and non-looping, with different weightings.
//
//	TODO; Code this thing!
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "mathlib/mathlib.h"
#include "ai_speech.h"
#include "stringregistry.h"
#include "gamerules.h"
#include "game.h"
#include <ctype.h>
#include "entitylist.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "ndebugoverlay.h"
#include "soundscape.h"
#include "igamesystem.h"
#include "KeyValues.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
#define MUSIC_UPDATE_RATE	5	// update at 5hz
#define MUSIC_THINK_DELAY ( 1.0f / float( MUSIC_UPDATE_RATE ) )

#define MUSIC_NUM_CHANNELS 4 //We need to allocate 4 channels, because we can overlap stereo sounds

// ========================== MUSIC CONTROLLER ====================================
class CMusicController : public CPointEntity
{
public:
	DECLARE_CLASS( CMusicController, CPointEntity );

	bool KeyValue( const char *szKeyName, const char *szValue );
	void Spawn( void );
	void Precache( void );
	void Activate( void );
	void RampThink( void );

	// Rules about which entities need to transmit along with me
	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
	virtual void UpdateOnRemove( void );

	void ToggleMusic();
	void PlayMusic( SoundFlags_t flags );

	// Input handlers
	void InputPlaySound( inputdata_t &inputdata );
	void InputStopSound( inputdata_t &inputdata );
	void InputToggleMusic( inputdata_t &inputdata );

	void InputVolume( inputdata_t &inputdata );
	void InputFadeIn( inputdata_t &inputdata );
	void InputFadeOut( inputdata_t &inputdata );
	void ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	// Outputs
	COutputEvent	m_outputOnNewTrack;	//Each time a new track is selected

	DECLARE_DATADESC();

	float m_flOverlap;
	float m_flVolume;
	soundlevel_t m_iSoundLevel;		// dB value

	bool m_fActive;		// only true when the entity is playing a looping sound
	bool m_fLooping;		// true when the sound played will loop

	int m_ifadein;			// volume fade in time 0 - 100
	int m_ifadeout;		// volume fade out time 0 - 100

	EHANDLE m_hSoundSource;	// entity from which the sound comes
	int		m_nSoundSourceEntIndex; // In case the entity goes away before we finish stopping the sound...

private:
	string_t m_iszSound1;
	int		m_nSound1Weight;

	string_t m_iszSound2;
	int		m_nSound2Weight;

	string_t m_iszSound3;
	int		m_nSound3Weight;

	string_t m_iszSound4;
	int		m_nSound4Weight;

	string_t m_iszSound5;
	int		m_nSound5Weight;

};

LINK_ENTITY_TO_CLASS( env_music, CMusicController );

BEGIN_DATADESC( CMusicController )

	// This is horrible, and im sure theres a better way to do this -M.M
	DEFINE_KEYFIELD( m_iszSound1, FIELD_SOUNDNAME, "music1" ),
	DEFINE_KEYFIELD( m_nSound1Weight, FIELD_INTEGER, "music1weight" ),

	DEFINE_KEYFIELD( m_iszSound2, FIELD_SOUNDNAME, "music2" ),
	DEFINE_KEYFIELD( m_nSound2Weight, FIELD_INTEGER, "music2weight" ),

	DEFINE_KEYFIELD( m_iszSound3, FIELD_SOUNDNAME, "music3" ),
	DEFINE_KEYFIELD( m_nSound3Weight, FIELD_INTEGER, "music3weight" ),

	DEFINE_KEYFIELD( m_iszSound4, FIELD_SOUNDNAME, "music4" ),
	DEFINE_KEYFIELD( m_nSound4Weight, FIELD_INTEGER, "music4weight" ),

	DEFINE_KEYFIELD( m_iszSound5, FIELD_SOUNDNAME, "music5" ),
	DEFINE_KEYFIELD( m_nSound5Weight, FIELD_INTEGER, "music5weight" ),

	DEFINE_FIELD( m_flVolume, FIELD_FLOAT ),
	DEFINE_FIELD( m_flOverlap, FIELD_FLOAT ),
	DEFINE_FIELD( m_fActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fLooping, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iSoundLevel, FIELD_INTEGER ),

	// HACKHACK - This is not really in the spirit of the save/restore design, but save this
	// out as a binary data block.  If the dynpitchvol_t is changed, old saved games will NOT
	// load these correctly, so bump the save/restore version if you change the size of the struct
	// The right way to do this is to split the input parms (read in keyvalue) into members and re-init this
	// struct in Precache(), but it's unlikely that the struct will change, so it's not worth the time right now.
	DEFINE_ARRAY( m_dpv, FIELD_CHARACTER, sizeof(dynpitchvol_t) ),

	// Function Pointers
	DEFINE_FUNCTION( RampThink ),

	// Inputs
	DEFINE_INPUTFUNC(FIELD_VOID, "PlaySound", InputPlaySound ),
	DEFINE_INPUTFUNC(FIELD_VOID, "StopSound", InputStopSound ),
	DEFINE_INPUTFUNC(FIELD_VOID, "ToggleSound", InputToggleMusic ),

	// Outputs
	DEFINE_OUTPUT( m_outputOnNewTrack, "OnNewTrack" ),

END_DATADESC()

#define SF_MUSIC_START_SILENT				16
#define SF_MUSIC_NOT_LOOPING				32
#define SF_MUSIC_PLAY_FIRST_TRACK			64
#define SF_MUSIC_LOWER_VOLUME_ON_DIALOUGE	128


void CMusicController::Precache( void )
{
	char *szSoundFile = (char *)STRING( m_iszSound1 );
	if ( m_iszSound1 != NULL_STRING && strlen( szSoundFile ) > 1 )
	{
		if (*szSoundFile != '!')
		{
			PrecacheScriptSound(szSoundFile);
		}
	}

	if ( !FBitSet (m_spawnflags, SF_MUSIC_START_SILENT ) )
	{
		// start the sound ASAP
		if (m_fLooping)
			m_fActive = true;
	}

	m_flVolume = m_iHealth * 10;
	if (m_flVolume > 100) m_flVolume = 100;
	if (m_flVolume < 0) m_flVolume = 0;

	m_iSoundLevel = SNDLVL_NONE;
	m_flOverlap = 0.1;	//Default overlap between tracks
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CMusicController::Spawn( void )
{
	Precache( );

	char *szSoundFile = (char *)STRING( m_iszSound1 );
	if ( !m_iszSound1 || strlen( szSoundFile ) < 1 )
	{
		Warning( "Empty %s (%s) at %.2f, %.2f, %.2f\n", GetClassname(), GetDebugName(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove(this);
		return;
	}

    SetSolid( SOLID_NONE );
    SetMoveType( MOVETYPE_NONE );

	// Set up think function for dynamic modification 
	// of ambient sound's pitch or volume. Don't
	// start thinking yet.

	SetThink(&CMusicController::RampThink);
	SetNextThink( TICK_NEVER_THINK );
	SetUse( &CMusicController::ToggleUse );

	m_fActive = false;

	if ( FBitSet ( m_spawnflags, SF_MUSIC_NOT_LOOPING ) )
		m_fLooping = false;
	else
		m_fLooping = true;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for changing volume.
// Input  : Float new volume, from 0 - 10.
//-----------------------------------------------------------------------------
void CMusicController::InputVolume( inputdata_t &inputdata )
{
	//
	// Multiply the input value by ten since volumes are expected to be from 0 - 100.
	//
	m_dpv.vol = clamp( inputdata.value.Float(), 0, 10 ) * 10;
	m_dpv.volfrac = m_dpv.vol << 8;

	PlayMusic( SND_CHANGE_VOL );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for fading in volume over time.
// Input  : Float volume fade in time 0 - 100 seconds
//-----------------------------------------------------------------------------
void CMusicController::InputFadeIn( inputdata_t &inputdata )
{
	// cancel any fade out that might be happening
	m_dpv.fadeout = 0;

	m_dpv.fadein = inputdata.value.Float();
	if (m_dpv.fadein > 100) m_dpv.fadein = 100;
	if (m_dpv.fadein < 0) m_dpv.fadein = 0;

	if (m_dpv.fadein > 0)
		m_dpv.fadein = ( 100 << 8 ) / ( m_dpv.fadein * MUSIC_UPDATE_RATE );

	SetNextThink( gpGlobals->curtime + 0.1f );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for fading out volume over time.
// Input  : Float volume fade out time 0 - 100 seconds
//-----------------------------------------------------------------------------
void CMusicController::InputFadeOut( inputdata_t &inputdata )
{
	// cancel any fade in that might be happening
	m_dpv.fadein = 0;

	m_dpv.fadeout = inputdata.value.Float();

	if (m_dpv.fadeout > 100) m_dpv.fadeout = 100;
	if (m_dpv.fadeout < 0) m_dpv.fadeout = 0;

	if (m_dpv.fadeout > 0)
		m_dpv.fadeout = ( 100 << 8 ) / ( m_dpv.fadeout * MUSIC_UPDATE_RATE );

	SetNextThink( gpGlobals->curtime + 0.1f );
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CMusicController::Activate( void )
{
	BaseClass::Activate();

	// Initialize sound source.  If no source was given, or source can't be found
	// then this is the source
	m_hSoundSource = this;
	m_nSoundSourceEntIndex = entindex();

	// If active start the sound
	if ( m_fActive )
	{
		int flags = SND_SPAWNING;
		// If we are loading a saved game, we can't write into the init/signon buffer here, so just issue
		//  as a regular sound message...
		if ( gpGlobals->eLoadType == MapLoad_Transition ||
			 gpGlobals->eLoadType == MapLoad_LoadGame || 
			 g_pGameRules->InRoundRestart() )
		{
			flags = SND_NOFLAGS;
		}
	
		// Tracker 76119:  8/12/07 ywb: 
		//  Make sure pitch and volume are set up to the correct value (especially after restoring a .sav file)
		flags |= ( SND_CHANGE_PITCH | SND_CHANGE_VOL );  

		// Don't bother sending over to client if volume is zero, though
		if ( m_dpv.vol > 0 )
		{
			PlayMusic( (SoundFlags_t)flags );
		}

		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}


//-----------------------------------------------------------------------------
// Rules about which entities need to transmit along with me
//-----------------------------------------------------------------------------
void CMusicController::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Ambient generics never transmit; this is just a way for us to ensure
	// the sound source gets transmitted; that's why we don't call pInfo->m_pTransmitEdict->Set
	if ( !m_hSoundSource || m_hSoundSource == this || !m_fActive )
		return;

	// Don't bother sending the position of the source if we have to play everywhere
	if ( FBitSet( m_spawnflags, SF_AMBIENT_SOUND_EVERYWHERE ) )
		return;

	Assert( pInfo->m_pClientEnt );
	CBaseEntity *pClient = (CBaseEntity*)(pInfo->m_pClientEnt->GetUnknown());
	if ( !pClient )
		return;

	// Send the sound source if he's close enough
	if ( ( m_flMaxRadius < 0 ) || ( pClient->GetAbsOrigin().DistToSqr( m_hSoundSource->GetAbsOrigin() ) <= m_flMaxRadius * m_flMaxRadius ) )
	{
		m_hSoundSource->SetTransmit( pInfo, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMusicController::UpdateOnRemove( void )
{
	if ( m_fActive )
	{
		// Stop the sound we're generating
		PlayMusic( SND_STOP );
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Think at 5hz if we are dynamically modifying pitch or volume of the
//			playing sound.  This function will ramp pitch and/or volume up or
//			down, modify pitch/volume with lfo if active.
//-----------------------------------------------------------------------------
void CMusicController::RampThink( void )
{
	int pitch = m_dpv.pitch; 
	int vol = m_dpv.vol;
	int flags = 0;
	int fChanged = 0;		// false if pitch and vol remain unchanged this round
	int	prev;

	if (!m_dpv.fadein && !m_dpv.fadeout)
		return;						// no ramps or lfo, stop thinking

	// ==================
	// amplitude envelope
	// ==================
	if (m_dpv.fadein || m_dpv.fadeout)
	{
		prev = m_dpv.volfrac >> 8;

		if (m_dpv.fadein > 0)
			m_dpv.volfrac += m_dpv.fadein;
		else if (m_dpv.fadeout > 0)
			m_dpv.volfrac -= m_dpv.fadeout;

		vol = m_dpv.volfrac >> 8;

		if (vol > m_flVolume)
		{
			vol = m_flVolume;
			m_dpv.volfrac = vol << 8;
			m_dpv.fadein = 0;				// done with ramp up
		}

		if (vol < m_dpv.volstart)
		{
			vol = m_dpv.volstart;
			m_dpv.vol = vol;
			m_dpv.volfrac = vol << 8;
			m_dpv.fadeout = 0;				// done with ramp down
			
			// shut sound off
			PlayMusic( SND_STOP );
			
			// return without setting m_flNextThink
			return;
		}

		if (vol > 100) 
		{
			vol = 100;
			m_dpv.volfrac = vol << 8;
		}
		if (vol < 1) 
		{
			vol = 1;
			m_dpv.volfrac = vol << 8;
		}

		m_dpv.vol = vol;

		fChanged |= (prev != vol);
		flags |= SND_CHANGE_VOL;
	}

	// Send update to playing sound only if we actually changed
	// pitch or volume in this routine.
	if (flags && fChanged) 
	{
		if (pitch == PITCH_NORM)
			pitch = PITCH_NORM + 1; // don't send 'no pitch' !

		CBaseEntity* pSoundSource = m_hSoundSource;
		if (pSoundSource)
		{
			UTIL_EmitAmbientSound(pSoundSource->GetSoundSourceIndex(), pSoundSource->GetAbsOrigin(), 
				STRING( m_iszSound1 ), (vol * 0.01), m_iSoundLevel, flags, pitch);
		}
	}

	// update ramps at 5hz
	SetNextThink( gpGlobals->curtime + MUSIC_THINK_DELAY );
	return;
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that begins playing the sound.
//-----------------------------------------------------------------------------
void CMusicController::InputPlaySound( inputdata_t &inputdata )
{
	if (!m_fActive)
	{
		//Adrian: Stop our current sound before starting a new one!
		PlayMusic( SND_STOP ); 

		ToggleMusic();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that stops playing the sound.
//-----------------------------------------------------------------------------
void CMusicController::InputStopSound( inputdata_t &inputdata )
{
	if (m_fActive)
	{
		ToggleMusic();
	}
}

void CMusicController::PlayMusic( SoundFlags_t flags)
{
	char *szSoundFile = (char *)STRING( m_iszSound1 );
	CBaseEntity* pSoundSource = m_hSoundSource;
	if ( pSoundSource )
	{
		if ( flags == SND_STOP )
		{
			UTIL_EmitAmbientSound(pSoundSource->GetSoundSourceIndex(), pSoundSource->GetAbsOrigin(), szSoundFile, 
						0, SNDLVL_NONE, flags, 0);
			m_fActive = false;
		}
		else
		{
			UTIL_EmitAmbientSound(pSoundSource->GetSoundSourceIndex(), pSoundSource->GetAbsOrigin(), szSoundFile, 
				(m_dpv.vol * 0.01), m_iSoundLevel, flags, m_dpv.pitch);

			// Only mark active if this is a looping sound.
			// If not looping, each trigger will cause the sound to play.
			// If the sound is still playing from a previous trigger press, 
			// it will be shut off and then restarted.
			if ( m_fLooping )
				m_fActive = true;
		}
	}	
	else
	{
		if ( ( flags == SND_STOP ) && 
			( m_nSoundSourceEntIndex != -1 ) )
		{
			UTIL_EmitAmbientSound(m_nSoundSourceEntIndex, GetAbsOrigin(), szSoundFile, 
					0, SNDLVL_NONE, flags, 0);
			m_fActive = false;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that stops playing the sound.
//-----------------------------------------------------------------------------
void CMusicController::InputToggleMusic( inputdata_t &inputdata )
{
	ToggleMusic();
}

void CMusicController::ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_fActive ) )
		return;

	// Toggle
	ToggleMusic();
}

//-----------------------------------------------------------------------------
// Purpose: Turns an ambient sound on or off.  If the ambient is a looping sound,
//			mark sound as active (m_fActive) if it's playing, innactive if not.
//			If the sound is not a looping sound, never mark it as active.
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CMusicController::ToggleMusic()
{
	// m_fActive is true only if a looping sound is playing.
	
	if ( m_fActive )
	{
		if (m_dpv.cspinup)
		{
			// Don't actually shut off. Each toggle causes
			// incremental spinup to max pitch

			if (m_dpv.cspincount <= m_dpv.cspinup)
			{	
				int pitchinc;

				// start a new spinup
				m_dpv.cspincount++;
				
				pitchinc = (255 - m_dpv.pitchstart) / m_dpv.cspinup;

				m_dpv.spinup = m_dpv.spinupsav;
				m_dpv.spindown = 0;

				m_dpv.pitchrun = m_dpv.pitchstart + pitchinc * m_dpv.cspincount;
				if (m_dpv.pitchrun > 255) m_dpv.pitchrun = 255;

				SetNextThink( gpGlobals->curtime + 0.1f );
			}
			
		}
		else
		{
			m_fActive = false;
			
			// HACKHACK - this makes the code in Precache() work properly after a save/restore
			m_spawnflags |= SF_MUSIC_START_SILENT;

			if (m_dpv.spindownsav || m_dpv.fadeoutsav)
			{
				// spin it down (or fade it) before shutoff if spindown is set
				m_dpv.spindown = m_dpv.spindownsav;
				m_dpv.spinup = 0;

				m_dpv.fadeout = m_dpv.fadeoutsav;
				m_dpv.fadein = 0;
				SetNextThink( gpGlobals->curtime + 0.1f );
			}
			else
			{
				PlayMusic( SND_STOP ); // stop sound
			}
		}
	}
	else 
	{
		// only toggle if this is a looping sound.  If not looping, each
		// trigger will cause the sound to play.  If the sound is still
		// playing from a previous trigger press, it will be shut off
		// and then restarted.

		if (m_fLooping)
			m_fActive = true;
		else
		{
			// shut sound off now - may be interrupting a long non-looping sound
			PlayMusic( SND_STOP ); // stop sound
		}

		PlayMusic( SND_NOFLAGS ); // send sound

		SetNextThink( gpGlobals->curtime + 0.1f );

	} 
}


// KeyValue - load keyvalue pairs into member data of the
// ambient generic. NOTE: called BEFORE spawn!
bool CMusicController::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "overlap"))
	{
		m_flOverlap = atoi(szValue);
	}
	else if (FStrEq(szKeyName, "fadeinsecs"))
	{
		m_dpv.fadein = atoi(szValue);

		if (m_dpv.fadein > 100) m_dpv.fadein = 100;
		if (m_dpv.fadein < 0) m_dpv.fadein = 0;

		if (m_dpv.fadein > 0)
			m_dpv.fadein = ( 100 << 8 ) / ( m_dpv.fadein * MUSIC_UPDATE_RATE );
		m_dpv.fadeinsav = m_dpv.fadein;
	}
	else if (FStrEq(szKeyName, "fadeoutsecs"))
	{
		m_dpv.fadeout = atoi(szValue);

		if (m_dpv.fadeout > 100) m_dpv.fadeout = 100;
		if (m_dpv.fadeout < 0) m_dpv.fadeout = 0;

		if (m_dpv.fadeout > 0)
			m_dpv.fadeout = ( 100 << 8 ) / ( m_dpv.fadeout * MUSIC_UPDATE_RATE );
		m_dpv.fadeoutsav = m_dpv.fadeout;
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}


void UTIL_RestartMusic( void )
{
	CMusicController *pAmbient = NULL;
	while ( ( pAmbient = (CMusicController*) gEntList.FindEntityByClassname( pAmbient, "env_music" ) ) != NULL )
	{
		if (pAmbient->m_fActive )
		{
			if ( strstr( STRING( pAmbient->m_iszSound1 ), "mp3" ) )
			{
				pAmbient->PlayMusic( SND_CHANGE_VOL ); // fake a change, so we don't create 2 sounds
			}
			pAmbient->PlayMusic( SND_CHANGE_VOL ); // fake a change, so we don't create 2 sounds
		}
	}
}

