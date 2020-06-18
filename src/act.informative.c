/**************************************************************************
*  File: act.informative.c                                 Part of tbaMUD *
*  Usage: Player-level commands of an informative nature.                 *
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
#include "constants.h"
#include "dg_scripts.h"
#include "mud_event.h"
#include "mail.h"         /**< For the has_mail function */
#include "act.h"
#include "class.h"
#include "fight.h"
#include "modify.h"
#include "asciimap.h"
#include "quest.h"

/* prototypes of local functions */
/* do_diagnose utility functions */
static void diag_char_to_char(struct char_data *i, struct char_data *ch);
/* do_look and do_examine utility functions */
static void do_auto_exits(struct char_data *ch);
static void list_char_to_char(struct char_data *list, struct char_data *ch);
static void list_one_char(struct char_data *i, struct char_data *ch);
static void look_at_char(struct char_data *i, struct char_data *ch);
static void look_at_target(struct char_data *ch, char *arg);
static void look_in_direction(struct char_data *ch, int dir);
static void look_in_obj(struct char_data *ch, char *arg);
/* do_look, do_inventory utility functions */
static void list_obj_to_char(struct obj_data *list, struct char_data *ch, int mode, int show);
/* do_look, do_equipment, do_examine, do_inventory */
static void show_obj_to_char(struct obj_data *obj, struct char_data *ch, int mode);
static void show_obj_modifiers(struct obj_data *obj, struct char_data *ch);
/* do_where utility functions */
static void perform_immort_where(struct char_data *ch, char *arg);
static void perform_mortal_where(struct char_data *ch, char *arg);
static void print_object_location(int num, struct obj_data *obj, struct char_data *ch, int recur);

/* Subcommands */
/* For show_obj_to_char 'mode'.	/-- arbitrary */
#define SHOW_OBJ_LONG     0
#define SHOW_OBJ_SHORT    1
#define SHOW_OBJ_ACTION   2

static void show_obj_to_char(struct obj_data *obj, struct char_data *ch, int mode)
{
  int found = 0;
  struct char_data *temp;

  if (!obj || !ch) {
    log("SYSERR: NULL pointer in show_obj_to_char(): obj=%p ch=%p", (void *)obj, (void *)ch);
    /*  SYSERR_DESC: Somehow a NULL pointer was sent to show_obj_to_char() in
     *  either the 'obj' or the 'ch' variable.  The error will indicate which
     *  was NULL by listing both of the pointers passed to it.  This is often a
     *  difficult one to trace, and may require stepping through a debugger. */
    return;
  }

  if ((mode == 0) && obj->description) {
    if (GET_OBJ_VAL(obj, 1) != 0 || OBJ_SAT_IN_BY(obj)) {
      for (temp = OBJ_SAT_IN_BY(obj); temp; temp = NEXT_SITTING(temp)) {
        if (temp == ch)
          found++;
      }
      if (found) {
        send_to_char(ch, "You are %s upon %s.", GET_POS(ch) == POS_SITTING ? "sitting" :
        "resting", obj->short_description);
        goto end;
      }
    }
  }

  switch (mode) {
  case SHOW_OBJ_LONG:
    /* Hide objects starting with . from non-holylighted people. - Elaseth */
    if (*obj->description == '.' && (IS_NPC(ch) || !PRF_FLAGGED(ch, PRF_HOLYLIGHT)))
      return;

    if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_SHOWVNUMS)) {
      send_to_char(ch, "[%d] ", GET_OBJ_VNUM(obj));
      if (SCRIPT(obj)) {
        if (!TRIGGERS(SCRIPT(obj))->next)
          send_to_char(ch, "[T%d] ", GET_TRIG_VNUM(TRIGGERS(SCRIPT(obj))));
        else
           send_to_char(ch, "[Ʈ����] ");
      }
    }
    send_to_char(ch, "%s", CCGRN(ch, C_NRM));
    send_to_char(ch, "%s", obj->description);
    break;

  case SHOW_OBJ_SHORT:
    if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_SHOWVNUMS)) {
      send_to_char(ch, "[%d] ", GET_OBJ_VNUM(obj));
      if (SCRIPT(obj)) {
        if (!TRIGGERS(SCRIPT(obj))->next)
          send_to_char(ch, "[T%d] ", GET_TRIG_VNUM(TRIGGERS(SCRIPT(obj))));
        else
          send_to_char(ch, "[Ʈ����] ");
      }
    }
    send_to_char(ch, "%s", obj->short_description);
    break;

  case SHOW_OBJ_ACTION:
    switch (GET_OBJ_TYPE(obj)) {
    case ITEM_NOTE:
      if (obj->action_description) {
        char notebuf[MAX_NOTE_LENGTH + 64];

       snprintf(notebuf, sizeof(notebuf), "�̹� ������ ���� �ֽ��ϴ�:\r\n\r\n%s", obj->action_description);
        page_string(ch->desc, notebuf, TRUE);
      } else
	send_to_char(ch, "�ƹ��͵� �������� �ʽ��ϴ�.\r\n");
      return;

    case ITEM_DRINKCON:
      send_to_char(ch, "���Ǹ��Ѱ� ��� ������ �����ϴ�.");
      break;

    default:
      send_to_char(ch, "Ư���� ���� ã�� �� �� �����ϴ�.");
      break;
    }
    break;

  default:
    log("SYSERR: Bad display mode (%d) in show_obj_to_char().", mode);
    /*  SYSERR_DESC:  show_obj_to_char() has some predefined 'mode's (argument
     *  #3) to tell it what to display to the character when it is called.  If
     *  the mode is not one of these, it will output this error, and indicate
     *  what mode was passed to it.  To correct it, you will need to find the
     *  call with the incorrect mode and change it to an acceptable mode. */
    return;
  }
  end:

  show_obj_modifiers(obj, ch);
  send_to_char(ch, "\r\n");
}

static void show_obj_modifiers(struct obj_data *obj, struct char_data *ch)
{
  if (OBJ_FLAGGED(obj, ITEM_INVISIBLE))
    send_to_char(ch, " (����)");

  if (OBJ_FLAGGED(obj, ITEM_BLESS) && AFF_FLAGGED(ch, AFF_DETECT_ALIGN))
    send_to_char(ch, " (Ǫ����)");

  if (OBJ_FLAGGED(obj, ITEM_MAGIC) && AFF_FLAGGED(ch, AFF_DETECT_MAGIC))
    send_to_char(ch, " (�����)");

  if (OBJ_FLAGGED(obj, ITEM_GLOW))
    send_to_char(ch, " (��ä)");

  if (OBJ_FLAGGED(obj, ITEM_HUM))
    send_to_char(ch, " (�Ҹ�)");
}

static void list_obj_to_char(struct obj_data *list, struct char_data *ch, int mode, int show)
{
  struct obj_data *i, *j, *display;
  bool found;
  int num;

  found = FALSE;

  /* Loop through the list of objects */
  for (i = list; i; i = i->next_content) {
    num = 0;

    /* Check the list to see if we've already counted this object */
    for (j = list; j != i; j = j->next_content)
      if ((j->short_description == i->short_description && j->name == i->name) ||
          (!strcmp(j->short_description, i->short_description) && !strcmp(j->name, i->name)))
        break; /* found a matching object */
    if (j != i)
      continue; /* we counted object i earlier in the list */

    /* Count matching objects, including this one */
    for (display = j = i; j; j = j->next_content)
      /* This if-clause should be exactly the same as the one in the loop above */
      if ((j->short_description == i->short_description && j->name == i->name) ||
          (!strcmp(j->short_description, i->short_description) && !strcmp(j->name, i->name)))
        if (CAN_SEE_OBJ(ch, j)) {
          ++num;
          /* If the original item can't be seen, switch it for this one */
          if (display == i && !CAN_SEE_OBJ(ch, display))
            display = j;
        }

    /* When looking in room, hide objects starting with '.', except for holylight */
    if (num > 0 && (mode != SHOW_OBJ_LONG || *display->description != '.' ||
        (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_HOLYLIGHT)))) {
      if (mode == SHOW_OBJ_LONG)
        send_to_char(ch, "%s", CCGRN(ch, C_NRM));
      if (num != 1)
        send_to_char(ch, "(%2i) ", num);
      show_obj_to_char(display, ch, mode);
      send_to_char(ch, "%s", CCNRM(ch, C_NRM));
      found = TRUE;
    }
  }
  if (!found && show)
    send_to_char(ch, "  �ƹ��͵� �����ϴ�.\r\n");
}

static void diag_char_to_char(struct char_data *i, struct char_data *ch)
{
  struct {
    byte percent;
    const char *text;
  } diagnosis[] = {
    { 100, "ü�� 100%"			},
    {  90, "ü�� 90%~99%"	},
    {  75, "ü�� 75%~89%"	},
    {  50, "ü�� 50%~74%"	},
    {  30, "ü�� 30%~49%"		},
    {  15, "ü�� 15%~29%"		},
    {   0, "ü�� 0%~14%"		},
    {  -1, "ü�� ���̳���"	}
  };
  int percent, ar_index;
  const char *pers = PERS(i, ch);

  if (GET_MAX_HIT(i) > 0)
    percent = (100 * GET_HIT(i)) / GET_MAX_HIT(i);
  else
    percent = -1;		/* How could MAX_HIT be < 1?? */

//  for (ar_index = 0; diagnosis[ar_index].percent >= 0; ar_index++)
//    if (percent >= diagnosis[ar_index].percent)
//      break;

  send_to_char(ch, "%s ���� ü��: %d%\r\n", pers, percent);
}

static void look_at_char(struct char_data *i, struct char_data *ch)
{
  int j, found;

  if (!ch->desc)
    return;

   if (i->player.description)
    send_to_char(ch, "%s", i->player.description);
//  else
//    act("$m���Լ� Ư���� ���� ã�� �� �������ϴ�.", FALSE, i, 0, ch, TO_VICT);

  diag_char_to_char(i, ch);

  found = FALSE;
  for (j = 0; !found && j < NUM_WEARS; j++)
    if (GET_EQ(i, j) && CAN_SEE_OBJ(ch, GET_EQ(i, j)))
      found = TRUE;

  if (found) {
    send_to_char(ch, "\r\n");	/* act() does capitalization. */
      act("$n$d ���:", FALSE, i, 0, ch, TO_VICT);
    for (j = 0; j < NUM_WEARS; j++)
      if (GET_EQ(i, j) && CAN_SEE_OBJ(ch, GET_EQ(i, j))) {
	send_to_char(ch, "%s", wear_where[j]);
	show_obj_to_char(GET_EQ(i, j), ch, SHOW_OBJ_SHORT);
      }
  }
  if (ch != i && (IS_THIEF(ch) || GET_LEVEL(ch) >= LVL_IMMORT)) {
    act("\r\n$s ����ǰ�� �����ϴ�:", FALSE, i, 0, ch, TO_VICT);
    list_obj_to_char(i->carrying, ch, SHOW_OBJ_SHORT, TRUE);
  }
}

static void list_one_char(struct char_data *i, struct char_data *ch)
{
  struct obj_data *furniture;
  const char *positions[] = {
    " �׾��ֽ��ϴ�.",
    " �������ֽ��ϴ�.",
    " �����ϰ� �ֽ��ϴ�.",
    " ������ �ֽ��ϴ�.",
    " �ڰ� �ֽ��ϴ�.",
    " ���� �ֽ��ϴ�.",
    " �ɾ� �ֽ��ϴ�.",
    " �ο�� �ֽ��ϴ�!",
    " �� �ֽ��ϴ�."
  };

  if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_SHOWVNUMS)) {
    if (IS_NPC(i))
    send_to_char(ch, "[%d] ", GET_MOB_VNUM(i));
    if (SCRIPT(i) && TRIGGERS(SCRIPT(i))) {
      if (!TRIGGERS(SCRIPT(i))->next)
        send_to_char(ch, "[T%d] ", GET_TRIG_VNUM(TRIGGERS(SCRIPT(i))));
      else
        send_to_char(ch, "[Ʈ����] ");
    }
  }

  if (GROUP(i)) {
    if (GROUP(i) == GROUP(ch))
      send_to_char(ch, "(%s%s%s) ", CBGRN(ch, C_NRM),
	GROUP_LEADER(GROUP(i)) == i ? "����" : "�׷��",
        CCNRM(ch, C_NRM));
    else
      send_to_char(ch, "(%s%s%s) ", CBRED(ch, C_NRM),
        GROUP_LEADER(GROUP(i)) == i ? "����" : "�׷��",
	CCNRM(ch, C_NRM));
  }

  if (IS_NPC(i) && i->player.long_descr && GET_POS(i) == GET_DEFAULT_POS(i)) {
    if (AFF_FLAGGED(i, AFF_INVISIBLE))
      send_to_char(ch, "*");

    if (AFF_FLAGGED(ch, AFF_DETECT_ALIGN)) {
      if (IS_EVIL(i))
	send_to_char(ch, "(����) ");
      else if (IS_GOOD(i))
	send_to_char(ch, "(����) ");
    }
    send_to_char(ch, "%s", i->player.long_descr);

    if (AFF_FLAGGED(i, AFF_SANCTUARY))
      act("...$e ���� ���� �۽ο� �ֽ��ϴ�!", FALSE, i, 0, ch, TO_VICT);
    if (AFF_FLAGGED(i, AFF_BLIND) && GET_LEVEL(i) < LVL_IMMORT)
      act("...$e ���� �־� �ֽ��ϴ�.", FALSE, i, 0, ch, TO_VICT);

    return;
  }

  if (IS_NPC(i))
    send_to_char(ch, "%c%s", UPPER(*i->player.short_descr), i->player.short_descr + 1);
  else
    send_to_char(ch, "%s%s%s", i->player.name, *GET_TITLE(i) ? " " : "", GET_TITLE(i));

  if (AFF_FLAGGED(i, AFF_INVISIBLE))
    send_to_char(ch, " (����)");
  if (AFF_FLAGGED(i, AFF_HIDE))
    send_to_char(ch, " (����)");
  if (!IS_NPC(i) && !i->desc)
    send_to_char(ch, " (���Ӳ���)");
  if (!IS_NPC(i) && PLR_FLAGGED(i, PLR_WRITING))
    send_to_char(ch, " (�۾���)");
  if (!IS_NPC(i) && PRF_FLAGGED(i, PRF_BUILDWALK))
    send_to_char(ch, " (������)");
  if (!IS_NPC(i) && PRF_FLAGGED(i, PRF_AFK))
    send_to_char(ch, " (��������)");

  if (GET_POS(i) != POS_FIGHTING) {
    if (!SITTING(i))
      send_to_char(ch, "%s", positions[(int) GET_POS(i)]);
  else {
    furniture = SITTING(i);
    send_to_char(ch, " is %s upon %s.", (GET_POS(i) == POS_SLEEPING ?
        "�ڴ���" : (GET_POS(i) == POS_RESTING ? "�޽���" : "�ɾ��ִ���")),
        OBJS(furniture, ch));
  }
  } else {
    if (FIGHTING(i)) {
		send_to_char(ch, " �ο�� �ֽ��ϴ�!");
      if (FIGHTING(i) == ch)
		send_to_char(ch, " ȥ�� �ܷӰ� �ο�� �ֽ��ϴ�!");
      else {
	if (IN_ROOM(i) == IN_ROOM(FIGHTING(i)))
	  send_to_char(ch, "%s!", PERS(FIGHTING(i), ch));
	else
send_to_char(ch,  "�̹� ���� ��������");
      }
    } else			/* NIL fighting pointer */
      send_to_char(ch, " ����� ���.");
  }

  if (AFF_FLAGGED(ch, AFF_DETECT_ALIGN)) {
    if (IS_EVIL(i))
      send_to_char(ch, " (����)");
    else if (IS_GOOD(i))
      send_to_char(ch, " (����)");
  }
  send_to_char(ch, "\r\n");

  if (AFF_FLAGGED(i, AFF_SANCTUARY))
    act("...$e ���� ���� �۽ο� �ֽ��ϴ�!", FALSE, i, 0, ch, TO_VICT);
}

