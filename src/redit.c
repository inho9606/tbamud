/**************************************************************************
*  File: redit.c                                           Part of tbaMUD *
*  Usage: Oasis OLC - Rooms.                                              *
*                                                                         *
* By Levork. Copyright 1996 Harvey Gilpin. 1997-2001 George Greer.        *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "boards.h"
#include "genolc.h"
#include "genwld.h"
#include "genzon.h"
#include "oasis.h"
#include "improved-edit.h"
#include "dg_olc.h"
#include "constants.h"
#include "modify.h"

/* local functions */
static void redit_setup_new(struct descriptor_data *d);
static void redit_disp_extradesc_menu(struct descriptor_data *d);
static void redit_disp_exit_menu(struct descriptor_data *d);
static void redit_disp_exit_flag_menu(struct descriptor_data *d);
static void redit_disp_flag_menu(struct descriptor_data *d);
static void redit_disp_sector_menu(struct descriptor_data *d);
static void redit_disp_menu(struct descriptor_data *d);

/* Utils and exported functions. */
ACMD(do_oasis_redit)
{
  char buf1[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  int number = NOWHERE, save = 0, real_num;
  struct descriptor_data *d;

  /* No building as a mob or while being forced. */
  if (IS_NPC(ch) || !ch->desc || STATE(ch->desc) != CON_PLAYING)
    return;

  /* Parse any arguments. */
  two_arguments(argument, buf1, buf2);

  if (!*buf1)
    number = GET_ROOM_VNUM(IN_ROOM(ch));
  else if (!isdigit(*buf1)) {
    if (str_cmp("저장", buf1) != 0) {
      send_to_char(ch, "앗!  누군가가 다칠 일을 멈추세요!\r\n");
      return;
    }

    save = TRUE;

    if (is_number(buf2))
      number = atoi(buf2);
    else if (GET_OLC_ZONE(ch) != NOWHERE) {
      zone_rnum zlok;

      if ((zlok = real_zone(GET_OLC_ZONE(ch))) == NOWHERE)
        number = NOWHERE;
      else
        number = genolc_zone_bottom(zlok);
    }

    if (number == NOWHERE) {
      send_to_char(ch, "어떤 존을 저장하시겠습니까?\r\n");
      return;
    }
  }

  /* If a numeric argument was given (like a room number), get it. */
  if (number == NOWHERE)
    number = atoi(buf1);

  if (number < IDXTYPE_MIN || number > IDXTYPE_MAX) {
    send_to_char(ch, "그런 방번호는 존재하지 않습니다.\r\n");
    return;
  }

  /* Check to make sure the room isn't already being edited. */
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) == CON_REDIT) {
      if (d->olc && OLC_NUM(d) == number) {
        send_to_char(ch, "그 방은 현재 %s님이 편집중입니다.\r\n",
          PERS(d->character, ch));
        return;
      }
    }
  }

  /* Retrieve the player's descriptor. */
  d = ch->desc;

  /* Give the descriptor an OLC structure. */
  if (d->olc) {
    mudlog(BRF, LVL_IMMORT, TRUE, "SYSERR: do_oasis_redit: Player already had olc structure.");
    free(d->olc);
  }

  /* Create the OLC structure. */
  CREATE(d->olc, struct oasis_olc_data, 1);

  /* Find the zone. */
  OLC_ZNUM(d) = save ? real_zone(number) : real_zone_by_thing(number);
  if (OLC_ZNUM(d) == NOWHERE) {
    send_to_char(ch, "그 번호에 해당하는 존이 없습니다!\r\n");
    free(d->olc);
    d->olc = NULL;
    return;
  }

  /* Make sure the builder is allowed to modify this zone. */
  if (!can_edit_zone(ch, OLC_ZNUM(d))) {
    send_cannot_edit(ch, zone_table[OLC_ZNUM(d)].number);
    /* Free the OLC structure. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  if (save) {
    send_to_char(ch, "%d 존 내의 모든 방을 저장합니다.\r\n", zone_table[OLC_ZNUM(d)].number);
    mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE, "OLC: %s saves room info for zone %d.", GET_NAME(ch), zone_table[OLC_ZNUM(d)].number);

    /* Save the rooms. */
    save_rooms(OLC_ZNUM(d));

    /* Free the olc data from the descriptor. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  OLC_NUM(d) = number;

  if ((real_num = real_room(number)) != NOWHERE)
    redit_setup_existing(d, real_num);
  else
    redit_setup_new(d);

  redit_disp_menu(d);
  STATE(d) = CON_REDIT;
  act("$n님이 방 편집을 시작합니다.", TRUE, d->character, 0, 0, TO_ROOM);
  SET_BIT_AR(PLR_FLAGS(ch), PLR_WRITING);

  mudlog(CMP, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "OLC: %s starts editing zone %d allowed zone %d",
    GET_NAME(ch), zone_table[OLC_ZNUM(d)].number, GET_OLC_ZONE(ch));
}

static void redit_setup_new(struct descriptor_data *d)
{
  CREATE(OLC_ROOM(d), struct room_data, 1);

  OLC_ROOM(d)->name = strdup("An unfinished room");
  OLC_ROOM(d)->description = strdup("You are in an unfinished room.\r\n");
  OLC_ROOM(d)->number = NOWHERE;
  OLC_ITEM_TYPE(d) = WLD_TRIGGER;
  OLC_ROOM(d)->proto_script = OLC_SCRIPT(d) = NULL;

  OLC_VAL(d) = 0;
}

void redit_setup_existing(struct descriptor_data *d, int real_num)
{
  struct room_data *room;
  int counter;

  /* Build a copy of the room for editing. */
  CREATE(room, struct room_data, 1);

  *room = world[real_num];
  
  /* Make new room people list be empty.                          */ 
  /* Fixes bug where copying a room from within that room creates */ 
  /* an infinite loop when you next act() in the new room (goto?) */ 
  /* and you are your next_in_room          -- anderyu (10-05-22) */ 
  room->people = NULL; 

  /* Nullify the events structure. */
  room->events = NULL;

  /* Allocate space for all strings. */
  room->name = str_udup(world[real_num].name);
  room->description = str_udup(world[real_num].description);

  /* Exits - We allocate only if necessary. */
  for (counter = 0; counter < DIR_COUNT; counter++) {
    if (world[real_num].dir_option[counter]) {
      CREATE(room->dir_option[counter], struct room_direction_data, 1);

      /* Copy the numbers over. */
      *room->dir_option[counter] = *world[real_num].dir_option[counter];
      /* Allocate the strings. */
      if (world[real_num].dir_option[counter]->general_description)
        room->dir_option[counter]->general_description = strdup(world[real_num].dir_option[counter]->general_description);
      if (world[real_num].dir_option[counter]->keyword)
        room->dir_option[counter]->keyword = strdup(world[real_num].dir_option[counter]->keyword);
    }
  }

  /* Extra descriptions, if necessary. */
  if (world[real_num].ex_description) {
    struct extra_descr_data *tdesc, *temp, *temp2;
    CREATE(temp, struct extra_descr_data, 1);

    room->ex_description = temp;
    for (tdesc = world[real_num].ex_description; tdesc; tdesc = tdesc->next) {
      temp->keyword = strdup(tdesc->keyword);
      temp->description = strdup(tdesc->description);
      if (tdesc->next) {
	CREATE(temp2, struct extra_descr_data, 1);
	temp->next = temp2;
	temp = temp2;
      } else
	temp->next = NULL;
    }
  }

  /* Attach copy of room to player's descriptor. */
  OLC_ROOM(d) = room;
  OLC_VAL(d) = 0;
  OLC_ITEM_TYPE(d) = WLD_TRIGGER;

  dg_olc_script_copy(d);
  room->proto_script = NULL;
  SCRIPT(room) = NULL;
}

