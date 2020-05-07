/* ***********************************************************************
*    File:   quest.c                                  Part of CircleMUD  *
* Version:   2.1 (December 2005) Written for CircleMud CWG / Suntzu      *
* Purpose:   To provide special quest-related code.                      *
* Copyright: Kenneth Ray                                                 *
* Original Version Details:                                              *
* Morgaelin - quest.c                                                    *
* Copyright (C) 1997 MS                                                  *
*********************************************************************** */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "comm.h"
#include "screen.h"
#include "quest.h"
#include "act.h" /* for do_tell */


/*--------------------------------------------------------------------------
 * Exported global variables
 *--------------------------------------------------------------------------*/
const char *quest_types[] = {
  "물건 찾기",
  "방 찾기",
  "맙 찾기",
  "맙 죽이기",
  "맙 구출하기",
  "물건 전달하기",
  "방 안 모든 맙 죽이기",
  "\n"
};
const char *aq_flags[] = {
  "반복 가능",
  "\n"
};


/*--------------------------------------------------------------------------
 * Local (file scope) global variables
 *--------------------------------------------------------------------------*/
static int cmd_tell;

static const char *quest_cmd[] = {
  "목록", "기록", "수행", "취소", "진행중", "상태", "\n"};

static const char *quest_mort_usage =
  "사용법: 목록 | 기록 | 진행중 | 수행 <nn> | 취소 임무";

static const char *quest_imm_usage =
  "사용법: 목록 | 기록 | 진행중 | 수행 <nn> | 취소 | 상태 <vnum> 임무";

/*--------------------------------------------------------------------------*/
/* Utility Functions                                                        */
/*--------------------------------------------------------------------------*/

qst_rnum real_quest(qst_vnum vnum)
{
  int rnum;

  for (rnum = 0; rnum < total_quests; rnum++)
    if (QST_NUM(rnum) == vnum)
      return(rnum);
  return(NOTHING);
}

int is_complete(struct char_data *ch, qst_vnum vnum)
{
  int i;

  for (i = 0; i < GET_NUM_QUESTS(ch); i++)
    if (ch->player_specials->saved.completed_quests[i] == vnum)
      return TRUE;
  return FALSE;
}

qst_vnum find_quest_by_qmnum(struct char_data *ch, mob_vnum qm, int num)
{
  qst_rnum rnum;
  int found=0;
  for (rnum = 0; rnum < total_quests; rnum++) {
    if (qm == QST_MASTER(rnum))
      if (++found == num)
        return (QST_NUM(rnum));
  }
  return NOTHING;
}

/*--------------------------------------------------------------------------*/
/* Quest Loading and Unloading Functions                                    */
/*--------------------------------------------------------------------------*/

void destroy_quests(void)
{
  qst_rnum rnum = 0;

  if (!aquest_table)
    return;

  for (rnum = 0; rnum < total_quests; rnum++){
    free_quest_strings(&aquest_table[rnum]);
  }
  free(aquest_table);
  aquest_table = NULL;
  total_quests = 0;

  return;
}

int count_quests(qst_vnum low, qst_vnum high)
{
  int i, j;

  if (!aquest_table)
    return 0;

  for (i = j = 0; i < total_quests; i++)
    if (QST_NUM(i) >= low && QST_NUM(i) <= high)
      j++;

  return j;
}

