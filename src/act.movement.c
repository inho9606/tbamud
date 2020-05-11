/**************************************************************************
*  File: act.movement.c                                    Part of tbaMUD *
*  Usage: Movement commands, door handling, & sleep/rest/etc state.       *
*                                                                         *
*  All rights reserved.  See license complete information.                *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "house.h"
#include "constants.h"
#include "dg_scripts.h"
#include "act.h"
#include "fight.h"
#include "oasis.h" /* for buildwalk */


/* local only functions */
/* do_simple_move utility functions */
static int has_boat(struct char_data *ch);
/* do_gen_door utility functions */
static int find_door(struct char_data *ch, const char *type, char *dir, const char *cmdname);
static int has_key(struct char_data *ch, obj_vnum key);
static void do_doorcmd(struct char_data *ch, struct obj_data *obj, int door, int scmd);
static int ok_pick(struct char_data *ch, obj_vnum keynum, int pickproof, int scmd);


/* simple function to determine if char can walk on water */
static int has_boat(struct char_data *ch)
{
  struct obj_data *obj;
  int i;

  if (GET_LEVEL(ch) > LVL_IMMORT)
    return (1);

  if (AFF_FLAGGED(ch, AFF_WATERWALK) || AFF_FLAGGED(ch, AFF_FLYING))
    return (1);

  /* non-wearable boats in inventory will do it */
  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_BOAT && (find_eq_pos(ch, obj, NULL) < 0))
      return (1);

  /* and any boat you're wearing will do it too */
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_BOAT)
      return (1);

  return (0);
}

/* Simple function to determine if char can fly. */
static int has_flight(struct char_data *ch)
{
  struct obj_data *obj;
  int i;

  if (GET_LEVEL(ch) > LVL_IMMORT)
    return (1);

  if (AFF_FLAGGED(ch, AFF_FLYING))
    return (1);

  /* Non-wearable flying items in inventory will do it. */
  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (OBJAFF_FLAGGED(obj, AFF_FLYING) && OBJAFF_FLAGGED(obj, AFF_FLYING))
      return (1);

  /* Any equipped objects with AFF_FLYING will do it too. */
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && OBJAFF_FLAGGED(GET_EQ(ch, i), AFF_FLYING))
      return (1);

  return (0);
}

/* Simple function to determine if char can scuba. */
static int has_scuba(struct char_data *ch)
{
  struct obj_data *obj;
  int i;

  if (GET_LEVEL(ch) > LVL_IMMORT)
    return (1);

  if (AFF_FLAGGED(ch, AFF_SCUBA))
    return (1);

  /* Non-wearable scuba items in inventory will do it. */
  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (OBJAFF_FLAGGED(obj, AFF_SCUBA) && (find_eq_pos(ch, obj, NULL) < 0))
      return (1);

  /* Any equipped objects with AFF_SCUBA will do it too. */
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && OBJAFF_FLAGGED(GET_EQ(ch, i), AFF_SCUBA))
      return (1);

  return (0);
}

/** Move a PC/NPC character from their current location to a new location. This
 * is the standard movement locomotion function that all normal walking
 * movement by characters should be sent through. This function also defines
 * the move cost of normal locomotion as:
 * ( (move cost for source room) + (move cost for destination) ) / 2
 *
 * @pre Function assumes that ch has no master controlling character, that
 * ch has no followers (in other words followers won't be moved by this
 * function) and that the direction traveled in is one of the valid, enumerated
 * direction.
 * @param ch The character structure to attempt to move.
 * @param dir The defined direction (NORTH, SOUTH, etc...) to attempt to
 * move into.
 * @param need_specials_check If TRUE will cause 
 * @retval int 1 for a successful move (ch is now in a new location)		
 * or 0 for a failed move (ch is still in the original location). */