void redit_save_internally(struct descriptor_data *d)
{
  int j, room_num, new_room = FALSE;
  struct descriptor_data *dsc;

  if (OLC_ROOM(d)->number == NOWHERE)
    new_room = TRUE;

  OLC_ROOM(d)->number = OLC_NUM(d); 
  /* FIXME: Why is this not set elsewhere? */
  OLC_ROOM(d)->zone = OLC_ZNUM(d);

  if ((room_num = add_room(OLC_ROOM(d))) == NOWHERE) {
    write_to_output(d, "잘못 입력하셨습니다..\r\n");
    log("SYSERR: redit_save_internally: Something failed! (%d)", room_num);
    return;
  }

  /* Update triggers and free old proto list */
  if (world[room_num].proto_script &&
      world[room_num].proto_script != OLC_SCRIPT(d))
    free_proto_script(&world[room_num], WLD_TRIGGER);

  world[room_num].proto_script = OLC_SCRIPT(d);
  assign_triggers(&world[room_num], WLD_TRIGGER);
  /* end trigger update */

  /* Don't adjust numbers on a room update. */
  if (!new_room)
    return;

  /* Idea contributed by C.Raehl 4/27/99 */
  for (dsc = descriptor_list; dsc; dsc = dsc->next) {
    if (dsc == d)
      continue;

    if (STATE(dsc) == CON_ZEDIT) {
      for (j = 0; OLC_ZONE(dsc)->cmd[j].command != 'S'; j++)
        switch (OLC_ZONE(dsc)->cmd[j].command) {
          case 'O':
          case 'M':
          case 'T':
          case 'V':
            OLC_ZONE(dsc)->cmd[j].arg3 += (OLC_ZONE(dsc)->cmd[j].arg3 >= room_num);
            break;
          case 'D':
            OLC_ZONE(dsc)->cmd[j].arg2 += (OLC_ZONE(dsc)->cmd[j].arg2 >= room_num);
            /* Fall through */
          case 'R':
            OLC_ZONE(dsc)->cmd[j].arg1 += (OLC_ZONE(dsc)->cmd[j].arg1 >= room_num);
            break;
          }
    } else if (STATE(dsc) == CON_REDIT) {
      for (j = 0; j < DIR_COUNT; j++)
        if (OLC_ROOM(dsc)->dir_option[j])
          if (OLC_ROOM(dsc)->dir_option[j]->to_room >= room_num)
            OLC_ROOM(dsc)->dir_option[j]->to_room++;
    }
  }
}