void parse_quest(FILE *quest_f, int nr)
{
  static char line[256];
  static int i = 0, j;
  int retval = 0, t[7];
  char f1[128], buf2[MAX_STRING_LENGTH];
  aquest_table[i].vnum = nr;
  aquest_table[i].qm = NOBODY;
  aquest_table[i].name = NULL;
  aquest_table[i].desc = NULL;
  aquest_table[i].info = NULL;
  aquest_table[i].done = NULL;
  aquest_table[i].quit = NULL;
  aquest_table[i].flags = 0;
  aquest_table[i].type = -1;
  aquest_table[i].target = -1;
  aquest_table[i].prereq = NOTHING;
  for (j = 0; j < 7; j++)
    aquest_table[i].value[j] = 0;
  aquest_table[i].prev_quest = NOTHING;
  aquest_table[i].next_quest = NOTHING;
  aquest_table[i].func = NULL;

  aquest_table[i].gold_reward = 0;
  aquest_table[i].exp_reward  = 0;
  aquest_table[i].obj_reward  = NOTHING;

  /* begin to parse the data */
  aquest_table[i].name = fread_string(quest_f, buf2);
  aquest_table[i].desc = fread_string(quest_f, buf2);
  aquest_table[i].info = fread_string(quest_f, buf2);
  aquest_table[i].done = fread_string(quest_f, buf2);
  aquest_table[i].quit = fread_string(quest_f, buf2);
  if (!get_line(quest_f, line) ||
      (retval = sscanf(line, " %d %d %s %d %d %d %d",
             t, t+1, f1, t+2, t+3, t + 4, t + 5)) != 7) {
    log("Format error in numeric line (expected 7, got %d), %s\n",
        retval, line);
    exit(1);
  }
  aquest_table[i].type       = t[0];
  aquest_table[i].qm         = (real_mobile(t[1]) == NOBODY) ? NOBODY : t[1];
  aquest_table[i].flags      = asciiflag_conv(f1);
  aquest_table[i].target     = (t[2] == -1) ? NOTHING : t[2];
  aquest_table[i].prev_quest = (t[3] == -1) ? NOTHING : t[3];
  aquest_table[i].next_quest = (t[4] == -1) ? NOTHING : t[4];
  aquest_table[i].prereq     = (t[5] == -1) ? NOTHING : t[5];
  if (!get_line(quest_f, line) ||
      (retval = sscanf(line, " %d %d %d %d %d %d %d",
          t, t+1, t+2, t+3, t+4, t + 5, t + 6)) != 7) {
    log("Format error in numeric line (expected 7, got %d), %s\n",
        retval, line);
    exit(1);
  }
  for (j = 0; j < 7; j++)
    aquest_table[i].value[j] = t[j];

  if (!get_line(quest_f, line) ||
      (retval = sscanf(line, " %d %d %d",
             t, t+1, t+2)) != 3) {
    log("Format error in numeric (rewards) line (expected 3, got %d), %s\n",
        retval, line);
    exit(1);
  }

  aquest_table[i].gold_reward = t[0];
  aquest_table[i].exp_reward  = t[1];
  aquest_table[i].obj_reward  = (t[2] == -1) ? NOTHING : t[2];

  for (;;) {
    if (!get_line(quest_f, line)) {
      log("Format error in %s\n", line);
      exit(1);
    }
    switch(*line) {
    case 'S':
      total_quests = ++i;
      return;
    }
  }
} /* parse_quest */

void assign_the_quests(void)
{
  qst_rnum rnum;
  mob_rnum mrnum;

  cmd_tell = find_command("tell");

  for (rnum = 0; rnum < total_quests; rnum ++) {
    if (QST_MASTER(rnum) == NOBODY) {
      log("SYSERR: Quest #%d has no questmaster specified.", QST_NUM(rnum));
      continue;
    }
    if ((mrnum = real_mobile(QST_MASTER(rnum))) == NOBODY) {
      log("SYSERR: Quest #%d has an invalid questmaster.", QST_NUM(rnum));
      continue;
    }
    if (mob_index[(mrnum)].func &&
 mob_index[(mrnum)].func != questmaster)
      QST_FUNC(rnum) = mob_index[(mrnum)].func;
    mob_index[(mrnum)].func = questmaster;
  }
}

/*--------------------------------------------------------------------------*/
/* Quest Completion Functions                                               */
/*--------------------------------------------------------------------------*/
void set_quest(struct char_data *ch, qst_rnum rnum)
{
  GET_QUEST(ch) = QST_NUM(rnum);
  GET_QUEST_TIME(ch) = QST_TIME(rnum);
  GET_QUEST_COUNTER(ch) = QST_QUANTITY(rnum);
  SET_BIT_AR(PRF_FLAGS(ch), PRF_QUEST);
  return;
}

void clear_quest(struct char_data *ch)
{
  GET_QUEST(ch) = NOTHING;
  GET_QUEST_TIME(ch) = -1;
  GET_QUEST_COUNTER(ch) = 0;
  REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_QUEST);
  return;
}

void add_completed_quest(struct char_data *ch, qst_vnum vnum)
{
  qst_vnum *temp;
  int i;

  CREATE(temp, qst_vnum, GET_NUM_QUESTS(ch) +1);
  for (i=0; i < GET_NUM_QUESTS(ch); i++)
    temp[i] = ch->player_specials->saved.completed_quests[i];

  temp[GET_NUM_QUESTS(ch)] = vnum;
  GET_NUM_QUESTS(ch)++;

  if (ch->player_specials->saved.completed_quests)
    free(ch->player_specials->saved.completed_quests);
  ch->player_specials->saved.completed_quests = temp;
}

