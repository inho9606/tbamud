/**************************************************************************
*  File: act.other.c                                       Part of tbaMUD *
*  Usage: Miscellaneous player-level commands.                             *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
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
#include "screen.h"
#include "house.h"
#include "constants.h"
#include "dg_scripts.h"
#include "act.h"
#include "spec_procs.h"
#include "class.h"
#include "fight.h"
#include "mail.h"  /* for has_mail() */
#include "shop.h"
#include "quest.h"
#include "modify.h"

/* Local defined utility functions */
/* do_group utility functions */
static void print_group(struct char_data *ch);
static void display_group_list(struct char_data * ch);

ACMD(do_quit)
{
  if (IS_NPC(ch) || !ch->desc)
    return;

  if (subcmd != SCMD_QUIT && GET_LEVEL(ch) < LVL_IMMORT)
    send_to_char(ch, "확실하게 종료라고 입력해주세요!\r\n");
  else if (GET_POS(ch) == POS_FIGHTING)
    send_to_char(ch, "전투중에는 종료할 수 없습니다!\r\n");
  else if (GET_POS(ch) < POS_STUNNED) {
    send_to_char(ch, "당신은 죽었습니다.\r\n");
    die(ch, NULL);
  } else if(ch->company != NULL)
    send_to_char(ch, "야구 선수는 게임을 종료할 수 없습니다.\r\n");
  else {
 act("$n님이 게임을 종료합니다.", TRUE, ch, 0, 0, TO_ROOM);
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s님이 게임에서 나갔습니다.", GET_NAME(ch));

    if (GET_QUEST_TIME(ch) != -1)
      quest_timeout(ch);

     send_to_char(ch, "안녕히 가십시오. 다음 방문을 기다리겠습니다!\r\n");

    /* We used to check here for duping attempts, but we may as well do it right
     * in extract_char(), since there is no check if a player rents out and it
     * can leave them in an equally screwy situation. */

    if (CONFIG_FREE_RENT)
      Crash_rentsave(ch, 0);

    GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));

    /* Stop snooping so you can't see passwords during deletion or change. */
    if (ch->desc->snoop_by) {
       write_to_output(ch->desc->snoop_by, "감시 대상이 게임을 종료 했습니다.\r\n");
      ch->desc->snoop_by->snooping = NULL;
      ch->desc->snoop_by = NULL;
    }

    extract_char(ch);		/* Char is saved before extracting. */
  }
}

ACMD(do_save)
{
  if (IS_NPC(ch) || !ch->desc)
    return;

  send_to_char(ch, "%s님의 데이터를 저장 합니다. \r\n", GET_NAME(ch) );
  save_char(ch);
  Crash_crashsave(ch);
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_HOUSE_CRASH))
    House_crashsave(GET_ROOM_VNUM(IN_ROOM(ch)));
  GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
}

/* Generic function for commands which are normally overridden by special
 * procedures - i.e., shop commands, mail commands, etc. */
ACMD(do_not_here)
{
   send_to_char(ch, "여기서 사용할 수 없는 명령입니다!\r\n");
}

ACMD(do_sneak)
{
  struct affected_type af;
  byte percent;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_SNEAK)) {
    send_to_char(ch, "그런 기술을 모릅니다.\r\n");
    return;
  }
   send_to_char(ch, "당신은 이제부터 조용히 움직입니다.\r\n");
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    affect_from_char(ch, SKILL_SNEAK);

  percent = rand_number(1, 101);	/* 101% is a complete failure */

  if (percent > GET_SKILL(ch, SKILL_SNEAK) + dex_app_skill[GET_DEX(ch)].sneak)
    return;

  new_affect(&af);
  af.spell = SKILL_SNEAK;
  af.duration = GET_LEVEL(ch);
  SET_BIT_AR(af.bitvector, AFF_SNEAK);
  affect_to_char(ch, &af);
}

ACMD(do_hide)
{
  byte percent;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_HIDE)) {
     send_to_char(ch, "그런 기술을 모릅니다.\r\n");
    return;
  }

  send_to_char(ch, "당신은 어둠속으로 몸을 숨깁니다.\r\n");

  if (AFF_FLAGGED(ch, AFF_HIDE))
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);

  percent = rand_number(1, 101);	/* 101% is a complete failure */

  if (percent > GET_SKILL(ch, SKILL_HIDE) + dex_app_skill[GET_DEX(ch)].hide)
    return;

  SET_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);
}

