/**************************************************************************
*  File: act.comm.c                                        Part of tbaMUD *
*  Usage: Player-level communication commands.                            *
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
#include "screen.h"
#include "improved-edit.h"
#include "dg_scripts.h"
#include "act.h"
#include "modify.h"

static bool legal_communication(char * arg);

static bool legal_communication(char * arg) 
{
  while (*arg) {
    if (*arg == '@') {
      arg++;
      if (*arg == '(' || *arg == ')' || *arg == '<' || *arg == '>')
        return FALSE; 
    }
    arg++;
  }
  return TRUE;
}

ACMD(do_say)
{
  skip_spaces(&argument);

  if (!*argument)
    send_to_char(ch, "���� ���� �ϰ� ��������?\r\n");
  else {
    char buf[MAX_INPUT_LENGTH + 14], *msg;
    struct char_data *vict;
 
    if (CONFIG_SPECIAL_IN_COMM && legal_communication(argument))
      parse_at(argument);

    snprintf(buf, sizeof(buf), "[��] $n > %s", argument);
    msg = act(buf, FALSE, ch, 0, 0, TO_ROOM | DG_NO_TRIG);

    for (vict = world[IN_ROOM(ch)].people; vict; vict = vict->next_in_room)
      if (vict != ch && GET_POS(vict) > POS_SLEEPING)
        add_history(vict, msg, HIST_SAY);

    if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(ch, "%s", CONFIG_OK);
    else {
      sprintf(buf, "[��] %s", argument);
      msg = act(buf, FALSE, ch, 0, 0, TO_CHAR | DG_NO_TRIG);
      add_history(ch, msg, HIST_SAY);
    }
  }

  /* Trigger check. */
  speech_mtrigger(ch, argument);
  speech_wtrigger(ch, argument);
}

ACMD(do_gsay)
{
  skip_spaces(&argument);
  
  if (!GROUP(ch)) {
    send_to_char(ch, "����� �׷쿡 ���� ���� �ʽ��ϴ�!\r\n");
    return;
  }
  if (!*argument)
    send_to_char(ch, "�׷������ ���� ���� �ϰ� ��������?\r\n");
  else {
		
    if (CONFIG_SPECIAL_IN_COMM && legal_communication(argument))
      parse_at(argument);		
		
    send_to_group(ch, ch->group, "%s%s%s���� �׷������ ���մϴ�. '%s'%s\r\n", CCGRN(ch, C_NRM), CCGRN(ch, C_NRM), GET_NAME(ch), argument, CCNRM(ch, C_NRM));
	
    if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(ch, "%s", CONFIG_OK);
    else
      send_to_char(ch, "%s����� �׷������ ���մϴ�. '%s'%s\r\n", CCGRN(ch, C_NRM), argument, CCNRM(ch, C_NRM));
	}
}

static void perform_tell(struct char_data *ch, struct char_data *vict, char *arg)
{
  char buf[MAX_STRING_LENGTH], *msg;

  snprintf(buf, sizeof(buf), "%s$n���� �Ӹ��� �մϴ�. '%s'%s", CCRED(vict, C_NRM), arg, CCNRM(vict, C_NRM));
  msg = act(buf, FALSE, ch, 0, vict, TO_VICT | TO_SLEEP);
  add_history(vict, msg, HIST_TELL);

  if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(ch, "%s", CONFIG_OK);
  else {
    snprintf(buf, sizeof(buf), "%s����� $N�Կ��� �Ӹ��� �մϴ�. '%s'%s", CCRED(ch, C_NRM), arg, CCNRM(ch, C_NRM));
    msg = act(buf, FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);     
    add_history(ch, msg, HIST_TELL);
  }

  if (!IS_NPC(vict) && !IS_NPC(ch))
    GET_LAST_TELL(vict) = GET_IDNUM(ch);
}