static void list_char_to_char(struct char_data *list, struct char_data *ch)
{
  struct char_data *i;

  for (i = list; i; i = i->next_in_room)
    if (ch != i) {
      /* hide npcs whose description starts with a '.' from non-holylighted people - Idea from Elaseth of TBA */
      if (!IS_NPC(ch) && !PRF_FLAGGED(ch, PRF_HOLYLIGHT) &&
      	   IS_NPC(i) && i->player.long_descr && *i->player.long_descr == '.')
        continue;
      send_to_char(ch, "%s", CCYEL(ch, C_NRM));
      if (CAN_SEE(ch, i))
        list_one_char(i, ch);
      else if (IS_DARK(IN_ROOM(ch)) && !CAN_SEE_IN_DARK(ch) &&
	       AFF_FLAGGED(i, AFF_INFRAVISION))
        send_to_char(ch, "����� �δ��� �Ӱ� Ÿ������ �ձ��� ���� �ݴϴ�.\r\n");
      send_to_char(ch, "%s", CCNRM(ch, C_NRM));
    }
}

static void do_auto_exits(struct char_data *ch)
{
  int door, slen = 0;

  send_to_char(ch, "%s[ �ⱸ: ", CCCYN(ch, C_NRM));

  for (door = 0; door < DIR_COUNT; door++) {
    if (!EXIT(ch, door) || EXIT(ch, door)->to_room == NOWHERE)
      continue;
    if (EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED) && !CONFIG_DISP_CLOSED_DOORS)
      continue;
	if (EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN) && !PRF_FLAGGED(ch, PRF_HOLYLIGHT))
	  continue;
    if (EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED))
	  send_to_char(ch, "%s(%s)%s ", EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN) ? CCWHT(ch, C_NRM) : CCRED(ch, C_NRM), autoexits[door], CCCYN(ch, C_NRM));
	else if (EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN))
	  send_to_char(ch, "%s%s%s ", CCWHT(ch, C_NRM), autoexits[door], CCCYN(ch, C_NRM));
    else
      send_to_char(ch, "\t(%s\t) ", autoexits[door]);
    slen++;
  }

  send_to_char(ch, "%s]%s\r\n", slen ? "" : "����!", CCNRM(ch, C_NRM));
}

ACMD(do_exits)
{
  int door, len = 0;

  if (AFF_FLAGGED(ch, AFF_BLIND) && GET_LEVEL(ch) < LVL_IMMORT) {
    send_to_char(ch, "����� ���� �־� ���� �� �� �����ϴ�!\r\n");
    return;
  }

  send_to_char(ch, "�̵� ������ �ⱸ:\r\n");

  for (door = 0; door < DIR_COUNT; door++) {
    if (!EXIT(ch, door) || EXIT(ch, door)->to_room == NOWHERE)
      continue;
    if (EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED) && !CONFIG_DISP_CLOSED_DOORS)
      continue;
    if (EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN) && !PRF_FLAGGED(ch, PRF_HOLYLIGHT))
	  continue;

    len++;

    if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_SHOWVNUMS) && !EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED))
      send_to_char(ch, "%-5s - [%5d]%s %s\r\n", dirs[door], GET_ROOM_VNUM(EXIT(ch, door)->to_room),
      EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN) ? " [������]" : "", world[EXIT(ch, door)->to_room].name);
    else if (CONFIG_DISP_CLOSED_DOORS && EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED)) {
      /* But we tell them the door is closed */
      send_to_char(ch, "%-5s - %s%s ���� �ֽ��ϴ�.\r\n", dirs[door],
		(EXIT(ch, door)->keyword)? fname(EXIT(ch, door)->keyword) : "��������",
		check_josa(fname(EXIT(ch, door)->keyword), 0), EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN) ? " [������]" : ".");
      }
    else
      send_to_char(ch, "%-5s - %s\r\n", dirs[door], IS_DARK(EXIT(ch, door)->to_room) &&
		!CAN_SEE_IN_DARK(ch) ? "�ʹ� ��ӽ��ϴ�." : world[EXIT(ch, door)->to_room].name);
  }

  if (!len)
   send_to_char(ch, " ����.\r\n");
}

void look_at_room(struct char_data *ch, int ignore_brief)
{
  trig_data *t;
  struct room_data *rm = &world[IN_ROOM(ch)];
  room_vnum target_room;

  target_room = IN_ROOM(ch);

  if (!ch->desc)
    return;

  if (IS_DARK(IN_ROOM(ch)) && !CAN_SEE_IN_DARK(ch)) {
    send_to_char(ch, "��ο��� �ƹ��͵� �� �� �����ϴ�.\r\n");
    return;
  } else if (AFF_FLAGGED(ch, AFF_BLIND) && GET_LEVEL(ch) < LVL_IMMORT) {
    send_to_char(ch, "����� ���� �־� �� �� �����ϴ�.\r\n");
    return;
  }
  send_to_char(ch, "%s", CCCYN(ch, C_NRM));
  if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_SHOWVNUMS)) {
    char buf[MAX_STRING_LENGTH];

    sprintbitarray(ROOM_FLAGS(IN_ROOM(ch)), room_bits, RF_ARRAY_MAX, buf);
    send_to_char(ch, "[%5d] ", GET_ROOM_VNUM(IN_ROOM(ch)));
    send_to_char(ch, "%s [ %s] [ %s ]", world[IN_ROOM(ch)].name, buf, sector_types[world[IN_ROOM(ch)].sector_type]);

    if (SCRIPT(rm)) {
      send_to_char(ch, "[T");
      for (t = TRIGGERS(SCRIPT(rm)); t; t = t->next)
        send_to_char(ch, " %d", GET_TRIG_VNUM(t));
      send_to_char(ch, "]");
    }
  }
  else
    send_to_char(ch, "%s", world[IN_ROOM(ch)].name);
  send_to_char(ch, "%s\r\n", CCNRM(ch, C_NRM));

  if ((!IS_NPC(ch) && !PRF_FLAGGED(ch, PRF_BRIEF)) || ignore_brief ||
      ROOM_FLAGGED(IN_ROOM(ch), ROOM_DEATH)) {
    if(!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_AUTOMAP) && can_see_map(ch))
        str_and_map(world[target_room].description, ch, target_room);
    else
      send_to_char(ch, "%s", world[IN_ROOM(ch)].description);
  }

  /* autoexits */
  if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_AUTOEXIT))
    do_auto_exits(ch);

  /* now list characters & objects */
  list_obj_to_char(world[IN_ROOM(ch)].contents, ch, SHOW_OBJ_LONG, FALSE);
  list_char_to_char(world[IN_ROOM(ch)].people, ch);
}

static void look_in_direction(struct char_data *ch, int dir)
{
  if (EXIT(ch, dir)) {
    if (EXIT(ch, dir)->general_description)
      send_to_char(ch, "%s", EXIT(ch, dir)->general_description);
    else
      send_to_char(ch, "Ư���� ���� ã�� �� �� �����ϴ�.\r\n");

    if (EXIT_FLAGGED(EXIT(ch, dir), EX_CLOSED) && EXIT(ch, dir)->keyword)
      send_to_char(ch, "%s%s �������ϴ�.\r\n", fname(EXIT(ch, dir)->keyword),
		check_josa(fname(EXIT(ch, dir)->keyword), 0));
    else if (EXIT_FLAGGED(EXIT(ch, dir), EX_ISDOOR) && EXIT(ch, dir)->keyword)
      send_to_char(ch, "%s%s ���Ƚ��ϴ�.\r\n", fname(EXIT(ch, dir)->keyword),
		check_josa(fname(EXIT(ch, dir)->keyword), 4));
  } else
    send_to_char(ch, "Ư���� ���� ã�� �� �� �����ϴ�.\r\n");
}

static void look_in_obj(struct char_data *ch, char *arg)
{
  struct obj_data *obj = NULL;
  struct char_data *dummy = NULL;
  int amt, bits;

  if (!*arg)
    send_to_char(ch, "������ ���ǰ̴ϱ�?\r\n");
  else if (!(bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM |
				 FIND_OBJ_EQUIP, ch, &dummy, &obj))) {
    send_to_char(ch, "%s%s ã���� �����ϴ�.\r\n", arg, check_josa(arg, 1));
  } else if ((GET_OBJ_TYPE(obj) != ITEM_DRINKCON) &&
	     (GET_OBJ_TYPE(obj) != ITEM_FOUNTAIN) &&
	     (GET_OBJ_TYPE(obj) != ITEM_CONTAINER))
    send_to_char(ch, "�ȿ��� �ƹ��͵� �����ϴ�!\r\n");
  else {
    if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER) {
      if (OBJVAL_FLAGGED(obj, CONT_CLOSED) && (GET_LEVEL(ch) < LVL_IMMORT || !PRF_FLAGGED(ch, PRF_NOHASSLE)))
	send_to_char(ch, "���� �ֽ��ϴ�.\r\n");
      else {
	send_to_char(ch, "%s", fname(obj->name));
	switch (bits) {
	case FIND_OBJ_INV:
	  send_to_char(ch, " (����ǰ): \r\n");
	  break;
	case FIND_OBJ_ROOM:
	  send_to_char(ch, " (����): \r\n");
	  break;
	case FIND_OBJ_EQUIP:
	  send_to_char(ch, " (���): \r\n");
	  break;
	}

	list_obj_to_char(obj->contains, ch, SHOW_OBJ_SHORT, TRUE);
      }
    } else {		/* item must be a fountain or drink container */
      if ((GET_OBJ_VAL(obj, 1) == 0) && (GET_OBJ_VAL(obj, 0) != -1))
	send_to_char(ch, "�װ��� �� ����ֽ��ϴ�.\r\n");
      else {
        if (GET_OBJ_VAL(obj, 0) < 0)
        {
          char buf2[MAX_STRING_LENGTH];
          sprinttype(GET_OBJ_VAL(obj, 2), color_liquid, buf2, sizeof(buf2));
          send_to_char(ch, "%s%s ���� �� �ֽ��ϴ�.\r\n", buf2, check_josa(buf2, 0));
        }
	else if (GET_OBJ_VAL(obj,1)>GET_OBJ_VAL(obj,0))
         send_to_char(ch, "���� �߸��� �� �����ϴ�.\r\n");/* BUG */
        else {
          char buf2[MAX_STRING_LENGTH];
	  amt = (GET_OBJ_VAL(obj, 1) * 3) / GET_OBJ_VAL(obj, 0);
	  sprinttype(GET_OBJ_VAL(obj, 2), color_liquid, buf2, sizeof(buf2));
		  send_to_char(ch, "%s%s%s �� �ֽ��ϴ�.\r\n", buf2, fullness[amt], check_josa(buf2, 0));
	}
      }
    }
  }
}

char *find_exdesc(char *word, struct extra_descr_data *list)
{
  struct extra_descr_data *i;

  for (i = list; i; i = i->next)
    if (*i->keyword == '.' ? isname(word, i->keyword + 1) : isname(word, i->keyword))
      return (i->description);

  return (NULL);
}

/* Given the argument "look at <target>", figure out what object or char
 * matches the target.  First, see if there is another char in the room with
 * the name.  Then check local objs for exdescs. Thanks to Angus Mezick for
 * the suggested fix to this problem. */