ACMD(do_steal)
{
  struct char_data *vict;
  struct obj_data *obj;
  char vict_name[MAX_INPUT_LENGTH], obj_name[MAX_INPUT_LENGTH];
  int percent, gold, eq_pos, pcsteal = 0, ohoh = 0;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_STEAL)) {
    send_to_char(ch, "그런 기술을 모릅니다.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "이곳에서는 분란이될 행동을 할수 없습니다.\r\n");
    return;
  }

  two_arguments(argument, obj_name, vict_name);

  if (!(vict = get_char_vis(ch, vict_name, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "누구에게 무엇을 훔칠까요?\r\n");
    return;
  } else if (vict == ch) {
    send_to_char(ch, "자기 자신에게 사용할 수 없습니다.\r\n");
    return;
  }

  /* 101% is a complete failure */
  percent = rand_number(1, 101-(GET_LUCK(ch)/20)) - (GET_DEX(ch)/5); // dex_app_skill[GET_DEX(ch)].p_pocket;

  if (GET_POS(vict) < POS_SLEEPING)
    percent = -1;		/* ALWAYS SUCCESS, unless heavy object. */

  if (!CONFIG_PT_ALLOWED && !IS_NPC(vict))
    pcsteal = 1;

  if (!AWAKE(vict))	/* Easier to steal from sleeping people. */
    percent -= 50;

  /* No stealing if not allowed. If it is no stealing from Imm's or Shopkeepers. */
  if (GET_LEVEL(vict) >= LVL_IMMORT || pcsteal || GET_MOB_SPEC(vict) == shop_keeper)
    percent = 101;		/* Failure */

  if (str_cmp(obj_name, "동전") && str_cmp(obj_name, "금화")) {

    if (!(obj = get_obj_in_list_vis(ch, obj_name, NULL, vict->carrying))) {

      for (eq_pos = 0; eq_pos < NUM_WEARS; eq_pos++)
	if (GET_EQ(vict, eq_pos) &&
	    (isname(obj_name, GET_EQ(vict, eq_pos)->name)) &&
	    CAN_SEE_OBJ(ch, GET_EQ(vict, eq_pos))) {
	  obj = GET_EQ(vict, eq_pos);
	  break;
	}
      if (!obj) {
	act("$E$V 그 물건을 가지고 있지 않습니다.", FALSE, ch, 0, vict, TO_CHAR);
	return;
      } else {			/* It is equipment */
	if ((GET_POS(vict) > POS_STUNNED)) {
	  send_to_char(ch, "입고 있는 장비를 훔칠 수 없습니다!\r\n");
	  return;
	} else {
          if (!give_otrigger(obj, vict, ch) ||
              !receive_mtrigger(ch, vict, obj) ) {
            send_to_char(ch, "불가능 합니다!\r\n");
            return;
          }
	  act("당신은 $p$L 훔쳤습니다.", FALSE, ch, obj, 0, TO_CHAR);
	  act("$n$j $p$L $N에게서 훔쳤습니다.", FALSE, ch, obj, vict, TO_NOTVICT);
	  obj_to_char(unequip_char(vict, eq_pos), ch);
	}
      }
    } else {			/* obj found in inventory */

      percent += GET_OBJ_WEIGHT(obj);	/* Make heavy harder */

      if (percent > GET_SKILL(ch, SKILL_STEAL)) {
	ohoh = TRUE;
	send_to_char(ch, "당신이 뭔가를 훔쳤습니다.\r\n");
	act("$n$j 뭔가를 훔쳤습니다!", FALSE, ch, 0, vict, TO_VICT);
	act("$n$j $N$d 뭔가를 훔쳤습니다!", TRUE, ch, 0, vict, TO_NOTVICT);
      } else {			/* Steal the item */
	if (IS_CARRYING_N(ch) + 1 < CAN_CARRY_N(ch)) {
          if (!give_otrigger(obj, vict, ch) ||
              !receive_mtrigger(ch, vict, obj) ) {
            send_to_char(ch, "불가능 합니다!\r\n");
            return;
          }
	  if (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj) < CAN_CARRY_W(ch)) {
	    obj_from_char(obj);
	    obj_to_char(obj, ch);
	    send_to_char(ch, "물건을 훔치는데 성공했습니다!\r\n");
	  }
	} else
	  send_to_char(ch, "가진것이 너무 많습니다.\r\n");
      }
    }
  } else {			/* Steal some coins */
    if (AWAKE(vict) && (percent > GET_SKILL(ch, SKILL_STEAL))) {
      ohoh = TRUE;
      send_to_char(ch, "당신은 돈을 훔쳤습니다.\r\n");
      act("당신의 지갑에서 $s을 훔쳐가는 $n$J 발견했습니다.", FALSE, ch, 0, vict, TO_VICT);
      act("$n$j $N$D 돈을 훔쳤습니다.", TRUE, ch, 0, vict, TO_NOTVICT);
    } else {
      /* Steal some gold coins */
      gold = (GET_GOLD(vict) * rand_number(1, 10)) / 100;
      gold = MIN(1782, gold);
      if (gold > 0) {
		increase_gold(ch, gold);
		decrease_gold(vict, gold);
        if (gold > 1)
			send_to_char(ch, "당신은 %d원을 훔치는데 성공했습니다!\r\n", gold);
		else
			send_to_char(ch, "겨우 동전 하나를 훔쳤습니다.\r\n");
	  } else {
		send_to_char(ch, "아무것도 훔치지 못했습니다.\r\n");
      }
    }
  }

  if (ohoh && IS_NPC(vict) && AWAKE(vict))
    hit(vict, ch, TYPE_UNDEFINED);
}