static int is_tell_ok(struct char_data *ch, struct char_data *vict)
{
  if (!ch)
    log("SYSERR: is_tell_ok called with no characters");
  else if (!vict)
    send_to_char(ch, "%s", CONFIG_NOPERSON);
  else if (ch == vict)
    send_to_char(ch, "����� �ڽſ��� �Ӹ��� �õ� �մϴ�...\r\n");
  else if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOTELL))
    send_to_char(ch, "����� �Ӹ� �ź��� �Դϴ�.\r\n");
  else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SOUNDPROOF) && (GET_LEVEL(ch) < LVL_GOD))
    send_to_char(ch, "���� ���� ����� �Ӹ��� ������ ���� �ʽ��ϴ�.\r\n");
  else if (!IS_NPC(vict) && !vict->desc)        /* linkless */
    act("$E���� ������ ���������ϴ�.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
  else if (PLR_FLAGGED(vict, PLR_WRITING))
    act("$E���� ���� �� �������Դϴ�.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
  else if ((!IS_NPC(vict) && PRF_FLAGGED(vict, PRF_NOTELL)) || (ROOM_FLAGGED(IN_ROOM(vict), ROOM_SOUNDPROOF) && (GET_LEVEL(ch) < LVL_GOD)))
    act("$E���� ���� �Ӹ� �ź��� �Դϴ�.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
  else
    return (TRUE);

  return (FALSE);
}

/* Yes, do_tell probably could be combined with whisper and ask, but it is
 * called frequently, and should IMHO be kept as tight as possible. */
ACMD(do_tell)
{
  struct char_data *vict = NULL;
  char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];

  half_chop(argument, buf, buf2);

  if (!*buf || !*buf2)
    send_to_char(ch, "�������� ���� ���� �Ͻ÷�����???\r\n");
  else if (!strcmp(buf, "m-w")) {
#ifdef CIRCLE_WINDOWS
   /* getpid() is not portable */
    send_to_char(ch, "Sorry, that is not available in the windows port.\r\n");
#else /* all other configurations */
    char word[MAX_INPUT_LENGTH], *p, *q;

    if (last_webster_teller != -1L) {
      if (GET_IDNUM(ch) == last_webster_teller) {
        send_to_char(ch, "����� ���� ����� ��ٸ��� �ֽ��ϴ�.\r\n");
        return;
      } else {
        send_to_char(ch, "������ �ٻ۰����ϴ�. ��� �� �ٽ� �õ� ���ּ���.\r\n");
        return;
      }
    }

    /* Only a-z and +/- allowed. */
    for (p = buf2, q = word; *p ; p++)
      if ((LOWER(*p) <= 'z' && LOWER(*p) >= 'a') || (*p == '+') || (*p == '-'))
        *q++ = *p;

    *q = '\0';

    if (!*word) {
      send_to_char(ch, "Sorry, only letters and +/- are allowed characters.\r\n");
      return;
    }
    snprintf(buf, sizeof(buf), "../bin/webster %s %d &", word, (int) getpid());
    last_webster_teller = GET_IDNUM(ch);
    send_to_char(ch, "You look up '%s' in Merriam-Webster.\r\n", word);
#endif /* platform specific part */
  } else if (GET_LEVEL(ch) < LVL_IMMORT && !(vict = get_player_vis(ch, buf, NULL, FIND_CHAR_WORLD)))
    send_to_char(ch, "%s", CONFIG_NOPERSON);
  else if (GET_LEVEL(ch) >= LVL_IMMORT && !(vict = get_char_vis(ch, buf, NULL, FIND_CHAR_WORLD)))
    send_to_char(ch, "%s", CONFIG_NOPERSON);
  else if (is_tell_ok(ch, vict)) {
    if (CONFIG_SPECIAL_IN_COMM && legal_communication(argument))
      parse_at(buf2);
    perform_tell(ch, vict, buf2);
	}
}

ACMD(do_reply)
{
  struct char_data *tch = character_list;

  if (IS_NPC(ch))
    return;

  skip_spaces(&argument);

  if (GET_LAST_TELL(ch) == NOBODY)
   send_to_char(ch, "����� ��밡 �����ϴ�!\r\n");
  else if (!*argument)
   send_to_char(ch, "����� ��� �ұ��?\r\n");
  else {
    /* Make sure the person you're replying to is still playing by searching
     * for them.  Note, now last tell is stored as player IDnum instead of
     * a pointer, which is much better because it's safer, plus will still
     * work if someone logs out and back in again. A descriptor list based 
     * search would be faster although we could not find link dead people.  
     * Not that they can hear tells anyway. :) -gg 2/24/98 */
    while (tch && (IS_NPC(tch) || GET_IDNUM(tch) != GET_LAST_TELL(ch)))
      tch = tch->next;

    if (!tch)
      send_to_char(ch, "����� ��밡 �������� �ƴմϴ�.\r\n");
    else if (is_tell_ok(ch, tch)) {
      if (CONFIG_SPECIAL_IN_COMM && legal_communication(argument))
        parse_at(argument);
      perform_tell(ch, tch, argument);
		}
  }
}

ACMD(do_spec_comm)
{
  char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
  struct char_data *vict;
  const char *action_sing, *action_plur, *action_others;

  switch (subcmd) {
  case SCMD_WHISPER:
     action_sing = "�ӻ��Դϴ�.";
    action_plur = "�ӻ��Դϴ�.";
    action_others = "$n$j $N�Կ��� ���𰡸� �ӻ��Դϴ�.";
    break;

  case SCMD_ASK:
    action_sing = "���� �մϴ�.";
    action_plur = "���� �մϴ�.";
    action_others = "$n$j $N�Կ��� �����ΰ��� ���� �մϴ�.";
    break;

  default:
    action_sing = "oops";
    action_plur = "oopses";
    action_others = "$n is tongue-tied trying to speak with $N.";
    break;
  }

  half_chop(argument, buf, buf2);

  if (!*buf || !*buf2)
    send_to_char(ch, "�������� %s �ұ��?\r\n", action_sing);
  else if (!(vict = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM)))
    send_to_char(ch, "%s", CONFIG_NOPERSON);
  else if (vict == ch)
    send_to_char(ch, "����� ȥ�㸻�� �մϴ�.\r\n");
  else {
    char buf1[MAX_STRING_LENGTH];

    if (CONFIG_SPECIAL_IN_COMM && legal_communication(argument))
      parse_at(buf2);


    snprintf(buf1, sizeof(buf1), "$n$j ��ſ��� '%s'%s %s", buf2, check_josa(buf2, 3), action_plur);
    act(buf1, FALSE, ch, 0, vict, TO_VICT);
	
    if ((!IS_NPC(ch)) && (PRF_FLAGGED(ch, PRF_NOREPEAT))) 
      send_to_char(ch, "%s", CONFIG_OK);
    else
  send_to_char(ch, "����� %s�Կ��� '%s'%s %s\r\n", 	GET_NAME(vict), buf2, check_josa(buf2, 3), action_sing);
	act(action_others, FALSE, ch, 0, vict, TO_NOTVICT);
  }
}