void redit_save_to_disk(zone_vnum zone_num)
{
  save_rooms(zone_num);		/* :) */
}

void free_room(struct room_data *room)
{
  /* Free the strings (Mythran). */
  free_room_strings(room);

  if (SCRIPT(room))
    extract_script(room, WLD_TRIGGER);
  free_proto_script(room, WLD_TRIGGER);

  /* Free the room. */
  free(room);	/* XXX ? */
}

/* Menu functions */
/* For extra descriptions. */
static void redit_disp_extradesc_menu(struct descriptor_data *d)
{
  struct extra_descr_data *extra_desc = OLC_DESC(d);

  clear_screen(d);
  write_to_output(d,
	  "%s1%s) Keywords: %s%s\r\n"
	  "%s2%s) Description:\r\n%s%s\r\n"
	  "%s3%s) Goto next description: ",

	  grn, nrm, yel, extra_desc->keyword ? extra_desc->keyword : "<NONE>",
	  grn, nrm, yel, extra_desc->description ? extra_desc->description : "<NONE>",
	  grn, nrm
	  );

  write_to_output(d, !extra_desc->next ? "Not Set.\r\n" : "Set.\r\n");
  write_to_output(d, "0부터 quit 중에 선택하세요 :");
  OLC_MODE(d) = REDIT_EXTRADESC_MENU;
}

/* For exits. */
static void redit_disp_exit_menu(struct descriptor_data *d)
{
  char door_buf[24];
  /* if exit doesn't exist, alloc/create it */
  if (OLC_EXIT(d) == NULL) {
    CREATE(OLC_EXIT(d), struct room_direction_data, 1);
    OLC_EXIT(d)->to_room = NOWHERE;
  }
  
  /* Weird door handling! */
  if (IS_SET(OLC_EXIT(d)->exit_info, EX_ISDOOR)) {
    if (IS_SET(OLC_EXIT(d)->exit_info, EX_PICKPROOF) && IS_SET(OLC_EXIT(d)->exit_info, EX_HIDDEN))
      strncpy(door_buf, "숨겨진 증명 필요", sizeof(door_buf)-1);
    else if (IS_SET(OLC_EXIT(d)->exit_info, EX_PICKPROOF))
      strncpy(door_buf, "증명 필요", sizeof(door_buf)-1);
    else if (IS_SET(OLC_EXIT(d)->exit_info, EX_HIDDEN))
      strncpy(door_buf, "숨겨진 문", sizeof(door_buf)-1);
    else
      strncpy(door_buf, "일반 문", sizeof(door_buf)-1);
  } else
    strncpy(door_buf, "문 없음", sizeof(door_buf)-1);

  get_char_colors(d->character);
  clear_screen(d);
  write_to_output(d,
	  "%s1%s) 연결 방번호     : %s%d\r\n"
	  "%s2%s) 묘사 :-\r\n%s%s\r\n"
	  "%s3%s) 문 이름   : %s%s\r\n"
	  "%s4%s) 열쇠         : %s%d\r\n"
	  "%s5%s) 문 속성  : %s%s\r\n"
	  "%s6%s) 출구 제거.\r\n"
	  "0에서 6 중에 선택하세요 : ",

	  grn, nrm, cyn, OLC_EXIT(d)->to_room != NOWHERE ? world[OLC_EXIT(d)->to_room].number : -1,
	  grn, nrm, yel, OLC_EXIT(d)->general_description ? OLC_EXIT(d)->general_description : "<NONE>",
	  grn, nrm, yel, OLC_EXIT(d)->keyword ? OLC_EXIT(d)->keyword : "<NONE>",
	  grn, nrm, cyn, OLC_EXIT(d)->key != NOTHING ? OLC_EXIT(d)->key : -1,
	  grn, nrm, cyn, door_buf, grn, nrm
	  );

  OLC_MODE(d) = REDIT_EXIT_MENU;
}