int do_simple_move(struct char_data *ch, int dir, int need_specials_check)
{
  /* Begin Local variable definitions */
  /*---------------------------------------------------------------------*/
  /* Used in our special proc check. By default, we pass a NULL argument
   * when checking for specials */
  char spec_proc_args[MAX_INPUT_LENGTH] = "";
  /* The room the character is currently in and will move from... */
  room_rnum was_in = IN_ROOM(ch);
  /* ... and the room the character will move into. */
  room_rnum going_to = EXIT(ch, dir)->to_room;
  /* How many movement points are required to travel from was_in to going_to.
   * We redefine this later when we need it. */
  int need_movement = 0;
  /* Contains the "leave" message to display to the was_in room. */
  char leave_message[SMALL_BUFSIZE];
  /*---------------------------------------------------------------------*/
  /* End Local variable definitions */


  /* Begin checks that can prevent a character from leaving the was_in room. */
  /* Future checks should be implemented within this section and return 0.   */
  /*---------------------------------------------------------------------*/
  /* Check for special routines that might activate because of the move and
   * also might prevent the movement. Special requires commands, so we pass
   * in the "command" equivalent of the direction (ie. North is '1' in the
   * command list, but NORTH is defined as '0').
   * Note -- only check if following; this avoids 'double spec-proc' bug */
  if (need_specials_check && special(ch, dir + 1, spec_proc_args))
    return 0;

  /* Leave Trigger Checks: Does a leave trigger block exit from the room? */
  if (!leave_mtrigger(ch, dir) || IN_ROOM(ch) != was_in) /* prevent teleport crashes */
    return 0;
  if (!leave_wtrigger(&world[IN_ROOM(ch)], ch, dir) || IN_ROOM(ch) != was_in) /* prevent teleport crashes */
    return 0;
  if (!leave_otrigger(&world[IN_ROOM(ch)], ch, dir) || IN_ROOM(ch) != was_in) /* prevent teleport crashes */
    return 0;

  /* Charm effect: Does it override the movement? */
  if (AFF_FLAGGED(ch, AFF_CHARM) && ch->master && was_in == IN_ROOM(ch->master))
  {
    send_to_char(ch, "The thought of leaving your master makes you weep.\r\n");
    act("$n bursts into tears.", FALSE, ch, 0, 0, TO_ROOM);
    return (0);
  }

  /* Water, No Swimming Rooms: Does the deep water prevent movement? */
  if ((SECT(was_in) == SECT_WATER_NOSWIM) ||
      (SECT(going_to) == SECT_WATER_NOSWIM))
  {
    if (!has_boat(ch))
    {
      send_to_char(ch, "보트가 있어야 갈 수 있습니다.\r\n");
      return (0);
    }
  }

  /* Flying Required: Does lack of flying prevent movement? */
  if ((SECT(was_in) == SECT_FLYING) || (SECT(going_to) == SECT_FLYING))
  {
    if (!has_flight(ch))
    {
      send_to_char(ch, "날아서 가야하는 곳입니다!\r\n");
      return (0);
    }
  }

  /* Underwater Room: Does lack of underwater breathing prevent movement? */
  if ((SECT(was_in) == SECT_UNDERWATER) || (SECT(going_to) == SECT_UNDERWATER))
  {
    if (!has_scuba(ch) && !IS_NPC(ch) && !PRF_FLAGGED(ch, PRF_NOHASSLE)) {
      send_to_char(ch, "잠수복이 있어야 갈 수 있습니다!\r\n");
      return (0);
    }
  }

  /* Houses: Can the player walk into the house? */
  if (ROOM_FLAGGED(was_in, ROOM_ATRIUM))
  {
    if (!House_can_enter(ch, GET_ROOM_VNUM(going_to)))
    {
      send_to_char(ch, "개인집 입니다 -- 출입금지!\r\n");
      return (0);
    }
  }

  /* Check zone level recommendations */
  if ((ZONE_MINLVL(GET_ROOM_ZONE(going_to)) != -1) && ZONE_MINLVL(GET_ROOM_ZONE(going_to)) > GET_LEVEL(ch)) {
    send_to_char(ch, "아직 이 지역으로 들어가기에 레벨이 낮습니다.\r\n");
    return (0);
  }
if ((GET_LEVEL(ch) < LVL_IMMORT) && (ZONE_MAXLVL(GET_ROOM_ZONE(going_to)) != -1) && ZONE_MAXLVL(GET_ROOM_ZONE(going_to)) < GET_LEVEL(ch)) {
    send_to_char(ch, "이 지역으로 들어가기에 레벨이 너무 높습니다.\r\n");
    return (0);
  }

  /* 제한된 존 체크 */
  if (ZONE_FLAGGED(GET_ROOM_ZONE(going_to), ZONE_CLOSED)) {
    send_to_char(ch, "신비로운 벽이 가로막습니다! 닫혀있는 구역입니다.\r\n");
    return (0);
  }
  if (ZONE_FLAGGED(GET_ROOM_ZONE(going_to), ZONE_NOIMMORT) && (GET_LEVEL(ch) >= LVL_IMMORT) && (GET_LEVEL(ch) < LVL_GRGOD)) {
    send_to_char(ch, "신비로운 벽이 가로막습니다! 출입제한 구역입니다.\r\n");
    return (0);
  }

  /* 방크기: 이미 사람이 방에 가득차 있는가? */
  if (ROOM_FLAGGED(going_to, ROOM_TUNNEL) &&
      num_pc_in_room(&(world[going_to])) >= CONFIG_TUNNEL_SIZE)
  {
    if (CONFIG_TUNNEL_SIZE > 1)
      send_to_char(ch, "더이상 들어갈 수 없습니다.!\r\n");
    else
      send_to_char(ch, "한사람만 들어갈 수 있습니다!\r\n");
    return (0);
  }

  /* Room Level Requirements: Is ch privileged enough to enter the room? */
  if (ROOM_FLAGGED(going_to, ROOM_GODROOM) && GET_LEVEL(ch) < LVL_GOD)
  {
    send_to_char(ch, "당신의 권한으로 들어갈 수 없습니다!\r\n");
    return (0);
  }

  /* All checks passed, nothing will prevent movement now other than lack of
   * move points. */
  /* move points needed is avg. move loss for src and destination sect type */
  need_movement = (movement_loss[SECT(was_in)] +
		   movement_loss[SECT(going_to)]) / 2;

  /* Move Point Requirement Check */
  if (GET_MOVE(ch) < need_movement && !IS_NPC(ch))
  {
    if (need_specials_check && ch->master)
      send_to_char(ch, "당신은 따라가기에 너무 지쳐 있습니다.\r\n");
    else
      send_to_char(ch, "당신은 너무 지쳐 있습니다.\r\n");

    return (0);
  }

  /*---------------------------------------------------------------------*/
  /* End checks that can prevent a character from leaving the was_in room. */


  /* Begin: the leave operation. */
  /*---------------------------------------------------------------------*/
  /* If applicable, subtract movement cost. */
  if (GET_LEVEL(ch) < LVL_IMMORT && !IS_NPC(ch))
    GET_MOVE(ch) -= need_movement;

  /* Generate the leave message and display to others in the was_in room. */
  if (!AFF_FLAGGED(ch, AFF_SNEAK))
  {
    snprintf(leave_message, sizeof(leave_message), "$n$j %s쪽으로 갑니다.", han_dirs[dir]);
    act(leave_message, TRUE, ch, 0, 0, TO_ROOM);
  }

  char_from_room(ch);
  char_to_room(ch, going_to);
  /*---------------------------------------------------------------------*/
  /* End: the leave operation. The character is now in the new room. */


  /* Begin: Post-move operations. */
  /*---------------------------------------------------------------------*/
  /* Post Move Trigger Checks: Check the new room for triggers.
   * Assumptions: The character has already truly left the was_in room. If
   * the entry trigger "prevents" movement into the room, it is the triggers
   * job to provide a message to the original was_in room. */
  if (!entry_mtrigger(ch) || !enter_wtrigger(&world[going_to], ch, dir)) {
    char_from_room(ch);
    char_to_room(ch, was_in);
    return 0;
  }

  /* Display arrival information to anyone in the destination room... */
  if (!AFF_FLAGGED(ch, AFF_SNEAK))
    act("$n$j 도착했습니다.", TRUE, ch, 0, 0, TO_ROOM);

  /* ... and the room description to the character. */
  if (ch->desc != NULL)
    look_at_room(ch, 0);

  /* ... and Kill the player if the room is a death trap. */
  if (ROOM_FLAGGED(going_to, ROOM_DEATH) && GET_LEVEL(ch) < LVL_IMMORT)
  {
    mudlog(BRF, LVL_IMMORT, TRUE, "%s hit death trap #%d (%s)", GET_NAME(ch), GET_ROOM_VNUM(going_to), world[going_to].name);
    death_cry(ch);
    extract_char(ch);
    return (0);
  }

  /* At this point, the character is safe and in the room. */
  /* Fire memory and greet triggers, check and see if the greet trigger
   * prevents movement, and if so, move the player back to the previous room. */
  entry_memory_mtrigger(ch);
  if (!greet_mtrigger(ch, dir))
  {
    char_from_room(ch);
    char_to_room(ch, was_in);
    look_at_room(ch, 0);
    /* Failed move, return a failure */
    return (0);
  }
  else
    greet_memory_mtrigger(ch);
  /*---------------------------------------------------------------------*/
  /* End: Post-move operations. */

  /* Only here is the move successful *and* complete. Return success for
   * calling functions to handle post move operations. */
  return (1);
}

