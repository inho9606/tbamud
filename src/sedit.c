/**************************************************************************
*  File: sedit.c                                           Part of tbaMUD *
*  Usage: Oasis OLC - Shops.                                              *
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
#include "shop.h"
#include "genolc.h"
#include "genshp.h"
#include "genzon.h"
#include "oasis.h"
#include "constants.h"
#include "shop.h"

/* local functions */
static void sedit_setup_new(struct descriptor_data *d);
static void sedit_save_to_disk(int zone_num);
static void sedit_products_menu(struct descriptor_data *d);
static void sedit_compact_rooms_menu(struct descriptor_data *d);
static void sedit_rooms_menu(struct descriptor_data *d);
static void sedit_namelist_menu(struct descriptor_data *d);
static void sedit_shop_flags_menu(struct descriptor_data *d);
static void sedit_no_trade_menu(struct descriptor_data *d);
static void sedit_types_menu(struct descriptor_data *d);
static void sedit_disp_menu(struct descriptor_data *d);


void sedit_save_internally(struct descriptor_data *d)
{
  OLC_SHOP(d)->vnum = OLC_NUM(d);
  add_shop(OLC_SHOP(d));
}

static void sedit_save_to_disk(int num)
{
  save_shops(num);
}

/* utility functions */
ACMD(do_oasis_sedit)
{
  int number = NOWHERE, save = 0;
  shop_rnum real_num;
  struct descriptor_data *d;
  char buf1[MAX_INPUT_LENGTH];
  char buf2[MAX_INPUT_LENGTH];

  /* No building as a mob or while being forced. */
  if (IS_NPC(ch) || !ch->desc || STATE(ch->desc) != CON_PLAYING)
    return;
    
  /* Parse any arguments. */
  two_arguments(argument, buf1, buf2);

  if (!*buf1) {
    send_to_char(ch, "편집할 상점 번호를 입력해 주세요.\r\n");
    return;
  } else if (!isdigit(*buf1)) {
    if (str_cmp("저장", buf1) != 0) {
      send_to_char(ch, "앗!  누군가 다칠 일을 그만 두세요!\r\n");
      return;
    }

    save = TRUE;

    if (is_number(buf2))
      number = atoi(buf2);
    else if (GET_OLC_ZONE(ch) > 0) {
      zone_rnum zlok;

      if ((zlok = real_zone(GET_OLC_ZONE(ch))) == NOWHERE)
        number = NOWHERE;
      else
        number = genolc_zone_bottom(zlok);
    }

    if (number == NOWHERE) {
      send_to_char(ch, "몇 번 존 상점을 저장하시겠습니까?\r\n");
      return;
    }
  }

  /* If a numeric argument was given, get it. */
  if (number == NOWHERE)
    number = atoi(buf1);

  if (number < IDXTYPE_MIN || number > IDXTYPE_MAX) {
    send_to_char(ch, "그 상점 번호는 존재할 수 없습니다.\r\n");
    return;
  }

  /* Check that the shop isn't already being edited. */
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) == CON_SEDIT) {
      if (d->olc && OLC_NUM(d) == number) {
        send_to_char(ch, "그 상점은 현재 %s님이 편집중입니다.\r\n",
          PERS(d->character, ch));
        return;
      }
    }
  }

  /* Point d to the builder's descriptor. */
  d = ch->desc;

  /* Give the descriptor an OLC structure. */
  if (d->olc) {
    mudlog(BRF, LVL_IMMORT, TRUE,
      "SYSERR: do_oasis_sedit: Player already had olc structure.");
    free(d->olc);
  }

  CREATE(d->olc, struct oasis_olc_data, 1);

  /* Find the zone. */
  OLC_ZNUM(d) = save ? real_zone(number) : real_zone_by_thing(number);
  if (OLC_ZNUM(d) == NOWHERE) {
    send_to_char(ch, "그 번호에 해당하는 존이 없습니다!\r\n");
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

  if (save) {
    send_to_char(ch, "%d 존의 모든 상점 정보를 저장합니다.\r\n",
      zone_table[OLC_ZNUM(d)].number);
    mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE,
      "OLC: %s saves shop info for zone %d.",
      GET_NAME(ch), zone_table[OLC_ZNUM(d)].number);

    /* Save the shops to the shop file. */
    save_shops(OLC_ZNUM(d));

    /* Free the OLC structure. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  OLC_NUM(d) = number;

  if ((real_num = real_shop(number)) != NOTHING)
    sedit_setup_existing(d, real_num);
  else
    sedit_setup_new(d);

  sedit_disp_menu(d);
  STATE(d) = CON_SEDIT;

  act("$n님이 상점 편집을 시작합니다.", TRUE, d->character, 0, 0, TO_ROOM);
  SET_BIT_AR(PLR_FLAGS(ch), PLR_WRITING);

  mudlog(CMP, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "OLC: %s starts editing zone %d allowed zone %d",
    GET_NAME(ch), zone_table[OLC_ZNUM(d)].number, GET_OLC_ZONE(ch));
}

static void sedit_setup_new(struct descriptor_data *d)
{
  struct shop_data *shop;

  /* Allocate a scratch shop structure. */
  CREATE(shop, struct shop_data, 1);

  /* Fill in some default values. */
  S_KEEPER(shop) = NOBODY;
  S_CLOSE1(shop) = 28;
  S_BUYPROFIT(shop) = 1.0;
  S_SELLPROFIT(shop) = 1.0;
  S_NOTRADE(shop) = 0;
  /* Add a spice of default strings. */
  S_NOITEM1(shop) = strdup("%s 죄송합니다, 그런 물건은 팔지 않습니다.");
  S_NOITEM2(shop) = strdup("%s 그런 물건을 가지고 계신것 같지 않은데요.");
  S_NOCASH1(shop) = strdup("%s 제가 사기에는 너무 비싸네요!");
  S_NOCASH2(shop) = strdup("%s 손님 좀만 더 쓰시죠!");
  S_NOBUY(shop) = strdup("%s 그런 물건은 취급하지 않습니다.");
  S_BUY(shop) = strdup("%s %d 골드입니다, 감사합니다.");
  S_SELL(shop) = strdup("%s 그 물건 값으로 %d 골드를 드리겠습니다.");
  /* Stir the lists lightly. */
  CREATE(S_PRODUCTS(shop), obj_vnum, 1);

  S_PRODUCT(shop, 0) = NOTHING;
  CREATE(S_ROOMS(shop), room_rnum, 1);

  S_ROOM(shop, 0) = NOWHERE;
  CREATE(S_NAMELISTS(shop), struct shop_buy_data, 1);

  S_BUYTYPE(shop, 0) = NOTHING;

   /* Presto! A shop. */
  OLC_SHOP(d) = shop;
}