void remove_completed_quest(struct char_data *ch, qst_vnum vnum)
{
  qst_vnum *temp;
  int i, j = 0;

  CREATE(temp, qst_vnum, GET_NUM_QUESTS(ch));
  for (i = 0; i < GET_NUM_QUESTS(ch); i++)
    if (ch->player_specials->saved.completed_quests[i] != vnum)
      temp[j++] = ch->player_specials->saved.completed_quests[i];

  GET_NUM_QUESTS(ch)--;

  if (ch->player_specials->saved.completed_quests)
    free(ch->player_specials->saved.completed_quests);
  ch->player_specials->saved.completed_quests = temp;
}

void generic_complete_quest(struct char_data *ch)
{
  qst_rnum rnum;
  qst_vnum vnum = GET_QUEST(ch);
  struct obj_data *new_obj;
  int happy_qp, happy_gold, happy_exp;

  if (--GET_QUEST_COUNTER(ch) <= 0) {
    rnum = real_quest(vnum);
    if (IS_HAPPYHOUR && IS_HAPPYQP) {
      happy_qp = (int)(QST_POINTS(rnum) * (((float)(100+HAPPY_QP))/(float)100));
      happy_qp = MAX(happy_qp, 0);
      GET_QUESTPOINTS(ch) += happy_qp;
      send_to_char(ch,
          "%s\r\n퀘스트 수행 대가로 %d 만큼의 임무점수를 얻었습니다.\r\n",
          QST_DONE(rnum), happy_qp);
	} else {
      GET_QUESTPOINTS(ch) += QST_POINTS(rnum);
      send_to_char(ch,
          "%s\r\n퀘스트 수행 대가로 %d 만큼의 임무점수를 얻었습니다.\r\n",
          QST_DONE(rnum), QST_POINTS(rnum));
    }
    if (QST_GOLD(rnum)) {
      if ((IS_HAPPYHOUR) && (IS_HAPPYGOLD)) {
        happy_gold = (int)(QST_GOLD(rnum) * (((float)(100+HAPPY_GOLD))/(float)100));
        happy_gold = MAX(happy_gold, 0);
        increase_gold(ch, happy_gold);
        send_to_char(ch,
              "퀘스트 수행 대가로 %d 골드를 받았습니다.\r\n",
              happy_gold);
	  } else {
        increase_gold(ch, QST_GOLD(rnum));
        send_to_char(ch,
              "퀘스트 수행 대가로 %d 골드를 받았습니다.\r\n",
              QST_GOLD(rnum));
      }
    }
    if (QST_EXP(rnum)) {
      gain_exp(ch, QST_EXP(rnum));
      if ((IS_HAPPYHOUR) && (IS_HAPPYEXP)) {
        happy_exp = (int)(QST_EXP(rnum) * (((float)(100+HAPPY_EXP))/(float)100));
        happy_exp = MAX(happy_exp, 0);
        send_to_char(ch,
              "퀘스트 수행 대가로 %d 만큼의 경험치를 획득합니다.\r\n",
              happy_exp);
      } else {
        send_to_char(ch,
              "퀘스트 수행 대가로 %d 만큼의 경험치를 획득합니다.\r\n",
              QST_EXP(rnum));
      }
    }
    if (QST_OBJ(rnum) && QST_OBJ(rnum) != NOTHING) {
      if (real_object(QST_OBJ(rnum)) != NOTHING) {
        if ((new_obj = read_object((QST_OBJ(rnum)),VIRTUAL)) != NULL) {
            obj_to_char(new_obj, ch);
            send_to_char(ch, "You have been presented with %s%s for your service.\r\n",
                GET_OBJ_SHORT(new_obj), CCNRM(ch, C_NRM));
        }
      }
    }
    if (!IS_SET(QST_FLAGS(rnum), AQ_REPEATABLE))
      add_completed_quest(ch, vnum);
    clear_quest(ch);
    if ((real_quest(QST_NEXT(rnum)) != NOTHING) &&
        (QST_NEXT(rnum) != vnum) &&
        !is_complete(ch, QST_NEXT(rnum))) {
      rnum = real_quest(QST_NEXT(rnum));
      set_quest(ch, rnum);
      send_to_char(ch,
          "다음 퀘스트로 진행합니다:\r\n%s",
          QST_INFO(rnum));
    }
  }
  save_char(ch);
}

