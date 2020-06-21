/**************************************************************************
*  File: zedit.c                                           Part of tbaMUD *
*  Usage: Oasis OLC - Zones and commands.                                 *
*                                                                         *
* Copyright 1996 Harvey Gilpin. 1997-2001 George Greer.                   *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "constants.h"
#include "genolc.h"
#include "genzon.h"
#include "oasis.h"
#include "dg_scripts.h"

/* Nasty internal macros to clean up the code. */
#define MYCMD		(OLC_ZONE(d)->cmd[subcmd])
#define OLC_CMD(d)	(OLC_ZONE(d)->cmd[OLC_VAL(d)])
#define MAX_DUPLICATES 100

/* local functions */
static int start_change_command(struct descriptor_data *d, int pos);
static void zedit_setup(struct descriptor_data *d, int room_num);
static void zedit_new_zone(struct char_data *ch, zone_vnum vzone_num, room_vnum bottom, room_vnum top);
static void zedit_save_internally(struct descriptor_data *d);
static void zedit_save_to_disk(int zone_num);
static void zedit_disp_menu(struct descriptor_data *d);
static void zedit_disp_comtype(struct descriptor_data *d);
static void zedit_disp_arg1(struct descriptor_data *d);
static void zedit_disp_arg2(struct descriptor_data *d);
static void zedit_disp_arg3(struct descriptor_data *d);