void sedit_setup_existing(struct descriptor_data *d, int rshop_num)
{
  /* Create a scratch shop structure. */
  CREATE(OLC_SHOP(d), struct shop_data, 1);

  /* don't waste time trying to free NULL strings -- Welcor */
  copy_shop(OLC_SHOP(d), shop_index + rshop_num, FALSE);
}

/* Menu functions */
static void sedit_products_menu(struct descriptor_data *d)
{
  struct shop_data *shop;
  int i;

  shop = OLC_SHOP(d);
  get_char_colors(d->character);

  clear_screen(d);
  write_to_output(d, "##     번호     상품\r\n");
  for (i = 0; S_PRODUCT(shop, i) != NOTHING; i++) {
    write_to_output(d, "%2d - [%s%5d%s] - %s%s%s\r\n", i,
	    cyn, obj_index[S_PRODUCT(shop, i)].vnum, nrm,
	    yel, obj_proto[S_PRODUCT(shop, i)].short_description, nrm);
  }
  write_to_output(d, "\r\n"
	  "%sA%s) 새로운 상품 추가\r\n"
	  "%sD%s) 상품 삭제.\r\n"
	  "%sQ%s) 종료\r\n"
	  "선택하세요 : ", grn, nrm, grn, nrm, grn, nrm);

  OLC_MODE(d) = SEDIT_PRODUCTS_MENU;
}