ACMD(do_stat_plus) {
	char stat[MAX_INPUT_LENGTH], value[MAX_INPUT_LENGTH];
	if(!(*argument)) {
		send_to_char(ch, "사용법: <특성치> <값> 올려\r\n");
		return;
	}
	two_arguments(argument, stat, value);
	if(!(value) || !is_number(value)) {
		send_to_char(ch, "사용법: <특성치> <값> 올려\r\n");
		return;
	}
	if(atoi(value) == 0) {
		send_to_char(ch, "값은 0보다 커야 합니다.\r\n");
		return;
	}
	if(atoi(value) > GET_POINT(ch)) {
		send_to_char(ch, "스탯포인트가 부족합니다.\r\n");
		return;
	}
	else {
		if(!str_cmp(stat, "힘")) {
			ch->real_abils.str += atoi(value);
			ch->real_abils.point -= atoi(value);
			send_to_char(ch, "기본 %s : %d, 남은 스탯포인트: %d\r\n", stat, ch->real_abils.str, ch->real_abils.point);
		}
		else if(!str_cmp(stat, "민첩")) {
			ch->real_abils.dex += atoi(value);
			ch->points.max_move += atoi(value)*5;
			ch->real_abils.point -= atoi(value);
			send_to_char(ch, "기본 %s : %d, 남은 스탯포인트: %d\r\n", stat, ch->real_abils.dex, ch->real_abils.point);
		}
		else if(!str_cmp(stat, "지식")) {
			ch->real_abils.intel += atoi(value);
			ch->points.max_mana += atoi(value)*5;
			ch->real_abils.point -= atoi(value);
			send_to_char(ch, "기본 %s : %d, 남은 스탯포인트: %d\r\n", stat, ch->real_abils.intel, ch->real_abils.point);
		}
		else if(!str_cmp(stat, "지혜")) {
			ch->real_abils.wis += atoi(value);
			ch->real_abils.point -= atoi(value);
			send_to_char(ch, "기본 %s : %d, 남은 스탯포인트: %d\r\n", stat, ch->real_abils.wis, ch->real_abils.point);
		}
		else if(!str_cmp(stat, "맷집")) {
			ch->real_abils.con += atoi(value);
			ch->points.max_hit += atoi(value)*5;
			ch->real_abils.point -= atoi(value);
			send_to_char(ch, "기본 %s : %d, 남은 스탯포인트: %d\r\n", stat, ch->real_abils.con, ch->real_abils.point);
		}
		else if(!str_cmp(stat, "통솔력")) {
			ch->real_abils.cha += atoi(value);
			ch->real_abils.point -= atoi(value);
			send_to_char(ch, "기본 %s : %d, 남은 스탯포인트: %d\r\n", stat, ch->real_abils.cha, ch->real_abils.point);
		}
		else if(!str_cmp(stat, "행운")) {
			ch->real_abils.luck += atoi(value);
			ch->real_abils.point -= atoi(value);
			send_to_char(ch, "기본 %s : %d, 남은 스탯포인트: %d\r\n", stat, ch->real_abils.luck, ch->real_abils.point);
		}
		else {
			send_to_char(ch, "그런 특성치는 없습니다.\r\n");
			return;
		}
		save_char(ch);
	}
}
ACMD(do_practice)
{
  char arg[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (*arg)
    send_to_char(ch, "길드에서만 수련이 가능합니다.\r\n");
  else
    list_skills(ch);
}

ACMD(do_visible)
{
  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    perform_immort_vis(ch);
    return;
  }

  if AFF_FLAGGED(ch, AFF_INVISIBLE) {
    appear(ch);
    send_to_char(ch, "당신의 모습을 드러냅니다.\r\n");
  } else
    send_to_char(ch, "당신은 모습을 숨기고 있습니다.\r\n");
}

ACMD(do_title)
{
  skip_spaces(&argument);
  delete_doubledollar(argument);
  parse_at(argument);

  if (IS_NPC(ch))
    send_to_char(ch, "맙은 사용할 수 없는 명령입니다..\r\n");
  else if (PLR_FLAGGED(ch, PLR_NOTITLE))
    send_to_char(ch, "당신은 칭호를 변경할 수 없습니다 -- 먼저 설정을 변경해주세요!\r\n");
  else if (strstr(argument, "(") || strstr(argument, ")"))
    send_to_char(ch, "칭호에 ( 또는 ) 문자를 포함할 수 없습니다.\r\n");
  else if (strlen(argument) > MAX_TITLE_LENGTH)
    send_to_char(ch, "%d글자 이상 넣을 수 없습니다.\r\n", MAX_TITLE_LENGTH / 2);
  else {
    set_title(ch, argument);
    send_to_char(ch, "이제 당신은 %s%s%s 입니다.\r\n",
		GET_NAME(ch), *GET_TITLE(ch) ? " " : "", GET_TITLE(ch));
  }
}

static void print_group(struct char_data *ch)
{
  struct char_data * k;

  send_to_char(ch, "Your group consists of:\r\n");

  while ((k = (struct char_data *) simple_list(ch->group->members)) != NULL)
    send_to_char(ch, "%-*s: %s[%4d/%-4d]H [%4d/%-4d]M [%4d/%-4d]V%s\r\n",
	    count_color_chars(GET_NAME(k))+22, GET_NAME(k), 
	    GROUP_LEADER(GROUP(ch)) == k ? CBGRN(ch, C_NRM) : CCGRN(ch, C_NRM),
	    GET_HIT(k), GET_MAX_HIT(k),
	    GET_MANA(k), GET_MAX_MANA(k),
	    GET_MOVE(k), GET_MAX_MOVE(k),
	    CCNRM(ch, C_NRM));
}