void autoquest_trigger_check(struct char_data *ch, struct char_data *vict,
                struct obj_data *object, int type)
{
  struct char_data *i;
  qst_rnum rnum;
  int found = TRUE;

  if (IS_NPC(ch))
    return;
  if (GET_QUEST(ch) == NOTHING)  /* No current quest, skip this */
    return;
  if (GET_QUEST_TYPE(ch) != type)
    return;
  if ((rnum = real_quest(GET_QUEST(ch))) == NOTHING)
    return;
  switch (type) {
    case AQ_OBJ_FIND:
      if (QST_TARGET(rnum) == GET_OBJ_VNUM(object))
        generic_complete_quest(ch);
      break;
    case AQ_ROOM_FIND:
      if (QST_TARGET(rnum) == world[IN_ROOM(ch)].number)
        generic_complete_quest(ch);
      break;
    case AQ_MOB_FIND:
      for (i=world[IN_ROOM(ch)].people; i; i = i->next_in_room)
        if (IS_NPC(i))
          if (QST_TARGET(rnum) == GET_MOB_VNUM(i))
            generic_complete_quest(ch);
      break;
    case AQ_MOB_KILL:
      if (!IS_NPC(ch) && IS_NPC(vict) && (ch != vict))
          if (QST_TARGET(rnum) == GET_MOB_VNUM(vict))
            generic_complete_quest(ch);
      break;
    case AQ_MOB_SAVE:
       if (ch == vict)
        found = FALSE;
      for (i = world[IN_ROOM(ch)].people; i && found; i = i->next_in_room)
          if (i && IS_NPC(i) && !MOB_FLAGGED(i, MOB_NOTDEADYET))
            if ((GET_MOB_VNUM(i) != QST_TARGET(rnum)) &&
                !AFF_FLAGGED(i, AFF_CHARM))
              found = FALSE;
      if (found)
        generic_complete_quest(ch);
      break;
    case AQ_OBJ_RETURN:
      if (IS_NPC(vict) && (GET_MOB_VNUM(vict) == QST_RETURNMOB(rnum)))
        if (object && (GET_OBJ_VNUM(object) == QST_TARGET(rnum)))
          generic_complete_quest(ch);
      break;
    case AQ_ROOM_CLEAR:
      if (QST_TARGET(rnum) == world[IN_ROOM(ch)].number) {
        for (i = world[IN_ROOM(ch)].people; i && found; i = i->next_in_room)
          if (i && IS_NPC(i) && !MOB_FLAGGED(i, MOB_NOTDEADYET))
            found = FALSE;
        if (found)
   generic_complete_quest(ch);
      }
      break;
    default:
      log("SYSERR: Invalid quest type passed to autoquest_trigger_check");
      break;
  }
}

void quest_timeout(struct char_data *ch)
{
  if ((GET_QUEST(ch) != NOTHING) && (GET_QUEST_TIME(ch) != -1)) {
    clear_quest(ch);
    send_to_char(ch, "타임아웃! 퀘스트 수행에 실패했습니다.\r\n");
  }
}

void check_timed_quests(void)
{
  struct char_data *ch;

  for (ch = character_list; ch; ch = ch->next)
    if (!IS_NPC(ch) && (GET_QUEST(ch) != NOTHING) && (GET_QUEST_TIME(ch) != -1))
      if (--GET_QUEST_TIME(ch) == 0)
        quest_timeout(ch);
}

/*--------------------------------------------------------------------------*/
/* Quest Command Helper Functions                                           */
/*--------------------------------------------------------------------------*/

void list_quests(struct char_data *ch, zone_rnum zone, qst_vnum vmin, qst_vnum vmax)
{
  qst_rnum rnum;
  qst_vnum bottom, top;
  int counter = 0;

  if (zone != NOWHERE) {
    bottom = zone_table[zone].bot;
    top    = zone_table[zone].top;
  } else {
    bottom = vmin;
    top    = vmax;
  }
  /* Print the header for the quest listing. */
  send_to_char (ch,
  "순번   임무번호   설명   퀘스트관리자\r\n"
  "----- ------- -------------------------------------------- -----------\r\n");
  for (rnum = 0; rnum < total_quests ; rnum++)
    if (QST_NUM(rnum) >= bottom && QST_NUM(rnum) <= top)
      send_to_char(ch, "\tg%d\tn) [\tg%d\tn] \tc%s\tn \ty[%d]\tn\r\n",
          ++counter, QST_NUM(rnum), QST_NAME(rnum),
          QST_MASTER(rnum) == NOBODY ? 0 : QST_MASTER(rnum));
  if (!counter)
    send_to_char(ch, "발견된 임무 없음.\r\n");
}