static void sedit_compact_rooms_menu(struct descriptor_data *d)
{
  struct shop_data *shop;
  int i;

  shop = OLC_SHOP(d);
  get_char_colors(d->character);

  clear_screen(d);
  for (i = 0; S_ROOM(shop, i) != NOWHERE; i++) {
    if (real_room(S_ROOM(shop, i)) != NOWHERE) {
      write_to_output(d, "%2d - [@\t%5d\tn] - \ty%s\tn\r\n", i, S_ROOM(shop, i), world[real_room(S_ROOM(shop, i))].name);
    } else {
      write_to_output(d, "%2d - [\tR!삭제된 방!\tn]\r\n", i);
    }
  }
  write_to_output(d, "\r\n"
	  "%sA%s) 새로운 방 추가.\r\n"
	  "%sD%s) 방 삭제.\r\n"
	  "%sL%s) 긴 화면.\r\n"
	  "%sQ%s) 종료\r\n"
	  "선택하세요 : ", grn, nrm, grn, nrm, grn, nrm, grn, nrm);

  OLC_MODE(d) = SEDIT_ROOMS_MENU;
}

static void sedit_rooms_menu(struct descriptor_data *d)
{
  struct shop_data *shop;
  int i;
  room_rnum rnum;

  shop = OLC_SHOP(d);
  get_char_colors(d->character);

  clear_screen(d);
  write_to_output(d, "##     번호     방\r\n\r\n");
  for (i = 0; S_ROOM(shop, i) != NOWHERE; i++) {
    rnum = real_room(S_ROOM(shop, i));
    /* if the room has been deleted, this may crash us otherwise. */
    /* set to 0 to be deletable.   -- Welcor 09/04                */
    if (rnum == NOWHERE)
      S_ROOM(shop, i) = rnum = 0;

    write_to_output(d, "%2d - [%s%5d%s] - %s%s%s\r\n", i, cyn, S_ROOM(shop, i), nrm,
	    yel, world[rnum].name, nrm);
  }
  write_to_output(d, "\r\n"
	  "%sA%s) 새로운 방 추가.\r\n"
	  "%sD%s) 방 삭제.\r\n"
	  "%sC%s) 콤팩트 화면.\r\n"
	  "%sQ%s) 종료\r\n"
	  "선택하세요 : ", grn, nrm, grn, nrm, grn, nrm, grn, nrm);

  OLC_MODE(d) = SEDIT_ROOMS_MENU;
}

static void sedit_namelist_menu(struct descriptor_data *d)
{
  struct shop_data *shop;
  int i;

  shop = OLC_SHOP(d);
  get_char_colors(d->character);

  clear_screen(d);
  write_to_output(d, "##              종류   이름목록\r\n\r\n");
  for (i = 0; S_BUYTYPE(shop, i) != NOTHING; i++) {
    write_to_output(d, "%2d - %s%15s%s - %s%s%s\r\n", i, cyn,
		item_types[S_BUYTYPE(shop, i)], nrm, yel,
		S_BUYWORD(shop, i) ? S_BUYWORD(shop, i) : "<None>", nrm);
  }
  write_to_output(d, "\r\n"
	  "%sA%s) 새로운 입력(entry) 추가.\r\n"
	  "%sD%s) 입력 삭제.\r\n"
	  "%sQ%s) 종료\r\n"
	  "선택하세요 : ", grn, nrm, grn, nrm, grn, nrm);

  OLC_MODE(d) = SEDIT_NAMELIST_MENU;
}

static void sedit_shop_flags_menu(struct descriptor_data *d)
{
  char bits[MAX_STRING_LENGTH];
  int i, count = 0;

  get_char_colors(d->character);
  clear_screen(d);
  for (i = 0; i < NUM_SHOP_FLAGS; i++) {
    write_to_output(d, "%s%2d%s) %-20.20s   %s", grn, i + 1, nrm, shop_bits[i],
		!(++count % 2) ? "\r\n" : "");
  }
  sprintbit(S_BITVECTOR(OLC_SHOP(d)), shop_bits, bits, sizeof(bits));
  write_to_output(d, "\r\n현재 상점 속성 : %s%s%s\r\n선택하세요 : ",
		cyn, bits, nrm);
  OLC_MODE(d) = SEDIT_SHOP_FLAGS;
}