/* For exit flags. */
static void redit_disp_exit_flag_menu(struct descriptor_data *d)
{
  get_char_colors(d->character);
  write_to_output(d, "%s0%s) 문 없음\r\n"
	  "%s1%s) 개폐식 문\r\n"
      "%s2%s) 증명필요 문\r\n"
      "%s3%s) 숨겨진 문\r\n"
      "%s4%s) 숨겨진 증명필요 문\r\n"
      "선택하세요 : ", grn, nrm, grn, nrm, grn, nrm, grn, nrm, grn, nrm);
}

/* For room flags. */
static void redit_disp_flag_menu(struct descriptor_data *d)
{
  char bits[MAX_STRING_LENGTH];

  get_char_colors(d->character);
  clear_screen(d);
  column_list(d->character, 0, room_bits, NUM_ROOM_FLAGS, TRUE);

  sprintbitarray(OLC_ROOM(d)->room_flags, room_bits, RF_ARRAY_MAX, bits);
  write_to_output(d, "\r\n방 속성: %s%s%s\r\n"
	  "방 속성을 선택하세요, 0에서 q : ", cyn, bits, nrm);
  OLC_MODE(d) = REDIT_FLAGS;
}

/* For sector type. */
static void redit_disp_sector_menu(struct descriptor_data *d)
{
  clear_screen(d);
  column_list(d->character, 0, sector_types, NUM_ROOM_SECTORS, TRUE);
  write_to_output(d, "\r\n지역 선택 : ");
  OLC_MODE(d) = REDIT_SECTOR;
}