int perform_move(struct char_data *ch, int dir, int need_specials_check)
{
  room_rnum was_in;
  struct follow_type *k, *next;

  if (ch == NULL || dir < 0 || dir >= NUM_OF_DIRS || FIGHTING(ch))
    return (0);
  else if (!CONFIG_DIAGONAL_DIRS && IS_DIAGONAL(dir))
    send_to_char(ch, "그쪽으로는 길이 없습니다.\r\n");
  else if ((!EXIT(ch, dir) && !buildwalk(ch, dir)) || EXIT(ch, dir)->to_room == NOWHERE)
    send_to_char(ch, "그쪽으로는 길이 없습니다.\r\n");
  else if (EXIT_FLAGGED(EXIT(ch, dir), EX_CLOSED) && (GET_LEVEL(ch) < LVL_IMMORT || (!IS_NPC(ch) && !PRF_FLAGGED(ch, PRF_NOHASSLE)))) {
    if (EXIT(ch, dir)->keyword)
      send_to_char(ch, "%s 문이 닫혀있습니다.\r\n",
		EXIT(ch, dir)->keyword);
    else
      send_to_char(ch, "잠겨 있는것 같습니다.\r\n");
  } else {
    if (!ch->followers)
      return (do_simple_move(ch, dir, need_specials_check));

    was_in = IN_ROOM(ch);
    if (!do_simple_move(ch, dir, need_specials_check))
      return (0);

    for (k = ch->followers; k; k = next) {
      next = k->next;
      if ((IN_ROOM(k->follower) == was_in) &&
	  (GET_POS(k->follower) >= POS_STANDING)) {
	act("당신은 $N$J 따라 이동합니다.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
	perform_move(k->follower, dir, 1);
      }
    }
    return (1);
  }
  return (0);
}

ACMD(do_move)
{
  /* These subcmd defines are mapped precisely to the direction defines. */
  perform_move(ch, subcmd, 0);
}

static int find_door(struct char_data *ch, const char *type, char *dir, const char *cmdname)
{
  int door;

  if (*dir) {			/* a direction was specified */
    if ((door = search_block(dir, dirs, FALSE)) == -1) { /* Partial Match */
      if ((door = search_block(dir, autoexits, FALSE)) == -1) { /* Check 'short' dirs too */
      send_to_char(ch, "그쪽으로는 길이 없습니다.\r\n");
        return (-1);
      }
    }
    if (EXIT(ch, door)) {	/* Braces added according to indent. -gg */
      if (EXIT(ch, door)->keyword) {
        if (is_name(type, EXIT(ch, door)->keyword))
          return (door);
        else {
	  send_to_char(ch, "%s 찾을 수 없습니다.\r\n", type);
          return (-1);
        }
      } else
	return (door);
    } else {
      send_to_char(ch, "어떻게 %s 보려구요?\r\n", cmdname);
      return (-1);
    }
  } else {			/* try to locate the keyword */
    if (!*type) {
      send_to_char(ch, "무엇을 %s 볼까요?\r\n", cmdname);
      return (-1);
    }
    for (door = 0; door < DIR_COUNT; door++)
    {
      if (EXIT(ch, door))
      {
        if (EXIT(ch, door)->keyword)
        {
          if (isname(type, EXIT(ch, door)->keyword))
          {
            if ((!IS_NPC(ch)) && (!PRF_FLAGGED(ch, PRF_AUTODOOR)))
              return door;
            else if (is_abbrev(cmdname, "열어"))
            {
              if (IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED))
                return door;
              else if (IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED))
                return door;
            }
            else if ((is_abbrev(cmdname, "닫아")) && (!(IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED))) )
              return door;
            else if ((is_abbrev(cmdname, "잠궈")) && (!(IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED))) )
              return door;
            else if ((is_abbrev(cmdname, "풀어")) && (IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED)) )
              return door;
            else if ((is_abbrev(cmdname, "따")) && (IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED)) )
              return door;
          }
        }
      }
    }

    if ((!IS_NPC(ch)) && (!PRF_FLAGGED(ch, PRF_AUTODOOR))) // 테스트
      send_to_char(ch, "There doesn't seem to be %s here.\r\n", type);
    else if (is_abbrev(cmdname, "열어"))
      send_to_char(ch, "There doesn't seem to be %s that can be opened.\r\n", type);
    else if (is_abbrev(cmdname, "닫아"))
      send_to_char(ch, "There doesn't seem to be %s that can be closed.\r\n", type);
    else if (is_abbrev(cmdname, "잠궈"))
      send_to_char(ch, "There doesn't seem to be %s that can be locked.\r\n", type);
    else if (is_abbrev(cmdname, "풀어"))
      send_to_char(ch, "There doesn't seem to be %s that can be unlocked.\r\n", type);
    else
      send_to_char(ch, "There doesn't seem to be %s that can be picked.\r\n", type);

    return (-1);
  }
}