static void sedit_no_trade_menu(struct descriptor_data *d)
{
  char bits[MAX_STRING_LENGTH];
  int i, count = 0;

  get_char_colors(d->character);
  clear_screen(d);
  for (i = 0; i < NUM_TRADERS; i++) {
    write_to_output(d, "%s%2d%s) %-20.20s   %s", grn, i + 1, nrm, trade_letters[i],
		!(++count % 2) ? "\r\n" : "");
  }
  sprintbit(S_NOTRADE(OLC_SHOP(d)), trade_letters, bits, sizeof(bits));
  write_to_output(d, "\r\n현재 거래 불가: %s%s%s\r\n"
	  "선택하세요 : ", cyn, bits, nrm);
  OLC_MODE(d) = SEDIT_NOTRADE;
}

static void sedit_types_menu(struct descriptor_data *d)
{
  int i, count = 0;

  get_char_colors(d->character);

  clear_screen(d);
  for (i = 0; i < NUM_ITEM_TYPES; i++) {
    write_to_output(d, "%s%2d%s) %s%-20s%s  %s", grn, i, nrm, cyn, item_types[i],
		nrm, !(++count % 3) ? "\r\n" : "");
  }
  write_to_output(d, "%s선택하세요 : ", nrm);
  OLC_MODE(d) = SEDIT_TYPE_MENU;
}