static void display_group_list(struct char_data * ch)
{
  struct group_data * group;
  int count = 0;
	
  if (group_list->iSize) {
    send_to_char(ch, "#   Group Leader     # of Members    In Zone\r\n"
                     "---------------------------------------------------\r\n");
		
    while ((group = (struct group_data *) simple_list(group_list)) != NULL) {
			if (IS_SET(GROUP_FLAGS(group), GROUP_NPC))
			  continue;
      if (GROUP_LEADER(group) && !IS_SET(GROUP_FLAGS(group), GROUP_ANON))
        send_to_char(ch, "%-2d) %s%-12s     %-2d              %s%s\r\n", 
          ++count,
          IS_SET(GROUP_FLAGS(group), GROUP_OPEN) ? CCGRN(ch, C_NRM) : CCRED(ch, C_NRM), 
          GET_NAME(GROUP_LEADER(group)), group->members->iSize, zone_table[world[IN_ROOM(GROUP_LEADER(group))].zone].name,
          CCNRM(ch, C_NRM));
      else
        send_to_char(ch, "%-2d) Hidden\r\n", ++count);
				
		}
  }
  if (count)
    send_to_char(ch, "\r\n"
                     "%sSeeking Members%s\r\n"
                     "%sClosed%s\r\n", 
                     CCGRN(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCRED(ch, C_NRM), CCNRM(ch, C_NRM));
  else
    send_to_char(ch, "\r\n"
                     "Currently no groups formed.\r\n");
}

/* Vatiken's Group System: Version 1.1 */
ACMD(do_group)
{
  char buf[MAX_STRING_LENGTH];
  struct char_data *vict;

  argument = one_argument(argument, buf);

  if (!*buf) {
    if (GROUP(ch))
      print_group(ch);
    else
      send_to_char(ch, "You must specify a group option, or type HELP GROUP for more info.\r\n");
    return;
  }
  
  if (is_abbrev(buf, "new")) {
    if (GROUP(ch))
      send_to_char(ch, "You are already in a group.\r\n");
    else
      create_group(ch);
  } else if (is_abbrev(buf, "list"))
    display_group_list(ch);
  else if (is_abbrev(buf, "join")) {
    skip_spaces(&argument);
    if (!(vict = get_char_vis(ch, argument, NULL, FIND_CHAR_ROOM))) {
      send_to_char(ch, "Join who?\r\n");
      return;
    } else if (vict == ch) {
      send_to_char(ch, "That would be one lonely grouping.\r\n");
      return;
    } else if (GROUP(ch)) {
      send_to_char(ch, "But you are already part of a group.\r\n");
      return;
    } else if (!GROUP(vict)) {
      act("$E$u is not part of a group!", FALSE, ch, 0, vict, TO_CHAR);
      return;
    } else if (!IS_SET(GROUP_FLAGS(GROUP(vict)), GROUP_OPEN)) {
      send_to_char(ch, "That group isn't accepting members.\r\n");
      return;
    }   
    join_group(ch, GROUP(vict)); 
  } else if (is_abbrev(buf, "kick")) {
    skip_spaces(&argument);
    if (!(vict = get_char_vis(ch, argument, NULL, FIND_CHAR_ROOM))) {
      send_to_char(ch, "Kick out who?\r\n");
      return;
    } else if (vict == ch) {
      send_to_char(ch, "There are easier ways to leave the group.\r\n");
      return;
    } else if (!GROUP(ch) ) {
      send_to_char(ch, "But you are not part of a group.\r\n");
      return;
    } else if (GROUP_LEADER(GROUP(ch)) != ch ) {
      send_to_char(ch, "Only the group's leader can kick members out.\r\n");
      return;
    } else if (GROUP(vict) != GROUP(ch)) {
      act("$E$u is not a member of your group!", FALSE, ch, 0, vict, TO_CHAR);
      return;
    } 
    send_to_char(ch, "You have kicked %s out of the group.\r\n", GET_NAME(vict));
    send_to_char(vict, "You have been kicked out of the group.\r\n"); 
    leave_group(vict);
  } else if (is_abbrev(buf, "regroup")) {
    if (!GROUP(ch)) {
      send_to_char(ch, "But you aren't part of a group!\r\n");
      return;
    }
    vict = GROUP_LEADER(GROUP(ch));
    if (ch == vict) {
      send_to_char(ch, "You are the group leader and cannot re-group.\r\n");
    } else {
      leave_group(ch);
      join_group(ch, GROUP(vict));
    }
  } else if (is_abbrev(buf, "leave")) {
    
    if (!GROUP(ch)) {
      send_to_char(ch, "But you aren't part of a group!\r\n");
      return;
    }
		
    leave_group(ch);
  } else if (is_abbrev(buf, "option")) {
    skip_spaces(&argument);
    if (!GROUP(ch)) {
      send_to_char(ch, "But you aren't part of a group!\r\n");
      return;
    } else if (GROUP_LEADER(GROUP(ch)) != ch) {
      send_to_char(ch, "Only the group leader can adjust the group flags.\r\n");
      return;
    }
    if (is_abbrev(argument, "open")) {
      TOGGLE_BIT(GROUP_FLAGS(GROUP(ch)), GROUP_OPEN);
      send_to_char(ch, "The group is now %s to new members.\r\n", IS_SET(GROUP_FLAGS(GROUP(ch)), GROUP_OPEN) ? "open" : "closed");
    } else if (is_abbrev(argument, "anonymous")) {
      TOGGLE_BIT(GROUP_FLAGS(GROUP(ch)), GROUP_ANON);
      send_to_char(ch, "The group location is now %s to other players.\r\n", IS_SET(GROUP_FLAGS(GROUP(ch)), GROUP_ANON) ? "invisible" : "visible");
    } else 
      send_to_char(ch, "The flag options are: Open, Anonymous\r\n");
  } else {
    send_to_char(ch, "You must specify a group option, or type HELP GROUP for more info.\r\n");		
  }

}

ACMD(do_report)
{
  struct group_data *group;

  if ((group = GROUP(ch)) == NULL) {
    send_to_char(ch, "But you are not a member of any group!\r\n");
    return;
  }

  send_to_group(NULL, group, "%s reports: %d/%dH, %d/%dM, %d/%dV\r\n",
	  GET_NAME(ch),
	  GET_HIT(ch), GET_MAX_HIT(ch),
	  GET_MANA(ch), GET_MAX_MANA(ch),
	  GET_MOVE(ch), GET_MAX_MOVE(ch));
}

ACMD(do_split)
{
  char buf[MAX_INPUT_LENGTH];
  int amount, num = 0, share, rest;
  size_t len;
  struct char_data *k;
  
  if (IS_NPC(ch))
    return;

  one_argument(argument, buf);

  if (is_number(buf)) {
    amount = atoi(buf);
    if (amount <= 0) {
      send_to_char(ch, "Sorry, you can't do that.\r\n");
      return;
    }
    if (amount > GET_GOLD(ch)) {
      send_to_char(ch, "You don't seem to have that much gold to split.\r\n");
      return;
    }
    
    if (GROUP(ch))
      while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL)
        if (IN_ROOM(ch) == IN_ROOM(k) && !IS_NPC(k))
          num++;

    if (num && GROUP(ch)) {
      share = amount / num;
      rest = amount % num;
    } else {
      send_to_char(ch, "With whom do you wish to share your gold?\r\n");
      return;
    }

    decrease_gold(ch, share * (num - 1));

    /* Abusing signed/unsigned to make sizeof work. */
    len = snprintf(buf, sizeof(buf), "%s splits %d coins; you receive %d.\r\n",
		GET_NAME(ch), amount, share);
    if (rest && len < sizeof(buf)) {
      snprintf(buf + len, sizeof(buf) - len,
		"%d coin%s %s not splitable, so %s keeps the money.\r\n", rest,
		(rest == 1) ? "" : "s", (rest == 1) ? "was" : "were", GET_NAME(ch));
    }

    while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL)
      if (k != ch && IN_ROOM(ch) == IN_ROOM(k) && !IS_NPC(k)) {
	      increase_gold(k, share);
	      send_to_char(k, "%s", buf);
			}

    send_to_char(ch, "You split %d coins among %d members -- %d coins each.\r\n",
	    amount, num, share);

    if (rest) {
      send_to_char(ch, "%d coin%s %s not splitable, so you keep the money.\r\n",
		rest, (rest == 1) ? "" : "s", (rest == 1) ? "was" : "were");
      increase_gold(ch, rest);
    }
  } else {
    send_to_char(ch, "How many coins do you wish to split with your group?\r\n");
    return;
  }
}