static void look_at_target(struct char_data *ch, char *arg)
{
  int bits, found = FALSE, j, fnum, i = 0;
  struct char_data *found_char = NULL;
  struct obj_data *obj, *found_obj = NULL;
  char *desc;

  if (!ch->desc)
    return;

  if (!*arg) {
     send_to_char(ch, "������ ���ðڽ��ϱ�?\r\n");
    return;
  }

  bits = generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP |
		      FIND_CHAR_ROOM, ch, &found_char, &found_obj);

  /* Is the target a character? */
  if (found_char != NULL) {
    look_at_char(found_char, ch);
    if (ch != found_char) {
      if (CAN_SEE(found_char, ch)) {
		act("$n$j ����� ���ϴ�.", TRUE, ch, 0, found_char, TO_VICT);
		act("$n$j $N$J ���ϴ�.", TRUE, ch, 0, found_char, TO_NOTVICT);
	  }
	 }
    return;
  }

  /* Strip off "number." from 2.foo and friends. */
  if (!(fnum = get_number(&arg))) {
    send_to_char(ch, "������ �����?\r\n");
    return;
  }

  /* Does the argument match an extra desc in the room? */
  if ((desc = find_exdesc(arg, world[IN_ROOM(ch)].ex_description)) != NULL && ++i == fnum) {
    page_string(ch->desc, desc, FALSE);
    return;
  }

  /* Does the argument match an extra desc in the char's equipment? */
  for (j = 0; j < NUM_WEARS && !found; j++)
    if (GET_EQ(ch, j) && CAN_SEE_OBJ(ch, GET_EQ(ch, j)))
      if ((desc = find_exdesc(arg, GET_EQ(ch, j)->ex_description)) != NULL && ++i == fnum) {
	send_to_char(ch, "%s", desc);
	found = TRUE;
      }

  /* Does the argument match an extra desc in the char's inventory? */
  for (obj = ch->carrying; obj && !found; obj = obj->next_content) {
    if (CAN_SEE_OBJ(ch, obj))
      if ((desc = find_exdesc(arg, obj->ex_description)) != NULL && ++i == fnum) {
	send_to_char(ch, "%s", desc);
	found = TRUE;
      }
  }

  /* Does the argument match an extra desc of an object in the room? */
  for (obj = world[IN_ROOM(ch)].contents; obj && !found; obj = obj->next_content)
    if (CAN_SEE_OBJ(ch, obj))
      if ((desc = find_exdesc(arg, obj->ex_description)) != NULL && ++i == fnum) {
	send_to_char(ch, "%s", desc);
	found = TRUE;
      }

  /* If an object was found back in generic_find */
  if (bits) {
    if (!found)
      show_obj_to_char(found_obj, ch, SHOW_OBJ_ACTION);
    else {
      show_obj_modifiers(found_obj, ch);
      send_to_char(ch, "\r\n");
    }
  } else if (!found)
   send_to_char(ch, "�׷����� �����ϴ�.\r\n");
}

ACMD(do_look)
{
  int look_type;
  int found = 0;
  char tempsave[MAX_INPUT_LENGTH];

  if (!ch->desc)
    return;

  if (GET_POS(ch) < POS_SLEEPING)
    send_to_char(ch, "�켱 �ῡ�� ������?\r\n");
  else if (AFF_FLAGGED(ch, AFF_BLIND) && GET_LEVEL(ch) < LVL_IMMORT)
    send_to_char(ch, "����� ���� �־��ֽ��ϴ�!\r\n");
  else if (IS_DARK(IN_ROOM(ch)) && !CAN_SEE_IN_DARK(ch)) {
    send_to_char(ch, "�ֺ��� �ʹ� ��ӽ��ϴ�.\r\n");
    list_char_to_char(world[IN_ROOM(ch)].people, ch);	/* glowing red eyes */
  } else {
    char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

    half_chop(argument, arg, arg2);

    if (subcmd == SCMD_READ) {
      if (!*arg)
	send_to_char(ch, "������ �������?\r\n");
      else
	look_at_target(ch, strcpy(tempsave, arg));
      return;
    }
    if (!*arg)			/* "look" alone, without an argument at all */
      look_at_room(ch, 1);
    else if (is_abbrev(arg, "��"))
      look_in_obj(ch, arg2);
    /* did the char type 'look <direction>?' */
    else if ((look_type = search_block(arg, dirs, FALSE)) >= 0)
      look_in_direction(ch, look_type);
    else if (is_abbrev(arg, "���") || is_abbrev(arg, "����ǰ"))
      look_at_target(ch, arg2);
    else if (is_abbrev(arg, "�ֺ�")) {
      struct extra_descr_data *i;

      for (i = world[IN_ROOM(ch)].ex_description; i; i = i->next) {
        if (*i->keyword != '.') {
          send_to_char(ch, "%s%s:\r\n%s",
          (found ? "\r\n" : ""), i->keyword, i->description);
          found = 1;
        }
      }
      if (!found)
         send_to_char(ch, "�ƹ��͵� ���ý��ϴ�.\r\n");
    } else
      look_at_target(ch, strcpy(tempsave, arg));
  }
}

ACMD(do_examine)
{
  struct char_data *tmp_char;
  struct obj_data *tmp_object;
  char tempsave[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "������ �����ұ��?\r\n");
    return;
  }

  /* look_at_target() eats the number. */
  look_at_target(ch, strcpy(tempsave, arg));	/* strcpy: OK */

  generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_CHAR_ROOM |
		      FIND_OBJ_EQUIP, ch, &tmp_char, &tmp_object);

  if (tmp_object) {
    if ((GET_OBJ_TYPE(tmp_object) == ITEM_DRINKCON) ||
	(GET_OBJ_TYPE(tmp_object) == ITEM_FOUNTAIN) ||
	(GET_OBJ_TYPE(tmp_object) == ITEM_CONTAINER)) {
      send_to_char(ch, "���� �鿩�� ���ҽ��ϴ�:\r\n");
      look_in_obj(ch, arg);
    }
  }
}

ACMD(do_gold)
{
  if (GET_GOLD(ch) == 0)
    send_to_char(ch, "��Ǭ�� �����ϴ�!\r\n");
  else if (GET_GOLD(ch) == 1)
    send_to_char(ch, "�޶� �����ϳ� ������ �ֽ��ϴ�.\r\n");
  else
    send_to_char(ch, "����� %d���� ������ �ֽ��ϴ�.\r\n", GET_GOLD(ch));
}

ACMD(do_score)
{
  struct time_info_data playing_time;

  if (IS_NPC(ch))
    return;

   send_to_char(ch, "����� %d�� �Դϴ�", GET_AGE(ch));

  if (age(ch)->month == 0 && age(ch)->day == 0)
   send_to_char(ch, "  ������ ����� �¾ ���Դϴ�.\r\n");
  else
    send_to_char(ch, "\r\n");
  send_to_char(ch, "��: %d  ��ø: %d  ����: %d  ����: %d  ����: %d  ��ַ�: %d  ���: %d\r\n",
	  GET_STR(ch), GET_DEX(ch), GET_CON(ch), GET_INT(ch),
	  GET_WIS(ch), GET_CHA(ch), GET_LUCK(ch));
  send_to_char(ch, "���� ��������Ʈ: %d  ���˵�: %d\r\n", GET_POINT(ch), GET_CRIME(ch));

  send_to_char(ch, "ü��: %d(%d), ������: %d(%d), �̵���: %d(%d)\r\n",
	  GET_HIT(ch), GET_MAX_HIT(ch), GET_MANA(ch), GET_MAX_MANA(ch),
	  GET_MOVE(ch), GET_MAX_MOVE(ch));

  send_to_char(ch, "����� ������ %d/10 �̰� ������ %d�Դϴ�.\r\n",
	  compute_armor_class(ch), GET_ALIGNMENT(ch));

  send_to_char(ch, "����� ���� %d���� ����ġ�� %d���� ��ȭ, %d���� �ӹ������� ������ �ֽ��ϴ�.\r\n",
	  GET_EXP(ch), GET_GOLD(ch), GET_QUESTPOINTS(ch));

  if (GET_LEVEL(ch) < LVL_IMMORT)
    send_to_char(ch, "���� ������ �Ǳ����� �ʿ��� ����ġ�� %d�� �Դϴ�.\r\n",
	level_exp(GET_CLASS(ch), GET_LEVEL(ch) + 1) - GET_EXP(ch));
  send_to_char(ch, "����� %d���� �ӹ������� ������ ������, ", GET_QUESTPOINTS(ch));
  send_to_char(ch, "����� ���� %d���� �ӹ��� �ϼ� �Ͽ����ϴ�.\r\n", GET_NUM_QUESTS(ch));
  if (GET_QUEST(ch) == NOTHING)
    send_to_char(ch, "����� ���� �������� �ӹ��� �����ϴ�.\r\n");
  else
  {
    send_to_char(ch, "����� ���� �������� �ӹ��� %s �Դϴ�.", QST_NAME(real_quest(GET_QUEST(ch))));

    if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_SHOWVNUMS))
        send_to_char(ch, " [%d]\r\n", GET_QUEST(ch));
    else
        send_to_char(ch, "\r\n");
  }

  playing_time = *real_time_passed((time(0) - ch->player.time.logon) +
				  ch->player.time.played, 0);
  send_to_char(ch, "����� ���� %d�� %d�ð� ���� �� ������ �÷����� �Դϴ�.\r\n", playing_time.day, playing_time.hours);

  send_to_char(ch, "����� ���� Ÿ��Ʋ�� %s %s (%d) �Դϴ�.\r\n",
	  GET_NAME(ch), GET_TITLE(ch), GET_LEVEL(ch));

  switch (GET_POS(ch)) {
  case POS_DEAD:	  
    send_to_char(ch, "����� �׾����ϴ�!\r\n");
    break;
  case POS_MORTALLYW:
    send_to_char(ch, "����� �ױ� �����Դϴ�! ������ �ʿ��մϴ�!\r\n");
    break;
  case POS_INCAP:
    send_to_char(ch, "����� ������� �����Դϴ�!\r\n");
    break;
  case POS_STUNNED:
    send_to_char(ch, "����� �����ؼ� ������ �� �����ϴ�!\r\n");
    break;
  case POS_SLEEPING:
    send_to_char(ch, "����� �ڰ� �ֽ��ϴ�.\r\n");
    break;
  case POS_RESTING:
    send_to_char(ch, "����� ���� �ֽ��ϴ�.\r\n");
    break;
  case POS_SITTING:
    if (!SITTING(ch))
      send_to_char(ch, "����� �ɾ� �ֽ��ϴ�.\r\n");
    else {
      struct obj_data *furniture = SITTING(ch);
      send_to_char(ch, "You are sitting upon %s.\r\n", furniture->short_description);
    }
    break;
  case POS_FIGHTING:
        send_to_char(ch, "%s �ο�� �ֽ��ϴ�.\r\n", FIGHTING(ch) ? PERS(FIGHTING(ch), ch) : "�ڽ�", check_josa(PERS(FIGHTING(ch), ch), 2));
    break;
  case POS_STANDING:
    send_to_char(ch, "����� �Ͼ �ֽ��ϴ�.\r\n");
    break;
  default:
    send_to_char(ch, "����� ����ó�� ���ٴմϴ�.\r\n");
    break;
  }
  if (GET_COND(ch, DRUNK) > 10)
    send_to_char(ch, "���� �����ֽ��ϴ�.\r\n");

  if (GET_COND(ch, HUNGER) == 0)
    send_to_char(ch, "����� �����̻�: ��, ��ø 50% ����\r\n");

  if (GET_COND(ch, THIRST) == 0)
    send_to_char(ch, "�񸶸� �����̻�: ����, ���� 50% ����\r\n");

  if (AFF_FLAGGED(ch, AFF_BLIND) && GET_LEVEL(ch) < LVL_IMMORT)
    send_to_char(ch, "���� �־� �ֽ��ϴ�!\r\n");

  if (AFF_FLAGGED(ch, AFF_INVISIBLE))
    send_to_char(ch, "������� �Դϴ�.\r\n");

  if (AFF_FLAGGED(ch, AFF_DETECT_INVIS))
    send_to_char(ch, "�����Ѱ��� ������ �� �ֽ��ϴ�.\r\n");

  if (AFF_FLAGGED(ch, AFF_SANCTUARY))
    send_to_char(ch, "�������� ���Դϴ�.\r\n");

  if (AFF_FLAGGED(ch, AFF_POISON))
    send_to_char(ch, "���� �ߵ��Ǿ� �ֽ��ϴ�!\r\n");

  if (AFF_FLAGGED(ch, AFF_CHARM))
    send_to_char(ch, "���ָ� �ް� �ֽ��ϴ�!\r\n");

  if (affected_by_spell(ch, SPELL_ARMOR))
    send_to_char(ch, "������ ��ȣ�� �ް� �ֽ��ϴ�.\r\n");

  if (AFF_FLAGGED(ch, AFF_INFRAVISION))
    send_to_char(ch, "��ο���� �� �� �ֽ��ϴ�.\r\n");

  if (PRF_FLAGGED(ch, PRF_SUMMONABLE))
    send_to_char(ch, "�ٸ�����ڿ��� ��ȯ ���� �մϴ�.\r\n");

  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    if (POOFIN(ch))
      send_to_char(ch, "%s����޽���:  %s%s %s%s\r\n", QYEL, QCYN, GET_NAME(ch), POOFIN(ch), QNRM);
    else
      send_to_char(ch, "%s����޽���:  %s%s%s\r\n", QYEL, QCYN, GET_NAME(ch), QNRM);

    if (POOFOUT(ch))
      send_to_char(ch, "%s����޽���: %s%s %s%s\r\n", QYEL, QCYN, GET_NAME(ch), POOFOUT(ch), QNRM);
    else
      send_to_char(ch, "%s����޽���: %s%s%s\r\n", QYEL, QCYN, GET_NAME(ch), QNRM);
  }
}

ACMD(do_inventory)
{
  send_to_char(ch, "����� ����ǰ:\r\n");
  list_obj_to_char(ch->carrying, ch, SHOW_OBJ_SHORT, TRUE);
}

ACMD(do_equipment)
{
  int i, found = 0;

 send_to_char(ch, "����� ���:\r\n");
  for (i = 0; i < NUM_WEARS; i++) {
    if (GET_EQ(ch, i)) {
      found = TRUE;
      if (CAN_SEE_OBJ(ch, GET_EQ(ch, i))) {
        send_to_char(ch, "%s", wear_where[i]);
        show_obj_to_char(GET_EQ(ch, i), ch, SHOW_OBJ_SHORT);
      } else {
        send_to_char(ch, "%s", wear_where[i]);
        send_to_char(ch, "����...\r\n");
      }
    }
  }
  if (!found)
    send_to_char(ch, " ����.\r\n");
}