ACMD(do_write)
{
  struct obj_data *paper, *pen = NULL;
  char *papername, *penname;
  char buf1[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];

  papername = buf1;
  penname = buf2;

  two_arguments(argument, papername, penname);

  if (!ch->desc)
    return;

  if (!*papername) {
    /* Nothing was delivered. */
     send_to_char(ch, "��� ������ ���ǰ̴ϱ�?\r\n");
    return;
  }
  if (*penname) {
    /* Nothing was delivered. */
    if (!(paper = get_obj_in_list_vis(ch, papername, NULL, ch->carrying))) {
     send_to_char(ch, "����� %s%s �����ϴ�.\r\n", papername, check_josa(papername, 0));
      return;
    }
    if (!(pen = get_obj_in_list_vis(ch, penname, NULL, ch->carrying))) {
     send_to_char(ch, "����� %s%s �����ϴ�.\r\n", penname, check_josa(penname, 0));
      return;
    }
  } else { /* There was one arg.. let's see what we can find. */
    if (!(paper = get_obj_in_list_vis(ch, papername, NULL, ch->carrying))) {
      send_to_char(ch, "����� ����ǰ�� %s%s �����ϴ�.\r\n", papername, check_josa(papername, 4));
      return;
    }
    if (GET_OBJ_TYPE(paper) == ITEM_PEN) { /* Oops, a pen. */
      pen = paper;
      paper = NULL;
    } else if (GET_OBJ_TYPE(paper) != ITEM_NOTE) {
      send_to_char(ch, "���� ���� �ִ� ������ �ƴմϴ�.\r\n");
      return;
    }

    /* One object was found.. now for the other one. */
    if (!GET_EQ(ch, WEAR_HOLD)) {
      send_to_char(ch, "%s�� ������ ���� �� �� �����ϴ�.\r\n", papername);
      return;
    }
    if (!CAN_SEE_OBJ(ch, GET_EQ(ch, WEAR_HOLD))) {
     send_to_char(ch, "�տ� �� �� ������ ���� ������ �ʽ��ϴ�! ����!!\r\n");
      return;
    }
    if (pen)
      paper = GET_EQ(ch, WEAR_HOLD);
    else
      pen = GET_EQ(ch, WEAR_HOLD);
  }

  /* Now let's see what kind of stuff we've found. */
  if (GET_OBJ_TYPE(pen) != ITEM_PEN)
    act("$p$k ���� �ۼ��� ������ �ƴմϴ�.", FALSE, ch, pen, 0, TO_CHAR);
  else if (GET_OBJ_TYPE(paper) != ITEM_NOTE)
   act("����� $p�� ���� �� �� �����ϴ�.", FALSE, ch, paper, 0, TO_CHAR);
  else {
    char *backstr = NULL;

    /* Something on it, display it as that's in input buffer. */
    if (paper->action_description) {
      backstr = strdup(paper->action_description);
      send_to_char(ch, "�̹� ������ ���� �ֽ��ϴ�:\r\n");
      send_to_char(ch, "%s", paper->action_description);
    }

    /* We can write. */
    act("$n���� �޸��ϱ� �����մϴ�.", TRUE, ch, 0, 0, TO_ROOM);
    send_editor_help(ch->desc);
    string_write(ch->desc, &paper->action_description, MAX_NOTE_LENGTH, 0, backstr);
  }
}

