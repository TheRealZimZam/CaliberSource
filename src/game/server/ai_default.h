//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Default schedules.
//
//=============================================================================//

#ifndef AI_DEFAULT_H
#define AI_DEFAULT_H
#ifdef _WIN32
#pragma once
#endif

//=========================================================
// These are the schedule types
//=========================================================
enum 
{
	SCHED_NONE = 0,
	SCHED_IDLE_STAND,
	SCHED_IDLE_WALK,
	SCHED_IDLE_WANDER,
	SCHED_WAKE_ANGRY,
	SCHED_ALERT_FACE,
	SCHED_ALERT_FACE_BESTSOUND,
	SCHED_ALERT_REACT_TO_COMBAT_SOUND,
	SCHED_ALERT_SCAN,
	SCHED_ALERT_STAND,
	SCHED_ALERT_WALK,
	SCHED_ALERT_SMALL_FLINCH,
	SCHED_INVESTIGATE_SOUND,
	SCHED_COMBAT_FACE,	// Face the enemy
	SCHED_COMBAT_SWEEP,	// Look around the area
	SCHED_FEAR_FACE,
	SCHED_COMBAT_STAND,
	SCHED_COMBAT_WALK,
	SCHED_CHASE_ENEMY,
	SCHED_CHASE_ENEMY_FAILED,
	SCHED_VICTORY_DANCE,	// Give the enemy a complimentary Tea-Bag
	SCHED_TARGET_FACE,
	SCHED_TARGET_CHASE,
	SCHED_SMALL_FLINCH,	// Legacy small flinch (small flinching is usually done through gestures)
	SCHED_BIG_FLINCH,	// Big flinch (always full animation)
	SCHED_BACK_AWAY_FROM_ENEMY,
	SCHED_MOVE_AWAY_FROM_ENEMY,
	SCHED_BACK_AWAY_FROM_SAVE_POSITION,	// Savepostion is acquired similar to other path types
	SCHED_TAKE_COVER_FROM_ENEMY,	// TAKE COVER; move to the closest place that the enemy cannot sea
	SCHED_TAKE_COVER_FROM_BEST_SOUND,	// TAKE COVER; move to the closest place that the current sound target cannot see
	SCHED_FLEE_FROM_BEST_SOUND,	// Fallback case of take_cover_from_sound
	SCHED_TAKE_COVER_FROM_ORIGIN,	// TAKE COVER; move to the closest place that your current location cannot see
	SCHED_FAIL_TAKE_COVER,
	SCHED_RUN_FROM_ENEMY,
	SCHED_RUN_FROM_ENEMY_FALLBACK,
	SCHED_MOVE_TO_WEAPON_RANGE,
	SCHED_ESTABLISH_LINE_OF_FIRE,
	SCHED_ESTABLISH_LINE_OF_FIRE_FALLBACK,
	SCHED_PRE_FAIL_ESTABLISH_LINE_OF_FIRE,
	SCHED_FAIL_ESTABLISH_LINE_OF_FIRE,
	SCHED_SHOOT_ENEMY_COVER,	// Mostly the same as range_attack1???
	SCHED_COWER, // usually a last resort!
	SCHED_STUNNED, // Knockdown from damage-different than a flinch
	SCHED_MELEE_ATTACK1,
	SCHED_MELEE_ATTACK2,
	SCHED_RANGE_ATTACK1,
	SCHED_RANGE_ATTACK2,
	SCHED_SPECIAL_ATTACK1,
	SCHED_SPECIAL_ATTACK2,
	SCHED_STANDOFF,	// Wait in the current postion for a few seconds
	SCHED_ARM_WEAPON,
	SCHED_DISARM_WEAPON,
	SCHED_HIDE_AND_RELOAD,
	SCHED_RELOAD,
	SCHED_AMBUSH,	// Wait in the current position until enemy comes into view
	SCHED_EVADE,	// Move/jump to the left or right, to dodge attacks
	SCHED_DIE,
	SCHED_DIE_RAGDOLL,
	SCHED_WAIT_FOR_SCRIPT,
	SCHED_AISCRIPT,
	SCHED_SCRIPTED_WALK,
	SCHED_SCRIPTED_RUN,
	SCHED_SCRIPTED_CUSTOM_MOVE,
	SCHED_SCRIPTED_WAIT,
	SCHED_SCRIPTED_FACE,
	SCHED_SCENE_GENERIC,
	SCHED_NEW_WEAPON,
	SCHED_NEW_WEAPON_CHEAT,
	SCHED_SWITCH_TO_PENDING_WEAPON,
	SCHED_GET_HEALTHKIT,
	SCHED_WAIT_FOR_SPEAK_FINISH,

	SCHED_MOVE_AWAY,
	SCHED_MOVE_AWAY_FAIL,
	SCHED_MOVE_AWAY_END,
	SCHED_FORCED_GO,
	SCHED_FORCED_GO_RUN,
	SCHED_NPC_FREEZE,
	SCHED_PATROL_WALK,
	SCHED_COMBAT_PATROL,	// Walk around randomly until spotted an enemy
	SCHED_PATROL_RUN,
	SCHED_RUN_RANDOM,	// Run to the nearest node
	SCHED_FALL_TO_GROUND,
	SCHED_DROPSHIP_DUSTOFF,

	SCHED_FLINCH_PHYSICS,	// Same as small_flinch but for physics damage

	SCHED_FAIL,
	SCHED_FAIL_NOSTOP,

	SCHED_RUN_FROM_ENEMY_MOB,

	SCHED_BURNING_RUN,
	SCHED_BURNING_STAND,

	SCHED_DUCK_DODGE,	// Dodge anim

	SCHED_INTERACTION_MOVE_TO_PARTNER,
	SCHED_INTERACTION_WAIT_FOR_PARTNER,

	SCHED_SLEEP,

	// ======================================
	// IMPORTANT: This must be the last enum
	// ======================================
	LAST_SHARED_SCHEDULE

};

#endif // AI_DEFAULT_H