int has_key(struct char_data *ch, obj_vnum key)
{
  struct obj_data *o;

  for (o = ch->carrying; o; o = o->next_content)
    if (GET_OBJ_VNUM(o) == key)
      return (1);

  if (GET_EQ(ch, WEAR_HOLD))
    if (GET_OBJ_VNUM(GET_EQ(ch, WEAR_HOLD)) == key)
      return (1);

  return (0);
}

#define NEED_OPEN	(1 << 0)
#define NEED_CLOSED	(1 << 1)
#define NEED_UNLOCKED	(1 << 2)
#define NEED_LOCKED	(1 << 3)

/* cmd_door is required external from act.movement.c */
const char *cmd_door[] =
{
  "열어",
  "닫아",
  "풀어",
  "잠궈",
  "따"
};

static const int flags_door[] =
{
  NEED_CLOSED | NEED_UNLOCKED,
  NEED_OPEN,
  NEED_CLOSED | NEED_LOCKED,
  NEED_CLOSED | NEED_UNLOCKED,
  NEED_CLOSED | NEED_LOCKED
};

#define EXITN(room, door)		(world[room].dir_option[door])
#define OPEN_DOOR(room, obj, door)	((obj) ?\
		(REMOVE_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) :\
		(REMOVE_BIT(EXITN(room, door)->exit_info, EX_CLOSED)))
#define CLOSE_DOOR(room, obj, door)	((obj) ?\
		(SET_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) :\
		(SET_BIT(EXITN(room, door)->exit_info, EX_CLOSED)))
#define LOCK_DOOR(room, obj, door)	((obj) ?\
		(SET_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) :\
		(SET_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))
#define UNLOCK_DOOR(room, obj, door)	((obj) ?\
		(REMOVE_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) :\
		(REMOVE_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))
#define TOGGLE_LOCK(room, obj, door)	((obj) ?\
		(TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) :\
		(TOGGLE_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))