ACMD(do_use)
{
  char buf[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];
  struct obj_data *mag_item;

  half_chop(argument, arg, buf);
  if (!*arg) {
    send_to_char(ch, "무엇을 %s 할까요?\r\n", CMD_NAME);
    return;
  }
  mag_item = GET_EQ(ch, WEAR_HOLD);

  if (!mag_item || !isname(arg, mag_item->name)) {
    switch (subcmd) {
    case SCMD_RECITE:
    case SCMD_QUAFF:
      if (!(mag_item = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
	send_to_char(ch, "%s%s 찾을 수 없습니다.\r\n", arg, check_josa(arg, 4));
	return;
      }
      break;
    case SCMD_USE:
       send_to_char(ch, "%s%s 찾을 수 없습니다.\r\n", arg, check_josa(arg, 4));
      return;
    default:
      log("SYSERR: Unknown subcmd %d passed to do_use.", subcmd);
      /* SYSERR_DESC: This is the same as the unhandled case in do_gen_ps(),
       * but in the function which handles 'quaff', 'recite', and 'use'. */
      return;
    }
  }
  switch (subcmd) {
  case SCMD_QUAFF:
    if (GET_OBJ_TYPE(mag_item) != ITEM_POTION) {
      send_to_char(ch, "물약만 마실 수 있습니다.\r\n");
      return;
    }
    break;
  case SCMD_RECITE:
    if (GET_OBJ_TYPE(mag_item) != ITEM_SCROLL) {
      send_to_char(ch, "주문서만 배울 수 있습니다.\r\n");
      return;
    }
    break;
  case SCMD_USE:
    if ((GET_OBJ_TYPE(mag_item) != ITEM_WAND) &&
	(GET_OBJ_TYPE(mag_item) != ITEM_STAFF)) {
      send_to_char(ch, "사용할 수 있는 종류가 아닙니다.\r\n");
      return;
    }
    break;
  }

  mag_objectmagic(ch, mag_item, buf);
}

ACMD(do_display)
{
  size_t i;

  if (IS_NPC(ch)) {
    send_to_char(ch, "몹은 사용할 수 없는 명령입니다..\r\n");
    return;
  }
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "사용법: { {체력|마력|이동력} | 모두 | 자동 | 없음 } 표시\r\n");
    return;
  }

  if (!str_cmp(argument, "모두")) {
    TOGGLE_BIT_AR(PRF_FLAGS(ch), PRF_DISPAUTO);
    send_to_char(ch, "모두 표시 %s합니다.\r\n", PRF_FLAGGED(ch, PRF_DISPAUTO) ? "" : "안");
    return;
  }

  if (!str_cmp(argument, "모두")) {
    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPHP);
    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMANA);
    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMOVE);
  } else if (!str_cmp(argument, "없음")) {
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPHP);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMANA);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMOVE);
  } else {
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPHP);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMANA);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMOVE);

    for (i = 0; i < strlen(argument); i++) {
		if(!is_abbrev(argument, "체력")) {
			SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPHP);
		} else if(!is_abbrev(argument, "마력")) {
			SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMANA);
		} if(!is_abbrev(argument, "이동력")) {
			SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMOVE);
		} else {
			send_to_char(ch, "사용법: { {체력|마력|이동력} | 모두 | 자동 | 없음 } 표시\r\n");
			return;
      }
    }
  }

  send_to_char(ch, "%s", CONFIG_OK);
}