/* The main menu. */
static void redit_disp_menu(struct descriptor_data *d)
{
  char buf1[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  struct room_data *room;

  get_char_colors(d->character);
  clear_screen(d);
  room = OLC_ROOM(d);

  sprintbitarray(room->room_flags, room_bits, RF_ARRAY_MAX, buf1);
  sprinttype(room->sector_type, sector_types, buf2, sizeof(buf2));
  write_to_output(d,
      "-- 방 번호 : [%s%d%s] 존 번호 : [%s%d%s]\r\n"
      "%s1%s) 이름        : %s%s\r\n"
      "%s2%s) 묘사 :\r\n%s%s"
      "%s3%s) 방 속성 : %s%s\r\n"
      "%s4%s) 지역 유형 : %s%s\r\n",
      cyn, OLC_NUM(d), nrm,
      cyn, zone_table[OLC_ZNUM(d)].number, nrm,
      grn, nrm, yel, room->name,
      grn, nrm, yel, room->description,
      grn, nrm, cyn, buf1,
      grn, nrm, cyn, buf2);

  if (!CONFIG_DIAGONAL_DIRS)
  {
    write_to_output(d,
      "%s5%s) 출구 북 : %s%d\r\n"
      "%s6%s) 출구 동 : %s%d\r\n"
      "%s7%s) 출구 남 : %s%d\r\n"
      "%s8%s) 출구 서 : %s%d\r\n",
      grn, nrm, cyn,
      room->dir_option[NORTH] && room->dir_option[NORTH]->to_room != NOWHERE ?
      world[room->dir_option[NORTH]->to_room].number : -1,
      grn, nrm, cyn,
      room->dir_option[EAST] && room->dir_option[EAST]->to_room != NOWHERE ?
      world[room->dir_option[EAST]->to_room].number : -1,
      grn, nrm, cyn,
      room->dir_option[SOUTH] && room->dir_option[SOUTH]->to_room != NOWHERE ?
      world[room->dir_option[SOUTH]->to_room].number : -1,
      grn, nrm, cyn,
      room->dir_option[WEST] && room->dir_option[WEST]->to_room != NOWHERE ?
      world[room->dir_option[WEST]->to_room].number : -1);
  } else {
    write_to_output(d,
      "%s5%s) 출구 북 : %s%-6d%s,  %sB%s) 출구 북서 : %s%d\r\n"
      "%s6%s) 출구 동 : %s%-6d%s,  %sC%s) 출구 북동 : %s%d\r\n"
      "%s7%s) 출구 남 : %s%-6d%s,  %sD%s) 출구 남동 : %s%d\r\n"
      "%s8%s) 출구 서 : %s%-6d%s,  %sE%s) 출구 남서 : %s%d\r\n",
      grn, nrm, cyn,
      room->dir_option[NORTH] && room->dir_option[NORTH]->to_room != NOWHERE ?
      world[room->dir_option[NORTH]->to_room].number : -1, nrm,
      grn, nrm, cyn,
      room->dir_option[NORTHWEST] && room->dir_option[NORTHWEST]->to_room != NOWHERE ?
      world[room->dir_option[NORTHWEST]->to_room].number : -1,
      grn, nrm, cyn,
      room->dir_option[EAST] && room->dir_option[EAST]->to_room != NOWHERE ?
      world[room->dir_option[EAST]->to_room].number : -1, nrm,
      grn, nrm, cyn,
      room->dir_option[NORTHEAST] && room->dir_option[NORTHEAST]->to_room != NOWHERE ?
      world[room->dir_option[NORTHEAST]->to_room].number : -1,
      grn, nrm, cyn,
      room->dir_option[SOUTH] && room->dir_option[SOUTH]->to_room != NOWHERE ?
      world[room->dir_option[SOUTH]->to_room].number : -1, nrm,
      grn, nrm, cyn,
      room->dir_option[SOUTHEAST] && room->dir_option[SOUTHEAST]->to_room != NOWHERE ?
      world[room->dir_option[SOUTHEAST]->to_room].number : -1,
      grn, nrm, cyn,
      room->dir_option[WEST] && room->dir_option[WEST]->to_room != NOWHERE ?
      world[room->dir_option[WEST]->to_room].number : -1, nrm,
      grn, nrm, cyn,
      room->dir_option[SOUTHWEST] && room->dir_option[SOUTHWEST]->to_room != NOWHERE ?
      world[room->dir_option[SOUTHWEST]->to_room].number : -1
      );
  }
  write_to_output(d,
      "%s9%s) 출구 위 : %s%d\r\n"
      "%sA%s) 출구 밑 : %s%d\r\n"
      "%sF%s) 기타 묘사 메뉴\r\n"
      "%sS%s) 스크립트      : %s%s\r\n"
       "%sW%s) 방 복사\r\n"
      "%sX%s) 방 삭제\r\n"
      "%sQ%s) 종료\r\n"
      "선택하세요 : ",
      grn, nrm, cyn,
      room->dir_option[UP] && room->dir_option[UP]->to_room != NOWHERE ?
      world[room->dir_option[UP]->to_room].number : -1,
      grn, nrm, cyn,
      room->dir_option[DOWN] && room->dir_option[DOWN]->to_room != NOWHERE ?
      world[room->dir_option[DOWN]->to_room].number : -1,
      grn, nrm,
          grn, nrm, cyn, OLC_SCRIPT(d) ? "Set." : "Not Set.",
          grn, nrm,
      grn, nrm,
      grn, nrm
      );

  OLC_MODE(d) = REDIT_MAIN_MENU;
}

/* The main loop*/
void redit_parse(struct descriptor_data *d, char *arg)
{
  int number;
  char *oldtext = NULL;

  switch (OLC_MODE(d)) {
  case REDIT_CONFIRM_SAVESTRING:
    switch (*arg) {
    case 'y':
    case 'Y':
      redit_save_internally(d);
      mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(d->character)), TRUE, "OLC: %s edits room %d.", GET_NAME(d->character), OLC_NUM(d));
      if (CONFIG_OLC_SAVE) {
        redit_save_to_disk(real_zone_by_thing(OLC_NUM(d)));
        write_to_output(d, "방 정보를 디스크에 저장했습니다.\r\n");
      } else
        write_to_output(d, "방 정보를 메모리에 저장했습니다.\r\n");
      /* Free everything. */
      cleanup_olc(d, CLEANUP_ALL);
      break;
    case 'n':
    case 'N':
      /* If not saving, we must free the script_proto list. We do so by 
       * assigning it to the edited room and letting free_room in 
       * cleanup_olc handle it. */
      OLC_ROOM(d)->proto_script = OLC_SCRIPT(d);
      cleanup_olc(d, CLEANUP_ALL);
      break;
    default:
      write_to_output(d, "잘못 입력하셨습니다!\r\n변경 내용을 저장하시겠습니까? : (y / n)");
      break;
    }
    return;

  case REDIT_MAIN_MENU:
    switch (*arg) {
    case 'q':
    case 'Q':
      if (OLC_VAL(d)) { /* Something has been modified. */
        write_to_output(d, "변경 내용을 저장하시겠습니까? : (y / n)");
        OLC_MODE(d) = REDIT_CONFIRM_SAVESTRING;
      } else
        cleanup_olc(d, CLEANUP_ALL);
      return;
    case '1':
      write_to_output(d, "방 이름을 입력하세요 :-\r\n] ");
      OLC_MODE(d) = REDIT_NAME;
      break;
    case '2':
      OLC_MODE(d) = REDIT_DESC;
      clear_screen(d);
      send_editor_help(d);
      write_to_output(d, "방 묘사를 입력하세요 :\r\n\r\n");

      if (OLC_ROOM(d)->description) {
	write_to_output(d, "%s", OLC_ROOM(d)->description);
	oldtext = strdup(OLC_ROOM(d)->description);
      }
      string_write(d, &OLC_ROOM(d)->description, MAX_ROOM_DESC, 0, oldtext);
      OLC_VAL(d) = 1;
      break;
    case '3':
      redit_disp_flag_menu(d);
      break;
    case '4':
      redit_disp_sector_menu(d);
      break;
    case '5':
      OLC_VAL(d) = NORTH;
      redit_disp_exit_menu(d);
      break;
    case '6':
      OLC_VAL(d) = EAST;
      redit_disp_exit_menu(d);
      break;
    case '7':
      OLC_VAL(d) = SOUTH;
      redit_disp_exit_menu(d);
      break;
    case '8':
      OLC_VAL(d) = WEST;
      redit_disp_exit_menu(d);
      break;
    case '9':
      OLC_VAL(d) = UP;
      redit_disp_exit_menu(d);
      break;
    case 'a':
    case 'A':
      OLC_VAL(d) = DOWN;
      redit_disp_exit_menu(d);
      break;
    case 'b':
    case 'B':
      if (!CONFIG_DIAGONAL_DIRS) {
        write_to_output(d, "잘못 입력하셨습니다!");
        redit_disp_menu(d);
      } else {
        OLC_VAL(d) = NORTHWEST;
        redit_disp_exit_menu(d);
      }
      break;
    case 'c':
    case 'C':
      if (!CONFIG_DIAGONAL_DIRS) {
        write_to_output(d, "잘못 입력하셨습니다!");
        redit_disp_menu(d);
      } else {
        OLC_VAL(d) = NORTHEAST;
        redit_disp_exit_menu(d);
      }
      break;
    case 'd':
    case 'D':
      if (!CONFIG_DIAGONAL_DIRS) {
        write_to_output(d, "잘못 입력하셨습니다!");
        redit_disp_menu(d);
      } else {
        OLC_VAL(d) = SOUTHEAST;
        redit_disp_exit_menu(d);
      }
      break;
    case 'e':
    case 'E':
      if (!CONFIG_DIAGONAL_DIRS) {
        write_to_output(d, "잘못 입력하셨습니다!");
        redit_disp_menu(d);
      } else {
        OLC_VAL(d) = SOUTHWEST;
        redit_disp_exit_menu(d);
      }
      break;
    case 'f':
    case 'F':
      /* If the extra description doesn't exist. */
      if (!OLC_ROOM(d)->ex_description)
	CREATE(OLC_ROOM(d)->ex_description, struct extra_descr_data, 1);
      OLC_DESC(d) = OLC_ROOM(d)->ex_description;
      redit_disp_extradesc_menu(d);
      break;
    case 'w':
    case 'W':
      write_to_output(d, "어떤 방을 복사하시겠습니까?");
      OLC_MODE(d) = REDIT_COPY;
      break;
    case 'x':
    case 'X':
      /* Delete the room, prompt first. */
      write_to_output(d, "정말로 이 방을 삭제하시겠습니까? (y / n)");
      OLC_MODE(d) = REDIT_DELETE;
      break;

    case 's':
    case 'S':
      OLC_SCRIPT_EDIT_MODE(d) = SCRIPT_MAIN_MENU;
      dg_script_menu(d);
      return;
    default:
      write_to_output(d, "잘못 입력하셨습니다!");
      redit_disp_menu(d);
      break;
    }
    return;


  case OLC_SCRIPT_EDIT:
    if (dg_script_edit_parse(d, arg)) return;
    break;

  case REDIT_NAME:
    if (!genolc_checkstring(d, arg))
      break;
    if (count_non_protocol_chars(arg) > MAX_ROOM_NAME / 2) {
      write_to_output(d, "%d 글자 이상 입력할 수 없습니다.\r\n", MAX_ROOM_NAME / 2);
      break;
    }
    if (OLC_ROOM(d)->name)
      free(OLC_ROOM(d)->name);
    arg[MAX_ROOM_NAME - 1] = '\0';
    OLC_ROOM(d)->name = str_udup(arg);
    break;

  case REDIT_DESC:
    /* We will NEVER get here, we hope. */
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: Reached REDIT_DESC case in parse_redit().");
    write_to_output(d, "Oops, in REDIT_DESC.\r\n");
    break;

  case REDIT_FLAGS:
    number = atoi(arg);
    if (number < 0 || number > NUM_ROOM_FLAGS) {
      write_to_output(d, "올바른 입력이 아닙니다!\r\n");
      redit_disp_flag_menu(d);
    } else if (number == 0)
      break;
    else {
      /* Toggle the bit. */
      TOGGLE_BIT_AR(OLC_ROOM(d)->room_flags, number - 1);
      redit_disp_flag_menu(d);
    }
    return;

  case REDIT_SECTOR:
    number = atoi(arg) - 1;
    if (number < 0 || number >= NUM_ROOM_SECTORS) {
      write_to_output(d, "잘못 입력하셨습니다!");
      redit_disp_sector_menu(d);
      return;
    }
    OLC_ROOM(d)->sector_type = number;
    break;

  case REDIT_EXIT_MENU:
    switch (*arg) {
    case '0':
      break;
    case '1':
      OLC_MODE(d) = REDIT_EXIT_NUMBER;
      write_to_output(d, "출구 방 번호 : ");
      return;
    case '2':
      OLC_MODE(d) = REDIT_EXIT_DESCRIPTION;
      send_editor_help(d);
      write_to_output(d, "출구 묘사를 입력하세요 :\r\n\r\n");
      if (OLC_EXIT(d)->general_description) {
	write_to_output(d, "%s", OLC_EXIT(d)->general_description);
	oldtext = strdup(OLC_EXIT(d)->general_description);
      }
      string_write(d, &OLC_EXIT(d)->general_description, MAX_EXIT_DESC, 0, oldtext);
      return;
    case '3':
      OLC_MODE(d) = REDIT_EXIT_KEYWORD;
      write_to_output(d, "키워드를 입력하세요 : ");
      return;
    case '4':
      OLC_MODE(d) = REDIT_EXIT_KEY;
      write_to_output(d, "열쇠 번호를 입력하세요 :");
      return;
    case '5':
      OLC_MODE(d) = REDIT_EXIT_DOORFLAGS;
      redit_disp_exit_flag_menu(d);
      return;
    case '6':
      /*
       * Delete an exit.
       */
      if (OLC_EXIT(d)->keyword)
	free(OLC_EXIT(d)->keyword);
      if (OLC_EXIT(d)->general_description)
	free(OLC_EXIT(d)->general_description);
      if (OLC_EXIT(d))
	free(OLC_EXIT(d));
      OLC_EXIT(d) = NULL;
      break;
    default:
      write_to_output(d, "다시 입력하세요 :");
      return;
    }
    break;

  case REDIT_EXIT_NUMBER:
    if ((number = atoi(arg)) != -1)
      if ((number = real_room(number)) == NOWHERE) {
	write_to_output(d, "그 방은 존재하지 않습니다, 다시 입력하세요 : ");
	return;
      }
    OLC_EXIT(d)->to_room = number;
    redit_disp_exit_menu(d);
    return;

  case REDIT_EXIT_DESCRIPTION:
    /* We should NEVER get here, hopefully. */
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: Reached REDIT_EXIT_DESC case in parse_redit");
    write_to_output(d, "Oops, in REDIT_EXIT_DESCRIPTION.\r\n");
    break;

  case REDIT_EXIT_KEYWORD:
    if (OLC_EXIT(d)->keyword)
      free(OLC_EXIT(d)->keyword);
    OLC_EXIT(d)->keyword = str_udup(arg);
    redit_disp_exit_menu(d);
    return;

  case REDIT_EXIT_KEY:
    number = atoi(arg);
    if (number < 0)
      OLC_EXIT(d)->key = NOTHING;
    else
      OLC_EXIT(d)->key = number;
    redit_disp_exit_menu(d);
    return;

  case REDIT_EXIT_DOORFLAGS:
    number = atoi(arg);
    if (number < 0 || number > 4) {
      write_to_output(d, "잘못 입력하셨습니다!\r\n");
      redit_disp_exit_flag_menu(d);
    } else {
      /* Doors are a bit idiotic, don't you think? :) -- I agree. -gg */
      OLC_EXIT(d)->exit_info = (number == 0 ? 0 :
				(number == 1 ? EX_ISDOOR :
                (number == 2 ? EX_ISDOOR | EX_PICKPROOF :
                (number == 3 ? EX_ISDOOR | EX_HIDDEN :
                (number == 4 ? EX_ISDOOR | EX_PICKPROOF | EX_HIDDEN : 0)))));
      /* Jump back to the menu system. */
      redit_disp_exit_menu(d);
    }
    return;

  case REDIT_EXTRADESC_KEY:
    if (genolc_checkstring(d, arg)) {
      if (OLC_DESC(d)->keyword)
        free(OLC_DESC(d)->keyword);
      OLC_DESC(d)->keyword = str_udup(arg);
    }
    redit_disp_extradesc_menu(d);
    return;

  case REDIT_EXTRADESC_MENU:
    switch ((number = atoi(arg))) {
    case 0:
      /* If something got left out, delete the extra description when backing 
         out to the menu. */
      if (OLC_DESC(d)->keyword == NULL || OLC_DESC(d)->description == NULL) {
	struct extra_descr_data *temp;
	if (OLC_DESC(d)->keyword)
	  free(OLC_DESC(d)->keyword);
	if (OLC_DESC(d)->description)
	  free(OLC_DESC(d)->description);

	/* Clean up pointers. */
	REMOVE_FROM_LIST(OLC_DESC(d), OLC_ROOM(d)->ex_description, next);
	free(OLC_DESC(d));
      }
      break;
    case 1:
      OLC_MODE(d) = REDIT_EXTRADESC_KEY;
      write_to_output(d, "키워드를 입력하세요, 스페이스를 기준으로 구분됩니다 : ");
      return;
    case 2:
      OLC_MODE(d) = REDIT_EXTRADESC_DESCRIPTION;
      send_editor_help(d);
      write_to_output(d, "Enter extra description:\r\n\r\n");
      if (OLC_DESC(d)->description) {
	write_to_output(d, "%s", OLC_DESC(d)->description);
	oldtext = strdup(OLC_DESC(d)->description);
      }
      string_write(d, &OLC_DESC(d)->description, MAX_MESSAGE_LENGTH, 0, oldtext);
      return;
    case 3:
      if (OLC_DESC(d)->keyword == NULL || OLC_DESC(d)->description == NULL) {
	write_to_output(d, "이 페이지를 완성하기 전에는 다음 내용을 편집할 수 없습니다.\r\n");
	redit_disp_extradesc_menu(d);
      } else {
	struct extra_descr_data *new_extra;

	if (OLC_DESC(d)->next)
	  OLC_DESC(d) = OLC_DESC(d)->next;
	else {
	  /* Make new extra description and attach at end. */
	  CREATE(new_extra, struct extra_descr_data, 1);
	  OLC_DESC(d)->next = new_extra;
	  OLC_DESC(d) = new_extra;
	}
	redit_disp_extradesc_menu(d);
      }
      return;
    }
    break;

  case REDIT_COPY:
    if ((number = real_room(atoi(arg))) != NOWHERE) {
      redit_setup_existing(d, number);
    } else
      write_to_output(d, "그 방은 존재하지 않습니다.\r\n");
    break;
  
  case REDIT_DELETE:
    if (*arg == 'y' || *arg == 'Y') {
      if (delete_room(real_room(OLC_ROOM(d)->number)))
        write_to_output(d, "방 삭제 완료.\r\n");
     else
        write_to_output(d, "삭제할 수 없는 방입니다!.\r\n");

      cleanup_olc(d, CLEANUP_ALL);
      return;
    } else if (*arg == 'n' || *arg == 'N') {
      redit_disp_menu(d);
      OLC_MODE(d) = REDIT_MAIN_MENU;
      return;
    } else
      write_to_output(d, "'Y' 또는 'N'으로 답해주세요 : ");

    break;

  default:
    /* We should never get here. */
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: Reached default case in parse_redit");
    break;
  }
  /* If we get this far, something has been changed. */
  OLC_VAL(d) = 1;
  redit_disp_menu(d);
}

void redit_string_cleanup(struct descriptor_data *d, int terminator)
{
  switch (OLC_MODE(d)) {
  case REDIT_DESC:
    redit_disp_menu(d);
    break;
  case REDIT_EXIT_DESCRIPTION:
    redit_disp_exit_menu(d);
    break;
  case REDIT_EXTRADESC_DESCRIPTION:
    redit_disp_extradesc_menu(d);
    break;
  }
}