static void quest_hist(struct char_data *ch)
{
  int i = 0, counter = 0;
  qst_rnum rnum = NOTHING;

  send_to_char(ch, "현재까지 완료한 임무:\r\n"
    "순번   설명   퀘스트관리자\r\n"
    "----- ---------------------------------------------------- -----------\r\n");
  for (i = 0; i < GET_NUM_QUESTS(ch); i++) {
    if ((rnum = real_quest(ch->player_specials->saved.completed_quests[i])) != NOTHING)
      send_to_char(ch, "\tg%d\tn) \tc%s\tn \ty%s\tn\r\n",
 ++counter, QST_DESC(rnum), (real_mobile(QST_MASTER(rnum)) == NOBODY) ? "Unknown" : GET_NAME(&mob_proto[(real_mobile(QST_MASTER(rnum)))]));
    else
      send_to_char(ch,
        "\tg%d\tn) \tc알 수 없는 퀘스트 (현재 존재하지 않음)\tn\r\n", ++counter);
  }
  if (!counter)
    send_to_char(ch, "아직 완료한 임무가 없습니다.\r\n");
}

static void quest_join(struct char_data *ch, struct char_data *qm, char argument[MAX_INPUT_LENGTH])
{
  qst_vnum vnum;
  qst_rnum rnum;
  char buf[MAX_INPUT_LENGTH];

  if (!*argument)
    snprintf(buf, sizeof(buf),
             "%s 몇 번 임무를 수행하시겠습니까?", GET_NAME(ch));
  else if (GET_QUEST(ch) != NOTHING)
    snprintf(buf, sizeof(buf),
             "%s 이미 다른 임무를 수행중입니다!", GET_NAME(ch));
  else if((vnum = find_quest_by_qmnum(ch, GET_MOB_VNUM(qm), atoi(argument))) == NOTHING)
    snprintf(buf, sizeof(buf),
             "%s 그런 퀘스트는 모르겠습니다!", GET_NAME(ch));
  else if ((rnum = real_quest(vnum)) == NOTHING)
    snprintf(buf, sizeof(buf),
             "%s 그런 퀘스트는 모르겠습니다!", GET_NAME(ch));
  else if (GET_LEVEL(ch) < QST_MINLEVEL(rnum))
    snprintf(buf, sizeof(buf),
             "%s 그 임무를 수행하기에는 경험이 부족합니다!", GET_NAME(ch));
  else if (GET_LEVEL(ch) > QST_MAXLEVEL(rnum))
    snprintf(buf, sizeof(buf),
             "%s 그 임무는 당신이 수행하기에 너무 쉽습니다!", GET_NAME(ch));
  else if (is_complete(ch, vnum))
    snprintf(buf, sizeof(buf),
             "%s 이미 완료한 퀘스트입니다!", GET_NAME(ch));
  else if ((QST_PREV(rnum) != NOTHING) && !is_complete(ch, QST_PREV(rnum)))
    snprintf(buf, sizeof(buf),
             "%s 아직 수행 가능한 퀘스트가 아닙니다!", GET_NAME(ch));
  else if ((QST_PREREQ(rnum) != NOTHING) &&
           (real_object(QST_PREREQ(rnum)) != NOTHING) &&
           (get_obj_in_list_num(real_object(QST_PREREQ(rnum)),
       ch->carrying) == NULL))
    snprintf(buf, sizeof(buf),
             "%s 먼저 %s가 필요합니다!", GET_NAME(ch),
      obj_proto[real_object(QST_PREREQ(rnum))].short_description);
  else {
    act("좋아요, 행운을 빕니다.",    TRUE, ch, NULL, NULL, TO_CHAR);
    act("$n님이 퀘스트를 수행하기 시작합니다.", TRUE, ch, NULL, NULL, TO_ROOM);
    snprintf(buf, sizeof(buf),
             "%s 설명을 잘 들어주세요.", GET_NAME(ch));
    do_tell(qm, buf, cmd_tell, 0);
    set_quest(ch, rnum);
    send_to_char(ch, "%s", QST_INFO(rnum));
    if (QST_TIME(rnum) != -1)
      snprintf(buf, sizeof(buf),
        "%s 이 임무에는 %d분의 시간제한이 있습니다.",
        GET_NAME(ch), QST_TIME(rnum));
    else
      snprintf(buf, sizeof(buf),
        "%s 이 임무는 시간 제한이 없습니다.",
 GET_NAME(ch));
  }
  do_tell(qm, buf, cmd_tell, 0);
  save_char(ch);
}