static void do_doorcmd(struct char_data *ch, struct obj_data *obj, int door, int scmd)
{
  char buf[MAX_STRING_LENGTH];
  size_t len;
  room_rnum other_room = NOWHERE;
  struct room_direction_data *back = NULL;

  if (!door_mtrigger(ch, scmd, door))
    return;

  if (!door_wtrigger(ch, scmd, door))
    return;

  len = snprintf(buf, sizeof(buf), "$n %ss ", cmd_door[scmd]);
  if (!obj && ((other_room = EXIT(ch, door)->to_room) != NOWHERE))
    if ((back = world[other_room].dir_option[rev_dir[door]]) != NULL)
      if (back->to_room != IN_ROOM(ch))
        back = NULL;

  switch (scmd) {
  case SCMD_OPEN:
    OPEN_DOOR(IN_ROOM(ch), obj, door);
    if (back)
      OPEN_DOOR(other_room, obj, rev_dir[door]);
    send_to_char(ch, "%s", CONFIG_OK);
    break;

  case SCMD_CLOSE:
    CLOSE_DOOR(IN_ROOM(ch), obj, door);
    if (back)
      CLOSE_DOOR(other_room, obj, rev_dir[door]);
    send_to_char(ch, "%s", CONFIG_OK);
    break;

  case SCMD_LOCK:
    LOCK_DOOR(IN_ROOM(ch), obj, door);
    if (back)
      LOCK_DOOR(other_room, obj, rev_dir[door]);
    send_to_char(ch, "*철컥*\r\n");
    break;

  case SCMD_UNLOCK:
    UNLOCK_DOOR(IN_ROOM(ch), obj, door);
    if (back)
      UNLOCK_DOOR(other_room, obj, rev_dir[door]);
    send_to_char(ch, "*찰칵*\r\n");
    break;

  case SCMD_PICK:
    TOGGLE_LOCK(IN_ROOM(ch), obj, door);
    if (back)
      TOGGLE_LOCK(other_room, obj, rev_dir[door]);
    send_to_char(ch, "당신의 교묘한 손놀림으로 자물쇠를 땁니다.\r\n");
    len = strlcpy(buf, "$n님의 교묘한 손놀림으로 자물쇠를 땁니다", sizeof(buf));
    break;
  }

  /* Notify the room. */
  if (len < sizeof(buf))
    snprintf(buf + len, sizeof(buf) - len, "%s.",
	obj ? "$p" : EXIT(ch, door)->keyword ? "$F" : "문");
  if (!obj || IN_ROOM(obj) != NOWHERE)
    act(buf, FALSE, ch, obj, obj ? 0 : EXIT(ch, door)->keyword, TO_ROOM);

  /* Notify the other room */
  if (back && (scmd == SCMD_OPEN || scmd == SCMD_CLOSE))
      send_to_room(EXIT(ch, door)->to_room, "%s 문이 반대 방향에서 %s.\r\n",
		back->keyword ? back->keyword : "", scmd==SCMD_OPEN ? "열립니다" : " 닫힙니다");
}

static int ok_pick(struct char_data *ch, obj_vnum keynum, int pickproof, int scmd)
{
  int percent, skill_lvl;

  if (scmd != SCMD_PICK)
    return (1);

  percent = rand_number(1, 101);
  skill_lvl = GET_SKILL(ch, SKILL_PICK_LOCK) + dex_app_skill[GET_DEX(ch)].p_locks;

  if (keynum == NOTHING)
    send_to_char(ch, "열쇠 구멍을 찾을 수 없습니다.\r\n");
  else if (pickproof)
    send_to_char(ch, "자물쇠가 너무 단단히 잠겨 있습니다.\r\n");
  else if (percent > skill_lvl)
    send_to_char(ch, "자물쇠를 따는데 실패했습니다.\r\n");
  else
    return (1);

  return (0);
}

#define DOOR_IS_OPENABLE(ch, obj, door)	((obj) ? ((GET_OBJ_TYPE(obj) == \
    ITEM_CONTAINER) && OBJVAL_FLAGGED(obj, CONT_CLOSEABLE)) :\
    (EXIT_FLAGGED(EXIT(ch, door), EX_ISDOOR)))
#define DOOR_IS_OPEN(ch, obj, door) ((obj) ? (!OBJVAL_FLAGGED(obj, \
    CONT_CLOSED)) : (!EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED)))
#define DOOR_IS_UNLOCKED(ch, obj, door)	((obj) ? (!OBJVAL_FLAGGED(obj, \
    CONT_LOCKED)) : (!EXIT_FLAGGED(EXIT(ch, door), EX_LOCKED)))
#define DOOR_IS_PICKPROOF(ch, obj, door) ((obj) ? (OBJVAL_FLAGGED(obj, \
    CONT_PICKPROOF)) : (EXIT_FLAGGED(EXIT(ch, door), EX_PICKPROOF)))
#define DOOR_IS_CLOSED(ch, obj, door) (!(DOOR_IS_OPEN(ch, obj, door)))
#define DOOR_IS_LOCKED(ch, obj, door) (!(DOOR_IS_UNLOCKED(ch, obj, door)))
#define DOOR_KEY(ch, obj, door)	((obj) ? (GET_OBJ_VAL(obj, 2)) : \
    (EXIT(ch, door)->key))