#define TOG_OFF 0
#define TOG_ON  1
ACMD(do_gen_tog)
{
  long result;
  int i;
  char arg[MAX_INPUT_LENGTH];

  const char *tog_messages[][2] = {
	{"이제부터 다른 사용자의 소환을 거부합니다.\r\n",
    "다른 사용자의 소환을 허용합니다.\r\n"},
    {"Nohassle disabled.\r\n",
    "Nohassle enabled.\r\n"},
    {"간략모드 꺼짐.\r\n",
    "간략모드 켜짐.\r\n"},
    {"공백제거 모드 켜짐.\r\n",
    "공백제거 모드 꺼짐.\r\n"},
    {"다른 사람과의 이야기를 허용합니다.\r\n",
    "다른 사람과의 이야기를 거부합니다.\r\n"},
    {"경매 채널을 켭니다.\r\n",
    "경매 채널을 듣지 않습니다.\r\n"},
    {"외침 채널을 켭니다.\r\n",
    "외침 채널을 듣지 않습니다.\r\n"},
    {"잡담 채널을 켭니다.\r\n",
    "잡담 채널을 듣지 않습니다.\r\n"},
    {"축하 채널을 켭니다.\r\n",
    "축하 채널을 듣지 않습니다.\r\n"},
    {"신채널을 듣습니다.\r\n",
    "신채널을 듣지 않습니다.\r\n"},
    {"당신은 이제 임무중이 아닙니다.\r\n",
    "당신은 이제 임무를 시작합니다.\r\n"},
    {"방속성을 더이상 보지 않습니다.\r\n",
    "이제 방속성을 봅니다.\r\n"},
    {"당신이 하는 말들을 봅니다.\r\n",
    "당신이 하는 말들을 보지 않습니다.\r\n"},
    {"암흑보기가 꺼집니다.\r\n",
    "암흑보기가 켜집니다.\r\n"},
    {"Nameserver_is_slow changed to NO; IP addresses will now be resolved.\r\n",
    "Nameserver_is_slow changed to YES; sitenames will no longer be resolved.\r\n"},
    {"자동출구를 사용하지 않습니다.\r\n",
    "자동출구를 사용합니다.\r\n"},
    {"이제 사용자들이 문을 통과해서 추적하지 못합니다.\r\n",
    "이제 사용자들이 문을 통과해서 추적을 합니다.\r\n"},
    {"편집화면을 일반모드로 봅니다.\r\n",
    "편집화면을 메뉴모드로 봅니다.\r\n"},
    {"존제작 모드를 사용하지 않습니다.\r\n",
    "존제작 모드를 사용합니다.\r\n"},
    {"접속유지를 해제합니다.\r\n",
    "접속유지로 들어갑니다.\r\n"},
    {"자동줍기를 사용하지 않습니다.\r\n",
    "자동줍기를 사용합니다.\r\n"},
    {"돈줍기를 사용하지 않습니다.\r\n",
    "돈줍기를 사용합니다.\r\n"},
    {"자동분배를 사용하지 않습니다.\r\n",
    "자동분배를 사용합니다.\r\n"},
    {"자동제물을 사용하지 않습니다.\r\n",
    "자동제물을 사용합니다.\r\n"},
    {"자동돕기를 사용하지 않습니다.\r\n",
    "자동돕기를 사용합니다.\r\n"},
    {"자동지도를 사용하지 않습니다.\r\n",
    "이제 조그만 지도를 방설명에 보여줍니다.\r\n"},
    {"이제 문을 수동으로 엽니다.\r\n",
    "열쇠가 있을때 문을 자동으로 엽니다.\r\n"},
    {"당신은 이제 문을 열고 닫거나 잠글때 방향을 따로 지정해야 합니다.\r\n",
    "당신은 이제 자동으로 문의 방향을 찾습니다.\r\n"},
    {"존의 리셋정보를 보지 않습니다.\r\n",
    "존의 리셋정보를 봅니다.\r\n"}
  };

  if (IS_NPC(ch))
    return;

  switch (subcmd) {
  case SCMD_NOSUMMON:
    result = PRF_TOG_CHK(ch, PRF_SUMMONABLE);
    break;
  case SCMD_NOHASSLE:
    result = PRF_TOG_CHK(ch, PRF_NOHASSLE);
    break;
  case SCMD_BRIEF:
    result = PRF_TOG_CHK(ch, PRF_BRIEF);
    break;
  case SCMD_COMPACT:
    result = PRF_TOG_CHK(ch, PRF_COMPACT);
    break;
  case SCMD_NOTELL:
    result = PRF_TOG_CHK(ch, PRF_NOTELL);
    break;
  case SCMD_NOAUCTION:
    result = PRF_TOG_CHK(ch, PRF_NOAUCT);
    break;
  case SCMD_NOSHOUT:
    result = PRF_TOG_CHK(ch, PRF_NOSHOUT);
    break;
  case SCMD_NOGOSSIP:
    result = PRF_TOG_CHK(ch, PRF_NOGOSS);
    break;
  case SCMD_NOGRATZ:
    result = PRF_TOG_CHK(ch, PRF_NOGRATZ);
    break;
  case SCMD_NOWIZ:
    result = PRF_TOG_CHK(ch, PRF_NOWIZ);
    break;
  case SCMD_QUEST:
    result = PRF_TOG_CHK(ch, PRF_QUEST);
    break;
  case SCMD_SHOWVNUMS:
    result = PRF_TOG_CHK(ch, PRF_SHOWVNUMS);
    break;
  case SCMD_NOREPEAT:
    result = PRF_TOG_CHK(ch, PRF_NOREPEAT);
    break;
  case SCMD_HOLYLIGHT:
    result = PRF_TOG_CHK(ch, PRF_HOLYLIGHT);
    break;
  case SCMD_AUTOEXIT:
    result = PRF_TOG_CHK(ch, PRF_AUTOEXIT);
    break;
  case SCMD_CLS:
    result = PRF_TOG_CHK(ch, PRF_CLS);
    break;    
  case SCMD_BUILDWALK:
    if (GET_LEVEL(ch) < LVL_BUILDER) {
      send_to_char(ch, "존제작자만 사용 가능합니다.\r\n");
      return;
    }
    result = PRF_TOG_CHK(ch, PRF_BUILDWALK);
    if (PRF_FLAGGED(ch, PRF_BUILDWALK)) {
      one_argument(argument, arg);
      for (i=0; *arg && *(sector_types[i]) != '\n'; i++)
        if (is_abbrev(arg, sector_types[i]))
          break;
      if (*(sector_types[i]) == '\n') 
        i=0;
      GET_BUILDWALK_SECTOR(ch) = i;
      send_to_char(ch, "Default sector type is %s\r\n", sector_types[i]);
  
      mudlog(CMP, GET_LEVEL(ch), TRUE,
             "OLC: %s turned buildwalk on. Allowed zone %d", GET_NAME(ch), GET_OLC_ZONE(ch));
    } else
      mudlog(CMP, GET_LEVEL(ch), TRUE,
             "OLC: %s turned buildwalk off. Allowed zone %d", GET_NAME(ch), GET_OLC_ZONE(ch));
    break;
  case SCMD_AFK:
    result = PRF_TOG_CHK(ch, PRF_AFK);
    if (PRF_FLAGGED(ch, PRF_AFK))
      act("$n$j 접속 유지상태로 $s 들어갑니다.", TRUE, ch, 0, 0, TO_ROOM);
    else {
      act("$n$j 접속유지 상태에서 다시 게임으로 $s 돌아옵니다.", TRUE, ch, 0, 0, TO_ROOM);
      if (has_mail(GET_IDNUM(ch)))
        send_to_char(ch, "편지가 와있습니다.\r\n");
    }
    break;
  case SCMD_AUTOLOOT:
    result = PRF_TOG_CHK(ch, PRF_AUTOLOOT);
    break;
  case SCMD_AUTOGOLD:
    result = PRF_TOG_CHK(ch, PRF_AUTOGOLD);
    break;
  case SCMD_AUTOSPLIT:
    result = PRF_TOG_CHK(ch, PRF_AUTOSPLIT);
    break;
  case SCMD_AUTOSAC:
    result = PRF_TOG_CHK(ch, PRF_AUTOSAC);
    break;
  case SCMD_AUTOASSIST:
    result = PRF_TOG_CHK(ch, PRF_AUTOASSIST);
    break;
  case SCMD_AUTOMAP:
    result = PRF_TOG_CHK(ch, PRF_AUTOMAP);
    break;
  case SCMD_AUTOKEY:
    result = PRF_TOG_CHK(ch, PRF_AUTOKEY);
    break;
  case SCMD_AUTODOOR:
    result = PRF_TOG_CHK(ch, PRF_AUTODOOR);
    break;
  case SCMD_ZONERESETS:
    result = PRF_TOG_CHK(ch, PRF_ZONERESETS);
    break;
  default:
    log("SYSERR: Unknown subcmd %d in do_gen_toggle.", subcmd);
    return;
  }

  if (result)
    send_to_char(ch, "%s", tog_messages[subcmd][TOG_ON]);
  else
    send_to_char(ch, "%s", tog_messages[subcmd][TOG_OFF]);

  return;
}