ACMD(do_page)
{
  struct descriptor_data *d;
  struct char_data *vict;
  char buf2[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];

  half_chop(argument, arg, buf2);

  if (IS_NPC(ch))
    send_to_char(ch, "���� ����� �� ���� ����Դϴ�..\r\n");
  else if (!*arg)
    send_to_char(ch, "�������� �����ϱ� ���ϼ���?\r\n");
  else {
    char buf[MAX_STRING_LENGTH];

    snprintf(buf, sizeof(buf), "\007\007*$n* %s", buf2);
    if (!str_cmp(arg, "���")) {
      if (GET_LEVEL(ch) > LVL_GOD) {
	for (d = descriptor_list; d; d = d->next)
	  if (STATE(d) == CON_PLAYING && d->character)
	    act(buf, FALSE, ch, 0, d->character, TO_VICT);
      } else
	send_to_char(ch, "������ �����ϴ�!\r\n");
      return;
    }
    if ((vict = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD))) {
      act(buf, FALSE, ch, 0, vict, TO_VICT);
      if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOREPEAT))
	send_to_char(ch, "%s", CONFIG_OK);
      else
	act(buf, FALSE, ch, 0, vict, TO_CHAR);
    } else
      send_to_char(ch, "�׷� ����� �����ϴ�!\r\n");
  }
}