ACMD(do_time)
{
  int weekday, day;

  /* day in [1..35] */
  day = time_info.day + 1;

  /* 35 days in a month, 7 days a week */
  weekday = ((35 * time_info.month) + day) % 7;

  send_to_char(ch, "���� �ð��� %s %d�� %s �Դϴ�.\r\n",
	  time_info.hours >= 12 ? "����" : "����",
	  (time_info.hours % 12 == 0) ? 12 : (time_info.hours % 12),
	  weekdays[weekday]);

  send_to_char(ch, "������ %d�� %s %d�� �Դϴ�.\r\n", time_info.year, month_name[time_info.month], day);
}

ACMD(do_weather)
{
  const char *sky_look[] = {
    "�������� ����",
    "�帮��",
    "�񰡿���",
    "������ ġ��"
  };

  if (OUTSIDE(ch))
        {
    send_to_char(ch, "�ϴ��� %s %s.\r\n", sky_look[weather_info.sky],
	    weather_info.change >= 0 ? "���ʿ��� ������ �ٶ��� �Ҿ�ɴϴ�." :
	     "�� ������ ������ �� �����ϴ�");
    if (GET_LEVEL(ch) >= LVL_GOD)
      send_to_char(ch, "���: %d (����: %d), �ϴ�: %d (%s)\r\n",
                 weather_info.pressure,
                 weather_info.change,
                 weather_info.sky,
                 sky_look[weather_info.sky]);
    }
  else
   send_to_char(ch, "�ǳ��� �־ ������ ��� ���� �� �� �����ϴ�.\r\n");
}

/* puts -'s instead of spaces */
void space_to_minus(char *str)
{
  while ((str = strchr(str, ' ')) != NULL)
    *str = '-';
}

int search_help(const char *argument, int level)
{
  int chk, bot, top, mid, minlen;

   bot = 0;
   top = top_of_helpt;
   minlen = strlen(argument);

  while (bot <= top) {
    mid = (bot + top) / 2;

    if (!(chk = strn_cmp(argument, help_table[mid].keywords, minlen)))  {
      while ((mid > 0) && !strn_cmp(argument, help_table[mid - 1].keywords, minlen))
         mid--;

      while (level < help_table[mid].min_level && mid < (bot + top) / 2)
        mid++;
      if (strn_cmp(argument, help_table[mid].keywords, minlen) || level < help_table[mid].min_level)
        break;
        
      return (mid);
    }
    else if (chk > 0)
      bot = mid + 1;
    else
      top = mid - 1;
  }
  return NOWHERE;
}

ACMD(do_help)
{
  int mid = 0;
  int i, found = 0;

    if (!ch->desc)
    return;

  skip_spaces(&argument);

  if (!help_table) {
   send_to_char(ch, "�� �׸��� ������ �����ϴ�.\r\n");
    return;
  }

  if (!*argument) {
    if (GET_LEVEL(ch) < LVL_IMMORT)
      page_string(ch->desc, help, 0);
    else
      page_string(ch->desc, ihelp, 0);
    return;
  }

  space_to_minus(argument);

  if ((mid = search_help(argument, GET_LEVEL(ch))) == NOWHERE) {
    send_to_char(ch, "�� �׸��� ������ �����ϴ�.\r\n");
    mudlog(NRM, MIN(LVL_IMPL, GET_INVIS_LEV(ch)), TRUE,
      "%s tried to get help on %s", GET_NAME(ch), argument);
    for (i = 0; i < top_of_helpt; i++)  {
      if (help_table[i].min_level > GET_LEVEL(ch))
        continue;
      /* To help narrow down results, if they don't start with the same letters, move on. */
      if (*argument != *help_table[i].keywords)
        continue;
      if (levenshtein_distance(argument, help_table[i].keywords) <= 2) {
        if (!found) {
          send_to_char(ch, "\r\n���� ����:\r\n");
          found = 1;
        }
       send_to_char(ch, "  %s\r\n", help_table[i].keywords);
      }
    }
    return;
  }
  page_string(ch->desc, help_table[mid].entry, 0);
}

#define WHO_FORMAT \
"����: [�ּҷ���[-�ִ뷹��]] [-n �̸�] [-c ����] [-k] [-l] [-n] [-q] [-r] [-s] [-z] ����\r\n"

/* Written by Rhade */
ACMD(do_who)
{
  struct descriptor_data *d;
  struct char_data *tch;
  int i, num_can_see = 0;
  char name_search[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH];
  char mode;
  int low = 0, high = LVL_IMPL, localwho = 0, questwho = 0;
  int showclass = 0, short_list = 0, outlaws = 0;
  int who_room = 0, showgroup = 0, showleader = 0;

  struct {
    char *disp;
    int min_level;
    int max_level;
    int count; /* must always start as 0 */
  } rank[] = {
    { "������\r\n---------\r\n", LVL_IMMORT, LVL_IMPL, 0},
    { "�����\r\n-------\r\n", 1, LVL_IMMORT - 1, 0 },
    { "\n", 0, 0, 0 }
  };

  skip_spaces(&argument);
  strcpy(buf, argument);   /* strcpy: OK (sizeof: argument == buf) */
  name_search[0] = '\0';

  while (*buf) {
    char arg[MAX_INPUT_LENGTH], buf1[MAX_INPUT_LENGTH];

    half_chop(buf, arg, buf1);
    if (isdigit(*arg)) {
      sscanf(arg, "%d-%d", &low, &high);
      strcpy(buf, buf1);   /* strcpy: OK (sizeof: buf1 == buf) */
    } else if (*arg == '-') {
      mode = *(arg + 1);       /* just in case; we destroy arg in the switch */
      switch (mode) {
      case 'k':
        outlaws = 1;
        strcpy(buf, buf1);   /* strcpy: OK (sizeof: buf1 == buf) */
        break;
      case 'z':
        localwho = 1;
        strcpy(buf, buf1);   /* strcpy: OK (sizeof: buf1 == buf) */
        break;
      case 's':
        short_list = 1;
        strcpy(buf, buf1);   /* strcpy: OK (sizeof: buf1 == buf) */
        break;
      case 'q':
        questwho = 1;
        strcpy(buf, buf1);   /* strcpy: OK (sizeof: buf1 == buf) */
        break;
      case 'n':
        half_chop(buf1, name_search, buf);
        break;
      case 'r':
        who_room = 1;
        strcpy(buf, buf1);   /* strcpy: OK (sizeof: buf1 == buf) */
        break;
      case 'c':
        half_chop(buf1, arg, buf);
        showclass = find_class_bitvector(arg);
        break;
      case 'l':
        showleader = 1;
        strcpy(buf, buf1);   /* strcpy: OK (sizeof: buf1 == buf) */
        break;
      case 'g':
        showgroup = 1;
        strcpy(buf, buf1);   /* strcpy: OK (sizeof: buf1 == buf) */
        break;
      default:
        send_to_char(ch, "%s", WHO_FORMAT);
        return;
      }
    } else {
      send_to_char(ch, "%s", WHO_FORMAT);
      return;
    }
  }

  for (d = descriptor_list; d && !short_list; d = d->next) {
    if (d->original)
      tch = d->original;
    else if (!(tch = d->character))
      continue;

    if (CAN_SEE(ch, tch) && IS_PLAYING(d)) {
      if (*name_search && str_cmp(GET_NAME(tch), name_search) &&
          !strstr(GET_TITLE(tch), name_search))
        continue;
      if (!CAN_SEE(ch, tch) || GET_LEVEL(tch) < low || GET_LEVEL(tch) > high)
        continue;
      if (outlaws && !PLR_FLAGGED(tch, PLR_KILLER) && !PLR_FLAGGED(tch, PLR_THIEF))
        continue;
      if (questwho && !PRF_FLAGGED(tch, PRF_QUEST))
        continue;
      if (localwho && world[IN_ROOM(ch)].zone != world[IN_ROOM(tch)].zone)
        continue;
      if (who_room && (IN_ROOM(tch) != IN_ROOM(ch)))
        continue;
      if (showclass && !(showclass & (1 << GET_CLASS(tch))))
        continue;
      if (showgroup && !GROUP(tch))
        continue;
      if (showleader && (!GROUP(tch) || GROUP_LEADER(GROUP(tch)) != tch))
        continue;
      for (i = 0; *rank[i].disp != '\n'; i++)
        if (GET_LEVEL(tch) >= rank[i].min_level && GET_LEVEL(tch) <= rank[i].max_level)
          rank[i].count++;
    }
  }

  for (i = 0; *rank[i].disp != '\n'; i++) {
    if (!rank[i].count && !short_list)
      continue;

    if (short_list)
      send_to_char(ch, "�����\r\n-------\r\n");
    else
      send_to_char(ch, "%s", rank[i].disp);

    for (d = descriptor_list; d; d = d->next) {
      if (d->original)
        tch = d->original;
      else if (!(tch = d->character))
        continue;

      if ((GET_LEVEL(tch) < rank[i].min_level || GET_LEVEL(tch) > rank[i].max_level) && !short_list)
        continue;
      if (!IS_PLAYING(d))
        continue;
      if (*name_search && str_cmp(GET_NAME(tch), name_search) &&
          !strstr(GET_TITLE(tch), name_search))
        continue;
      if (!CAN_SEE(ch, tch) || GET_LEVEL(tch) < low || GET_LEVEL(tch) > high)
        continue;
      if (outlaws && !PLR_FLAGGED(tch, PLR_KILLER) && !PLR_FLAGGED(tch, PLR_THIEF))
        continue;
      if (questwho && !PRF_FLAGGED(tch, PRF_QUEST))
        continue;
      if (localwho && world[IN_ROOM(ch)].zone != world[IN_ROOM(tch)].zone)
        continue;
      if (who_room && (IN_ROOM(tch) != IN_ROOM(ch)))
        continue;
      if (showclass && !(showclass & (1 << GET_CLASS(tch))))
        continue;
      if (showgroup && !GROUP(tch))
        continue;
      if (showleader && (!GROUP(tch) || GROUP_LEADER(GROUP(tch)) != tch))
        continue;

      if (short_list) {
        send_to_char(ch, "%s[%2d %s] %-12.12s%s%s",
          (GET_LEVEL(tch) >= LVL_IMMORT ? CCYEL(ch, C_SPR) : ""),
          GET_LEVEL(tch), CLASS_ABBR(tch), GET_NAME(tch),
          CCNRM(ch, C_SPR), ((!(++num_can_see % 4)) ? "\r\n" : ""));
      } else {
        num_can_see++;
        send_to_char(ch, "%s[%2d %s] %s%s%s%s",
            (GET_LEVEL(tch) >= LVL_IMMORT ? CCYEL(ch, C_SPR) : ""),
            GET_LEVEL(tch), CLASS_ABBR(tch),
            GET_NAME(tch), (*GET_TITLE(tch) ? " " : ""), GET_TITLE(tch),
            CCNRM(ch, C_SPR));
        
        if (GET_INVIS_LEV(tch))
          send_to_char(ch, " (?��?)", GET_INVIS_LEV(tch));
        else if (AFF_FLAGGED(tch, AFF_INVISIBLE))
          send_to_char(ch, " (����)");

        if (PLR_FLAGGED(tch, PLR_MAILING))
          send_to_char(ch, " (��������)");
        else if (d->olc)
          send_to_char(ch, " (����)");
        else if (PLR_FLAGGED(tch, PLR_WRITING))
          send_to_char(ch, " (�۾���)");

      if (d->original)
        send_to_char(ch, " (���ٲٱ�)");

        if (d->connected == CON_OEDIT)
          send_to_char(ch, " (��������)");
        if (d->connected == CON_MEDIT)
          send_to_char(ch, " (������)");
        if (d->connected == CON_ZEDIT)
          send_to_char(ch, " (������)");
        if (d->connected == CON_SEDIT)
          send_to_char(ch, " (��������)");
        if (d->connected == CON_REDIT)
          send_to_char(ch, " (������)");
        if (d->connected == CON_TEDIT)
          send_to_char(ch, " (�ؽ�Ʈ����)");
        if (d->connected == CON_TRIGEDIT)
          send_to_char(ch, " (Ʈ��������)");
        if (d->connected == CON_AEDIT)
          send_to_char(ch, " (��������)");
        if (d->connected == CON_CEDIT)
          send_to_char(ch, " (��������)");
        if (d->connected == CON_HEDIT)
          send_to_char(ch, " (��������)");
        if (d->connected == CON_QEDIT)
          send_to_char(ch, " (�ӹ�����)");
        if (PRF_FLAGGED(tch, PRF_BUILDWALK))
          send_to_char(ch, " (������)");
        if (PRF_FLAGGED(tch, PRF_AFK))
          send_to_char(ch, " (��������)");
        if (PRF_FLAGGED(tch, PRF_NOGOSS))
          send_to_char(ch, " (���ź�)");
        if (PRF_FLAGGED(tch, PRF_NOWIZ))
          send_to_char(ch, " (��ä�ΰź�)");
        if (PRF_FLAGGED(tch, PRF_NOSHOUT))
          send_to_char(ch, " (��ħ�ź�)");
        if (PRF_FLAGGED(tch, PRF_NOTELL))
          send_to_char(ch, " (�̾߱�ź�)");
        if (PRF_FLAGGED(tch, PRF_QUEST))
          send_to_char(ch, " (�ӹ�)");
        if (PLR_FLAGGED(tch, PLR_THIEF))
          send_to_char(ch, " (����)");
        if (PLR_FLAGGED(tch, PLR_KILLER))
          send_to_char(ch, " (������)");
        send_to_char(ch, "\r\n");
      }
    }
    send_to_char(ch, "\r\n");
    if (short_list)
      break;
  }
  if (short_list && num_can_see % 4)
    send_to_char(ch, "\r\n");
  if (!num_can_see)
    send_to_char(ch, "�ƹ��� �����ϴ�!\r\n");
  else if (num_can_see == 1)
    send_to_char(ch, "Ȧ�� �ܷӰ� �������Դϴ�.\r\n");
  else
    send_to_char(ch, "%d���� �������Դϴ�.\r\n", num_can_see);

  if (IS_HAPPYHOUR > 0){
    send_to_char(ch, "It's a Happy Hour! Type \tRhappyhour\tW to see the current bonuses.\r\n");
  }
}