static void show_happyhour(struct char_data *ch)
{
  char happyexp[80], happygold[80], happyqp[80];
  int secs_left;

  if ((IS_HAPPYHOUR) || (GET_LEVEL(ch) >= LVL_GRGOD))
  {
      if (HAPPY_TIME)
        secs_left = ((HAPPY_TIME - 1) * SECS_PER_MUD_HOUR) + next_tick;
      else
        secs_left = 0;

      sprintf(happyqp,   "%s+%d%%%s to Questpoints per quest\r\n", CCYEL(ch, C_NRM), HAPPY_QP,   CCNRM(ch, C_NRM));
      sprintf(happygold, "%s+%d%%%s to Gold gained per kill\r\n",  CCYEL(ch, C_NRM), HAPPY_GOLD, CCNRM(ch, C_NRM));
      sprintf(happyexp,  "%s+%d%%%s to Experience per kill\r\n",   CCYEL(ch, C_NRM), HAPPY_EXP,  CCNRM(ch, C_NRM));

      send_to_char(ch, "tbaMUD Happy Hour!\r\n"
                       "------------------\r\n"
                       "%s%s%sTime Remaining: %s%d%s hours %s%d%s mins %s%d%s secs\r\n",
                       (IS_HAPPYEXP || (GET_LEVEL(ch) >= LVL_GOD)) ? happyexp : "",
                       (IS_HAPPYGOLD || (GET_LEVEL(ch) >= LVL_GOD)) ? happygold : "",
                       (IS_HAPPYQP || (GET_LEVEL(ch) >= LVL_GOD)) ? happyqp : "",
                       CCYEL(ch, C_NRM), (secs_left / 3600), CCNRM(ch, C_NRM),
                       CCYEL(ch, C_NRM), (secs_left % 3600) / 60, CCNRM(ch, C_NRM),
                       CCYEL(ch, C_NRM), (secs_left % 60), CCNRM(ch, C_NRM) );
  }
  else
  {
      send_to_char(ch, "Sorry, there is currently no happy hour!\r\n");
  }
}