ACMD(do_oasis_zedit)
{
  int number = NOWHERE, save = 0, real_num;
  struct descriptor_data *d;
  char *stop;
  char sbot[MAX_STRING_LENGTH];
  char buf1[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  room_vnum bottom, top;

  /* No building as a mob or while being forced. */
  if (IS_NPC(ch) || !ch->desc || STATE(ch->desc) != CON_PLAYING)
    return;

  /* Parse any arguments. */
  stop = one_argument(two_arguments(argument, buf1, buf2), sbot);

  /* If no argument was given, use the zone the builder is standing in. */
  if (!*buf1)
    number = GET_ROOM_VNUM(IN_ROOM(ch));
  else if (!isdigit(*buf1)) {
    if (str_cmp("저장", buf1) == 0) {
      save = TRUE;

      if (is_number(buf2))
        number = atoidx(buf2);
      else if (GET_OLC_ZONE(ch) > 0) {
        zone_rnum zlok;

        if ((zlok = real_zone(GET_OLC_ZONE(ch))) == NOWHERE)
          number = NOWHERE;
        else
          number = genolc_zone_bottom(zlok);
      }

      if (number == NOWHERE) {
        send_to_char(ch, "몇 번 존 정보를 저장하시겠습니까?\r\n");
        return;
      }
    } else if (GET_LEVEL(ch) >= LVL_IMPL) {
      if (str_cmp("제작", buf1) || !stop || !*stop)
        send_to_char(ch, "형식: 제작 <존 번호> <시작 방> "
           "<끝 방> 존편집\r\n");
      else {
        if (atoi(stop) < 0 || atoi(sbot) < 0) {
          send_to_char(ch, "존 번호에는 음수를 포함할 수 없습니다.\r\n");
          return;
        }
        number = atoidx(buf2);
        if (number < 0)
          number = NOWHERE;
        bottom = atoidx(sbot);
        top = atoidx(stop);

        /* Setup the new zone (displays the menu to the builder). */
        zedit_new_zone(ch, number, bottom, top);
      }

      /* Done now, exit the function. */
      return;

    } else {
      send_to_char(ch, "아잇! 누군가가 다칠 지도 모르는 일을 멈추세요!\r\n");
      return;
    }
  }

  /* If a numeric argument was given, retrieve it. */
  if (number == NOWHERE)
    number = atoidx(buf1);

  /* Check that nobody is currently editing this zone. */
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) == CON_ZEDIT) {
      if (d->olc && OLC_NUM(d) == number) {
        send_to_char(ch, "그 존은 현재 %s님이 편집중입니다.\r\n",
          PERS(d->character, ch));
        return;
      }
    }
  }

  /* Store the builder's descriptor in d. */
  d = ch->desc;

  /* Give the builder's descriptor an OLC structure. */
  if (d->olc) {
    mudlog(BRF, LVL_IMMORT, TRUE, "SYSERR: do_oasis_zedit: Player already "
      "had olc structure.");
    free(d->olc);
  }

  CREATE(d->olc, struct oasis_olc_data, 1);

  /* Find the zone. */
  OLC_ZNUM(d) = save ? real_zone(number) : real_zone_by_thing(number);
  if (OLC_ZNUM(d) == NOWHERE) {
    send_to_char(ch, "그 번호에 해당하는 존이 없습니다!\r\n");

    /* Free the descriptor's OLC structure. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  /* Everyone but IMPLs can only edit zones they have been assigned. */
  if (!can_edit_zone(ch, OLC_ZNUM(d))) {
    send_cannot_edit(ch, zone_table[OLC_ZNUM(d)].number);
    /* Free the OLC structure. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  /* If we need to save, then save the zone. */
  if (save) {
    send_to_char(ch, "%d 존의 모든 존 정보를 저장합니다.\r\n",
      zone_table[OLC_ZNUM(d)].number);
    mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE,
      "OLC: %s saves zone information for zone %d.", GET_NAME(ch),
      zone_table[OLC_ZNUM(d)].number);

    /* Save the zone information to the zone file. */
    save_zone(OLC_ZNUM(d));

    /* Free the descriptor's OLC structure. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  OLC_NUM(d) = number;

  if ((real_num = real_room(number)) == NOWHERE) {
    write_to_output(d, "그 방은 존재하지 않습니다.\r\n");

    /* Free the descriptor's OLC structure. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  zedit_setup(d, real_num);
  STATE(d) = CON_ZEDIT;

  act("$n님이 존 편집을 시작합니다.", TRUE, d->character, 0, 0, TO_ROOM);
  SET_BIT_AR(PLR_FLAGS(ch), PLR_WRITING);

  mudlog(CMP, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "OLC: %s starts editing zone %d allowed zone %d",
    GET_NAME(ch), zone_table[OLC_ZNUM(d)].number, GET_OLC_ZONE(ch));
}

static void zedit_setup(struct descriptor_data *d, int room_num)
{
  struct zone_data *zone;
  int subcmd = 0, count = 0, cmd_room = NOWHERE, i;

  /* Allocate one scratch zone structure. */
  CREATE(zone, struct zone_data, 1);

  /* Copy all the zone header information over. */
  zone->name = strdup(zone_table[OLC_ZNUM(d)].name);
  if (zone_table[OLC_ZNUM(d)].builders)
    zone->builders = strdup(zone_table[OLC_ZNUM(d)].builders);
  zone->lifespan = zone_table[OLC_ZNUM(d)].lifespan;
  zone->bot = zone_table[OLC_ZNUM(d)].bot;
  zone->top = zone_table[OLC_ZNUM(d)].top;
  zone->reset_mode = zone_table[OLC_ZNUM(d)].reset_mode;
  /* The remaining fields are used as a 'has been modified' flag */
  zone->number = 0;	/* Header information has changed.	*/
  zone->age = 0;	/* The commands have changed.		*/

  for (i=0; i<ZN_ARRAY_MAX; i++)
    zone->zone_flags[(i)] = zone_table[OLC_ZNUM(d)].zone_flags[(i)];

  zone->min_level = zone_table[OLC_ZNUM(d)].min_level;
  zone->max_level = zone_table[OLC_ZNUM(d)].max_level;

  /* Start the reset command list with a terminator. */
  CREATE(zone->cmd, struct reset_com, 1);
  zone->cmd[0].command = 'S';

  /* Add all entries in zone_table that relate to this room. */
  while (ZCMD(OLC_ZNUM(d), subcmd).command != 'S') {
    switch (ZCMD(OLC_ZNUM(d), subcmd).command) {
    case 'M':
    case 'O':
    case 'T':
    case 'V':
      cmd_room = ZCMD(OLC_ZNUM(d), subcmd).arg3;
      break;
    case 'D':
    case 'R':
      cmd_room = ZCMD(OLC_ZNUM(d), subcmd).arg1;
      break;
    default:
      break;
    }
    if (cmd_room == room_num) {
      add_cmd_to_list(&(zone->cmd), &ZCMD(OLC_ZNUM(d), subcmd), count);
      count++;
    }
    subcmd++;
  }

  OLC_ZONE(d) = zone;
  /*
   * Display main menu.
   */
  zedit_disp_menu(d);
}

/* Create a new zone. */
static void zedit_new_zone(struct char_data *ch, zone_vnum vzone_num, room_vnum bottom, room_vnum top)
{
  int result;
  const char *error;
  struct descriptor_data *dsc;

  if ((result = create_new_zone(vzone_num, bottom, top, &error)) == NOWHERE) {
    write_to_output(ch->desc, "%s", error);
    return;
  }

  for (dsc = descriptor_list; dsc; dsc = dsc->next) {
    switch (STATE(dsc)) {
      case CON_REDIT:
        OLC_ROOM(dsc)->zone += (OLC_ZNUM(dsc) >= result);
        /* Fall through. */
      case CON_ZEDIT:
      case CON_MEDIT:
      case CON_SEDIT:
      case CON_OEDIT:
      case CON_TRIGEDIT:
      case CON_QEDIT:
        OLC_ZNUM(dsc) += (OLC_ZNUM(dsc) >= result);
        break;
      default:
        break;
    }
  }

  zedit_save_to_disk(result); /* save to disk .. */

  mudlog(BRF, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE, "OLC: %s creates new zone #%d", GET_NAME(ch), vzone_num);
  write_to_output(ch->desc, "새로운 존을 성공적으로 만들었습니다.\r\n");
}

/* Save all the information in the player's temporary buffer back into
 * the current zone table. */
static void zedit_save_internally(struct descriptor_data *d)
{
  int	mobloaded = FALSE,
	objloaded = FALSE,
	subcmd, i;
  room_rnum room_num = real_room(OLC_NUM(d));

  if (room_num == NOWHERE) {
    log("SYSERR: zedit_save_internally: OLC_NUM(d) room %d not found.", OLC_NUM(d));
    return;
  }

  remove_room_zone_commands(OLC_ZNUM(d), room_num);

  /* Now add all the entries in the players descriptor list */
  for (subcmd = 0; MYCMD.command != 'S'; subcmd++) {
    /* Since Circle does not keep track of what rooms the 'G', 'E', and
     * 'P' commands are exitted in, but OasisOLC groups zone commands
     * by rooms, this creates interesting problems when builders use these
     * commands without loading a mob or object first.  This fix prevents such
     * commands from being saved and 'wandering' through the zone command
     * list looking for mobs/objects to latch onto.
     * C.Raehl 4/27/99 */
    switch (MYCMD.command) {
      /* Possible fail cases. */
      case 'G':
      case 'E':
        if (mobloaded)
          break;
        write_to_output(d, "맙을 먼저 로드하지 않았기 때문에 장비/소지품 생성 작업은 저장되지 않았습니다.\r\n");
        continue;
      case 'P':
        if (objloaded)
          break;
        write_to_output(d, "다른 물건을 먼저 로드하지 않았기 때문에 물건 넣기 작업은 저장되지 않았습니다.\r\n");
        continue;
      /* Pass cases. */
      case 'M':
        mobloaded = TRUE;
        break;
      case 'O':
        objloaded = TRUE;
        break;
      default:
        mobloaded = objloaded = FALSE;
        break;
    }
    add_cmd_to_list(&(zone_table[OLC_ZNUM(d)].cmd), &MYCMD, subcmd);
  }

  /* Finally, if zone headers have been changed, copy over */
  if (OLC_ZONE(d)->number) {
    free(zone_table[OLC_ZNUM(d)].name);
    free(zone_table[OLC_ZNUM(d)].builders);

    zone_table[OLC_ZNUM(d)].name = strdup(OLC_ZONE(d)->name);
    zone_table[OLC_ZNUM(d)].builders = strdup(OLC_ZONE(d)->builders);
    zone_table[OLC_ZNUM(d)].bot = OLC_ZONE(d)->bot;
    zone_table[OLC_ZNUM(d)].top = OLC_ZONE(d)->top;
    zone_table[OLC_ZNUM(d)].reset_mode = OLC_ZONE(d)->reset_mode;
    zone_table[OLC_ZNUM(d)].lifespan = OLC_ZONE(d)->lifespan;
    zone_table[OLC_ZNUM(d)].min_level = OLC_ZONE(d)->min_level;
    zone_table[OLC_ZNUM(d)].max_level = OLC_ZONE(d)->max_level;
    for (i=0; i<ZN_ARRAY_MAX; i++)
      zone_table[OLC_ZNUM(d)].zone_flags[(i)] = OLC_ZONE(d)->zone_flags[(i)];
  }
  add_to_save_list(zone_table[OLC_ZNUM(d)].number, SL_ZON);
}

static void zedit_save_to_disk(int zone)
{
  save_zone(zone);
}

/* Error check user input and then setup change */
static int start_change_command(struct descriptor_data *d, int pos)
{
  if (pos < 0 || pos >= count_commands(OLC_ZONE(d)->cmd))
    return 0;

  /* Ok, let's get editing. */
  OLC_VAL(d) = pos;
  return 1;
}

/*------------------------------------------------------------------*/
static void zedit_disp_flag_menu(struct descriptor_data *d)
{
  char bits[MAX_STRING_LENGTH];

  clear_screen(d);
  column_list(d->character, 0, zone_bits, NUM_ZONE_FLAGS, TRUE);

  sprintbitarray(OLC_ZONE(d)->zone_flags, zone_bits, ZN_ARRAY_MAX, bits);
  write_to_output(d, "\r\n존 속성: \tc%s\tn\r\n"
         "존 속성을 입력하세요, 0에서 quit : ", bits);
  OLC_MODE(d) = ZEDIT_ZONE_FLAGS;
}

/*------------------------------------------------------------------*/
static bool zedit_get_levels(struct descriptor_data *d, char *buf)
{
  /* Create a string for the recommended levels for this zone. */
  if ((OLC_ZONE(d)->min_level == -1) && (OLC_ZONE(d)->max_level == -1)) {
    sprintf(buf, "<설정값 없음!>");
    return FALSE;
  }

  if (OLC_ZONE(d)->min_level == -1) {
    sprintf(buf, "최대 레벨 %d", OLC_ZONE(d)->max_level);
    return TRUE;
  }

  if (OLC_ZONE(d)->max_level == -1) {
    sprintf(buf, "최소 레벨 %d", OLC_ZONE(d)->min_level);
    return TRUE;
  }

  sprintf(buf, "%d 레벨에서 %d 레벨까지", OLC_ZONE(d)->min_level, OLC_ZONE(d)->max_level);
  return TRUE;
}

/*------------------------------------------------------------------*/
/* Menu functions */
/* the main menu */
static void zedit_disp_menu(struct descriptor_data *d)
{
  int subcmd = 0, counter = 0;
  char buf1[MAX_STRING_LENGTH], lev_string[50];
  bool levels_set = FALSE;

  get_char_colors(d->character);
  clear_screen(d);

  sprintbitarray(OLC_ZONE(d)->zone_flags, zone_bits, ZN_ARRAY_MAX, buf1);
  levels_set = zedit_get_levels(d, lev_string);

  /* Menu header */
  send_to_char(d->character,
	  "방 번호 : %s%d%s 존 번호: %s%d\r\n"
	  "%s1%s) 제작자       : %s%s\r\n"
	  "%sZ%s) 존 이름      : %s%s\r\n"
	  "%sL%s) 리젠주기       : %s%d 분\r\n"
	  "%sB%s) 존 시작 : %s%d\r\n"
	  "%sT%s) 존 끝    : %s%d\r\n"
	  "%sR%s) 리셋 모드     : %s%s\r\n"
	  "%sF%s) 존 속성     : %s%s\r\n"
	  "%sM%s) 출입 가능 레벨 : %s%s%s\r\n"
	  "[리젠 시 수행 작업 목록]\r\n",

	  cyn, OLC_NUM(d), nrm,
	  cyn, zone_table[OLC_ZNUM(d)].number,
	  grn, nrm, yel, OLC_ZONE(d)->builders ? OLC_ZONE(d)->builders : "None.",
	  grn, nrm, yel, OLC_ZONE(d)->name ? OLC_ZONE(d)->name : "<NONE!>",
	  grn, nrm, yel, OLC_ZONE(d)->lifespan,
	  grn, nrm, yel, OLC_ZONE(d)->bot,
	  grn, nrm, yel, OLC_ZONE(d)->top,
	  grn, nrm, yel,
	  OLC_ZONE(d)->reset_mode ? ((OLC_ZONE(d)->reset_mode == 1) ? "Reset when no players are in zone." : "Normal reset.") : "Never reset",
	  grn, nrm, cyn, buf1,
	  grn, nrm, levels_set ? cyn : yel, lev_string,
	  nrm
	  );

  /* Print the commands for this room into display buffer. */
  while (MYCMD.command != 'S') {
    /* Translate what the command means. */
    write_to_output(d, "%s%d - %s", nrm, counter++, yel);
    switch (MYCMD.command) {
    case 'M':
      write_to_output(d, "%s 맙 로드 %s [%s%d%s], 최대개수 : %d",
              MYCMD.if_flag ? " then " : "",
              mob_proto[MYCMD.arg1].player.short_descr, cyn,
              mob_index[MYCMD.arg1].vnum, yel, MYCMD.arg2
              );
      break;
    case 'G':
      write_to_output(d, "%s %s[%s%d%s] 물건을 위 맙 소지품에 추가, 최대개수 : %d",
	      MYCMD.if_flag ? " then " : "",
	      obj_proto[MYCMD.arg1].short_description,
	      cyn, obj_index[MYCMD.arg1].vnum, yel,
	      MYCMD.arg2
	      );
      break;
    case 'O':
      write_to_output(d, "%s %s[%s%d%s] 물건을 방에 로드, 최대개수 : %d",
	      MYCMD.if_flag ? " then " : "",
	      obj_proto[MYCMD.arg1].short_description,
	      cyn, obj_index[MYCMD.arg1].vnum, yel,
	      MYCMD.arg2
	      );
      break;
    case 'E':
      write_to_output(d, "%s 장비 착용 %s [%s%d%s], %s, 최대개수 : %d",
	      MYCMD.if_flag ? " then " : "",
	      obj_proto[MYCMD.arg1].short_description,
	      cyn, obj_index[MYCMD.arg1].vnum, yel,
	      equipment_types[MYCMD.arg3],
	      MYCMD.arg2
	      );
      break;
    case 'P':
      write_to_output(d, "%s%s 물건[%s%d%s]을 %s[%s%d%s]에 넣음, 최대개수 : %d",
	      MYCMD.if_flag ? " then " : "",
	      obj_proto[MYCMD.arg1].short_description,
	      cyn, obj_index[MYCMD.arg1].vnum, yel,
	      obj_proto[MYCMD.arg3].short_description,
	      cyn, obj_index[MYCMD.arg3].vnum, yel,
	      MYCMD.arg2
	      );
      break;
    case 'R':
      write_to_output(d, "%s %s[%s%d%s] 물건을 방에서 삭제.",
	      MYCMD.if_flag ? " then " : "",
	      obj_proto[MYCMD.arg2].short_description,
	      cyn, obj_index[MYCMD.arg2].vnum, yel
	      );
      break;
    case 'D':
      write_to_output(d, "%s%s쪽 방향 문을 %s 상태로 초기화.",
	      MYCMD.if_flag ? " then " : "",
	      dirs[MYCMD.arg2],
	      MYCMD.arg3 ? ((MYCMD.arg3 == 1) ? "closed" : "locked") : "open"
	      );
      break;
    case 'T':
      write_to_output(d, "%s Attach trigger %s%s%s [%s%d%s] to %s",
        MYCMD.if_flag ? " then " : "",
        cyn, trig_index[MYCMD.arg2]->proto->name, yel,
        cyn, trig_index[MYCMD.arg2]->vnum, yel,
        ((MYCMD.arg1 == MOB_TRIGGER) ? "mobile" :
          ((MYCMD.arg1 == OBJ_TRIGGER) ? "object" :
            ((MYCMD.arg1 == WLD_TRIGGER)? "room" : "????"))));
      break;
    case 'V':
      write_to_output(d, "%sAssign global %s:%d to %s = %s",
        MYCMD.if_flag ? " then " : "",
        MYCMD.sarg1, MYCMD.arg2,
        ((MYCMD.arg1 == MOB_TRIGGER) ? "mobile" :
          ((MYCMD.arg1 == OBJ_TRIGGER) ? "object" :
            ((MYCMD.arg1 == WLD_TRIGGER)? "room" : "????"))),
        MYCMD.sarg2);
      break;
    default:
      write_to_output(d, "<알 수 없는 작업>");
      break;
    }
    write_to_output(d, "\r\n");
    subcmd++;
  }
  /* Finish off menu */
   write_to_output(d,
	  "%s%d - <목록 끝>\r\n"
	  "%sN%s) 새로운 작업 추가.\r\n"
	  "%sE%s) 작업 수정.\r\n"
	  "%sD%s) 작업 삭제.\r\n"
	  "%sQ%s) 종료\r\n선택하세요 : ",
	  nrm, counter, grn, nrm, grn, nrm, grn, nrm, grn, nrm
	  );

  OLC_MODE(d) = ZEDIT_MAIN_MENU;
}

/* Print the command type menu and setup response catch. */
static void zedit_disp_comtype(struct descriptor_data *d)
{
  get_char_colors(d->character);
  clear_screen(d);
  write_to_output(d,
	"\r\n"
	"%sM%s) 방에 맙 창조             %sO%s) 방에 물건 창조\r\n"
	"%sE%s) 맙에 장비 착용        %sG%s) 맙 소지품에 물건 추가\r\n"
	"%sP%s) 물건을 다른 물건에 넣음    %sD%s) 열린/닫힌/잠긴 문 설정\r\n"
	"%sR%s) 방에서 물건 제거\r\n"
        "%sT%s) 트리거 할당                %sV%s) 전역 변수 설정\r\n"
	"\r\n"
	"어떤 작업을 하시겠습니까?",
	grn, nrm, grn, nrm, grn, nrm, grn, nrm, grn, nrm,
	grn, nrm, grn, nrm, grn, nrm, grn, nrm
	);
  OLC_MODE(d) = ZEDIT_COMMAND_TYPE;
}

/* Print the appropriate message for the command type for arg1 and set
   up the input catch clause */
static void zedit_disp_arg1(struct descriptor_data *d)
{
  write_to_output(d, "\r\n");

  switch (OLC_CMD(d).command) {
  case 'M':
    write_to_output(d, "맙 번호를 입력하세요 : ");
    OLC_MODE(d) = ZEDIT_ARG1;
    break;
  case 'O':
  case 'E':
  case 'P':
  case 'G':
    write_to_output(d, "물건 번호를 입력하세요 : ");
    OLC_MODE(d) = ZEDIT_ARG1;
    break;
  case 'D':
  case 'R':
    /* Arg1 for these is the room number, skip to arg2 */
    OLC_CMD(d).arg1 = real_room(OLC_NUM(d));
    zedit_disp_arg2(d);
    break;
  case 'T':
  case 'V':
    write_to_output(d, "트리거 종류(0:맙, 1:물건, 2:방)를 입력하세요 :");
    OLC_MODE(d) = ZEDIT_ARG1;
    break;
  default:
    /* We should never get here. */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: zedit_disp_arg1(): Help!");
    write_to_output(d, "이런... 개발자에게 문의하세요\r\n");
    return;
  }
}

/* Print the appropriate message for the command type for arg2 and set
   up the input catch clause. */
static void zedit_disp_arg2(struct descriptor_data *d)
{
  int i;

  write_to_output(d, "\r\n");

  switch (OLC_CMD(d).command) {
  case 'M':
  case 'O':
  case 'E':
  case 'P':
  case 'G':
    write_to_output(d, "리젠으로 생성될 수 있는 최대 수를 입력하세요 : ");
    break;
  case 'D':
    for (i = 0; *dirs[i] != '\n'; i++) {
      write_to_output(d, "%d) 출구 %s.\r\n", i, dirs[i]);
    }
    write_to_output(d, "문 값을 변경할 출구 번호를 입력하세요 : ");
    break;
  case 'R':
    write_to_output(d, "물건 번호를 입력하세요 : ");
    break;
  case 'T':
    write_to_output(d, "트리거 번호를 입력하세요 : ");
    break;
  case 'V':
    write_to_output(d, "전 지역 상황 (0은 none) : ");
    break;
  default:
    /* We should never get here, but just in case.  */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: zedit_disp_arg2(): Help!");
    write_to_output(d, "이런... 개발자에게 문의하세요\r\n");
    return;
  }
  OLC_MODE(d) = ZEDIT_ARG2;
}

/* Print the appropriate message for the command type for arg3 and set
   up the input catch clause. */
static void zedit_disp_arg3(struct descriptor_data *d)
{

  write_to_output(d, "\r\n");

  switch (OLC_CMD(d).command) {
  case 'E':
    column_list(d->character, 0, equipment_types, NUM_WEARS, TRUE);
    write_to_output(d, "장비 위치 : ");
    break;
  case 'P':
    write_to_output(d, "보관함의 가상 번호 : ");
    break;
  case 'D':
    write_to_output(d, 	"0)  문 열림\r\n"
		"1)  문 닫힘\r\n"
		"2)  문 잠김\r\n"
		"문 상태를 입력하세요 : ");
    break;
  case 'V':
  case 'T':
  case 'M':
  case 'O':
  case 'R':
  case 'G':
  default:
    /* We should never get here, just in case. */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: zedit_disp_arg3(): Help!");
    write_to_output(d, "이런... 개발자에게 문의하세요\r\n");
    return;
  }
  OLC_MODE(d) = ZEDIT_ARG3;
}

/*-------------------------------------------------------------------*/
/*
 * Print the recommended levels menu and setup response catch.
 */
static void zedit_disp_levels(struct descriptor_data *d)
{
  char lev_string[50];
  bool levels_set = FALSE;

  levels_set = zedit_get_levels(d, lev_string);

  clear_screen(d);
  write_to_output(d,
	"\r\n"
	"\ty1\tn) 권장 최소 레벨 설정\r\n"
	"\ty2\tn) 권장 최고 레벨 설정\r\n"
	"\ty3\tn) 권장 레벨 초기화\r\n\r\n"
	"\ty0\tn) 메인 메뉴\r\n"
        "\tg현재 설정: %s%s%s\r\n"
	"\r\n"
	"선택하세요 (0에서 quit): ", levels_set ? "\tc" : "\ty", lev_string, "\tn"
	);
  OLC_MODE(d) = ZEDIT_LEVELS;
}

/* The event handler */
void zedit_parse(struct descriptor_data *d, char *arg)
{
  int pos, number;

  switch (OLC_MODE(d)) {
  case ZEDIT_CONFIRM_SAVESTRING:
    switch (*arg) {
    case 'y':
    case 'Y':
      /* Save the zone in memory, hiding invisible people. */
      zedit_save_internally(d);
      if (CONFIG_OLC_SAVE) {
        write_to_output(d, "존 정보를 디스크에 저장합니다.\r\n");
        zedit_save_to_disk(OLC_ZNUM(d));
      } else
        write_to_output(d, "존 정보를 메모리에 저장합니다.\r\n");

      mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(d->character)), TRUE, "OLC: %s edits zone info for room %d.", GET_NAME(d->character), OLC_NUM(d));
      /* FALL THROUGH */
    case 'n':
    case 'N':
      cleanup_olc(d, CLEANUP_ALL);
      break;
    default:
      write_to_output(d, "잘못 입력하셨습니다! (Y / N)\r\n");
      write_to_output(d, "변경 내용을 저장하시겠습니까?(y/n) :");
      break;
    }
    break;
   /* End of ZEDIT_CONFIRM_SAVESTRING */

  case ZEDIT_MAIN_MENU:
    switch (*arg) {
    case 'q':
    case 'Q':
      if (OLC_ZONE(d)->age || OLC_ZONE(d)->number) {
	write_to_output(d, "변경 내용을 저장하시겠습니까?(y/n) :");
	OLC_MODE(d) = ZEDIT_CONFIRM_SAVESTRING;
      } else {
	write_to_output(d, "변경 내용이 없습니다.\r\n");
	cleanup_olc(d, CLEANUP_ALL);
      }
      break;
    case 'n':
    case 'N':
      /* New entry. */
      if (OLC_ZONE(d)->cmd[0].command == 'S') {
        /* first command */
        if (new_command(OLC_ZONE(d), 0) && start_change_command(d, 0)) {
	  zedit_disp_comtype(d);
	  OLC_ZONE(d)->age = 1;
          break;
	}
      }
      write_to_output(d, "새로운 작업의 우선순위는 목록에서 몇 번째입니까?");
      OLC_MODE(d) = ZEDIT_NEW_ENTRY;
      break;
    case 'e':
    case 'E':
      /* Change an entry. */
      write_to_output(d, "몇 번 작업을 변경하시겠습니까?:");
      OLC_MODE(d) = ZEDIT_CHANGE_ENTRY;
      break;
    case 'd':
    case 'D':
      /* Delete an entry. */
      write_to_output(d, "몇 번 작업을 삭제하시겠습니까?");
      OLC_MODE(d) = ZEDIT_DELETE_ENTRY;
      break;
    case 'z':
    case 'Z':
      /* Edit zone name. */
      write_to_output(d, "새로운 존 이름을 입력하세요 : ");
      OLC_MODE(d) = ZEDIT_ZONE_NAME;
      break;
    case '1':
      /* Edit zone builders. */
      write_to_output(d, "존 제작자 명단을 입력하세요 : ");
      OLC_MODE(d) = ZEDIT_ZONE_BUILDERS;
      break;
    case 'b':
    case 'B':
      /* Edit bottom of zone. */
      if (GET_LEVEL(d->character) < LVL_IMPL)
	zedit_disp_menu(d);
      else {
	write_to_output(d, "존 시작 방 번호를 입력하세요 : ");
	OLC_MODE(d) = ZEDIT_ZONE_BOT;
      }
      break;
    case 't':
    case 'T':
      /* Edit top of zone. */
      if (GET_LEVEL(d->character) < LVL_IMPL)
	zedit_disp_menu(d);
      else {
	write_to_output(d, "존 끝 방 번호를 입력하세요 : ");
	OLC_MODE(d) = ZEDIT_ZONE_TOP;
      }
      break;
    case 'l':
    case 'L':
      /* Edit zone lifespan. */
      write_to_output(d, "리젠 주기를 입력하세요 : ");
      OLC_MODE(d) = ZEDIT_ZONE_LIFE;
      break;
    case 'r':
    case 'R':
      /* Edit zone reset mode. */
      write_to_output(d, "\r\n"
		"0) 리젠 안 함\r\n"
		"1) 존에 아무도 없을 때만 리젠\r\n"
		"2) 일반 리젠\r\n"
		"리젠 타입을 선택하세요 : ");
      OLC_MODE(d) = ZEDIT_ZONE_RESET;
      break;
    case 'f':
    case 'F':
      zedit_disp_flag_menu(d);
      break;
    case 'm':
    case 'M':
      /*** Edit zone level restrictions (sub-menu) ***/
      zedit_disp_levels(d);
      break;
    default:
      zedit_disp_menu(d);
      break;
    }
    break;
    /* End of ZEDIT_MAIN_MENU */

/*-------------------------------------------------------------------*/
  case ZEDIT_LEVELS:
    switch (*arg) {
    case '1': write_to_output(d, "이 존의 최소 레벨을 입력하세요 (0-%d, -1은 없음): ", (LVL_IMMORT-1));
              OLC_MODE(d) = ZEDIT_LEV_MIN;
              break;

    case '2': write_to_output(d, "이 존의 최대 레벨을 입력하세요 (0-%d, -1은 없음): ", (LVL_IMMORT-1));
              OLC_MODE(d) = ZEDIT_LEV_MAX;
              break;

    case '3': OLC_ZONE(d)->min_level = -1;
              OLC_ZONE(d)->max_level = -1;
              OLC_ZONE(d)->number = 1;
              zedit_disp_menu(d);
              break;

    case '0':
      zedit_disp_menu(d);
      break;

    default: write_to_output(d, "잘못 입력하셨습니다!\r\n");
             break;
    }
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_LEV_MIN:
    pos = atoi(arg);
    OLC_ZONE(d)->min_level = MIN(MAX(pos,-1), 100);
    OLC_ZONE(d)->number = 1;
    zedit_disp_levels(d);
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_LEV_MAX:
    pos = atoi(arg);
    OLC_ZONE(d)->max_level = MIN(MAX(pos,-1), 100);
    OLC_ZONE(d)->number = 1;
    zedit_disp_levels(d);
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_NEW_ENTRY:
    /* Get the line number and insert the new line. */
    pos = atoi(arg);
    if (isdigit(*arg) && new_command(OLC_ZONE(d), pos)) {
      if (start_change_command(d, pos)) {
	zedit_disp_comtype(d);
	OLC_ZONE(d)->age = 1;
      }
    } else
      zedit_disp_menu(d);
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_DELETE_ENTRY:
    /* Get the line number and delete the line. */
    pos = atoi(arg);
    if (isdigit(*arg)) {
      delete_zone_command(OLC_ZONE(d), pos);
      OLC_ZONE(d)->age = 1;
    }
    zedit_disp_menu(d);
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_CHANGE_ENTRY:
    /* Parse the input for which line to edit, and goto next quiz. Abort edit,
       and return to main menu idea from Mark Garringer zizazat@hotmail.com */
    if (toupper(*arg) == 'A') {
      if (OLC_CMD(d).command == 'N') {
        OLC_CMD(d).command = '*';
      }
      zedit_disp_menu(d);
      break;
    }

    pos = atoi(arg);
    if (isdigit(*arg) && start_change_command(d, pos)) {
      zedit_disp_comtype(d);
      OLC_ZONE(d)->age = 1;
    } else
      zedit_disp_menu(d);
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_COMMAND_TYPE:
    /* Parse the input for which type of command this is, and goto next quiz. */
    OLC_CMD(d).command = toupper(*arg);
    if (!OLC_CMD(d).command || (strchr("MOPEDGRTV", OLC_CMD(d).command) == NULL)) {
      write_to_output(d, "잘못 입력하셨습니다, 다시 시도 : ");
    } else {
      if (OLC_VAL(d)) {	/* If there was a previous command. */
        if (OLC_CMD(d).command == 'T' || OLC_CMD(d).command == 'V') {
          OLC_CMD(d).if_flag = 1;
          zedit_disp_arg1(d);
        } else {
	  write_to_output(d, "이 작업은 이전 작업 결과에 영향을 받습니까?(y/n)\r\n");
	  OLC_MODE(d) = ZEDIT_IF_FLAG;
        }
      } else {	/* 'if-flag' not appropriate. */
	OLC_CMD(d).if_flag = 0;
	zedit_disp_arg1(d);
      }
    }
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_IF_FLAG:
    /* Parse the input for the if flag, and goto next quiz. */
    switch (*arg) {
    case 'y':
    case 'Y':
      OLC_CMD(d).if_flag = 1;
      break;
    case 'n':
    case 'N':
      OLC_CMD(d).if_flag = 0;
      break;
    default:
      write_to_output(d, "다시 시도해 주세요 : ");
      return;
    }
    zedit_disp_arg1(d);
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_ARG1:
    /* Parse the input for arg1, and goto next quiz. */
    if (!isdigit(*arg)) {
      write_to_output(d, "숫자를 입력해야 합니다, 다시 시도 : ");
      return;
    }
    switch (OLC_CMD(d).command) {
    case 'M':
      if ((pos = real_mobile(atoi(arg))) != NOBODY) {
	OLC_CMD(d).arg1 = pos;
	zedit_disp_arg2(d);
      } else
	write_to_output(d, "그 번호의 맙은 존재하지 않습니다, 다시 시도 : ");
      break;
    case 'O':
    case 'P':
    case 'E':
    case 'G':
      if ((pos = real_object(atoi(arg))) != NOTHING) {
	OLC_CMD(d).arg1 = pos;
	zedit_disp_arg2(d);
      } else
	write_to_output(d, "그 번호의 물건은 존재하지 않습니다, 다시 시도 : ");
      break;
    case 'T':
    case 'V':
      if (atoi(arg)<MOB_TRIGGER || atoi(arg)>WLD_TRIGGER)
        write_to_output(d, "잘못 입력하셨습니다.");
      else {
       OLC_CMD(d).arg1 = atoi(arg);
        zedit_disp_arg2(d);
      }
      break;
    case 'D':
    case 'R':
    default:
      /* We should never get here. */
      cleanup_olc(d, CLEANUP_ALL);
      mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: zedit_parse(): case ARG1: Ack!");
      write_to_output(d, "이런... 개발자에게 문의하세요\r\n");
      break;
    }
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_ARG2:
    /* Parse the input for arg2, and goto next quiz. */
    if (!isdigit(*arg)) {
      write_to_output(d, "숫자를 입력해야 합니다, 다시 시도 : ");
      return;
    }
    switch (OLC_CMD(d).command) {
    case 'M':
    case 'O':
      OLC_CMD(d).arg2 = MIN(MAX_DUPLICATES, atoi(arg));
      OLC_CMD(d).arg3 = real_room(OLC_NUM(d));
      zedit_disp_menu(d);
      break;
    case 'G':
      OLC_CMD(d).arg2 = MIN(MAX_DUPLICATES, atoi(arg));
      zedit_disp_menu(d);
      break;
    case 'P':
    case 'E':
      OLC_CMD(d).arg2 = MIN(MAX_DUPLICATES, atoi(arg));
      zedit_disp_arg3(d);
      break;
    case 'V':
      OLC_CMD(d).arg2 = atoi(arg); /* context */
      OLC_CMD(d).arg3 = real_room(OLC_NUM(d));
      write_to_output(d, "Enter the global name : ");
      OLC_MODE(d) = ZEDIT_SARG1;
      break;
    case 'T':
      if (real_trigger(atoi(arg)) != NOTHING) {
        OLC_CMD(d).arg2 = real_trigger(atoi(arg)); /* trigger */
        OLC_CMD(d).arg3 = real_room(OLC_NUM(d));
        zedit_disp_menu(d);
      } else
        write_to_output(d, "그 트리거는 존재하지 않습니다, 다시 시도 : ");
      break;
    case 'D':
      pos = atoi(arg);
      /* Count directions. */
      if (pos < 0 || pos > DIR_COUNT)
	write_to_output(d, "다시 시도하세요 : ");
      else {
	OLC_CMD(d).arg2 = pos;
	zedit_disp_arg3(d);
      }
      break;
    case 'R':
      if ((pos = real_object(atoi(arg))) != NOTHING) {
	OLC_CMD(d).arg2 = pos;
	zedit_disp_menu(d);
      } else
	write_to_output(d, "그 물건은 존재하지 않습니다, 다시 시도 : ");
      break;
    default:
      /* We should never get here, but just in case. */
      cleanup_olc(d, CLEANUP_ALL);
      mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: zedit_parse(): case ARG2: Ack!");
      write_to_output(d, "이런... 개발자에게 문의하세요\r\n");
      break;
    }
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_ARG3:
    /* Parse the input for arg3, and go back to main menu. */
    if (!isdigit(*arg)) {
      write_to_output(d, "숫자를 입력해야 합니다, 다시 시도 : ");
      return;
    }
    switch (OLC_CMD(d).command) {
    case 'E':
      pos = atoi(arg) - 1;
      /* Count number of wear positions. */
      if (pos < 0 || pos >= NUM_WEARS)
	write_to_output(d, "다시 시도 : ");
      else {
	OLC_CMD(d).arg3 = pos;
	zedit_disp_menu(d);
      }
      break;
    case 'P':
      if ((pos = real_object(atoi(arg))) != NOTHING) {
	OLC_CMD(d).arg3 = pos;
	zedit_disp_menu(d);
      } else
	write_to_output(d, "그 물건은 존재하지 않습니다, 다시 시도 : ");
      break;
    case 'D':
      pos = atoi(arg);
      if (pos < 0 || pos > 2)
	write_to_output(d, "다시 시도 : ");
      else {
	OLC_CMD(d).arg3 = pos;
	zedit_disp_menu(d);
      }
      break;
    case 'M':
    case 'O':
    case 'G':
    case 'R':
    case 'T':
    case 'V':
    default:
      /* We should never get here, but just in case. */
      cleanup_olc(d, CLEANUP_ALL);
      mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: zedit_parse(): case ARG3: Ack!");
      write_to_output(d, "이런... 개발자에게 문의하세요\r\n");
      break;
    }
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_SARG1:
    if (strlen(arg)) {
      OLC_CMD(d).sarg1 = strdup(arg);
      OLC_MODE(d) = ZEDIT_SARG2;
      write_to_output(d, "Enter the global value : ");
    } else
      write_to_output(d, "Must have some name to assign : ");
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_SARG2:
    if (strlen(arg)) {
      OLC_CMD(d).sarg2 = strdup(arg);
      zedit_disp_menu(d);
    } else
      write_to_output(d, "설정 값이 필요합니다 :");
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_ZONE_NAME:
    /* Add new name and return to main menu. */
    if (genolc_checkstring(d, arg)) {
      if (OLC_ZONE(d)->name)
        free(OLC_ZONE(d)->name);
      else
        log("SYSERR: OLC: ZEDIT_ZONE_NAME: no name to free!");
      OLC_ZONE(d)->name = strdup(arg);
      OLC_ZONE(d)->number = 1;
    }
    zedit_disp_menu(d);
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_ZONE_BUILDERS:
    /* Add new builders list and return to main menu. */
    if (genolc_checkstring(d, arg)) {
      if (OLC_ZONE(d)->builders)
        free(OLC_ZONE(d)->builders);
      else
        log("SYSERR: OLC: ZEDIT_ZONE_BUILDERS: no builders list to free!");
      OLC_ZONE(d)->builders = strdup(arg);
      OLC_ZONE(d)->number = 1;
    }
    zedit_disp_menu(d);
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_ZONE_RESET:
    /* Parse and add new reset_mode and return to main menu. */
    pos = atoi(arg);
    if (!isdigit(*arg) || pos < 0 || pos > 2)
      write_to_output(d, "다시 시도하세요 (0-2) : ");
    else {
      OLC_ZONE(d)->reset_mode = pos;
      OLC_ZONE(d)->number = 1;
      zedit_disp_menu(d);
    }
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_ZONE_LIFE:
    /* Parse and add new lifespan and return to main menu. */
    pos = atoi(arg);
    if (!isdigit(*arg) || pos < 0 || pos > 240)
      write_to_output(d, "다시 시도하세요 (0-240) : ");
    else {
      OLC_ZONE(d)->lifespan = pos;
      OLC_ZONE(d)->number = 1;
      zedit_disp_menu(d);
    }
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_ZONE_FLAGS:
    number = atoi(arg);
    if (number < 0 || number > NUM_ZONE_FLAGS) {
      write_to_output(d, "잘못 입력하셨습니다!\r\n");
      zedit_disp_flag_menu(d);
    } else if (number == 0) {
      zedit_disp_menu(d);
      break;
      }
    else {
      /*
       * Toggle the bit.
       */
      TOGGLE_BIT_AR(OLC_ZONE(d)->zone_flags, number - 1);
      OLC_ZONE(d)->number = 1;
      zedit_disp_flag_menu(d);
    }
    return;

/*-------------------------------------------------------------------*/
  case ZEDIT_ZONE_BOT:
    /* Parse and add new bottom room in zone and return to main menu. */
    if (OLC_ZNUM(d) == 0)
      OLC_ZONE(d)->bot = LIMIT(atoi(arg), 0, OLC_ZONE(d)->top);
    else
      OLC_ZONE(d)->bot = LIMIT(atoi(arg), zone_table[OLC_ZNUM(d) - 1].top + 1, OLC_ZONE(d)->top);
    OLC_ZONE(d)->number = 1;
    zedit_disp_menu(d);
    break;

/*-------------------------------------------------------------------*/
  case ZEDIT_ZONE_TOP:
    /* Parse and add new top room in zone and return to main menu. */
    if (OLC_ZNUM(d) == top_of_zone_table)
      OLC_ZONE(d)->top = LIMIT(atoi(arg), genolc_zonep_bottom(OLC_ZONE(d)), 32000);
    else
      OLC_ZONE(d)->top = LIMIT(atoi(arg), genolc_zonep_bottom(OLC_ZONE(d)), genolc_zone_bottom(OLC_ZNUM(d) + 1) - 1);
    OLC_ZONE(d)->number = 1;
    zedit_disp_menu(d);
    break;

  default:
    /* We should never get here, but just in case... */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: zedit_parse(): Reached default case!");
    write_to_output(d, "이런... 개발자에게 문의하세요\r\n");
    break;
  }
}