ACMD(do_gen_door)
{
  int door = -1;
  obj_vnum keynum;
  char type[MAX_INPUT_LENGTH], dir[MAX_INPUT_LENGTH];
  struct obj_data *obj = NULL;
  struct char_data *victim = NULL;

  skip_spaces(&argument);
  if (!*argument) {
    send_to_char(ch, "무엇을 %s 볼까요?\r\n", cmd_door[subcmd]);
    return;
  }
  two_arguments(argument, type, dir);
  if (!generic_find(type, FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &victim, &obj))
    door = find_door(ch, type, dir, cmd_door[subcmd]);

  if ((obj) && (GET_OBJ_TYPE(obj) != ITEM_CONTAINER)) {
    obj = NULL;
    door = find_door(ch, type, dir, cmd_door[subcmd]);
  }

  if ((obj) || (door >= 0)) {
    keynum = DOOR_KEY(ch, obj, door);
   if (!(DOOR_IS_OPENABLE(ch, obj, door)))
      send_to_char(ch, "%s볼 수 없습니다!\r\n", cmd_door[subcmd]);
    else if (!DOOR_IS_OPEN(ch, obj, door) &&
	     IS_SET(flags_door[subcmd], NEED_OPEN))
      send_to_char(ch, "이미 닫혀 있습니다!\r\n");
    else if (!DOOR_IS_CLOSED(ch, obj, door) &&
	     IS_SET(flags_door[subcmd], NEED_CLOSED))
      send_to_char(ch, "이미 열려 있습니다!\r\n");
    else if (!(DOOR_IS_LOCKED(ch, obj, door)) &&
	     IS_SET(flags_door[subcmd], NEED_LOCKED))
      send_to_char(ch, "이미 잠겨 있습니다!\r\n");
    else if (!(DOOR_IS_UNLOCKED(ch, obj, door)) && IS_SET(flags_door[subcmd], NEED_UNLOCKED) && (PRF_FLAGGED(ch, PRF_AUTOKEY)) && (has_key(ch, keynum)) )
    {
      send_to_char(ch, "잠겨있는데 당신이 맞는 열쇠를 가지고 있습니다.\r\n");
      send_to_char(ch, "*찰칵*\r\n");
      do_doorcmd(ch, obj, door, subcmd);
    }
    else if (!(DOOR_IS_UNLOCKED(ch, obj, door)) && IS_SET(flags_door[subcmd], NEED_UNLOCKED) && (PRF_FLAGGED(ch, PRF_AUTOKEY)) && (!has_key(ch, keynum)) )
    {
      send_to_char(ch, "잠겨있는데 당신이 맞는 열쇠를 가지고 있지 않습니다!\r\n");
    }
    else if (!(DOOR_IS_UNLOCKED(ch, obj, door)) &&
	     IS_SET(flags_door[subcmd], NEED_UNLOCKED) &&
             (GET_LEVEL(ch) < LVL_IMMORT || !PRF_FLAGGED(ch, PRF_NOHASSLE)))
      send_to_char(ch, "잠겨서 꿈쩍도 하지 않습니다.\r\n");
    else if (!has_key(ch, keynum) && (GET_LEVEL(ch) < LVL_GOD) &&
	     ((subcmd == SCMD_LOCK) || (subcmd == SCMD_UNLOCK)))
      send_to_char(ch, "맞는 열쇠가 없습니다.\r\n");
    else if (ok_pick(ch, keynum, DOOR_IS_PICKPROOF(ch, obj, door), subcmd))
      do_doorcmd(ch, obj, door, subcmd);
  }
  return;
}

ACMD(do_enter)
{
  char buf[MAX_INPUT_LENGTH];
  int door;

  one_argument(argument, buf);

  if (*buf) {			/* an argument was supplied, search for door
				 * keyword */
    for (door = 0; door < DIR_COUNT; door++)
      if (EXIT(ch, door))
        if (EXIT(ch, door)->keyword)
          if (!str_cmp(EXIT(ch, door)->keyword, buf)) {
            perform_move(ch, door, 1);
            return;
          }
    send_to_char(ch, "%s 찾을 수 없습니다.\r\n", buf, check_josa(buf, 1));
  } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_INDOORS))
    send_to_char(ch, "당신은 이미 문안에 서 있습니다.\r\n");
  else {
    /* try to locate an entrance */
    for (door = 0; door < DIR_COUNT; door++)
      if (EXIT(ch, door))
	if (EXIT(ch, door)->to_room != NOWHERE)
	  if (!EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED) &&
	      ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_INDOORS)) {
	    perform_move(ch, door, 1);
	    return;
	  }
    send_to_char(ch, "들어갈 만한 곳이 보이지 않습니다.\r\n");
  }
}

ACMD(do_leave)
{
  int door;

  if (OUTSIDE(ch))
    send_to_char(ch, "이미 밖엔데 어디로 갈까요?\r\n");
  else {
    for (door = 0; door < DIR_COUNT; door++)
      if (EXIT(ch, door))
	if (EXIT(ch, door)->to_room != NOWHERE)
	  if (!EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED) &&
	    !ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_INDOORS)) {
	    perform_move(ch, door, 1);
	    return;
	  }
    send_to_char(ch, "밖으로 나가는 출구가 없습니다.\r\n");
  }
}