ACMD(do_happyhour)
{
  char arg[MAX_INPUT_LENGTH], val[MAX_INPUT_LENGTH];
  int num;

  if (GET_LEVEL(ch) < LVL_GOD)
  {
    show_happyhour(ch);
    return;
  }

  /* Only Imms get here, so check args */
  two_arguments(argument, arg, val);

  if (is_abbrev(arg, "경험치"))
  {
    num = MIN(MAX((atoi(val)), 0), 1000);
    HAPPY_EXP = num;
    send_to_char(ch, "Happy Hour Exp rate set to +%d%%\r\n", HAPPY_EXP);
  }
  else if ((is_abbrev(arg, "gold")) || (is_abbrev(arg, "coins")))
  {
    num = MIN(MAX((atoi(val)), 0), 1000);
    HAPPY_GOLD = num;
    send_to_char(ch, "Happy Hour Gold rate set to +%d%%\r\n", HAPPY_GOLD);
  }
  else if ((is_abbrev(arg, "time")) || (is_abbrev(arg, "ticks")))
  {
    num = MIN(MAX((atoi(val)), 0), 1000);
    if (HAPPY_TIME && !num)
      game_info("Happyhour has been stopped!");
    else if (!HAPPY_TIME && num)
      game_info("A Happyhour has started!");

    HAPPY_TIME = num;
    send_to_char(ch, "Happy Hour Time set to %d ticks (%d hours %d mins and %d secs)\r\n",
                                HAPPY_TIME,
                                 (HAPPY_TIME*SECS_PER_MUD_HOUR)/3600,
                                ((HAPPY_TIME*SECS_PER_MUD_HOUR)%3600) / 60,
                                 (HAPPY_TIME*SECS_PER_MUD_HOUR)%60 );
  }
  else if ((is_abbrev(arg, "qp")) || (is_abbrev(arg, "questpoints")))
  {
    num = MIN(MAX((atoi(val)), 0), 1000);
    HAPPY_QP = num;
    send_to_char(ch, "Happy Hour Questpoints rate set to +%d%%\r\n", HAPPY_QP);
  }
  else if (is_abbrev(arg, "show"))
  {
    show_happyhour(ch);
  }
  else if (is_abbrev(arg, "default"))
  {
    HAPPY_EXP = 100;
    HAPPY_GOLD = 50;
    HAPPY_QP  = 50;
    HAPPY_TIME = 48;
    game_info("A Happyhour has started!");
  }
  else
  {
    send_to_char(ch, "Usage: %shappyhour              %s- show usage (this info)\r\n"
                     "       %shappyhour show         %s- display current settings (what mortals see)\r\n"
                     "       %shappyhour time <ticks> %s- set happyhour time and start timer\r\n"
                     "       %shappyhour qp <num>     %s- set qp percentage gain\r\n"
                     "       %shappyhour exp <num>    %s- set exp percentage gain\r\n"
                     "       %shappyhour gold <num>   %s- set gold percentage gain\r\n"
                     "       \tyhappyhour default      \tw- sets a default setting for happyhour\r\n\r\n"
                     "Configure the happyhour settings and start a happyhour.\r\n"
                     "Currently 1 hour IRL = %d ticks\r\n"
                     "If no number is specified, 0 (off) is assumed.\r\nThe command \tyhappyhour time\tn will therefore stop the happyhour timer.\r\n",
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
                     (3600 / SECS_PER_MUD_HOUR) );
  }
}