#define USERS_FORMAT \
"format: [-l �ּҷ���[-�ִ뷹��]] [-n �̸�] [-h �����ּ�] [-c ����] [-o] [-p] �����\r\n"

ACMD(do_users)
{
  char line[200], line2[220], idletime[10], classname[20];
  char state[30], timestr[9], mode;
  char name_search[MAX_INPUT_LENGTH], host_search[MAX_INPUT_LENGTH];
  struct char_data *tch;
  struct descriptor_data *d;
  int low = 0, high = LVL_IMPL, num_can_see = 0;
  int showclass = 0, outlaws = 0, playing = 0, deadweight = 0;
  char buf[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];

  host_search[0] = name_search[0] = '\0';

  strcpy(buf, argument);	/* strcpy: OK (sizeof: argument == buf) */
  while (*buf) {
    char buf1[MAX_INPUT_LENGTH];

    half_chop(buf, arg, buf1);
    if (*arg == '-') {
      mode = *(arg + 1);  /* just in case; we destroy arg in the switch */
      switch (mode) {
      case 'o':
      case 'k':
	outlaws = 1;
	playing = 1;
	strcpy(buf, buf1);	/* strcpy: OK (sizeof: buf1 == buf) */
	break;
      case 'p':
	playing = 1;
	strcpy(buf, buf1);	/* strcpy: OK (sizeof: buf1 == buf) */
	break;
      case 'd':
	deadweight = 1;
	strcpy(buf, buf1);	/* strcpy: OK (sizeof: buf1 == buf) */
	break;
      case 'l':
	playing = 1;
	half_chop(buf1, arg, buf);
	sscanf(arg, "%d-%d", &low, &high);
	break;
      case 'n':
	playing = 1;
	half_chop(buf1, name_search, buf);
	break;
      case 'h':
	playing = 1;
	half_chop(buf1, host_search, buf);
	break;
      case 'c':
	playing = 1;
	half_chop(buf1, arg, buf);
	showclass = find_class_bitvector(arg);
	break;
      default:
	send_to_char(ch, "%s", USERS_FORMAT);
	return;
      }				/* end of switch */

    } else {			/* endif */
      send_to_char(ch, "%s", USERS_FORMAT);
      return;
    }
  }				/* end while (parser) */
  send_to_char(ch,
	 "��ȣ ����    �̸�         ����           Idl   Login@@   �����ּ�\r\n"
	 "---- ------- ------------ -------------- ----- -------- ------------------------\r\n");
  one_argument(argument, arg);

  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) != CON_PLAYING && playing)
      continue;
    if (STATE(d) == CON_PLAYING && deadweight)
      continue;
    if (IS_PLAYING(d)) {
      if (d->original)
        tch = d->original;
      else if (!(tch = d->character))
        continue;

      if (*host_search && !strstr(d->host, host_search))
        continue;
      if (*name_search && str_cmp(GET_NAME(tch), name_search))
        continue;
      if (!CAN_SEE(ch, tch) || GET_LEVEL(tch) < low || GET_LEVEL(tch) > high)
        continue;
      if (outlaws && !PLR_FLAGGED(tch, PLR_KILLER) &&
	  !PLR_FLAGGED(tch, PLR_THIEF))
        continue;
      if (showclass && !(showclass & (1 << GET_CLASS(tch))))
        continue;
      if (GET_INVIS_LEV(tch) > GET_LEVEL(ch))
        continue;

      if (d->original)
	sprintf(classname, "[%2d %s]", GET_LEVEL(d->original),
		CLASS_ABBR(d->original));
      else
	sprintf(classname, "[%2d %s]", GET_LEVEL(d->character),
		CLASS_ABBR(d->character));
    } else
      strcpy(classname, "   -   ");

    strftime(timestr, sizeof(timestr), "%H:%M:%S", localtime(&(d->login_time)));

    if (STATE(d) == CON_PLAYING && d->original)
       strcpy(state, "���ٲٱ�");
    else
      strcpy(state, connected_types[STATE(d)]);

    if (d->character && STATE(d) == CON_PLAYING)
      sprintf(idletime, "%5d", d->character->char_specials.timer *
	      SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN);
    else
      strcpy(idletime, "     ");

    sprintf(line, "%3d %-7s %-12s %-14s %-3s %-8s ", d->desc_num, classname,
	d->original && d->original->player.name ? d->original->player.name :
	d->character && d->character->player.name ? d->character->player.name :
	"UNDEFINED",
	state, idletime, timestr);

    if (*d->host)
      sprintf(line + strlen(line), "[%s]\r\n", d->host);
    else
     strcat(line, "[Ȯ�ε��� ���� �ּ�]\r\n");

    if (STATE(d) != CON_PLAYING) {
      sprintf(line2, "%s%s%s", CCGRN(ch, C_SPR), line, CCNRM(ch, C_SPR));
      strcpy(line, line2);
    }
    if (STATE(d) != CON_PLAYING || (STATE(d) == CON_PLAYING && CAN_SEE(ch, d->character))) {
      send_to_char(ch, "%s", line);
      num_can_see++;
    }
  }

  send_to_char(ch, "\r\n%d���� ����Ǿ� �ֽ��ϴ�.\r\n", num_can_see);
}

/* Generic page_string function for displaying text */
ACMD(do_gen_ps)
{
  if (IS_NPC(ch)) {
   send_to_char(ch, "���� ������ �� �ִ� ����� �ƴմϴ�!\r\n");
    return;
  }

  switch (subcmd) {
  case SCMD_CREDITS:
    page_string(ch->desc, credits, 0);
    break;
  case SCMD_NEWS:
    GET_LAST_NEWS(ch) = time(0);
    page_string(ch->desc, news, 0);
    break;
  case SCMD_INFO:
    page_string(ch->desc, info, 0);
    break;
  case SCMD_WIZLIST:
    page_string(ch->desc, wizlist, 0);
    break;
  case SCMD_IMMLIST:
    page_string(ch->desc, immlist, 0);
    break;
  case SCMD_HANDBOOK:
    page_string(ch->desc, handbook, 0);
    break;
  case SCMD_POLICIES:
    page_string(ch->desc, policies, 0);
    break;
  case SCMD_MOTD:
    GET_LAST_MOTD(ch) = time(0);
    page_string(ch->desc, motd, 0);
    break;
  case SCMD_IMOTD:
    page_string(ch->desc, imotd, 0);
    break;
  case SCMD_CLEAR:
    send_to_char(ch, "\033[H\033[J");
    break;
  case SCMD_VERSION:
    send_to_char(ch, "%s\r\n", tbamud_version);
    break;
  case SCMD_WHOAMI:
    send_to_char(ch, "%s\r\n", GET_NAME(ch));
    break;
  default:
    log("SYSERR: Unhandled case in do_gen_ps. (%d)", subcmd);
    /* SYSERR_DESC: General page string function for such things as 'credits',
     * 'news', 'wizlist', 'clear', 'version'.  This occurs when a call is made
     * to this routine that is not one of the predefined calls.  To correct it,
     * either a case needs to be added into the function to account for the
     * subcmd that is being passed to it, or the call to the function needs to
     * have the correct subcmd put into place. */
    return;
  }
}

static void perform_mortal_where(struct char_data *ch, char *arg)
{
  struct char_data *i;
  struct descriptor_data *d;
  int j;

  if (!*arg) {
    j = world[(IN_ROOM(ch))].zone;
    send_to_char(ch, "%s �������� �����@n.\r\n--------------------\r\n", zone_table[j].name);
    for (d = descriptor_list; d; d = d->next) {
      if (STATE(d) != CON_PLAYING || d->character == ch)
	continue;
      if ((i = (d->original ? d->original : d->character)) == NULL)
	continue;
      if (IN_ROOM(i) == NOWHERE || !CAN_SEE(ch, i))
	continue;
      if (world[IN_ROOM(ch)].zone != world[IN_ROOM(i)].zone)
	continue;
      send_to_char(ch, "%-20s%s - %s%s\r\n", GET_NAME(i), QNRM, world[IN_ROOM(i)].name, QNRM);
    }
  } else {			/* print only FIRST char, not all. */
    for (i = character_list; i; i = i->next) {
      if (IN_ROOM(i) == NOWHERE || i == ch)
	continue;
      if (!CAN_SEE(ch, i) || world[IN_ROOM(i)].zone != world[IN_ROOM(ch)].zone)
	continue;
      if (!isname(arg, i->player.name))
	continue;
      send_to_char(ch, "%-25s%s - %s%s\r\n", GET_NAME(i), QNRM, world[IN_ROOM(i)].name, QNRM);
      return;
    }
    send_to_char(ch, "�׷��̸��� ����� ������ �����ϴ�.\r\n");
  }
}

static void print_object_location(int num, struct obj_data *obj, struct char_data *ch,
			        int recur)
{
  if (num > 0)
    send_to_char(ch, "O%3d. %-25s%s - ", num, obj->short_description, QNRM);
  else
    send_to_char(ch, "%33s", " - ");

  if (SCRIPT(obj)) {
    if (!TRIGGERS(SCRIPT(obj))->next)
      send_to_char(ch, "[T%d] ", GET_TRIG_VNUM(TRIGGERS(SCRIPT(obj))));
    else
      send_to_char(ch, "[Ʈ����] ");
  }

  if (IN_ROOM(obj) != NOWHERE)
    send_to_char(ch, "[%5d] %s%s\r\n", GET_ROOM_VNUM(IN_ROOM(obj)), world[IN_ROOM(obj)].name, QNRM);
  else if (obj->carried_by)
    send_to_char(ch, "���� -> %s%s\r\n", PERS(obj->carried_by, ch), QNRM);
  else if (obj->worn_by)
    send_to_char(ch, "��� -> %s%s\r\n", PERS(obj->worn_by, ch), QNRM);
  else if (obj->in_obj) {
    send_to_char(ch, "���� -> %s%s%s\r\n", obj->in_obj->short_description, QNRM, (recur ? "-> " : " "));
    if (recur)
      print_object_location(0, obj->in_obj, ch, recur);
  } else
    send_to_char(ch, "�˼����� ���� �ֽ��ϴ�.\r\n");
}

static void perform_immort_where(struct char_data *ch, char *arg)
{
  struct char_data *i;
  struct obj_data *k;
  struct descriptor_data *d;
  int num = 0, found = 0;

  if (!*arg) {
    send_to_char(ch, "�����   ���ȣ  ��ġ                           ��\r\n");
    send_to_char(ch, "-------- ------- ------------------------------ -------------------\r\n");
    for (d = descriptor_list; d; d = d->next)
      if (IS_PLAYING(d)) {
        i = (d->original ? d->original : d->character);
        if (i && CAN_SEE(ch, i) && (IN_ROOM(i) != NOWHERE)) {
          if (d->original)
            send_to_char(ch, "%-8s%s - [%5d] %s%s (in %s%s)\r\n",
              GET_NAME(i), QNRM, GET_ROOM_VNUM(IN_ROOM(d->character)),
              world[IN_ROOM(d->character)].name, QNRM, GET_NAME(d->character), QNRM);
          else
            send_to_char(ch, "%-8s%s %s[%s%5d%s]%s %-*s%s %s%s\r\n", GET_NAME(i), QNRM,
              QCYN, QYEL, GET_ROOM_VNUM(IN_ROOM(i)), QCYN, QNRM,
              30+count_color_chars(world[IN_ROOM(i)].name), world[IN_ROOM(i)].name, QNRM,
              zone_table[(world[IN_ROOM(i)].zone)].name, QNRM);
        }
      }
  } else {
    for (i = character_list; i; i = i->next)
      if (CAN_SEE(ch, i) && IN_ROOM(i) != NOWHERE && isname(arg, i->player.name)) {
        found = 1;
        send_to_char(ch, "M%3d. %-25s%s - [%5d] %-25s%s", ++num, GET_NAME(i), QNRM,
               GET_ROOM_VNUM(IN_ROOM(i)), world[IN_ROOM(i)].name, QNRM);
        if (SCRIPT(i) && TRIGGERS(SCRIPT(i))) {
          if (!TRIGGERS(SCRIPT(i))->next)
            send_to_char(ch, "[T%d] ", GET_TRIG_VNUM(TRIGGERS(SCRIPT(i))));
          else
            send_to_char(ch, "[Ʈ����] ");
        }
      send_to_char(ch, "%s\r\n", QNRM);
      }
    for (num = 0, k = object_list; k; k = k->next)
      if (CAN_SEE_OBJ(ch, k) && isname(arg, k->name)) {
        found = 1;
        print_object_location(++num, k, ch, TRUE);
      }
    if (!found)
      send_to_char(ch, "�׷����� ã�� �� �����ϴ�.\r\n");
  }
}

ACMD(do_where)
{
  char arg[MAX_INPUT_LENGTH];

  one_argument(argument, arg);

  if (GET_LEVEL(ch) >= LVL_IMMORT)
    perform_immort_where(ch, arg);
  else
    perform_mortal_where(ch, arg);
}