/* Display main menu. */
static void sedit_disp_menu(struct descriptor_data *d)
{
  char buf1[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  struct shop_data *shop;

  shop = OLC_SHOP(d);
  get_char_colors(d->character);

  clear_screen(d);
  sprintbit(S_NOTRADE(shop), trade_letters, buf1, sizeof(buf1));
  sprintbit(S_BITVECTOR(shop), shop_bits, buf2, sizeof(buf2));
  write_to_output(d,
	  "-- 상점 번호 : [%s%d%s]\r\n"
	  "%s0%s) 상점 주인      : [%s%d%s] %s%s\r\n"
          "%s1%s) 개장시간 1 : %s%4d%s          %s2%s) 폐장시간 1 : %s%4d\r\n"
          "%s3%s) 개장시간 2 : %s%4d%s          %s4%s) 폐장시간 2 : %s%4d\r\n"
	  "%s5%s) 판매율   : %s%1.2f%s          %s6%s) 구매율    : %s%1.2f\r\n"
	  "%s7%s) 상점 주인이 아이템이 없을 때 : %s%s\r\n"
	  "%s8%s) 사용자가 아이템이 없을 때 : %s%s\r\n"
	  "%s9%s) 상점 주인이 돈이 없을 때 : %s%s\r\n"
	  "%sA%s) 사용자가 돈이 없을 때 : %s%s\r\n"
	  "%sB%s) 상점에서 물건을 안 살 때 : %s%s\r\n"
	  "%sC%s) 상점에서 물건을 살 때 : %s%s\r\n"
	  "%sD%s) 상점에서 물건을 팔 때 : %s%s\r\n"
	  "%sE%s) 거래 불가 : %s%s\r\n"
	  "%sF%s) 상점 속성     : %s%s\r\n"
	  "%sR%s) 방 메뉴\r\n"
	  "%sP%s) 상품 메뉴\r\n"
	  "%sT%s) 거래 가능 물건 종류\r\n"
          "%sW%s) 상점 복사\r\n"
	  "%sQ%s) 종료\r\n"
	  "선택하세요 : ",

	  cyn, OLC_NUM(d), nrm,
	  grn, nrm, cyn, S_KEEPER(shop) == NOBODY ? -1 : mob_index[S_KEEPER(shop)].vnum,
	  nrm, yel, S_KEEPER(shop) == NOBODY ? "None" : mob_proto[S_KEEPER(shop)].player.short_descr,
	  grn, nrm, cyn, S_OPEN1(shop), nrm,
	  grn, nrm, cyn, S_CLOSE1(shop),
	  grn, nrm, cyn, S_OPEN2(shop), nrm,
	  grn, nrm, cyn, S_CLOSE2(shop),
	  grn, nrm, cyn, S_BUYPROFIT(shop), nrm,
	  grn, nrm, cyn, S_SELLPROFIT(shop),
	  grn, nrm, yel, S_NOITEM1(shop),
	  grn, nrm, yel, S_NOITEM2(shop),
	  grn, nrm, yel, S_NOCASH1(shop),
	  grn, nrm, yel, S_NOCASH2(shop),
	  grn, nrm, yel, S_NOBUY(shop),
	  grn, nrm, yel, S_BUY(shop),
	  grn, nrm, yel, S_SELL(shop),
	  grn, nrm, cyn, buf1,
	  grn, nrm, cyn, buf2,
	  grn, nrm, grn, nrm, grn, nrm, grn, nrm, grn, nrm
  );

  OLC_MODE(d) = SEDIT_MAIN_MENU;
}

/* The GARGANTUAN event handler */
void sedit_parse(struct descriptor_data *d, char *arg)
{
  int i;

  if (OLC_MODE(d) > SEDIT_NUMERICAL_RESPONSE) {
    if (!isdigit(arg[0]) && ((*arg == '-') && (!isdigit(arg[1])))) {
      write_to_output(d, "필드는 숫자여야 합니다, 다시 시도 : ");
      return;
    }
  }
  switch (OLC_MODE(d)) {
  case SEDIT_CONFIRM_SAVESTRING:
    switch (*arg) {
    case 'y':
    case 'Y':
      sedit_save_internally(d);
      mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(d->character)), TRUE,
             "OLC: %s edits shop %d", GET_NAME(d->character),
             OLC_NUM(d));
      if (CONFIG_OLC_SAVE) {
        sedit_save_to_disk(real_zone_by_thing(OLC_NUM(d)));
        write_to_output(d, "상점 정보가 디스크에 저장되었습니다.\r\n");
      } else
        write_to_output(d, "상점 정보가 메모리에 저장되었습니다.\r\n");

      cleanup_olc(d, CLEANUP_STRUCTS);
      return;
    case 'n':
    case 'N':
      cleanup_olc(d, CLEANUP_ALL);
      return;
    default:
     write_to_output(d, "잘못 입력하셨습니다!\r\nDo you wish to save your changes? : ");
      return;
    }

  case SEDIT_MAIN_MENU:
    i = 0;
    switch (*arg) {
    case 'w':
    case 'W':
      write_to_output(d, "어떤 상점을 복사하시겠습니까?");
      OLC_MODE(d) = SEDIT_COPY;
      return;
    case 'q':
    case 'Q':
      if (OLC_VAL(d)) {		/* Anything been changed? */
	write_to_output(d, "변경 내용을 저장하시겠습니까?");
	OLC_MODE(d) = SEDIT_CONFIRM_SAVESTRING;
      } else
	cleanup_olc(d, CLEANUP_ALL);
      return;
    case '0':
      OLC_MODE(d) = SEDIT_KEEPER;
      write_to_output(d, "상점을 관리할 맙 번호를 입력해 주세요 : ");
      return;
    case '1':
      OLC_MODE(d) = SEDIT_OPEN1;
      i++;
      break;
    case '2':
      OLC_MODE(d) = SEDIT_CLOSE1;
      i++;
      break;
    case '3':
      OLC_MODE(d) = SEDIT_OPEN2;
      i++;
      break;
    case '4':
      OLC_MODE(d) = SEDIT_CLOSE2;
      i++;
      break;
    case '5':
      OLC_MODE(d) = SEDIT_BUY_PROFIT;
      i++;
      break;
    case '6':
      OLC_MODE(d) = SEDIT_SELL_PROFIT;
      i++;
      break;
    case '7':
      OLC_MODE(d) = SEDIT_NOITEM1;
      i--;
      break;
    case '8':
      OLC_MODE(d) = SEDIT_NOITEM2;
      i--;
      break;
    case '9':
      OLC_MODE(d) = SEDIT_NOCASH1;
      i--;
      break;
    case 'a':
    case 'A':
      OLC_MODE(d) = SEDIT_NOCASH2;
      i--;
      break;
    case 'b':
    case 'B':
      OLC_MODE(d) = SEDIT_NOBUY;
      i--;
      break;
    case 'c':
    case 'C':
      OLC_MODE(d) = SEDIT_BUY;
      i--;
      break;
    case 'd':
    case 'D':
      OLC_MODE(d) = SEDIT_SELL;
      i--;
      break;
    case 'e':
    case 'E':
      sedit_no_trade_menu(d);
      return;
    case 'f':
    case 'F':
      sedit_shop_flags_menu(d);
      return;
    case 'r':
    case 'R':
      sedit_rooms_menu(d);
      return;
    case 'p':
    case 'P':
      sedit_products_menu(d);
      return;
    case 't':
    case 'T':
      sedit_namelist_menu(d);
      return;
    default:
      sedit_disp_menu(d);
      return;
    }

    if (i == 0)
      break;
    else if (i == 1)
      write_to_output(d, "\r\n새로운 값 입력 : ");
    else if (i == -1)
      write_to_output(d, "\r\n내용 입력 :\r\n] ");
    else
      write_to_output(d, "이런... 개발자에게 문의하세요\r\n");
    return;

  case SEDIT_NAMELIST_MENU:
    switch (*arg) {
    case 'a':
    case 'A':
      sedit_types_menu(d);
      return;
    case 'd':
    case 'D':
      write_to_output(d, "\r\n몇 번 입력을 삭제하시겠습니까?");
      OLC_MODE(d) = SEDIT_DELETE_TYPE;
      return;
    case 'q':
    case 'Q':
      break;
    }
    break;

  case SEDIT_PRODUCTS_MENU:
    switch (*arg) {
    case 'a':
    case 'A':
      write_to_output(d, "\r\n상품으로 등록할 물건 번호를 입력하세요 : ");
      OLC_MODE(d) = SEDIT_NEW_PRODUCT;
      return;
    case 'd':
    case 'D':
      write_to_output(d, "\r\n몇 번 상품을 삭제하시겠습니까?");
      OLC_MODE(d) = SEDIT_DELETE_PRODUCT;
      return;
    case 'q':
    case 'Q':
      break;
    }
    break;

  case SEDIT_ROOMS_MENU:
    switch (*arg) {
    case 'a':
    case 'A':
      write_to_output(d, "\r\n상점으로 지정할 방 번호를 입력하세요 : ");
      OLC_MODE(d) = SEDIT_NEW_ROOM;
      return;
    case 'c':
    case 'C':
      sedit_compact_rooms_menu(d);
      return;
    case 'l':
    case 'L':
      sedit_rooms_menu(d);
      return;
    case 'd':
    case 'D':
      write_to_output(d, "\r\n몇 번 방을 삭제하시겠습니까?");
      OLC_MODE(d) = SEDIT_DELETE_ROOM;
      return;
    case 'q':
    case 'Q':
      break;
    }
    break;

    /* String edits. */
  case SEDIT_NOITEM1:
    if (genolc_checkstring(d, arg))
      modify_shop_string(&S_NOITEM1(OLC_SHOP(d)), arg);
    break;
  case SEDIT_NOITEM2:
    if (genolc_checkstring(d, arg))
      modify_shop_string(&S_NOITEM2(OLC_SHOP(d)), arg);
    break;
  case SEDIT_NOCASH1:
    if (genolc_checkstring(d, arg))
      modify_shop_string(&S_NOCASH1(OLC_SHOP(d)), arg);
    break;
  case SEDIT_NOCASH2:
    if (genolc_checkstring(d, arg))
      modify_shop_string(&S_NOCASH2(OLC_SHOP(d)), arg);
    break;
  case SEDIT_NOBUY:
    if (genolc_checkstring(d, arg))
      modify_shop_string(&S_NOBUY(OLC_SHOP(d)), arg);
    break;
  case SEDIT_BUY:
    if (genolc_checkstring(d, arg))
      modify_shop_string(&S_BUY(OLC_SHOP(d)), arg);
    break;
  case SEDIT_SELL:
    if (genolc_checkstring(d, arg))
      modify_shop_string(&S_SELL(OLC_SHOP(d)), arg);
    break;
  case SEDIT_NAMELIST:
    if (genolc_checkstring(d, arg)) {
      struct shop_buy_data new_entry;

      BUY_TYPE(new_entry) = OLC_VAL(d);
      BUY_WORD(new_entry) = strdup(arg);
      add_shop_to_type_list(&(S_NAMELISTS(OLC_SHOP(d))), &new_entry);
    }
    sedit_namelist_menu(d);
    return;

    /* Numerical responses. */
  case SEDIT_KEEPER:
    if ((i = atoi(arg)) != -1)
      if ((i = real_mobile(i)) == NOBODY) {
	write_to_output(d, "That mobile does not exist, try again : ");
	return;
      }
    S_KEEPER(OLC_SHOP(d)) = i;
    if (i == -1)
      break;
    /* Fiddle with special procs. */
    S_FUNC(OLC_SHOP(d)) = mob_index[i].func != shop_keeper ? mob_index[i].func : NULL;
    mob_index[i].func = shop_keeper;
    break;
  case SEDIT_OPEN1:
    S_OPEN1(OLC_SHOP(d)) = LIMIT(atoi(arg), 0, 28);
    break;
  case SEDIT_OPEN2:
    S_OPEN2(OLC_SHOP(d)) = LIMIT(atoi(arg), 0, 28);
    break;
  case SEDIT_CLOSE1:
    S_CLOSE1(OLC_SHOP(d)) = LIMIT(atoi(arg), 0, 28);
    break;
  case SEDIT_CLOSE2:
    S_CLOSE2(OLC_SHOP(d)) = LIMIT(atoi(arg), 0, 28);
    break;
  case SEDIT_BUY_PROFIT:
    sscanf(arg, "%f", &S_BUYPROFIT(OLC_SHOP(d)));
    break;
  case SEDIT_SELL_PROFIT:
    sscanf(arg, "%f", &S_SELLPROFIT(OLC_SHOP(d)));
    break;
  case SEDIT_TYPE_MENU:
    OLC_VAL(d) = LIMIT(atoi(arg), 0, NUM_ITEM_TYPES - 1);
    write_to_output(d, "이름 목록 입력 (엔터는 없음) :-\r\n] ");
    OLC_MODE(d) = SEDIT_NAMELIST;
    return;
  case SEDIT_DELETE_TYPE:
    remove_shop_from_type_list(&(S_NAMELISTS(OLC_SHOP(d))), atoi(arg));
    sedit_namelist_menu(d);
    return;
  case SEDIT_NEW_PRODUCT:
    if ((i = atoi(arg)) != -1)
      if ((i = real_object(i)) == NOTHING) {
	write_to_output(d, "그 물건은 존재하지 않습니다, 다시 시도 : ");
	return;
      }
    if (i > 0)
      add_shop_to_int_list(&(S_PRODUCTS(OLC_SHOP(d))), i);
    sedit_products_menu(d);
    return;
  case SEDIT_DELETE_PRODUCT:
    remove_shop_from_int_list(&(S_PRODUCTS(OLC_SHOP(d))), atoi(arg));
    sedit_products_menu(d);
    return;
  case SEDIT_NEW_ROOM:
    if ((i = atoi(arg)) != -1)
      if ((i = real_room(i)) == NOWHERE) {
	write_to_output(d, "그 방은 존재하지 않습니다, 다시 시도 : ");
	return;
      }
    if (i >= 0)
      add_shop_to_int_list(&(S_ROOMS(OLC_SHOP(d))), atoi(arg));
    sedit_rooms_menu(d);
    return;
  case SEDIT_DELETE_ROOM:
    remove_shop_from_int_list(&(S_ROOMS(OLC_SHOP(d))), atoi(arg));
    sedit_rooms_menu(d);
    return;
  case SEDIT_SHOP_FLAGS:
    if ((i = LIMIT(atoi(arg), 0, NUM_SHOP_FLAGS)) > 0) {
      TOGGLE_BIT(S_BITVECTOR(OLC_SHOP(d)), 1 << (i - 1));
      sedit_shop_flags_menu(d);
      return;
    }
    break;
  case SEDIT_NOTRADE:
    if ((i = LIMIT(atoi(arg), 0, NUM_TRADERS)) > 0) {
      TOGGLE_BIT(S_NOTRADE(OLC_SHOP(d)), 1 << (i - 1));
      sedit_no_trade_menu(d);
      return;
    }
    break;
  case SEDIT_COPY:
    if ((i = real_shop(atoi(arg))) != NOWHERE) {
      sedit_setup_existing(d, i);
    } else
      write_to_output(d, "그 상점은 존재하지 않습니다.\r\n");
    break;

  default:
    /* We should never get here. */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: sedit_parse(): Reached default case!");
    write_to_output(d, "이런... 개발자에게 문의하세요\r\n");
    break;
  }

/* If we get here, we have probably changed something, and now want to return 
   to main menu.  Use OLC_VAL as a 'has changed' flag. */
  OLC_VAL(d) = 1;
  sedit_disp_menu(d);
}