/* Generalized communication function by Fred C. Merkel (Torg). */
ACMD(do_gen_comm)
{
  struct descriptor_data *i;
  char color_on[24];
  char buf1[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH], *msg;
  bool emoting = FALSE;

  /* Array of flags which must _not_ be set in order for comm to be heard. */
  int channels[] = {
    0,
    PRF_NOSHOUT,
    PRF_NOGOSS,
    PRF_NOAUCT,
    PRF_NOGRATZ,
    PRF_NOGOSS,
    0
  };

  int hist_type[] = {
    HIST_HOLLER,
    HIST_SHOUT,
    HIST_GOSSIP,
    HIST_AUCTION,
    HIST_GRATS,
  };

  /* com_msgs: [0] Message if you can't perform the action because of noshout
   *           [1] name of the action
   *           [2] message if you're not on the channel
   *           [3] a color string. */
  const char *com_msgs[][4] = {
 {"����� ������ �Ҽ� �����ϴ�!!\r\n",
      "����",
      "",
      KYEL},

    {"����� ��ĥ�� �����ϴ�!!\r\n",
      "��ħ",
      "���� ��ħä���� ���ּ���!\r\n",
      KYEL},

    {"����� ����� �Ҽ� �����ϴ�!!\r\n",
      "���",
      "���� ���ä���� ���ּ���!\r\n",
      KYEL},

    {"����� ��Ÿ� �Ҽ� �����ϴ�!!\r\n",
      "���",
      "���� ���ä���� ���ּ���!\r\n",
      KMAG},

    {"����� ���ϸ� �Ҽ� �����ϴ�!\r\n",
      "����",
      "���� ����ä���� ���ּ���!\r\n",
      KGRN},

    {"You cannot gossip your emotions!\r\n",
      "gossip",
      "You aren't even on the channel!\r\n",
      KYEL}
  };

  if (PLR_FLAGGED(ch, PLR_NOSHOUT)) {
    send_to_char(ch, "%s", com_msgs[subcmd][0]);
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SOUNDPROOF) && (GET_LEVEL(ch) < LVL_GOD)) {
    send_to_char(ch, "������ ����� �Ҹ��� �����ϴ�.\r\n");
    return;
  }


  if (subcmd == SCMD_GEMOTE) {
    if (!*argument)
      send_to_char(ch, "Gemote? Yes? Gemote what?\r\n");
    else
      do_gmote(ch, argument, 0, 1);
    return;
  }

  /* Level_can_shout defined in config.c. */
  if (GET_LEVEL(ch) < CONFIG_LEVEL_CAN_SHOUT) {
    send_to_char(ch, "%d���� �̻� %s�� �� �ֽ��ϴ�.\r\n", CONFIG_LEVEL_CAN_SHOUT, com_msgs[subcmd][1]);
    return;
  }
  /* Make sure the char is on the channel. */
  if (!IS_NPC(ch) && PRF_FLAGGED(ch, channels[subcmd])) {
    send_to_char(ch, "%s", com_msgs[subcmd][2]);
    return;
  }

  /* skip leading spaces */
  skip_spaces(&argument);

  /* Make sure that there is something there to say! */
  if (!*argument) {
    send_to_char(ch, "���� %s%s �ұ��?\r\n", com_msgs[subcmd][1], check_josa(argument, 1));
    return;
  }
  if (subcmd == SCMD_HOLLER) {
    if (GET_MOVE(ch) < CONFIG_HOLLER_MOVE_COST) {
      send_to_char(ch, "����� �ʹ� �����ֽ��ϴ�.\r\n");
      return;
    } else
      GET_MOVE(ch) -= CONFIG_HOLLER_MOVE_COST;
  }
  /* Set up the color on code. */
  strlcpy(color_on, com_msgs[subcmd][3], sizeof(color_on));

  /* First, set up strings to be given to the communicator. */
  if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(ch, "%s", CONFIG_OK);
  else {
		if (CONFIG_SPECIAL_IN_COMM && legal_communication(argument))
      parse_at(argument);
      
    snprintf(buf1, sizeof(buf1), "%s����� %s : '%s'%s%s", 
		  COLOR_LEV(ch) >= C_CMP ? color_on : "", com_msgs[subcmd][1], argument,
		  COLOR_LEV(ch) >= C_CMP ? color_on : "", CCNRM(ch, C_CMP));
    
    msg = act(buf1, FALSE, ch, 0, 0, TO_CHAR | TO_SLEEP);
    add_history(ch, msg, hist_type[subcmd]);
  }
  if (!emoting)
    snprintf(buf1, sizeof(buf1), "$n %ss, '%s'", com_msgs[subcmd][1], argument);

  /* Now send all the strings out. */
  for (i = descriptor_list; i; i = i->next) {
    if (STATE(i) != CON_PLAYING || i == ch->desc || !i->character )
      continue;
    if (!IS_NPC(ch) && (PRF_FLAGGED(i->character, channels[subcmd]) || PLR_FLAGGED(i->character, PLR_WRITING)))
      continue;

    if (ROOM_FLAGGED(IN_ROOM(i->character), ROOM_SOUNDPROOF) && (GET_LEVEL(ch) < LVL_GOD))
      continue;

    if (subcmd == SCMD_SHOUT && ((world[IN_ROOM(ch)].zone != world[IN_ROOM(i->character)].zone) ||
         !AWAKE(i->character)))
      continue;

    snprintf(buf2, sizeof(buf2), "%s%s%s", (COLOR_LEV(i->character) >= C_NRM) ? color_on : "", buf1, KNRM); 
    msg = act(buf2, FALSE, ch, 0, i->character, TO_VICT | TO_SLEEP);
    add_history(i->character, msg, hist_type[subcmd]);
  }
}

ACMD(do_qcomm)
{
  if (!PRF_FLAGGED(ch, PRF_QUEST)) {
    send_to_char(ch, "�ӹ����� �ƴմϴ�!\r\n");
    return;
  }
  skip_spaces(&argument);

  if (!*argument)
    send_to_char(ch, "����� �ӹ����� �ұ��??\r\n");
  else {
    char buf[MAX_STRING_LENGTH];
    struct descriptor_data *i;
    
    if (CONFIG_SPECIAL_IN_COMM && legal_communication(argument))
      parse_at(argument);

    if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(ch, "%s", CONFIG_OK);
    else if (subcmd == SCMD_QSAY) {
      snprintf(buf, sizeof(buf), "����� '%s'%s �ӹ����� �մϴ�.",argument, check_josa(argument, 3));
      act(buf, FALSE, ch, 0, argument, TO_CHAR);
    } else
      act(argument, FALSE, ch, 0, argument, TO_CHAR);

    if (subcmd == SCMD_QSAY)
      snprintf(buf, sizeof(buf), "$n���� '%s'%s �ӹ����� �մϴ�.",argument, check_josa(argument, 3));
    else {
      strlcpy(buf, argument, sizeof(buf));
      mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE, "(GC) %s qechoed: %s", GET_NAME(ch), argument);
    }
    for (i = descriptor_list; i; i = i->next)
      if (STATE(i) == CON_PLAYING && i != ch->desc && PRF_FLAGGED(i->character, PRF_QUEST))
        act(buf, 0, ch, 0, i->character, TO_VICT | TO_SLEEP);
  }
}