ACMD(do_levels)
{
  char buf[MAX_STRING_LENGTH], arg[MAX_STRING_LENGTH];
  size_t len = 0, nlen;
  int i, ret, min_lev=1, max_lev=LVL_IMMORT, val;

  if (IS_NPC(ch)) {
    send_to_char(ch, "���� ����� �� ���� ����Դϴ�.\r\n");
    return;
  }
  one_argument(argument, arg);

  if (*arg) {
    if (isdigit(*arg)) {
      ret = sscanf(arg, "%d-%d", &min_lev, &max_lev);
      if (ret == 0) {
        /* No valid args found */
        min_lev = 1;
        max_lev = LVL_IMMORT;
      }
      else if (ret == 1) {
        /* One arg = range is (num) either side of current level */
        val = min_lev;
        max_lev = MIN(GET_LEVEL(ch) + val, LVL_IMMORT);
        min_lev = MAX(GET_LEVEL(ch) - val, 1);
      }
      else if (ret == 2) {
        /* Two args = min-max range limit - just do sanity checks */
        min_lev = MAX(min_lev, 1);
        max_lev = MIN(max_lev + 1, LVL_IMMORT);
      }
    }
    else
    {
      send_to_char(ch, "����: %s [<�ּҷ���>-<�ִ뷹��> | <����>] ��������%s\r\n\r\n", QYEL, QNRM);
      send_to_char(ch, "������ �ʿ��� ����ġ�� �����ݴϴ�.\r\n");
      send_to_char(ch, "%s ��������%s      - ��緹���� �����ݴϴ� (1-%d)\r\n", QCYN, QNRM, (LVL_IMMORT-1));
      send_to_char(ch, "%s 5 ��������%s    - ���� �������� 5�ܰ��� ������ �����ݴϴ�\r\n", QCYN, QNRM);
      send_to_char(ch, "%s 10-40 ��������%s- 10-40������ ���� �����ݴϴ�\r\n",QCYN, QNRM);
      return;
    }
  }

  for (i = min_lev; i < max_lev; i++) {
    nlen = snprintf(buf + len, sizeof(buf) - len, "[%2d] %8d-%-8d : ", (int)i,
	level_exp(GET_CLASS(ch), i), level_exp(GET_CLASS(ch), i + 1) - 1);
    if (len + nlen >= sizeof(buf))
      break;
    len += nlen;

    switch (GET_SEX(ch)) {
    case SEX_MALE:
    case SEX_NEUTRAL:
      nlen = snprintf(buf + len, sizeof(buf) - len, "%s\r\n", title_male(GET_CLASS(ch), i));
      break;
    case SEX_FEMALE:
      nlen = snprintf(buf + len, sizeof(buf) - len, "%s\r\n", title_female(GET_CLASS(ch), i));
      break;
    default:
      nlen = snprintf(buf + len, sizeof(buf) - len, "����� ������ ���°� �����ϴ�.\r\n");
      break;
    }
    if (len + nlen >= sizeof(buf))
      break;
    len += nlen;
  }

  if (len < sizeof(buf) && max_lev == LVL_IMMORT)
    snprintf(buf + len, sizeof(buf) - len, "[%2d] %8d          : Immortality\r\n",
		LVL_IMMORT, level_exp(GET_CLASS(ch), LVL_IMMORT));
  page_string(ch->desc, buf, TRUE);
}

ACMD(do_consider)
{
  char buf[MAX_INPUT_LENGTH];
  struct char_data *victim;
  int diff;

  one_argument(argument, buf);

  if (!(victim = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "������ ���ұ��?\r\n");
    return;
  }
  if (victim == ch) {
    send_to_char(ch, "�ڱ� �ڽŰ� ���� �� �����ϴ�!\r\n");
    return;
  }
  if (!IS_NPC(victim)) {
    send_to_char(ch, "���� ���ϰ� ���ڳ� �غ��� ��������?\r\n");
    return;
  }
  diff = (GET_LEVEL(victim) - GET_LEVEL(ch));

  if (diff <= -10)
    send_to_char(ch, "�ǵ�⸸ �ص� ������������ �����ϴ�.\r\n");
  else if (diff <= -5)
    send_to_char(ch, "���ҷӱ� ¦�� �����ϴ�!\r\n");
  else if (diff <= -2)
    send_to_char(ch, "�ʹ� ���� ���̴� ����Դϴ�.\r\n");
  else if (diff <= -1)
    send_to_char(ch, "����� �����δ� ���� ������ ����Դϴ�.\r\n");
  else if (diff == 0)
    send_to_char(ch, "���� ���ǽºΰ� �ɰ� �����ϴ�!\r\n");
  else if (diff <= 1)
    send_to_char(ch, "�ణ�� � �����شٸ� �̱�� ������ �����ϴ�!\r\n");
  else if (diff <= 2)
    send_to_char(ch, "����� ���� ������� �Ұ� �����ϴ�!\r\n");
  else if (diff <= 3)
    send_to_char(ch, "������ �����ϰ� �ο�ٸ� �̱���� ������ �����ϴ�.\r\n");
  else if (diff <= 5)
    send_to_char(ch, "��ſ��� �ʹ� ���� ����Դϴ�.\r\n");
  else if (diff <= 10)
    send_to_char(ch, "���� �ٶ󺸴� ���濡 ����� ���� ������ ���ϴ�!\r\n");
  else if (diff <= 100)
    send_to_char(ch, "���� ���ϰ� ���ڰ��� �غ��� ��������.\r\n");
}

ACMD(do_diagnose)
{
  char buf[MAX_INPUT_LENGTH];
  struct char_data *vict;

  one_argument(argument, buf);

  if (*buf) {
    if (!(vict = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM)))
      send_to_char(ch, "%s", CONFIG_NOPERSON);
    else
      diag_char_to_char(vict, ch);
  } else {
    if (FIGHTING(ch))
      diag_char_to_char(FIGHTING(ch), ch);
    else
      send_to_char(ch, "������ �����մϱ�?\r\n");
  }
}