ACMD(do_stand)
{
  switch (GET_POS(ch)) {
  case POS_STANDING:
    send_to_char(ch, "당신은 이미 서 있습니다.\r\n");
    break;
  case POS_SITTING:
    send_to_char(ch, "당신은 일어섭니다.\r\n");
    act("$n$j $s 일어 섭니다.", TRUE, ch, 0, 0, TO_ROOM);
    /* Were they sitting in something? */
    char_from_furniture(ch);
    /* Will be sitting after a successful bash and may still be fighting. */
    GET_POS(ch) = FIGHTING(ch) ? POS_FIGHTING : POS_STANDING;
    break;
  case POS_RESTING:
    send_to_char(ch, "휴식을 마치고 일어섭니다.\r\n");
    act("$n$j 휴식을 마치고 $s 일어섭니다.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_STANDING;
    /* Were they sitting in something. */
    char_from_furniture(ch);
    break;
  case POS_SLEEPING:
    send_to_char(ch, "먼저 잠에서 깨어나야 합니다!\r\n");
    break;
  case POS_FIGHTING:
    send_to_char(ch, "싸움중에는 이미 일어날 필요가 없습니다.\r\n");
    break;
  default:
    send_to_char(ch, "당신이 떠다니다가 땅에 발을 내려놓습니다.\r\n");
    act("$n$j 떠다니다가 $s 땅에 발을 내려놓습니다.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_STANDING;
    break;
  }
}

ACMD(do_sit)
{
  char arg[MAX_STRING_LENGTH];
  struct obj_data *furniture;
  struct char_data *tempch;
  int found;

  one_argument(argument, arg);

  if (!(furniture = get_obj_in_list_vis(ch, arg, NULL, world[ch->in_room].contents)))
    found = 0;
  else
    found = 1;

  switch (GET_POS(ch)) {
  case POS_STANDING:
    if (found == 0) {
      send_to_char(ch, "당신은 자리에 앉습니다\r\n");
      act("$n$j 자리에 앉습니다.", FALSE, ch, 0, 0, TO_ROOM);
      GET_POS(ch) = POS_SITTING;
    } else {
      if (GET_OBJ_TYPE(furniture) != ITEM_FURNITURE) {
        send_to_char(ch, "앉을수 있는 종류가 아닙니다!\r\n");
        return;
      } else if (GET_OBJ_VAL(furniture, 1) > GET_OBJ_VAL(furniture, 0)) {
        /* Val 1 is current number sitting, 0 is max in sitting. */
		act("$p : 이미 많은 사람이 앉아 있습니다.", TRUE, ch, furniture, 0, TO_CHAR);
		log("SYSERR: Furniture %d holding too many people.", GET_OBJ_VNUM(furniture));
        return;
      } else if (GET_OBJ_VAL(furniture, 1) == GET_OBJ_VAL(furniture, 0)) {
		act("$p : 앉을 자리가 없습니다.", TRUE, ch, furniture, 0, TO_CHAR);
        return;
      } else {
        if (OBJ_SAT_IN_BY(furniture) == NULL)
          OBJ_SAT_IN_BY(furniture) = ch;
        for (tempch = OBJ_SAT_IN_BY(furniture); tempch != ch ; tempch = NEXT_SITTING(tempch)) {
          if (NEXT_SITTING(tempch))
            continue;
          NEXT_SITTING(tempch) = ch;
        }
        act("$p에 앉습니다.", TRUE, ch, furniture, 0, TO_CHAR);
        act("$n$j $p에 앉습니다.", TRUE, ch, furniture, 0, TO_ROOM);
        SITTING(ch) = furniture;
        NEXT_SITTING(ch) = NULL;
        GET_OBJ_VAL(furniture, 1) += 1;
        GET_POS(ch) = POS_SITTING;
      }
    }
    break;
  case POS_SITTING:
    send_to_char(ch, "당신은 이미 앉아 있습니다.\r\n");
    break;
  case POS_RESTING:
    send_to_char(ch, "휴식을 중단하고 앉습니다.\r\n");
    act("$n$j 휴식을 중단하고 앉습니다.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    break;
  case POS_SLEEPING:
    send_to_char(ch, "먼저 잠에서 깨어나야 합니다.\r\n");
    break;
  case POS_FIGHTING:
    send_to_char(ch, "싸우는 중에는 앉을 수 없습니다.\r\n");
    break;
  default:
    send_to_char(ch, "당신이 떠다니다가 자리에 앉습니다.\r\n");
    act("$n$j 떠다니다가 자리에 앉습니다.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    break;
  }
}

ACMD(do_rest)
{
  switch (GET_POS(ch)) {
  case POS_STANDING:
    send_to_char(ch, "당신은 앉아서 휴식을 취합니다.\r\n");
    act("$n$j 앉아서 휴식을 취합니다.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_RESTING;
    break;
  case POS_SITTING:
    send_to_char(ch, "당신은 휴식을 취합니다.\r\n");
    act("$n$j 휴식을 취합니다.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_RESTING;
    break;
  case POS_RESTING:
    send_to_char(ch, "당신은 이미 휴식중입니다.\r\n");
    break;
  case POS_SLEEPING:
    send_to_char(ch, "먼저 잠에서 깨어나야 합니다.\r\n");
    break;
  case POS_FIGHTING:
    send_to_char(ch, "싸우는 중에는 휴식할 수 없습니다.\r\n");
    break;
  default:
    send_to_char(ch, "당신이 떠다니다가 휴식을 취합니다.\r\n");
    act("$n$j 떠다니다가 휴식을 취합니다.", FALSE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_RESTING;
    break;
  }
}

ACMD(do_sleep)
{
  switch (GET_POS(ch)) {
  case POS_STANDING:
  case POS_SITTING:
  case POS_RESTING:
    send_to_char(ch, "잠을 자기 시작합니다.\r\n");
    act("$n$j 누워서 잠을 자기 시작합니다.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SLEEPING;
    break;
  case POS_SLEEPING:
    send_to_char(ch, "이미 잠을 자는 중입니다.\r\n");
    break;
  case POS_FIGHTING:
    send_to_char(ch, "싸우는 중에는 잠을 잘 수 없습니다.\r\n");
    break;
  default:
    send_to_char(ch, "당신이 떠다니다가 누워서 잠을 자기 시작합니다.\r\n");
    act("$n$j 떠다니다가 누워서 잠을 자기 시작합니다.",
	TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SLEEPING;
    break;
  }
}

ACMD(do_wake)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  int self = 0;

  one_argument(argument, arg);
  if (*arg) {
    if (GET_POS(ch) == POS_SLEEPING)
      send_to_char(ch, "먼저 자신이 깨어나야 합니다.\r\n");
    else if ((vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)) == NULL)
      send_to_char(ch, "%s", CONFIG_NOPERSON);
    else if (vict == ch)
      self = 1;
    else if (AWAKE(vict))
      act("$E님은 이미 깨어 있습니다.", FALSE, ch, 0, vict, TO_CHAR);
    else if (AFF_FLAGGED(vict, AFF_SLEEP))
      act("당신은 $M님을 깨울 수 없습니다!", FALSE, ch, 0, vict, TO_CHAR);
    else if (GET_POS(vict) < POS_SLEEPING)
      act("$E님이 깰 수 없는 상태입니다!", FALSE, ch, 0, vict, TO_CHAR);
    else {
      act("당신은 $M님을 깨웁니다.", FALSE, ch, 0, vict, TO_CHAR);
      act("당신은 $n님에 의해 깨어납니다.", FALSE, ch, 0, vict, TO_VICT | TO_SLEEP);
      GET_POS(vict) = POS_SITTING;
    }
    if (!self)
      return;
  }
  if (AFF_FLAGGED(ch, AFF_SLEEP))
    send_to_char(ch, "잠에서 깨어날 수 없습니다!\r\n");
  else if (GET_POS(ch) > POS_SLEEPING)
    send_to_char(ch, "당신은 이미 깨어 있습니다.\r\n");
  else {
    send_to_char(ch, "당신은 이미 깨어서 앉아 있습니다.\r\n");
    act("$n$j 잠에서 깨어납니다.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
  }
}

ACMD(do_follow)
{
  char buf[MAX_INPUT_LENGTH];
  struct char_data *leader;

  one_argument(argument, buf);

  if (*buf) {
    if (!(leader = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM))) {
      send_to_char(ch, "%s", CONFIG_NOPERSON);
      return;
    }
  } else {
    if (ch->master != (char_data*)  NULL) {
      send_to_char(ch, "당신은 이데 %s님을 따릅니다.\r\n", 
         GET_NAME(ch->master));
    } else {
    send_to_char(ch, "누구를 따를까요?\r\n");
    }
    return;
  }

  if (ch->master == leader) {
  act("당신은 이미 $M$J 따르고 있습니다.", FALSE, ch, 0, leader, TO_CHAR);
    return;
  }
  if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master)) {
    act("But you only feel like following $N!", FALSE, ch, 0, ch->master, TO_CHAR);
  } else {			/* Not Charmed follow person */
    if (leader == ch) {
      if (!ch->master) {
	send_to_char(ch, "자기 자신을 이미 따르고 있습니다.\r\n");
	return;
      }
      stop_follower(ch);
    } else {
      if (circle_follow(ch, leader)) {
		send_to_char(ch, "한번에 한명씩만 따를수 있습니다.\r\n");
        return;
      }
      if (ch->master)
        stop_follower(ch);

      add_follower(ch, leader);
    }
  }
}

ACMD(do_unfollow)
{
  if (ch->master) {
    if (AFF_FLAGGED(ch, AFF_CHARM)) { 
       send_to_char(ch, "당신은 이제 %s님을 따르지 않습니다.\r\n",
         GET_NAME(ch->master));
    } else {
      stop_follower(ch);
    }
  } else {
    send_to_char(ch, "당신은 지금 아무도 따르고 있지 않습니다.\r\n");
  }
  return;
}