void quest_list(struct char_data *ch, struct char_data *qm, char argument[MAX_INPUT_LENGTH])
{
  qst_vnum vnum;
  qst_rnum rnum;

  if ((vnum = find_quest_by_qmnum(ch, GET_MOB_VNUM(qm), atoi(argument))) == NOTHING)
    send_to_char(ch, "유효하지 않은 퀘스트입니다!\r\n");
  else if ((rnum = real_quest(vnum)) == NOTHING)
    send_to_char(ch, "유효한 퀘스트가 아닙니다!\r\n");
  else if (QST_INFO(rnum)) {
    send_to_char(ch,"%d번 퀘스트의 상세 내용을 완성하세요 \tc%s\tn:\r\n%s",
                      vnum,
         QST_DESC(rnum),
         QST_INFO(rnum));
    if (QST_PREV(rnum) != NOTHING)
      send_to_char(ch, "%s 퀘스트를 먼저 수행해야 합니다.\r\n",
          QST_NAME(real_quest(QST_PREV(rnum))));
    if (QST_TIME(rnum) != -1)
      send_to_char(ch,
         "이 임무는 %d분의 시간제한이 있습니다.\r\n",
          QST_TIME(rnum));
  } else
    send_to_char(ch, "이 임무에 대한 추가 정보가 없습니다.\r\n");
}

static void quest_quit(struct char_data *ch)
{
  qst_rnum rnum;

  if (GET_QUEST(ch) == NOTHING)
    send_to_char(ch, "현재 임무 수행중이 아닙니다!\r\n");
  else if ((rnum = real_quest(GET_QUEST(ch))) == NOTHING) {
    clear_quest(ch);
    send_to_char(ch, "수행중인 퀘스트를 취소합니다.\r\n");
    save_char(ch);
  } else {
    clear_quest(ch);
    if (QST_QUIT(rnum) && (str_cmp(QST_QUIT(rnum), "undefined") != 0))
      send_to_char(ch, "%s", QST_QUIT(rnum));
    else
      send_to_char(ch, "수행중인 퀘스트를 취소합니다.\r\n");
    if (QST_PENALTY(rnum)) {
      GET_QUESTPOINTS(ch) -= QST_PENALTY(rnum);
      send_to_char(ch,
        "퀘스트를 취소하여 %d점의 임무점수를 잃었습니다.\r\n",
        QST_PENALTY(rnum));
    }
    save_char(ch);
  }
}

static void quest_progress(struct char_data *ch)
{
  qst_rnum rnum;

  if (GET_QUEST(ch) == NOTHING)
    send_to_char(ch, "현재 수행중인 임무가 없습니다!\r\n");
  else if ((rnum = real_quest(GET_QUEST(ch))) == NOTHING) {
    clear_quest(ch);
    send_to_char(ch, "진행중이던 퀘스트가 지금은 존재하지 않습니다.\r\n");
  } else {
    send_to_char(ch, "현재 진행중인 퀘스트는 다음과 같습니다:\r\n%s\r\n%s",
        QST_DESC(rnum), QST_INFO(rnum));
    if (QST_QUANTITY(rnum) > 1)
      send_to_char(ch,
          "퀘스트 완료까지 남은 개수: %d\r\n",
   GET_QUEST_COUNTER(ch));
    if (GET_QUEST_TIME(ch) > 0)
      send_to_char(ch,
          "퀘스트 종료까지 남은시간: %d분\r\n",
   GET_QUEST_TIME(ch));
  }
}

static void quest_show(struct char_data *ch, mob_vnum qm)
{
  qst_rnum rnum;
  int counter = 0;

  send_to_char(ch,
  "수행 가능한 퀘스트:\r\n"
  "순번   설명   (번호)   완료여부?\r\n"
  "----- ---------------------------------------------------- ------- -----\r\n");
  for (rnum = 0; rnum < total_quests; rnum++)
    if (qm == QST_MASTER(rnum))
      send_to_char(ch, "\tg%d\tn) \tc%s\tn \ty(%d)\tn \ty(%s)\tn\r\n",
        ++counter, QST_DESC(rnum), QST_NUM(rnum),
        (is_complete(ch, QST_NUM(rnum)) ? "완료" : "미완료"));
  if (!counter)
    send_to_char(ch, "지금은 수행 가능한 임무가 없습니다.\r\n");
}