ACMD(do_toggle)
{
  char buf2[10], arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  int toggle, tp, wimp_lev, result = 0, len = 0, i;
  const char *types[] = { "off", "brief", "normal", "on", "\n" };

    const struct {
    char *command;
    bitvector_t toggle; /* this needs changing once hashmaps are implemented */
    char min_level;
    char *disable_msg;
    char *enable_msg;
  } tog_messages[] = {
    {"��ȯ", PRF_SUMMONABLE, 0,
    "�������� �ٸ� ������� ��ȯ�� �ź��մϴ�.\r\n",
    "�ٸ� ������� ��ȯ�� ����մϴ�.\r\n"},
    {"nohassle", PRF_NOHASSLE, LVL_IMMORT,
    "Nohassle disabled.\r\n",
    "Nohassle enabled.\r\n"},
    {"����", PRF_BRIEF, 0,
    "������� ����.\r\n",
    "������� ����.\r\n"},
    {"����", PRF_COMPACT, 0,
    "�������� ��� ����.\r\n",
    "�������� ��� ����.\r\n"},
    {"�̾߱�", PRF_NOTELL, 0,
    "�ٸ� ������� �̾߱⸦ ����մϴ�.\r\n",
    "�ٸ� ������� �̾߱⸦ �ź��մϴ�.\r\n"},
    {"���", PRF_NOAUCT, 0,
    "��� ä���� �մϴ�.\r\n",
    "��� ä���� ���� �ʽ��ϴ�.\r\n"},
    {"��ħ", PRF_NOSHOUT, 0,
    "��ħ ä���� �մϴ�.\r\n",
    "��ħ ä���� ���� �ʽ��ϴ�.\r\n"},
    {"���", PRF_NOGOSS, 0,
    "��� ä���� �մϴ�.\r\n",
    "��� ä���� ���� �ʽ��ϴ�.\r\n"},
    {"����", PRF_NOGRATZ, 0,
    "���� ä���� �մϴ�.\r\n",
    "���� ä���� ���� �ʽ��ϴ�.\r\n"},
    {"��ä��", PRF_NOWIZ, LVL_IMMORT,
    "��ä���� ����ϴ�.\r\n",
    "��ä���� ���� �ʽ��ϴ�.\r\n"},
    {"�ӹ�", PRF_QUEST, 0,
    "����� ���� �ӹ����� �ƴմϴ�.\r\n",
    "����� ���� �ӹ��� �����մϴ�.\r\n"},
    {"��Ӽ�", PRF_SHOWVNUMS, LVL_IMMORT,
    "��Ӽ��� ���̻� ���� �ʽ��ϴ�.\r\n",
    "���� ��Ӽ��� ���ϴ�.\r\n"},
    {"�ݺ�", PRF_NOREPEAT, 0,
    "����� �ϴ� ������ ���ϴ�.\r\n",
    "����� �ϴ� ������ ���� �ʽ��ϴ�.\r\n"},
    {"���溸��", PRF_HOLYLIGHT, LVL_IMMORT,
    "���溸�Ⱑ �����ϴ�.\r\n",
    "���溸�Ⱑ �����ϴ�.\r\n"},
    {"slownameserver", 0, LVL_IMPL,
    "Nameserver_is_slow changed to OFF; IP addresses will now be resolved.\r\n",
    "Nameserver_is_slow changed to ON; sitenames will no longer be resolved.\r\n"},
    {"�ڵ��ⱸ", PRF_AUTOEXIT, 0,
    "�ڵ��ⱸ�� ������� �ʽ��ϴ�.\r\n",
    "�ڵ��ⱸ�� ����մϴ�.\r\n"},
    {"��������", 0, LVL_IMPL,
    "���� ����ڵ��� ���� ����ؼ� �������� ���մϴ�.\r\n",
    "���� ����ڵ��� ���� ����ؼ� ������ �մϴ�.\r\n"},
    {"����ȭ��", PRF_CLS, LVL_BUILDER,
    "����ȭ���� �Ϲݸ��� ���ϴ�.\r\n",
    "����ȭ���� �޴����� ���ϴ�.\r\n"},
    {"������", PRF_BUILDWALK, LVL_BUILDER,
    "������ ��带 ������� �ʽ��ϴ�.\r\n",
    "������ ��带 ����մϴ�.\r\n"},
    {"��������", PRF_AFK, 0,
    "���������� �����մϴ�.\r\n",
    "���������� ���ϴ�.\r\n"},
    {"�ڵ�����", PRF_AUTOLOOT, 0,
    "�ڵ����Ÿ� ������� �ʽ��ϴ�.\r\n",
    "�ڵ����Ÿ� ����մϴ�.\r\n"},
    {"���ݱ�", PRF_AUTOGOLD, 0,
    "���ݱ⸦ ������� �ʽ��ϴ�.\r\n",
    "���ݱ⸦ ����մϴ�.\r\n"},
    {"�ڵ��й�", PRF_AUTOSPLIT, 0,
    "�ڵ��й踦 ������� �ʽ��ϴ�.\r\n",
    "�ڵ��й踦 ����մϴ�.\r\n"},
    {"�ڵ�����", PRF_AUTOSAC, 0,
    "�ڵ������� ������� �ʽ��ϴ�.\r\n",
    "�ڵ������� ����մϴ�.\r\n"},
    {"�ڵ�����", PRF_AUTOASSIST, 0,
    "�ڵ����⸦ ������� �ʽ��ϴ�.\r\n",
    "�ڵ����⸦ ����մϴ�.\r\n"},
    {"�ڵ�����", PRF_AUTOMAP, 1,
    "�ڵ������� ������� �ʽ��ϴ�.\r\n",
    "���� ���׸� ������ �漳�� �����ݴϴ�.\r\n"},
    {"�ڵ�����", PRF_AUTOKEY, 0,
    "���� ���� �������� ���ϴ�.\r\n",
    "���谡 ������ ���� �ڵ����� ���ϴ�.\r\n"},
    {"�ڵ���", PRF_AUTODOOR, 0,
    "����� ���� ���� ���� �ݰų� ��۶� ������ ���� �����ؾ� �մϴ�.\r\n",
    "����� ���� �ڵ����� ���� ������ ã���ϴ�.\r\n"},
    {"����", 0, 0, "\n", "\n"},
    {"�����", 0, LVL_IMMORT, "\n", "\n"},
    {"�ڵ�����", 0, 0, "\n", "\n"},
    {"����������", 0, 0, "\n", "\n"},
    {"ȭ�����", 0, 0, "\n", "\n"},
    {"��������", PRF_ZONERESETS, LVL_IMPL, "����� ���� ���� ���� ������ ���� �ʽ��ϴ�.\r\n", "����� ���� ���� ���� ������ ���ϴ�.\r\n"},
    {"\n", 0, -1, "\n", "\n"} /* must be last */
  };

  if (IS_NPC(ch))
    return;

  argument = one_argument(argument, arg);
  any_one_arg(argument, arg2); /* so that we don't skip 'on' */

  if (!*arg) {
    if (!GET_WIMP_LEV(ch))
      strcpy(buf2, "����");        /* strcpy: OK */
    else
      sprintf(buf2, "%-3.3d", GET_WIMP_LEV(ch));  /* sprintf: OK */

	if (GET_LEVEL(ch) == LVL_IMPL) {
      send_to_char(ch,
        " SlowNameserver: %-3s   "
	"                        "
	" Trackthru Doors: %-3s\r\n",

	ONOFF(CONFIG_NS_IS_SLOW),
	ONOFF(CONFIG_TRACK_T_DOORS));
    }

    if (GET_LEVEL(ch) >= LVL_IMMORT) {
      send_to_char(ch,
        "         ������: %-4s    "
        "     ��ä�ΰź�: %-4s    "
        "       ����ȭ��: %-4s\r\n"
        "       NoHassle: %-4s    "
        "       ���溸��: %-4s    "
        "         ��Ӽ�: %-4s\r\n"
        "       �����: %-4s\r\n",

        ONOFF(PRF_FLAGGED(ch, PRF_BUILDWALK)),
        ONOFF(PRF_FLAGGED(ch, PRF_NOWIZ)),
        ONOFF(PRF_FLAGGED(ch, PRF_CLS)),
        ONOFF(PRF_FLAGGED(ch, PRF_NOHASSLE)),
        ONOFF(PRF_FLAGGED(ch, PRF_HOLYLIGHT)),
        ONOFF(PRF_FLAGGED(ch, PRF_SHOWVNUMS)),
        types[(PRF_FLAGGED(ch, PRF_LOG1) ? 1 : 0) + (PRF_FLAGGED(ch, PRF_LOG2) ? 2 : 0)],
        GET_LEVEL(ch) == LVL_IMPL ? "" : "\r\n");
    }
    if (GET_LEVEL(ch) >= LVL_IMPL) {
      send_to_char(ch,
        "     ZoneResets: %-3s\r\n",
        ONOFF(PRF_FLAGGED(ch, PRF_ZONERESETS)));
    }

  send_to_char(ch,
    "       ü��ǥ��: %-4s    "
    "           ����: %-4s    "
    "       ��ȯ����: %-4s\r\n"

    "     �̵���ǥ��: %-4s    "
    "       ��������: %-4s    "
    "           �ӹ�: %-4s\r\n"

    "       ����ǥ��: %-4s    "
    "     �̾߱⼳��: %-4s    "
    "       �ݺ�����: %-4s\r\n"

    "       �ڵ��ⱸ: %-4s    "
    "       ��ħ����: %-4s    "
    "       �ڵ�����: %-4s\r\n"

    "       ���ź�: %-4s    "
    "       ��Űź�: %-4s    "
    "       ���ϰź�: %-4s\r\n"

    "       �ڵ�����: %-4s    "
    "         ���ݱ�: %-4s    "
    "       �ڵ��й�: %-4s\r\n"

    "       �ڵ�����: %-4s    "
    "       �ڵ�����: %-4s    "
    "       �ڵ�����: %-4s\r\n"

    "     ����������: %-4d    "
    "       ȭ�����: %-4d    "
    "       ��������: %-4s\r\n"

    "       �ڵ�����: %-4s    "
    "         �ڵ���: %-4s    "
    "           ����: %s     \r\n ",

    ONOFF(PRF_FLAGGED(ch, PRF_DISPHP)),
    ONOFF(PRF_FLAGGED(ch, PRF_BRIEF)),
    ONOFF(PRF_FLAGGED(ch, PRF_SUMMONABLE)),

    ONOFF(PRF_FLAGGED(ch, PRF_DISPMOVE)),
    ONOFF(PRF_FLAGGED(ch, PRF_COMPACT)),
    ONOFF(PRF_FLAGGED(ch, PRF_QUEST)),

    ONOFF(PRF_FLAGGED(ch, PRF_DISPMANA)),
    ONOFF(PRF_FLAGGED(ch, PRF_NOTELL)),
    ONOFF(PRF_FLAGGED(ch, PRF_NOREPEAT)),

    ONOFF(PRF_FLAGGED(ch, PRF_AUTOEXIT)),
    ONOFF(PRF_FLAGGED(ch, PRF_NOSHOUT)),
    buf2,

    ONOFF(PRF_FLAGGED(ch, PRF_NOGOSS)),
    ONOFF(PRF_FLAGGED(ch, PRF_NOAUCT)),
    ONOFF(PRF_FLAGGED(ch, PRF_NOGRATZ)),

    ONOFF(PRF_FLAGGED(ch, PRF_AUTOLOOT)),
    ONOFF(PRF_FLAGGED(ch, PRF_AUTOGOLD)),
    ONOFF(PRF_FLAGGED(ch, PRF_AUTOSPLIT)),

    ONOFF(PRF_FLAGGED(ch, PRF_AUTOSAC)),
    ONOFF(PRF_FLAGGED(ch, PRF_AUTOASSIST)),
    ONOFF(PRF_FLAGGED(ch, PRF_AUTOMAP)),

    GET_PAGE_LENGTH(ch),
    GET_SCREEN_WIDTH(ch),
    ONOFF(PRF_FLAGGED(ch, PRF_AFK)),

    ONOFF(PRF_FLAGGED(ch, PRF_AUTOKEY)),
    ONOFF(PRF_FLAGGED(ch, PRF_AUTODOOR)),
    types[COLOR_LEV(ch)]);
    return;
  }

  len = strlen(arg);
  for (toggle = 0; *tog_messages[toggle].command != '\n'; toggle++)
    if (!strncmp(arg, tog_messages[toggle].command, len))
      break;

  if (*tog_messages[toggle].command == '\n' || tog_messages[toggle].min_level > GET_LEVEL(ch)) {
     send_to_char(ch, "�׷��� ������ �� �����ϴ�!\r\n");
    return;
  }

  switch (toggle) {
  case SCMD_COLOR:
    if (!*arg2) {
      send_to_char(ch, "����� ���� ������ %s �Դϴ�.\r\n", types[COLOR_LEV(ch)]);
      return;
    }

    if (((tp = search_block(arg2, types, FALSE)) == -1)) {
     send_to_char(ch, "����: ���� { ���� | ���� | ���� | ���� } ����\r\n");
      return;
    }
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_COLOR_1);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_COLOR_2);
    if (tp & 1) SET_BIT_AR(PRF_FLAGS(ch), PRF_COLOR_1);
    if (tp & 2) SET_BIT_AR(PRF_FLAGS(ch), PRF_COLOR_2);

    send_to_char(ch, "����� %s����%s�� ���� %s �Դϴ�.\r\n", CCRED(ch, C_SPR), CCNRM(ch, C_OFF), types[tp]);
    return;
  case SCMD_SYSLOG:
    if (!*arg2) {
      send_to_char(ch, "����� ���� ������� %s �Դϴ�.\r\n",
        types[(PRF_FLAGGED(ch, PRF_LOG1) ? 1 : 0) + (PRF_FLAGGED(ch, PRF_LOG2) ? 2 : 0)]);
      return;
    }
    if (((tp = search_block(arg2, types, FALSE)) == -1)) {
      send_to_char(ch, "����: ����� { ���� | ���� | ���� | ���� } ����\r\n");
      return;
    }
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_LOG1);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_LOG2);
    if (tp & 1) SET_BIT_AR(PRF_FLAGS(ch), PRF_LOG1);
    if (tp & 2) SET_BIT_AR(PRF_FLAGS(ch), PRF_LOG2);

    send_to_char(ch, "����� ������� ���� %s �Դϴ�.\r\n", types[tp]);
    return;
  case SCMD_SLOWNS:
    result = (CONFIG_NS_IS_SLOW = !CONFIG_NS_IS_SLOW);
    break;
  case SCMD_TRACK:
    result = (CONFIG_TRACK_T_DOORS = !CONFIG_TRACK_T_DOORS);
    break;
  case SCMD_BUILDWALK:
    if (GET_LEVEL(ch) < LVL_BUILDER) {
      send_to_char(ch, "�����ڸ� ��밡���մϴ�.\r\n");
      return;
    }
    result = PRF_TOG_CHK(ch, PRF_BUILDWALK);
    if (PRF_FLAGGED(ch, PRF_BUILDWALK)) {
      for (i=0; *arg2 && *(sector_types[i]) != '\n'; i++)
        if (is_abbrev(arg2, sector_types[i]))
          break;
      if (*(sector_types[i]) == '\n') 
        i=0;
      GET_BUILDWALK_SECTOR(ch) = i;
      send_to_char(ch, "Default sector type is %s\r\n", sector_types[i]);
      mudlog(CMP, GET_LEVEL(ch), TRUE,
             "OLC: %s turned buildwalk on.  Allowed zone %d", GET_NAME(ch), GET_OLC_ZONE(ch));
    } else
      mudlog(CMP, GET_LEVEL(ch), TRUE,
             "OLC: %s turned buildwalk off.  Allowed zone %d", GET_NAME(ch), GET_OLC_ZONE(ch));
    break;
  case SCMD_AFK:
    if ((result = PRF_TOG_CHK(ch, PRF_AFK)))
      act("$n���� ������ �����ϱ� ���� ���·� $s ���ϴ�.", TRUE, ch, 0, 0, TO_ROOM);
    else {
      act("$n���� ������������ �ٽ� �������� $s ���ƿɴϴ�.", TRUE, ch, 0, 0, TO_ROOM);
      if (has_mail(GET_IDNUM(ch)))
        send_to_char(ch, "������ ���ֽ��ϴ�.\r\n");
    }
    break;
  case SCMD_WIMPY:
    if (!*arg2) {
      if (GET_WIMP_LEV(ch)) {
        send_to_char(ch, "����� ���� �ڵ����� ü�¼�ġ�� %d �Դϴ�.\r\n", GET_WIMP_LEV(ch));
        return;
      } else {
        send_to_char(ch, "�ڵ������� �����Ǿ����� �ʽ��ϴ�.\r\n");
        return;
      }
    }
    if (isdigit(*arg2)) {
      if ((wimp_lev = atoi(arg2)) != 0) {
        if (wimp_lev < 0)
          send_to_char(ch, "��ġ�� ��Ȯ�ϰ� �Է��ϼ���.\r\n");
        else if (wimp_lev > GET_MAX_HIT(ch))
          send_to_char(ch, "�ڽ��� �ִ�ü�º��� ���� ������ �� �����ϴ�.\r\n");
        else if (wimp_lev > (GET_MAX_HIT(ch) / 2))
          send_to_char(ch, "�ִ�ü���� ���̻����� ������ �� �����ϴ�.\r\n");
        else {
          send_to_char(ch, "����� �������� ü�� %d�̸��϶� ������ ���ϴ�.", wimp_lev);
          GET_WIMP_LEV(ch) = wimp_lev;
        }
      } else {
        send_to_char(ch, "�ڵ������� ������� �ʽ��ϴ�. ������ ���� �ο�ϴ�.");
        GET_WIMP_LEV(ch) = 0;
      }
    } else
      send_to_char(ch, "�ڵ������� ��Ȯ�� ü�� ��ġ�� �Է��ϼ���.  (������ 0)\r\n");
    break;
  case SCMD_PAGELENGTH:
    if (!*arg2)
      send_to_char(ch, "����� ������ ���̴� ���� %d�� �Դϴ�.", GET_PAGE_LENGTH(ch));
    else if (is_number(arg2)) {
      GET_PAGE_LENGTH(ch) = MIN(MAX(atoi(arg2), 5), 255);
      send_to_char(ch, "������ ���̰� %d�ٷ� �����˴ϴ�.", GET_PAGE_LENGTH(ch));
    } else
      send_to_char(ch, "������ ���̷� ��밡���� ������ (5 - 255) �Դϴ�.");
      break;
  case SCMD_SCREENWIDTH:
    if (!*arg2)
      send_to_char(ch, "����� ���� ȭ����̴� %d���� �Դϴ�.", GET_SCREEN_WIDTH(ch));
    else if (is_number(arg2)) {
      GET_SCREEN_WIDTH(ch) = MIN(MAX(atoi(arg2), 40), 200);
      send_to_char(ch, "ȭ����̰� %d���ڷ� �����˴ϴ�.", GET_SCREEN_WIDTH(ch));
    } else
      send_to_char(ch, "ȭ����̷� ��밡���� ������ (40 - 200) �Դϴ�.");
      break;
  case SCMD_AUTOMAP:
    if (can_see_map(ch)) {
      if (!*arg2) {
        TOGGLE_BIT_AR(PRF_FLAGS(ch), tog_messages[toggle].toggle);
        result = (PRF_FLAGGED(ch, tog_messages[toggle].toggle));
      } else if (!strcmp(arg2, "����")) {
        SET_BIT_AR(PRF_FLAGS(ch), tog_messages[toggle].toggle);
        result = 1;
      } else if (!strcmp(arg2, "����")) {
        REMOVE_BIT_AR(PRF_FLAGS(ch), tog_messages[toggle].toggle);
      } else {
        send_to_char(ch, "%s%s ���� �ݵ�� '����' �̳� '����' �Դϴ�.\r\n",
			tog_messages[toggle].command, check_josa(tog_messages[toggle].command, 4));
        return;
      }
    } else {
      send_to_char(ch, "�˼��մϴ�. �ڵ����� ������ �������ϴ�.\r\n");
	}
	break;
    if (!*arg2) {
      TOGGLE_BIT_AR(PRF_FLAGS(ch), tog_messages[toggle].toggle);
      result = (PRF_FLAGGED(ch, tog_messages[toggle].toggle));
    } else if (!strcmp(arg2, "����")) {
      SET_BIT_AR(PRF_FLAGS(ch), tog_messages[toggle].toggle);
      result = 1;
    } else if (!strcmp(arg2, "����")) {
      REMOVE_BIT_AR(PRF_FLAGS(ch), tog_messages[toggle].toggle);
    } else {
        send_to_char(ch, "%s%s ���� �ݵ�� '����' �̳� '����' �Դϴ�.\r\n",
			tog_messages[toggle].command, check_josa(tog_messages[toggle].command, 4));
      return;
    }
  }
  if (result)
    send_to_char(ch, "%s", tog_messages[toggle].enable_msg);
  else
    send_to_char(ch, "%s", tog_messages[toggle].disable_msg);
}

ACMD(do_commands)
{
  int no, i, cmd_num;
  int socials = 0;
  const char *commands[1000];
  int overflow = sizeof(commands) / sizeof(commands[0]);

  if (!ch->desc)
    return;

  if (subcmd == SCMD_SOCIALS)
    socials = 1;

  send_to_char(ch, "����� ����� �� �ִ� %s �Դϴ� :\r\n", socials ? "����ǥ��" : "��ɾ�");

  /* cmd_num starts at 1, not 0, to remove 'RESERVED' */
  for (no = 0, cmd_num = 1;
       complete_cmd_info[cmd_sort_info[cmd_num]].command[0] != '\n';
       ++cmd_num) {

    i = cmd_sort_info[cmd_num];

    if (complete_cmd_info[i].minimum_level < 0 || GET_LEVEL(ch) < complete_cmd_info[i].minimum_level)

      continue;

    if (complete_cmd_info[i].minimum_level >= LVL_IMMORT)
      continue;

    if (socials != (complete_cmd_info[i].command_pointer == do_action))
      continue;

    if (--overflow < 0)
      continue;

    /* matching command: copy to commands list */
    commands[no++] = complete_cmd_info[i].command;
  }

  /* display commands list in a nice columnized format */
  column_list(ch, 0, commands, no, FALSE);
}

void free_history(struct char_data *ch, int type)
{
  struct txt_block *tmp = GET_HISTORY(ch, type), *ftmp;

  while ((ftmp = tmp)) {
    tmp = tmp->next;
    if (ftmp->text)
      free(ftmp->text);
    free(ftmp);
  }
  GET_HISTORY(ch, type) = NULL;
}

