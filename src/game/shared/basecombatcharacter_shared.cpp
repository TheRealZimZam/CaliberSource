//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "ammodef.h"
#include "gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Switches to the best weapon that is also better than the given weapon.
// Input  : pCurrent - The current weapon used by the player.
// Output : Returns true if the weapon was switched, false if there was no better
//			weapon to switch to.
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::SwitchToNextBestWeapon(CBaseCombatWeapon *pCurrent)
{
	CBaseCombatWeapon *pNewWeapon = g_pGameRules->GetNextBestWeapon(this, pCurrent);
	
	if ( ( pNewWeapon != NULL ) && ( pNewWeapon != pCurrent ) )
	{
		return Weapon_Switch( pNewWeapon );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Switches to the given weapon (providing it has ammo)
// Input  :
// Output : true is switch suceeded
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ ) 
{
	if ( pWeapon == NULL )
		return false;

	// Already have it out?
	if ( m_hActiveWeapon.Get() == pWeapon )
	{
		if ( !m_hActiveWeapon->IsWeaponVisible() )
			return m_hActiveWeapon->Deploy( );
		return false;
	}

	if (!Weapon_CanSwitchTo(pWeapon))
	{
		return false;
	}

	if ( m_hActiveWeapon )
	{
		if ( !m_hActiveWeapon->Holster( pWeapon ) )
			return false;
	}

	m_hActiveWeapon = pWeapon;
	return pWeapon->Deploy( );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we can switch to the given weapon.
// Input  : pWeapon - 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
	if (IsPlayer())
	{
		CBasePlayer *pPlayer = (CBasePlayer *)this;
#if !defined( CLIENT_DLL )
		IServerVehicle *pVehicle = pPlayer->GetVehicle();
#else
		IClientVehicle *pVehicle = pPlayer->GetVehicle();
#endif
		if (pVehicle && !pPlayer->UsingStandardWeaponsInVehicle())
			return false;
	}

	if ( !pWeapon->HasAnyAmmo() && !GetAmmoCount( pWeapon->m_iPrimaryAmmoType ) )
		return false;

	if ( !pWeapon->CanDeploy() )
		return false;
	
	if ( m_hActiveWeapon )
	{
		if ( !m_hActiveWeapon->CanHolster() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseCombatWeapon
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::GetActiveWeapon() const
{
	return m_hActiveWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Give the player some ammo.
// Input  : iCount - Amount of ammo to give.
//			iAmmoIndex - Index of the ammo into the AmmoInfoArray
//			iMax - Max carrying capability of the player
// Output : Amount of ammo actually given
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound)
{
	if (iCount <= 0)
		return 0;

	if ( !g_pGameRules->CanHaveAmmo( this, iAmmoIndex ) )
	{
		// game rules say I can't have any more of this ammo type.
		return 0;
	}

	if ( iAmmoIndex < 0 || iAmmoIndex >= MAX_AMMO_SLOTS )
		return 0;

	int iMax = GetAmmoDef()->MaxCarry(iAmmoIndex);
	int iAdd = min( iCount, iMax - m_iAmmo[iAmmoIndex] );
	if ( iAdd < 1 )
		return 0;

	// Ammo pickup sound
	if ( !bSuppressSound )
	{
		EmitSound( "BaseCombatCharacter.AmmoPickup" );
	}

	m_iAmmo.Set( iAmmoIndex, m_iAmmo[iAmmoIndex] + iAdd );

	return iAdd;
}

int CBaseCombatCharacter::GiveAmmo( int iCount, const char *szName, bool bSuppressSound )
{
	int iAmmoType = GetAmmoDef()->Index(szName);
	if (iAmmoType == -1)
	{
		Msg("ERROR: Attempting to give unknown ammo type (%s)\n",szName);
		return 0;
	}
	return GiveAmmo( iCount, iAmmoType, bSuppressSound );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iCount - 
//			iAmmoIndex - 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::RemoveAmmo( int iCount, int iAmmoIndex )
{
	if (iCount <= 0)
		return;

	// Infinite ammo?
	if ( GetAmmoDef()->MaxCarry( iAmmoIndex ) == INFINITE_AMMO )
		return;

	// Ammo pickup sound
	m_iAmmo.Set( iAmmoIndex, max( m_iAmmo[iAmmoIndex] - iCount, 0 ) );
}

void CBaseCombatCharacter::RemoveAmmo( int iCount, const char *szName )
{
	RemoveAmmo( iCount, GetAmmoDef()->Index(szName) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::RemoveAllAmmo( )
{
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_iAmmo.Set( i, 0 );
	}
}

//-----------------------------------------------------------------------------
// FIXME: This is a sort of hack back-door only used by physgun!
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetAmmoCount( int iCount, int iAmmoIndex )
{
	// NOTE: No sound, no max check! Seems pretty bogus to me!
	m_iAmmo.Set( iAmmoIndex, iCount );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of a particular type owned
//			owned by the character
// Input  :	Ammo Index
// Output :	The amount of ammo
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GetAmmoCount( int iAmmoIndex ) const
{
	if ( iAmmoIndex == -1 )
		return 0;

	// Infinite ammo?
	if ( GetAmmoDef()->MaxCarry( iAmmoIndex ) == INFINITE_AMMO )
		return 999;

	return m_iAmmo[ iAmmoIndex ];
}

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of the specified type the character's carrying
//-----------------------------------------------------------------------------
int	CBaseCombatCharacter::GetAmmoCount( char *szName ) const
{
	return GetAmmoCount( GetAmmoDef()->Index(szName) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns weapon if already owns a weapon of this class
//-----------------------------------------------------------------------------
CBaseCombatWeapon* CBaseCombatCharacter::Weapon_OwnsThisType( const char *pszWeapon, int iSubType ) const
{
	// Check for duplicates
	for (int i=0;i<MAX_WEAPONS;i++) 
	{
		if ( m_hMyWeapons[i].Get() && FClassnameIs( m_hMyWeapons[i], pszWeapon ) )
		{
			// Make sure it matches the subtype
			if ( m_hMyWeapons[i]->GetSubType() == iSubType )
				return m_hMyWeapons[i];
		}
	}
	return NULL;
}


int CBaseCombatCharacter::BloodColor()
{
	return m_bloodColor;
}


//-----------------------------------------------------------------------------
// Blood color (see BLOOD_COLOR_* macros in baseentity.h)
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetBloodColor( int nBloodColor )
{
	m_bloodColor = nBloodColor;
}