static void quest_stat(struct char_data *ch, char argument[MAX_STRING_LENGTH])
{
  qst_rnum rnum;
  mob_rnum qmrnum;
  char buf[MAX_STRING_LENGTH];
  char targetname[MAX_STRING_LENGTH];

  if (!*argument)
    send_to_char(ch, "%s\r\n", quest_imm_usage);
  else if ((rnum = real_quest(atoi(argument))) == NOTHING )
    send_to_char(ch, "존재하지 않는 임무입니다.\r\n");
  else {
    sprintbit(QST_FLAGS(rnum), aq_flags, buf, sizeof(buf));
    switch (QST_TYPE(rnum)) {
      case AQ_OBJ_FIND:
      case AQ_OBJ_RETURN:
        snprintf(targetname, sizeof(targetname), "%s",
                 real_object(QST_TARGET(rnum)) == NOTHING ?
                 "알 수 없는 물건" :
    obj_proto[real_object(QST_TARGET(rnum))].short_description);
 break;
      case AQ_ROOM_FIND:
      case AQ_ROOM_CLEAR:
        snprintf(targetname, sizeof(targetname), "%s",
          real_room(QST_TARGET(rnum)) == NOWHERE ?
                 "알 수 없는 방" :
    world[real_room(QST_TARGET(rnum))].name);
        break;
      case AQ_MOB_FIND:
      case AQ_MOB_KILL:
      case AQ_MOB_SAVE:
 snprintf(targetname, sizeof(targetname), "%s",
                 real_mobile(QST_TARGET(rnum)) == NOBODY ?
    "알 수 없는 맙" :
    GET_NAME(&mob_proto[real_mobile(QST_TARGET(rnum))]));
 break;
      default:
 snprintf(targetname, sizeof(targetname), "알 수 없음");
 break;
    }
    qmrnum = real_mobile(QST_MASTER(rnum));
    send_to_char(ch,
        "임무번호 : [\ty%d\tn], RNum: [\ty%d\tn] -- 퀘스트관리자: [\ty%d\tn] \ty%s\tn\r\n"
        "이름: \ty%s\tn\r\n"
 "설명: \ty%s\tn\r\n"
 "시작 메시지:\r\n\tc%s\tn"
 "완료 메시지:\r\n\tc%s\tn"
 "취소 메시지:\r\n\tc%s\tn"
 "유형: \ty%s\tn\r\n"
        "대상: \ty%d\tn \ty%s\tn, 횟수: \ty%d\tn\r\n"
 "보상 임무점수: \ty%d\tn, 실패 임무점수 삭감: \ty%d\tn, 최소레벨: \ty%d\tn, 최고레벨: \ty%d\tn\r\n"
 "속성: \tc%s\tn\r\n",
     QST_NUM(rnum), rnum,
 QST_MASTER(rnum) == NOBODY ? -1 : QST_MASTER(rnum),
 (qmrnum == NOBODY) ? "(Invalid vnum)" : GET_NAME(&mob_proto[(qmrnum)]),
        QST_NAME(rnum), QST_DESC(rnum),
        QST_INFO(rnum), QST_DONE(rnum),
 (QST_QUIT(rnum) &&
  (str_cmp(QST_QUIT(rnum), "undefined") != 0)
          ? QST_QUIT(rnum) : "Nothing\r\n"),
     quest_types[QST_TYPE(rnum)],
 QST_TARGET(rnum) == NOBODY ? -1 : QST_TARGET(rnum),
 targetname,
 QST_QUANTITY(rnum),
     QST_POINTS(rnum), QST_PENALTY(rnum), QST_MINLEVEL(rnum),
 QST_MAXLEVEL(rnum), buf);
    if (QST_PREREQ(rnum) != NOTHING)
      send_to_char(ch, "선행 퀘스트: [\ty%d\tn] \ty%s\tn\r\n",
        QST_PREREQ(rnum) == NOTHING ? -1 : QST_PREREQ(rnum),
        QST_PREREQ(rnum) == NOTHING ? "" :
   real_object(QST_PREREQ(rnum)) == NOTHING ? "알 수 없는 물건" :
       obj_proto[real_object(QST_PREREQ(rnum))].short_description);
    if (QST_TYPE(rnum) == AQ_OBJ_RETURN)
      send_to_char(ch, "맙: [\ty%d\tn] \ty%s\tn\r\n",
        QST_RETURNMOB(rnum),
 real_mobile(QST_RETURNMOB(rnum)) == NOBODY ? "알 수 없는 맙" :
           mob_proto[real_mobile(QST_RETURNMOB(rnum))].player.short_descr);
    if (QST_TIME(rnum) != -1)
      send_to_char(ch, "시간제한: %d분\r\n",
   QST_TIME(rnum));
    else
      send_to_char(ch, "시간제한: 없음\r\n");
    if (QST_PREV(rnum) == NOTHING)
      send_to_char(ch, "이전 퀘스트: \ty없음.\tn\r\n");
    else
      send_to_char(ch, "이전 퀘스트: [\ty%d\tn] \tc%s\tn\r\n",
        QST_PREV(rnum), QST_DESC(real_quest(QST_PREV(rnum))));
    if (QST_NEXT(rnum) == NOTHING)
      send_to_char(ch, "다음 퀘스트: \tyNone.\tn\r\n");
    else
      send_to_char(ch, "다음 퀘스트: [\ty%d\tn] \tc%s\tn\r\n",
        QST_NEXT(rnum), QST_DESC(real_quest(QST_NEXT(rnum))));
  }
}