ACMD(do_history)
{
  char arg[MAX_INPUT_LENGTH];
  int type;

  one_argument(argument, arg);

  type = search_block(arg, history_types, FALSE);
  if (!*arg || type < 0) {
    int i;

    send_to_char(ch, "����: ��Ϻ��� <");
    for (i = 0; *history_types[i] != '\n'; i++) {
      send_to_char(ch, " %s ", history_types[i]);
      if (*history_types[i + 1] == '\n')
        send_to_char(ch, ">\r\n");
      else
        send_to_char(ch, "|");
    }
    return;
  }

  if (GET_HISTORY(ch, type) && GET_HISTORY(ch, type)->text && *GET_HISTORY(ch, type)->text) {
    struct txt_block *tmp;
    for (tmp = GET_HISTORY(ch, type); tmp; tmp = tmp->next)
      send_to_char(ch, "%s", tmp->text);
/* Make this a 1 if you want history to clear after viewing */
#if 0
      free_history(ch, type);
#endif
  } else
    send_to_char(ch, "�� ä�ο� ��ϵ� ������ �����ϴ�.\r\n");
}

#define HIST_LENGTH 100
void add_history(struct char_data *ch, char *str, int type)
{
  int i = 0;
  char time_str[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH];
  struct txt_block *tmp;
  time_t ct;

  if (IS_NPC(ch))
    return;

  tmp = GET_HISTORY(ch, type);
  ct = time(0);
  strftime(time_str, sizeof(time_str), "%H:%M ", localtime(&ct));

  sprintf(buf, "%s%s", time_str, str);

  if (!tmp) {
    CREATE(GET_HISTORY(ch, type), struct txt_block, 1);
    GET_HISTORY(ch, type)->text = strdup(buf);
  }
  else {
    while (tmp->next)
      tmp = tmp->next;
    CREATE(tmp->next, struct txt_block, 1);
    tmp->next->text = strdup(buf);

    for (tmp = GET_HISTORY(ch, type); tmp; tmp = tmp->next, i++);

    for (; i > HIST_LENGTH && GET_HISTORY(ch, type); i--) {
      tmp = GET_HISTORY(ch, type);
      GET_HISTORY(ch, type) = tmp->next;
      if (tmp->text)
        free(tmp->text);
      free(tmp);
    }
  }
  /* add this history message to ALL */
  if (type != HIST_ALL)
    add_history(ch, str, HIST_ALL);
}

ACMD(do_whois)
{
  struct char_data *victim = 0;
  int hours;
  int got_from_file = 0;
  char buf[MAX_STRING_LENGTH];

  one_argument(argument, buf);

  if (!*buf) {
    send_to_char(ch, "������ ������ �����?\r\n");
    return;
  }

  if (!(victim=get_player_vis(ch, buf, NULL, FIND_CHAR_WORLD)))
  {
     CREATE(victim, struct char_data, 1);
     clear_char(victim);
     
     new_mobile_data(victim);
     
     CREATE(victim->player_specials, struct player_special_data, 1);

     if (load_char(buf, victim) > -1)
       got_from_file = 1;
     else {
        send_to_char (ch, "�׷� ����� �����ϴ�.\r\n");
        free_char (victim);
        return;
     }
  }

  /* We either have our victim from file or he's playing or function has returned. */
  sprinttype(GET_SEX(victim), genders, buf, sizeof(buf));
  send_to_char(ch, "�̸�: %s %s\r\n����: %s\r\n", GET_NAME(victim),
                   (victim->player.title ? victim->player.title : ""), buf);


  sprinttype (victim->player.chclass, pc_class_types, buf, sizeof(buf));
  send_to_char(ch, "����: %s\r\n", buf);

  send_to_char(ch, "����: %d\r\n", GET_LEVEL(victim));

  if (!(GET_LEVEL(victim) < LVL_IMMORT) || (GET_LEVEL(ch) >= GET_LEVEL(victim))) {
    strftime(buf, sizeof(buf), "%a %b %d %Y", localtime(&(victim->player.time.logon)));

    hours = (time(0) - victim->player.time.logon) / 3600;

    if (!got_from_file) {
  send_to_char(ch, "������ ����: ���� �������Դϴ�!  (Idle %d Minutes)",
           victim->char_specials.timer * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN);

      if (!victim->desc)
        send_to_char(ch, "  (���Ӳ���)\r\n");
      else
        send_to_char(ch, "\r\n");

      if (PRF_FLAGGED(victim, PRF_AFK))
        send_to_char(ch, "%s%s is afk right now, so %s may not respond to communication.%s\r\n", CBGRN(ch, C_NRM), GET_NAME(victim), HSSH(victim), CCNRM(ch, C_NRM));
    }
    else if (hours > 0)
      send_to_char(ch, "������ ����: %s (%d�� %d�ð� ��)\r\n", buf, hours/24, hours%24);
    else
      send_to_char(ch, "������ ����: %s (%d����.)\r\n",
                   buf, (int)(time(0) - victim->player.time.logon)/60);
  }

  if (has_mail(GET_IDNUM(victim)))
     act("$E$u ������ ���ֽ��ϴ�.", FALSE, ch, 0, victim, TO_CHAR);
  else
     act("$E$u ������ �����ϴ�.", FALSE, ch, 0, victim, TO_CHAR);

  if (PLR_FLAGGED(victim, PLR_DELETED))
    send_to_char (ch, "***������***\r\n");

  if (!got_from_file && victim->desc != NULL && GET_LEVEL(ch) >= LVL_GOD) {
    protocol_t * prot = victim->desc->pProtocol;
    send_to_char(ch, "Client:  %s [%s]\r\n", prot->pVariables[eMSDP_CLIENT_ID]->pValueString, prot->pVariables[eMSDP_CLIENT_VERSION]->pValueString ? prot->pVariables[eMSDP_CLIENT_VERSION]->pValueString : "Unknown");
    send_to_char(ch, "Color:   %s\r\n", prot->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt ? "Xterm" : (prot->pVariables[eMSDP_ANSI_COLORS]->ValueInt ? "Ansi" : "None"));
    send_to_char(ch, "MXP:     %s\r\n", prot->bMXP ? "Yes" : "No");
    send_to_char(ch, "Charset: %s\r\n", prot->bCHARSET ? "Yes" : "No");
    send_to_char(ch, "MSP:     %s\r\n", prot->bMSP ? "Yes" : "No");
    send_to_char(ch, "ATCP:    %s\r\n", prot->bATCP ? "Yes" : "No");
    send_to_char(ch, "MSDP:    %s\r\n", prot->bMSDP ? "Yes" : "No");
  }

  if (got_from_file)
    free_char (victim);
}

static bool get_zone_levels(zone_rnum znum, char *buf)
{
  /* Create a string for the level restrictions for this zone. */
  if ((zone_table[znum].min_level == -1) && (zone_table[znum].max_level == -1)) {
    sprintf(buf, "<�̼���>");
    return FALSE;
  }

  if (zone_table[znum].min_level == -1) {
    sprintf(buf, "%d��������", zone_table[znum].max_level);
    return TRUE;
  }

  if (zone_table[znum].max_level == -1) {
    sprintf(buf, "%d��������", zone_table[znum].min_level);
    return TRUE;
  }

  sprintf(buf, "%d�������� %d��������", zone_table[znum].min_level, zone_table[znum].max_level);
  return TRUE;
}

ACMD(do_areas)
{
  int i, hilev=-1, lolev=-1, zcount=0, lev_set, len=0, tmp_len=0;
  char arg[MAX_INPUT_LENGTH], *second, lev_str[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
  bool show_zone = FALSE, overlap = FALSE, overlap_shown = FALSE;

  one_argument(argument, arg);

  if (*arg) {
    /* There was an arg typed - check for level range */
    second = strchr(arg, '-');
    if (second) {
      /* Check for 1st value */
      if (second == arg)
        lolev = 0;
      else
        lolev = atoi(arg);

      /* Check for 2nd value */
      if (*(second+1) == '\0' || !isdigit(*(second+1)) )
        hilev = 100;
      else
        hilev = atoi(second+1);

    } else {
      /* No range - single number */
      lolev = atoi(arg);
      hilev = -1;  /* No high level - indicates single level */
    }
  }
  if (hilev != -1 && lolev > hilev) {
    /* Swap hi and lo lev if needed */
    i     = lolev;
    lolev = hilev;
    hilev = i;
  }
 if (hilev != -1)
    send_to_char(ch, "������ ������ Ȯ��: %s%d to %d%s\r\n", QYEL, lolev, hilev, QNRM);
  else if (lolev != -1)
    send_to_char(ch, "������ Ȯ��: %s%d%s\r\n", QYEL, lolev, QNRM);
  else
    send_to_char(ch, "��� ������ Ȯ��.\r\n");

  for (i = 0; i <= top_of_zone_table; i++) {    /* Go through the whole zone table */
    show_zone = FALSE;
    overlap = FALSE;

    if (ZONE_FLAGGED(i, ZONE_GRID)) {           /* Is this zone 'on the grid' ?    */
      if (lolev == -1) {
        /* No range supplied, show all zones */
        show_zone = TRUE;
      } else if ((hilev == -1) && (lolev >= ZONE_MINLVL(i)) && (lolev <= ZONE_MAXLVL(i))) {
        /* Single number supplied, it's in this zone's range */
        show_zone = TRUE;
      } else if ((hilev != -1) && (lolev >= ZONE_MINLVL(i)) && (hilev <= ZONE_MAXLVL(i))) {
        /* Range supplied, it's completely within this zone's range (no overlap) */
        show_zone = TRUE;
      } else if ((hilev != -1) && ((lolev >= ZONE_MINLVL(i) && lolev <= ZONE_MAXLVL(i)) || (hilev <= ZONE_MAXLVL(i) && hilev >= ZONE_MINLVL(i)))) {
        /* Range supplied, it overlaps this zone's range */
        show_zone = TRUE;
        overlap = TRUE;
      } else if (ZONE_MAXLVL(i) < 0 && (lolev >= ZONE_MINLVL(i))) {
        /* Max level not set for this zone, but specified min in range */
        show_zone = TRUE;
      } else if (ZONE_MAXLVL(i) < 0 && (hilev >= ZONE_MINLVL(i))) {
        /* Max level not set for this zone, so just display it as red */
        show_zone = TRUE;
        overlap = TRUE;
      }
    }

    if (show_zone) {
      if (overlap) overlap_shown = TRUE;
      lev_set = get_zone_levels(i, lev_str);
      tmp_len = snprintf(buf+len, sizeof(buf)-len, "\tn(%3d) %s%-*s\tn %s%s\tn\r\n", ++zcount, overlap ? QRED : QCYN,
                 count_color_chars(zone_table[i].name)+30, zone_table[i].name,
                 lev_set ? "\tc" : "\tn", lev_set ? lev_str : "��� ����");
      len += tmp_len;
    }
  }
  tmp_len = snprintf(buf+len, sizeof(buf)-len, "%s%d%s���� �������� ã�ҽ��ϴ�.\r\n", QYEL, zcount, QNRM);
  len += tmp_len;

  if (overlap_shown) {
    snprintf(buf+len, sizeof(buf)-len, "Areas shown in \trred\tn may have some creatures outside the specified range.\r\n");
  }

  if (zcount == 0)
    send_to_char(ch, "�˻��� ������ �����ϴ�.\r\n");
  else
    page_string(ch->desc, buf, TRUE);
}

static void list_scanned_chars(struct char_data * list, struct char_data * ch, int
distance, int door)
{
  char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH - 1];

  const char *how_far[] = {
    "close by",
    "a ways off",
    "far off to the"
  };

  struct char_data *i;
  int count = 0;
  *buf = '\0';

/* this loop is a quick, easy way to help make a grammatical sentence
   (i.e., "You see x, x, y, and z." with commas, "and", etc.) */

  for (i = list; i; i = i->next_in_room)

/* put any other conditions for scanning someone in this if statement -
   i.e., if (CAN_SEE(ch, i) && condition2 && condition3) or whatever */

    if (CAN_SEE(ch, i))
     count++;

  if (!count)
    return;

  for (i = list; i; i = i->next_in_room) {

/* make sure to add changes to the if statement above to this one also, using
   or's to join them.. i.e.,
   if (!CAN_SEE(ch, i) || !condition2 || !condition3) */

    if (!CAN_SEE(ch, i))
      continue;
    if (!*buf)
      snprintf(buf, sizeof(buf), "You see %s", GET_NAME(i));
    else
      strncat(buf, GET_NAME(i), sizeof(buf) - strlen(buf) - 1);
    if (--count > 1)
      strncat(buf, ", ", sizeof(buf) - strlen(buf) - 1);
    else if (count == 1)
      strncat(buf, " and ", sizeof(buf) - strlen(buf) - 1);
    else {
      snprintf(buf2, sizeof(buf2), " %s %s.\r\n", how_far[distance], dirs[door]);
      strncat(buf, buf2, sizeof(buf) - strlen(buf) - 1);
    }

  }
  send_to_char(ch, "%s", buf);
}

ACMD(do_scan)
{
  int door;
  bool found=FALSE;

  int range;
  int maxrange = 3;

  room_rnum scanned_room = IN_ROOM(ch);

  if (IS_AFFECTED(ch, AFF_BLIND)) {
    send_to_char(ch, "You can't see a damned thing, you're blind!\r\n");
    return;
  }

  for (door = 0; door < DIR_COUNT; door++) {
    for (range = 1; range<= maxrange; range++) {
      if (world[scanned_room].dir_option[door] && world[scanned_room].dir_option[door]->to_room != NOWHERE &&
       !IS_SET(world[scanned_room].dir_option[door]->exit_info, EX_CLOSED) &&
       !IS_SET(world[scanned_room].dir_option[door]->exit_info, EX_HIDDEN)) {
        scanned_room = world[scanned_room].dir_option[door]->to_room;
        if (IS_DARK(scanned_room) && !CAN_SEE_IN_DARK(ch)) {
          if (world[scanned_room].people)
            send_to_char(ch, "%s: It's too dark to see, but you can hear shuffling.\r\n", dirs[door]);
          else
            send_to_char(ch, "%s: It is too dark to see anything.\r\n", dirs[door]);
          found=TRUE;
        } else {
          if (world[scanned_room].people) {
            list_scanned_chars(world[scanned_room].people, ch, range - 1, door);
            found=TRUE;
          }
        }
      }                  // end of if
      else
        break;
    }                    // end of range
    scanned_room = IN_ROOM(ch);
  }                      // end of directions
  if (!found) {
    send_to_char(ch, "You don't see anything nearby!\r\n");
  }
} // end of do_scan