/*--------------------------------------------------------------------------*/
/* Quest Command Processing Function and Questmaster Special                */
/*--------------------------------------------------------------------------*/

ACMD(do_quest)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  int  tp;

  two_arguments(argument, arg1, arg2);
  if (!*arg1)
    send_to_char(ch, "%s\r\n", GET_LEVEL(ch) < LVL_IMMORT ?
                     quest_mort_usage : quest_imm_usage);
  else if (((tp = search_block(arg1, quest_cmd, FALSE)) == -1))
    send_to_char(ch, "%s\r\n", GET_LEVEL(ch) < LVL_IMMORT ?
                     quest_mort_usage : quest_imm_usage);
  else {
    switch (tp) {
      case SCMD_QUEST_LIST:
      case SCMD_QUEST_JOIN:
        /* list, join should hve been handled by questmaster spec proc */
        send_to_char(ch, "여기서는 사용할 수 없는 명령입니다!\r\n");
        break;
      case SCMD_QUEST_HISTORY:
        quest_hist(ch);
        break;
      case SCMD_QUEST_LEAVE:
        quest_quit(ch);
        break;
      case SCMD_QUEST_PROGRESS:
 quest_progress(ch);
 break;
      case SCMD_QUEST_STATUS:
        if (GET_LEVEL(ch) < LVL_IMMORT)
          send_to_char(ch, "%s\r\n", quest_mort_usage);
        else
          quest_stat(ch, arg2);
        break;
      default: /* Whe should never get here, but... */
        send_to_char(ch, "%s\r\n", GET_LEVEL(ch) < LVL_IMMORT ?
                     quest_mort_usage : quest_imm_usage);
 break;
    } /* switch on subcmd number */
  }
}

SPECIAL(questmaster)
{
  qst_rnum rnum;
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  int  tp;
  struct char_data *qm = (struct char_data *)me;

  /* check that qm mob has quests assigned */
  for (rnum = 0; (rnum < total_quests &&
      QST_MASTER(rnum) != GET_MOB_VNUM(qm)) ; rnum ++);
  if (rnum >= total_quests)
    return FALSE; /* No quests for this mob */
  else if (QST_FUNC(rnum) && (QST_FUNC(rnum) (ch, me, cmd, argument)))
    return TRUE;  /* The secondary spec proc handled this command */
  else if (CMD_IS("임무")) {
    two_arguments(argument, arg1, arg2);
    if (!*arg1)
      return FALSE;
    else if (((tp = search_block(arg1, quest_cmd, FALSE)) == -1))
      return FALSE;
    else {
      switch (tp) {
      case SCMD_QUEST_LIST:
        if (!*arg2)
          quest_show(ch, GET_MOB_VNUM(qm));
        else
   quest_list(ch, qm, arg2);
        break;
      case SCMD_QUEST_JOIN:
        quest_join(ch, qm, arg2);
        break;
      default:
 return FALSE; /* fall through to the do_quest command processor */
      } /* switch on subcmd number */
      return TRUE;
    }
  } else {
    return FALSE; /* not a questmaster command */
  }
}
