/**************************************************************************
*  File: interpreter.c                                     Part of tbaMUD *
*  Usage: Parse user commands, search for specials, call ACMD functions.  *
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
#include "db.h"
#include "spells.h"
#include "handler.h"
#include "mail.h"
#include "screen.h"
#include "genolc.h"
#include "oasis.h"
#include "improved-edit.h"
#include "dg_scripts.h"
#include "constants.h"
#include "act.h" /* ACMDs located within the act*.c files */
#include "ban.h"
#include "class.h"
#include "graph.h"
#include "hedit.h"
#include "house.h"
#include "config.h"
#include "modify.h" /* for do_skillset... */
#include "quest.h"
#include "asciimap.h"
#include "prefedit.h"
#include "ibt.h"
#include "mud_event.h"
#include "baseball.h"

/* local (file scope) functions */
static int perform_dupe_check(struct descriptor_data *d);
static struct alias_data *find_alias(struct alias_data *alias_list, char *str);
static void perform_complex_alias(struct txt_q *input_q, char *orig, struct alias_data *a);
static int _parse_name(char *arg, char *name);
static bool perform_new_char_dupe_check(struct descriptor_data *d);
/* sort_commands utility */
static int sort_commands_helper(const void *a, const void *b);

/* globals defined here, used here and elsewhere */
int *cmd_sort_info = NULL;

struct command_info *complete_cmd_info;

/* This is the Master Command List. You can put new commands in, take commands
 * out, change the order they appear in, etc.  You can adjust the "priority"
 * of commands simply by changing the order they appear in the command list.
 * (For example, if you want "as" to mean "assist" instead of "ask", just put
 * "assist" above "ask" in the Master Command List. In general, utility
 * commands such as "at" should have high priority; infrequently used and
 * dangerously destructive commands should have low priority. */

cpp_extern const struct command_info cmd_info[] = {
  { "RESERVED", "", 0, 0, 0, 0 }, /* this must be first -- for specprocs */

  /* directions must come before other commands but after RESERVED */
  { "북"    , "ㅂ"       , POS_STANDING, do_move     , 0, SCMD_NORTH },
  { "동"     , "ㄷ"       , POS_STANDING, do_move     , 0, SCMD_EAST },
  { "남"    , "ㄴ"       , POS_STANDING, do_move     , 0, SCMD_SOUTH },
  { "서"     , "ㅅ"       , POS_STANDING, do_move     , 0, SCMD_WEST },
  { "위"       , "ㅇ"       , POS_STANDING, do_move     , 0, SCMD_UP },
  { "밑"     , "ㅁ"       , POS_STANDING, do_move     , 0, SCMD_DOWN },
  { "ㅂ"    , "ㅂ"       , POS_STANDING, do_move     , 0, SCMD_NORTH },
  { "ㄷ"     , "ㄷ"       , POS_STANDING, do_move     , 0, SCMD_EAST },
  { "ㄴ"    , "ㄴ"       , POS_STANDING, do_move     , 0, SCMD_SOUTH },
  { "ㅅ"     , "ㅅ"       , POS_STANDING, do_move     , 0, SCMD_WEST },
  { "ㅇ"       , "ㅇ"       , POS_STANDING, do_move     , 0, SCMD_UP },
  { "ㅁ"     , "ㅁ"       , POS_STANDING, do_move     , 0, SCMD_DOWN },
  { "북서", "ㅄ"  , POS_STANDING, do_move     , 0, SCMD_NW },
  { "ㅄ"       , "ㅄ"      , POS_STANDING, do_move     , 0, SCMD_NW },
  { "북동", "ㅂㄷ"  , POS_STANDING, do_move     , 0, SCMD_NE },
  { "ㅂㄷ"       , "ㅂㄷ"      , POS_STANDING, do_move     , 0,  SCMD_NE },
  { "남동", "ㄴㄷ"  , POS_STANDING, do_move     , 0, SCMD_SE },
  { "ㄴㄷ"       , "ㄴㄷ"      , POS_STANDING, do_move     , 0, SCMD_SE },
  { "남서", "ㄴㅅ"  , POS_STANDING, do_move     , 0, SCMD_SW },
  { "ㄴㅅ"       , "ㄴㅅ"      , POS_STANDING, do_move     , 0, SCMD_SW },
  { "north"    , "n"       , POS_STANDING, do_move     , 0, SCMD_NORTH },
  { "east"     , "e"       , POS_STANDING, do_move     , 0, SCMD_EAST },
  { "south"    , "s"       , POS_STANDING, do_move     , 0, SCMD_SOUTH },
  { "west"     , "w"       , POS_STANDING, do_move     , 0, SCMD_WEST },
  { "up"       , "u"       , POS_STANDING, do_move     , 0, SCMD_UP },
  { "down"     , "d"       , POS_STANDING, do_move     , 0, SCMD_DOWN },
  { "northwest", "northw"  , POS_STANDING, do_move     , 0, SCMD_NW },
  { "nw"       , "nw"      , POS_STANDING, do_move     , 0, SCMD_NW },
  { "northeast", "northe"  , POS_STANDING, do_move     , 0, SCMD_NE },
  { "ne"       , "ne"      , POS_STANDING, do_move     , 0,  SCMD_NE },
  { "southeast", "southe"  , POS_STANDING, do_move     , 0, SCMD_SE },
  { "se"       , "se"      , POS_STANDING, do_move     , 0, SCMD_SE },
  { "southwest", "southw"  , POS_STANDING, do_move     , 0, SCMD_SW },
  { "sw"       , "sw"      , POS_STANDING, do_move     , 0, SCMD_SW },
  
  /* now, the main list */
  { "원격"       , "at"      , POS_DEAD    , do_at       , LVL_IMMORT, 0 },
  { "레벨구간"   , "lev"     , POS_DEAD    , do_levels   , 0, 0 },
  { "승급"  , "adv"     , POS_DEAD    , do_advance  , LVL_GRGOD, 0 },
  { "aedit"    , "aed"     , POS_DEAD    , do_oasis_aedit, LVL_GOD, 0 },
  { "줄임말"    , "ali"     , POS_DEAD    , do_alias    , 0, 0 },
  { "접속유지"      , "afk"     , POS_DEAD    , do_gen_tog  , 0, SCMD_AFK },
  { "지역"    , "are"     , POS_DEAD    , do_areas    , 0, 0 },
  { "지원"   , "as"      , POS_FIGHTING, do_assist   , 1, 0 },
  { "질문"      , "ask"     , POS_RESTING , do_spec_comm, 0, SCMD_ASK },
  { "감정" , "id"      , POS_STANDING, do_not_here , 1, 0 }, // identify
  { "감정상세"    , "ast"     , POS_DEAD    , do_astat    , 0, 0 },
  { "attach"   , "attach"  , POS_DEAD    , do_attach   , LVL_BUILDER, 0 },
  { "경매"  , "auc"     , POS_SLEEPING, do_gen_comm , 0, SCMD_AUCTION },
  { "자동출구" , "autoex"  , POS_DEAD    , do_gen_tog , 0, SCMD_AUTOEXIT },
  { "자동돕기","autoass" , POS_DEAD    , do_gen_tog , 0, SCMD_AUTOASSIST },
  { "자동문감지" , "autodoor", POS_DEAD    , do_gen_tog , 0, SCMD_AUTODOOR },
  { "자동돈줍기" , "autogold", POS_DEAD    , do_gen_tog , 0, SCMD_AUTOGOLD },
  { "자동문따기"  , "autokey" , POS_DEAD    , do_gen_tog , 0, SCMD_AUTOKEY },
  { "자동줍기" , "autoloot", POS_DEAD    , do_gen_tog , 0, SCMD_AUTOLOOT },
  { "자동지도"  , "automap" , POS_DEAD    , do_gen_tog , 0, SCMD_AUTOMAP },
  { "자동제물"  , "autosac" , POS_DEAD    , do_gen_tog , 0, SCMD_AUTOSAC },
  { "자동분배", "autospl" , POS_DEAD    , do_gen_tog , 0, SCMD_AUTOSPLIT },

  { "backstab" , "ba"      , POS_STANDING, do_backstab , 1, 0 },
  { "접속금지"      , "ban"     , POS_DEAD    , do_ban      , LVL_GRGOD, 0 },
  { "bandage"  , "band"    , POS_RESTING , do_bandage  , 1, 0 },
  { "잔액"  , "bal"     , POS_STANDING, do_not_here , 1, 0 },
  { "bash"     , "bas"     , POS_FIGHTING, do_bash     , 1, 0 },
  { "간략"    , "br"      , POS_DEAD    , do_gen_tog  , 0, SCMD_BRIEF },
  { "존제작", "buildwalk", POS_STANDING, do_gen_tog , LVL_BUILDER, SCMD_BUILDWALK },
  { "사"      , "bu"      , POS_STANDING, do_not_here , 0, 0 },
  { "버그"      , "bug"     , POS_DEAD    , do_ibt      , 0, SCMD_BUG },

  { "캐스팅"     , "c"       , POS_SITTING , do_cast     , 1, 0 }, // cast
  { "cedit"    , "cedit"   , POS_DEAD    , do_oasis_cedit, LVL_IMPL, 0 },
  { "changelog", "cha"     , POS_DEAD    , do_changelog, LVL_IMPL, 0 },
  { "편지확인"    , "ch"      , POS_STANDING, do_not_here , 1, 0 }, // check
  { "편지받아"  , "rece"    , POS_STANDING, do_not_here , 1, 0 }, // receive
  { "편지쓰기"     , "mail"    , POS_STANDING, do_not_here , 1, 0 }, // mail
  { "확인", "checkl"  , POS_DEAD    , do_checkloadstatus, LVL_GOD, 0 },
  { "닫아"    , "cl"      , POS_SITTING , do_gen_door , 0, SCMD_CLOSE },
  { "화면정리"    , "cle"     , POS_DEAD    , do_gen_ps   , 0, SCMD_CLEAR },
  { "화면초기화"      , "cls"     , POS_DEAD    , do_gen_ps   , 0, SCMD_CLEAR },
  { "비교" , "con"     , POS_RESTING , do_consider , 0, 0 },
  { "모든명령" , "com"     , POS_DEAD    , do_commands , 0, SCMD_COMMANDS },
  { "공격"      , "hit"     , POS_FIGHTING, do_hit      , 0, SCMD_HIT },
  { "운영공지"    , "imo"     , POS_DEAD    , do_gen_ps   , LVL_IMMORT, SCMD_IMOTD }, // imotd
  { "공백제거"  , "comp"    , POS_DEAD    , do_gen_tog  , 0, SCMD_COMPACT },
  { "업데이트" , "copyover", POS_DEAD    , do_copyover , LVL_GRGOD, 0 },
  { "credits"  , "cred"    , POS_DEAD    , do_gen_ps   , 0, SCMD_CREDITS },

  { "실제시간"     , "da"      , POS_DEAD    , do_date     , LVL_IMMORT, SCMD_DATE }, // date
  { "접속끊기"       , "dc"      , POS_DEAD    , do_dc       , LVL_GOD, 0 },
  { "입어"     , "wea"     , POS_RESTING , do_wear     , 0, 0 },
  { "입금"  , "depo"    , POS_STANDING, do_not_here , 1, 0 },
  { "detach"   , "detach"  , POS_DEAD    , do_detach   , LVL_BUILDER, 0 },
  { "진단" , "diag"    , POS_RESTING , do_diagnose , 0, 0 },
  { "방연결"      , "dig"     , POS_DEAD    , do_dig      , LVL_BUILDER, 0 },
  { "표시"  , "disp"    , POS_DEAD    , do_display  , 0, 0 }, // display
  { "기부"   , "don"     , POS_RESTING , do_drop     , 0, SCMD_DONATE },
  { "마셔"    , "dri"     , POS_RESTING , do_drink    , 0, SCMD_DRINK },
  { "버려"     , "dro"     , POS_RESTING , do_drop     , 0, SCMD_DROP },

  { "먹어"      , "ea"      , POS_RESTING , do_eat      , 0, SCMD_EAT },
  { "방공지"     , "ec"      , POS_SLEEPING, do_echo     , LVL_IMMORT, SCMD_ECHO }, // echo
  { "표현"    , "em"      , POS_RESTING , do_echo     , 0, SCMD_EMOTE },
  { ":"        , ":"       , POS_RESTING, do_echo      , 1, SCMD_EMOTE },
  { "들어가"    , "ent"     , POS_STANDING, do_enter    , 0, 0 },
  { "장비", "eq"      , POS_SLEEPING, do_equipment, 0, 0 },
  { "출구"    , "ex"      , POS_RESTING , do_exits    , 0, 0 },
  { "조사"  , "exa"     , POS_SITTING , do_examine  , 0, 0 },
  { "export"   , "export"  , POS_DEAD    , do_export_zone, LVL_IMPL, 0 },

  { "강제명령"    , "force"   , POS_SLEEPING, do_force    , LVL_GOD, 0 }, // force
  { "채워"     , "fil"     , POS_STANDING, do_pour     , 0, SCMD_FILL },
  { "로그조회"     , "file"    , POS_SLEEPING, do_file     , LVL_GOD, 0 },
  { "도망"     , "fl"      , POS_FIGHTING, do_flee     , 1, 0 },
  { "따라"   , "fol"     , POS_RESTING , do_follow   , 0, 0 },
  { "냉동"   , "freeze"  , POS_DEAD    , do_wizutil  , LVL_GRGOD, SCMD_FREEZE },

  { "가져"      , "g"       , POS_RESTING , do_get      , 0, 0 },
  { "게임공지"    , "gecho"   , POS_DEAD    , do_gecho    , LVL_GOD, 0 }, // gecho
  { "푸념"   , "gem"     , POS_SLEEPING, do_gen_comm , 0, SCMD_GEMOTE },
  { "줘"     , "giv"     , POS_RESTING , do_give     , 0, 0 },
  { "순간이동"     , "go"      , POS_SLEEPING, do_goto     , LVL_IMMORT, 0 },
  { "돈"     , "gol"     , POS_RESTING , do_gold     , 0, 0 },
  { "잡담"   , "gos"     , POS_SLEEPING, do_gen_comm , 0, SCMD_GOSSIP },
  { "그룹"    , "gr"      , POS_RESTING , do_group    , 1, 0 },
  { "쥐어"     , "grab"    , POS_RESTING , do_grab     , 0, 0 },
  { "축하"    , "grat"    , POS_SLEEPING, do_gen_comm , 0, SCMD_GRATZ },
  { "그룹말"     , "gsay"    , POS_SLEEPING, do_gsay     , 0, 0 },
  { "["    , "gt"      , POS_SLEEPING, do_gsay     , 0, 0 },

  { "help"     , "h"       , POS_DEAD    , do_help     , 0, 0 },
  { "행복시간", "ha"      , POS_DEAD    , do_happyhour, 0, 0 },
  { "hedit"    , "hedit"   , POS_DEAD    , do_oasis_hedit, LVL_GOD , 0 },
  { "helpcheck", "helpch"  , POS_DEAD    , do_helpcheck, LVL_GOD, 0 },
  { "숨어"     , "hi"      , POS_RESTING , do_hide     , 1, 0 },
  { "hindex"   , "hind"    , POS_DEAD    , do_hindex   , 0, 0 },
  { "핸드북" , "handb"   , POS_DEAD    , do_gen_ps   , LVL_IMMORT, SCMD_HANDBOOK },
  { "집설정" , "hcontrol", POS_DEAD    , do_hcontrol , LVL_GRGOD, 0 },
  { "기록보기"  , "history" , POS_DEAD    , do_history, 0, 0},
  { "hold"     , "hold"    , POS_RESTING , do_grab     , 1, 0 },
  { "holler"   , "holler"  , POS_RESTING , do_gen_comm , 1, SCMD_HOLLER },
  { "암흑보기", "holy"    , POS_DEAD    , do_gen_tog  , LVL_IMMORT, SCMD_HOLYLIGHT },
  { "초대"    , "house"   , POS_RESTING , do_house    , 0, 0 },

  { "소지품", "i"       , POS_DEAD    , do_inventory, 0, 0 },
  { "건의"     , "ide"      , POS_DEAD    , do_ibt      , 0, SCMD_IDEA }, // idea
  { "정보"     , "info"    , POS_SLEEPING, do_gen_ps   , 0, SCMD_INFO },
  { "소각"     , "j"       , POS_RESTING , do_drop     , 0, SCMD_JUNK }, // junk
  { "투명화"    , "invi"    , POS_DEAD    , do_invis    , LVL_IMMORT, 0 }, // invi
  { "차기"     , "ki"      , POS_FIGHTING, do_kick     , 1, 0 }, // kick
  { "봐"     , "l"       , POS_RESTING , do_look     , 0, SCMD_LOOK },
  { "마지막접속"     , "last"    , POS_DEAD    , do_last     , LVL_GOD, 0 }, // last
  { "나가"    , "lea"     , POS_STANDING, do_leave    , 0, 0 }, // leave
  { "품목"     , "lis"     , POS_STANDING, do_not_here , 0, 0 }, // list
  { "존출구"    , "lin"     , POS_STANDING, do_links    , LVL_GOD, 0 }, // links
  { "잠가"     , "loc"     , POS_SITTING , do_gen_door , 0, SCMD_LOCK }, // lock
  { "창조"     , "load"     , POS_DEAD    , do_load     , LVL_BUILDER, 0 }, // load

  { "immlist"  , "imm"     , POS_DEAD    , do_gen_ps   , 0, SCMD_IMMLIST },

  { "쳐"     , "k"       , POS_FIGHTING, do_kill     , 0, 0 }, // kill


  { "공지"     , "motd"    , POS_DEAD    , do_gen_ps   , 0, SCMD_MOTD }, // motd
  { "지도"      , "map"     , POS_STANDING, do_map      , 1, 0 },
  { "맙편집"    , "med"     , POS_DEAD    , do_oasis_medit, LVL_BUILDER, 0 },
  { "맙목록"    , "mlist"   , POS_DEAD    , do_oasis_list, LVL_BUILDER, SCMD_OASIS_MLIST },
  { "맙복사"    , "mcopy"   , POS_DEAD    , do_oasis_copy, LVL_GOD, CON_MEDIT },
  { "메세지편집"  , "msgedit" , POS_DEAD    , do_msgedit,   LVL_GOD, 0 },
  { "실어증"     , "mute"    , POS_DEAD    , do_wizutil  , LVL_GOD, SCMD_MUTE }, // mute

  { "뉴스"     , "news"    , POS_SLEEPING, do_gen_ps   , 0, SCMD_NEWS }, // news
  { "경매거부", "noauction",POS_DEAD    , do_gen_tog  , 0, SCMD_NOAUCTION },
  { "잡담거부" , "nogossip", POS_DEAD    , do_gen_tog  , 0, SCMD_NOGOSSIP },
  { "축하거부"  , "nograts" , POS_DEAD    , do_gen_tog  , 0, SCMD_NOGRATZ }, // nograts
  { "유령화" , "nohassle", POS_DEAD    , do_gen_tog  , LVL_IMMORT, SCMD_NOHASSLE }, // nohassle
  { "norepeat" , "norepeat", POS_DEAD    , do_gen_tog  , 0, SCMD_NOREPEAT },
  { "외침거부"  , "noshout" , POS_SLEEPING, do_gen_tog  , 1, SCMD_NOSHOUT },
  { "소환거부" , "nosummon", POS_DEAD    , do_gen_tog  , 1, SCMD_NOSUMMON },
  { "귓말거부"   , "notell"  , POS_DEAD    , do_gen_tog  , 1, SCMD_NOTELL },
  { "칭호"    , "title"   , POS_DEAD    , do_title    , 0, 0 }, // title
  { "칭호고정"  , "notitle" , POS_DEAD    , do_wizutil  , LVL_GOD, SCMD_NOTITLE }, // notitle
  { "신채널거부"    , "nowiz"   , POS_DEAD    , do_gen_tog  , LVL_IMMORT, SCMD_NOWIZ },
  { "야구"     , "야"       , POS_STANDING , do_baseball , 0, 0 },
  { "투구"     , "투"       , POS_STANDING , do_pitch , 0, 0 },
  { "타격"     , "타"       , POS_STANDING , do_bat , 0, 0 },
  { "송구"     , "송"       , POS_STANDING , do_pass , 0, 0 },

  { "열어"     , "o"       , POS_SITTING , do_gen_door , 0, SCMD_OPEN },
  { "명령"    , "ord"     , POS_RESTING , do_order    , 1, 0 }, // order
  { "offer"    , "off"     , POS_STANDING, do_not_here , 1, 0 },
  { "편집상태"      , "olc"     , POS_DEAD    , do_show_save_list, LVL_BUILDER, 0 }, // olc
  { "물건목록"    , "olist"   , POS_DEAD    , do_oasis_list, LVL_BUILDER, SCMD_OASIS_OLIST }, // olist
  { "물건편집"    , "oedit"   , POS_DEAD    , do_oasis_oedit, LVL_BUILDER, 0 },
  { "oset"     , "oset"    , POS_DEAD    , do_oset,        LVL_BUILDER, 0 },  
  { "물건복사"    , "ocopy"   , POS_DEAD    , do_oasis_copy, LVL_GOD, CON_OEDIT },

  { "넣어"      , "p"       , POS_RESTING , do_put      , 0, 0 }, // put
  { "전투중지"    , "pe"      , POS_DEAD    , do_peace    , LVL_BUILDER, 0 }, // peace
  { "문따기"     , "pi"      , POS_STANDING, do_gen_door , 1, SCMD_PICK }, // pick
  { "수련" , "pr"      , POS_RESTING , do_practice , 1, 0 },
  { "호출"     , "pag"     , POS_DEAD    , do_page     , 1, 0 }, // page
  { "용서"   , "pardon"  , POS_DEAD    , do_wizutil  , LVL_GOD, SCMD_PARDON }, // pardon
  { "플레이어목록"    , "plist"   , POS_DEAD    , do_plist    , LVL_GOD, 0 }, // plist
  { "법"   , "pol"     , POS_DEAD    , do_gen_ps   , 0, SCMD_POLICIES }, // policy
  { "부워"     , "pour"    , POS_STANDING, do_pour     , 0, SCMD_POUR }, // pour
  { "프롬프트"   , "pro"     , POS_DEAD    , do_display  , 0, 0 }, // prompt
  { "prefedit" , "pre"     , POS_DEAD    , do_oasis_prefedit , 0, 0 },
  { "없애"    , "purge"   , POS_DEAD    , do_purge    , LVL_BUILDER, 0 }, // purge

  { "임무"    , "que"     , POS_DEAD    , do_quest    , 0, 0 }, // quest
  { "임무말"     , "qsay"    , POS_RESTING , do_qcomm    , 0, SCMD_QSAY }, // qsay
  { "임무목록"    , "qlist"   , POS_DEAD    , do_oasis_list, LVL_BUILDER, SCMD_OASIS_QLIST }, // qlist
  { "임무편집"    , "qedit"   , POS_DEAD    , do_oasis_qedit, LVL_BUILDER, 0 },
  { "복용"    , "qua"     , POS_RESTING , do_use      , 0, SCMD_QUAFF }, // quaff
  { "임무공지"    , "qec"     , POS_DEAD    , do_qcomm    , LVL_GOD, SCMD_QECHO }, // qecho
  { "끝"     , "quit"    , POS_DEAD    , do_quit     , 0, SCMD_QUIT },

  { "대답"    , "r"       , POS_SLEEPING, do_reply    , 0, 0 },
  { "쉬어"     , "res"     , POS_RESTING , do_rest     , 0, 0 },
  { "읽어"     , "rea"     , POS_RESTING , do_look     , 0, SCMD_READ },
  { "새로고침"   , "reload"  , POS_DEAD    , do_reboot   , LVL_IMPL, 0 }, // reload
  { "찢어"   , "reci"    , POS_RESTING , do_use      , 0, SCMD_RECITE }, // recite
  { "최근접속"   , "recent"  , POS_DEAD    , do_recent   , LVL_IMMORT, 0 },
  { "벗어"   , "rem"     , POS_RESTING , do_remove   , 0, 0 },
  { "rent"     , "rent"    , POS_STANDING, do_not_here , 1, 0 },
  { "report"   , "repo"    , POS_RESTING , do_report   , 0, 0 },
  { "reroll"   , "rero"    , POS_DEAD    , do_wizutil  , LVL_GRGOD, SCMD_REROLL },
  { "구조"   , "resc"    , POS_FIGHTING, do_rescue   , 1, 0 }, // rescue
  { "완치"  , "resto"   , POS_DEAD    , do_restore  , LVL_GOD, 0 }, // restore
  { "빙의"   , "switch"  , POS_DEAD    , do_switch   , LVL_GOD, 0 }, // switch
  { "빙의해제"   , "retu"    , POS_DEAD    , do_return   , 0, 0 },
  { "방편집"    , "redit"   , POS_DEAD    , do_oasis_redit, LVL_BUILDER, 0 },
  { "방목록"    , "rlist"   , POS_DEAD    , do_oasis_list, LVL_BUILDER, SCMD_OASIS_RLIST },
  { "방복사"    , "rcopy"   , POS_DEAD    , do_oasis_copy, LVL_GOD, CON_REDIT },
  { "방속성", "roomflags", POS_DEAD   , do_gen_tog  , LVL_IMMORT, SCMD_SHOWVNUMS },

  { "제물", "sac"     , POS_RESTING , do_sac      , 0, 0 }, // sacrifice
  { "말"      , "s"       , POS_RESTING , do_say      , 0, 0 },
  { "점수"    , "sc"      , POS_DEAD    , do_score    , 0, 0 },
  { "스캔"     , "sca"     , POS_RESTING , do_scan     , 0, 0 }, // scan
  { "scopy"    , "scopy"   , POS_DEAD    , do_oasis_copy, LVL_GOD, CON_SEDIT },
  { "앉아"      , "si"      , POS_RESTING , do_sit      , 0, 0 },
  { "'"        , "'"       , POS_RESTING , do_say      , 0, 0 },
  { "저장"     , "sav"     , POS_SLEEPING, do_save     , 0, 0 },
  { "saveall"  , "saveall" , POS_DEAD    , do_saveall  , LVL_BUILDER, 0},
  { "팔아"     , "sell"    , POS_STANDING, do_not_here , 0, 0 },
  { "상점편집"    , "sedit"   , POS_DEAD    , do_oasis_sedit, LVL_BUILDER, 0 },
  { "상점목록"    , "slist"   , POS_SLEEPING, do_oasis_list, LVL_BUILDER, SCMD_OASIS_SLIST }, // slist
  { "send"     , "send"    , POS_SLEEPING, do_send     , LVL_GOD, 0 },
  { "셋팅"      , "set"     , POS_DEAD    , do_set      , LVL_IMMORT, 0 },
  { "외쳐"    , "sho"     , POS_RESTING , do_gen_comm , 0, SCMD_SHOUT },
  { "올려"    , "올"     , POS_DEAD , do_stat_plus , 0, 0 },
  { "show"     , "show"    , POS_DEAD    , do_show     , LVL_IMMORT, 0 },
  { "shutdow"  , "shutdow" , POS_DEAD    , do_shutdown , LVL_IMPL, 0 },
  { "셧다운" , "shutdown", POS_DEAD    , do_shutdown , LVL_IMPL, SCMD_SHUTDOWN },
  { "sip"      , "sip"     , POS_RESTING , do_drink    , 0, SCMD_SIP },
  { "기술셋팅" , "skillset", POS_SLEEPING, do_skillset , LVL_GRGOD, 0 }, // skillset
  { "자"    , "sl"      , POS_SLEEPING, do_sleep    , 0, 0 }, // sleep
  { "잠행"    , "sneak"   , POS_STANDING, do_sneak    , 1, 0 }, // sneak
  { "감시"    , "snoop"   , POS_DEAD    , do_snoop    , LVL_GOD, 0 }, // snoop
  { "socials"  , "socials" , POS_DEAD    , do_commands , 0, SCMD_SOCIALS },
  { "분배"    , "split"   , POS_SITTING , do_split    , 1, 0 }, // split
  { "일어나"    , "st"      , POS_RESTING , do_stand    , 0, 0 }, // stand
  { "조회"     , "stat"    , POS_DEAD    , do_stat     , LVL_IMMORT, 0 }, // stat
  { "훔쳐"    , "ste"     , POS_STANDING, do_steal    , 1, 0 }, // steal

  { "tell"     , "t"       , POS_DEAD    , do_tell     , 0, 0 },
  { "take"     , "ta"      , POS_RESTING , do_get      , 0, 0 },
  { "맛봐"    , "tas"     , POS_RESTING , do_eat      , 0, SCMD_TASTE }, // taste
  { "공간이동" , "tele"    , POS_DEAD    , do_teleport , LVL_BUILDER, 0 }, // teleport
  { "tedit"    , "tedit"   , POS_DEAD    , do_tedit    , LVL_GOD, 0 },  /* XXX: Oasisify */
  { "해동"     , "thaw"    , POS_DEAD    , do_wizutil  , LVL_GRGOD, SCMD_THAW }, // thaw
  { "시간"     , "time"    , POS_DEAD    , do_time     , 0, 0 }, // time
  { "설정"   , "toggle"  , POS_DEAD    , do_toggle   , 0, 0 }, // toggle
  { "track"    , "track"   , POS_STANDING, do_track    , 0, 0 },
  { "불러" , "transfer", POS_SLEEPING, do_trans    , LVL_GOD, 0 },
  { "trigedit" , "trigedit", POS_DEAD    , do_oasis_trigedit, LVL_BUILDER, 0 },
  { "typo"     , "typo"    , POS_DEAD    , do_ibt      , 0, SCMD_TYPO },
  { "tlist"    , "tlist"   , POS_DEAD    , do_oasis_list, LVL_BUILDER, SCMD_OASIS_TLIST },
  { "tcopy"    , "tcopy"   , POS_DEAD    , do_oasis_copy, LVL_GOD, CON_TRIGEDIT },
  { "tstat"    , "tstat"   , POS_DEAD    , do_tstat    , LVL_BUILDER, 0 },

  { "풀어"   , "unlock"  , POS_SITTING , do_gen_door , 0, SCMD_UNLOCK }, // unlock
  { "unban"    , "unban"   , POS_DEAD    , do_unban    , LVL_GRGOD, 0 },
  { "unaffect" , "unaffect", POS_DEAD    , do_wizutil  , LVL_GOD, SCMD_UNAFFECT },
  { "unfollow" , "unf"     , POS_RESTING , do_unfollow , 0, 0 },
  { "uptime"   , "uptime"  , POS_DEAD    , do_date     , LVL_GOD, SCMD_UPTIME },
  { "use"      , "use"     , POS_SITTING , do_use      , 1, SCMD_USE },
  { "users"    , "users"   , POS_DEAD    , do_users    , LVL_GOD, 0 },

  { "가격"    , "val"     , POS_STANDING, do_not_here , 0, 0 }, // value
  { "version"  , "ver"     , POS_DEAD    , do_gen_ps   , 0, SCMD_VERSION },
  { "visible"  , "vis"     , POS_RESTING , do_visible  , 1, 0 },
  { "번호조회"     , "vnum"    , POS_DEAD    , do_vnum     , LVL_IMMORT, 0 }, // vnum
  { "vstat"    , "vstat"   , POS_DEAD    , do_vstat    , LVL_IMMORT, 0 },
  { "vdelete"  , "vdelete" , POS_DEAD    , do_vdelete  , LVL_BUILDER, 0 },

  { "깨어나"     , "wake"    , POS_SLEEPING, do_wake     , 0, 0 },
  { "날씨"  , "weather" , POS_RESTING , do_weather  , 0, 0 },
  { "누구"      , "wh"      , POS_DEAD    , do_who      , 0, 0 },
  { "자세히"    , "whoi"    , POS_DEAD    , do_whois    , 0, 0 }, // whois // whois
  { "whoami"   , "whoami"  , POS_DEAD    , do_gen_ps   , 0, SCMD_WHOAMI },
  { "어디"    , "where"   , POS_RESTING , do_where    , 1, 0 },
  { "whirlwind", "whirl"   , POS_FIGHTING, do_whirlwind, 0, 0 },
  { "속삭여"  , "whisper" , POS_RESTING , do_spec_comm, 0, SCMD_WHISPER }, // whisper
  { "무장"    , "wie"     , POS_RESTING , do_wield    , 0, 0 }, // wield
  { "출금" , "withdraw", POS_STANDING, do_not_here , 1, 0 }, // withdraw
  { "신채널"   , "wiz"     , POS_DEAD    , do_wiznet   , LVL_IMMORT, 0 }, // wiznet
  { ";"        , ";"       , POS_DEAD    , do_wiznet   , LVL_IMMORT, 0 },
  { "운영명령"  , "wizhelp" , POS_DEAD    , do_wizhelp  , LVL_IMMORT, 0 }, // wizhelp
  { "wizlist"  , "wizlist" , POS_DEAD    , do_gen_ps   , 0, SCMD_WIZLIST },
  { "wizupdate", "wizupde" , POS_DEAD    , do_wizupdate, LVL_GRGOD, 0 },
  { "wizlock"  , "wizlock" , POS_DEAD    , do_wizlock  , LVL_IMPL, 0 },
  { "write"    , "write"   , POS_STANDING, do_write    , 1, 0 },

  { "zoneresets", "zoner" ,  POS_DEAD    , do_gen_tog , LVL_IMPL, SCMD_ZONERESETS },
  { "존리셋"   , "zreset"  , POS_DEAD    , do_zreset   , LVL_BUILDER, 0 }, // zreset
  { "존편집"    , "zedit"   , POS_DEAD    , do_oasis_zedit, LVL_BUILDER, 0 },
  { "존목록"    , "zlist"   , POS_DEAD    , do_oasis_list, LVL_BUILDER, SCMD_OASIS_ZLIST },
  { "존청소"   , "zpurge"  , POS_DEAD    , do_zpurge   , LVL_BUILDER, 0 }, // zpurge
  { "zlock"    , "zlock"   , POS_DEAD    , do_zlock    , LVL_GOD, 0 },
  { "zunlock"  , "zunlock" , POS_DEAD    , do_zunlock  , LVL_GOD, 0 },
  { "zcheck"   , "zcheck"  , POS_DEAD    , do_zcheck   , LVL_BUILDER, 0 },

  { "\n", "zzzzzzz", 0, 0, 0, 0 } };    /* this must be last */


  /* Thanks to Melzaren for this change to allow DG Scripts to be attachable
   *to player's while still disallowing them to manually use the DG-Commands. */
static const struct mob_script_command_t mob_script_commands[] = {

  /* DG trigger commands. minimum_level should be set to -1. */
  { "masound"  , do_masound  , 0 },
  { "mkill"    , do_mkill    , 0 },
  { "mjunk"    , do_mjunk    , 0 },
  { "mdamage"  , do_mdamage  , 0 },
  { "mdoor"    , do_mdoor    , 0 },
  { "mecho"    , do_mecho    , 0 },
  { "mrecho"   , do_mrecho   , 0 },
  { "mechoaround", do_mechoaround , 0 },
  { "msend"    , do_msend    , 0 },
  { "mload"    , do_mload    , 0 },
  { "mpurge"   , do_mpurge   , 0 },
  { "mgoto"    , do_mgoto    , 0 },
  { "mat"      , do_mat      , 0 },
  { "mteleport", do_mteleport, 0 },
  { "mforce"   , do_mforce   , 0 },
  { "mhunt"    , do_mhunt    , 0 },
  { "mremember", do_mremember, 0 },
  { "mforget"  , do_mforget  , 0 },
  { "mtransform", do_mtransform , 0 },
  { "mzoneecho", do_mzoneecho, 0 },
  { "mfollow"  , do_mfollow  , 0 },
  { "mlog"     , do_mlog     , 0 },
  { "\n" , do_not_here , 0 } };

int script_command_interpreter(struct char_data *ch, char *arg) {
  /* DG trigger commands */

  int i;
  char first_arg[MAX_INPUT_LENGTH];
  char *line;

  skip_spaces(&arg);
  if (!*arg)
  return 0;

  line = any_one_arg(arg, first_arg);

  for (i = 0; *mob_script_commands[i].command_name != '\n'; i++)
  if (!str_cmp(first_arg, mob_script_commands[i].command_name))
  break; // NB - only allow full matches.

  if (*mob_script_commands[i].command_name == '\n')
  return 0; // no matching commands.

  /* Poiner to the command? */
  ((*mob_script_commands[i].command_pointer) (ch, line, 0,
  mob_script_commands[i].subcmd));
  return 1; // We took care of execution. Let caller know.
}

static const char *fill[] =
{
  "in",
  "from",
  "with",
  "the",
  "on",
  "at",
  "to",
  "\n"
};

static const char *reserved[] =
{
  "a",
  "an",
  "self",
  "me",
  "all",
  "room",
  "someone",
  "something",
  "\n"
};

static int sort_commands_helper(const void *a, const void *b)
{
  return strcmp(complete_cmd_info[*(const int *)a].sort_as,
                complete_cmd_info[*(const int *)b].sort_as);
}

void sort_commands(void)
{
  int a, num_of_cmds = 0;

  while (complete_cmd_info[num_of_cmds].command[0] != '\n')
    num_of_cmds++;
  num_of_cmds++;  /* \n */

  CREATE(cmd_sort_info, int, num_of_cmds);

  for (a = 0; a < num_of_cmds; a++)
    cmd_sort_info[a] = a;

  /* Don't sort the RESERVED or \n entries. */
  qsort(cmd_sort_info + 1, num_of_cmds - 2, sizeof(int), sort_commands_helper);
}


/* This is the actual command interpreter called from game_loop() in comm.c
 * It makes sure you are the proper level and position to execute the command,
 * then calls the appropriate function. */
void command_interpreter(struct char_data *ch, char *argument)
{
  int cmd, length;
  char *line;
  char arg[MAX_INPUT_LENGTH];
  // 한글처리 : 어순변경
  char hanparse[MAX_INPUT_LENGTH];
  char hancommand[MAX_INPUT_LENGTH];
  
  REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);

  /* just drop to next line for hitting CR */
  skip_spaces(&argument);
  if (!*argument)
    return;

  // 한글처리 : 어순변경
  sprintf(hanparse, "%s", argument);
  if(strrchr(hanparse, ' ')) {
    sprintf(hancommand, "%s ", strrchr(hanparse, ' '));
    hanparse[strlen(hanparse) - (strlen(hancommand) - 1)] = '\0';
    strcat(hancommand, hanparse);
    argument = hancommand;
  } else
    argument = hanparse; 

  skip_spaces(&argument);

  /* special case to handle one-character, non-alphanumeric commands; requested
   * by many people so "'hi" or ";godnet test" is possible. Patch sent by Eric
   * Green and Stefan Wasilewski. */
// 한글처리
  if (!ishanalp(*argument)) { 
//  if (!isalpha(*argument)) {
    arg[0] = argument[0];
    arg[1] = '\0';
    line = argument + 1;
  } else
    line = any_one_arg(argument, arg);

  /* Since all command triggers check for valid_dg_target before acting, the levelcheck
   * here has been removed. Otherwise, find the command. */
  {
    int cont;                                            /* continue the command checks */
    cont = command_wtrigger(ch, arg, line);              /* any world triggers ? */
    if (!cont) cont = command_mtrigger(ch, arg, line);   /* any mobile triggers ? */
    if (!cont) cont = command_otrigger(ch, arg, line);   /* any object triggers ? */
    if (cont) return;                                    /* yes, command trigger took over */
  }

  /* Allow IMPLs to switch into mobs to test the commands. */
   if (IS_NPC(ch) && ch->desc && GET_LEVEL(ch->desc->original) >= LVL_IMPL) {
     if (script_command_interpreter(ch, argument))
       return;
   }

  for (length = strlen(arg), cmd = 0; *complete_cmd_info[cmd].command != '\n'; cmd++)
    if(complete_cmd_info[cmd].command_pointer != do_action &&
       !strncmp(complete_cmd_info[cmd].command, arg, length))
      if (GET_LEVEL(ch) >= complete_cmd_info[cmd].minimum_level)
        break;

  /* it's not a 'real' command, so it's a social */

  if(*complete_cmd_info[cmd].command == '\n')
    for (length = strlen(arg), cmd = 0; *complete_cmd_info[cmd].command != '\n'; cmd++)
      if (complete_cmd_info[cmd].command_pointer == do_action &&
          !strncmp(complete_cmd_info[cmd].command, arg, length))
        if (GET_LEVEL(ch) >= complete_cmd_info[cmd].minimum_level)
          break;

  if (*complete_cmd_info[cmd].command == '\n') {
    int found = 0;
    send_to_char(ch, "%s", CONFIG_HUH);

    for (cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
    {
      if (*arg != *cmd_info[cmd].command || cmd_info[cmd].minimum_level > GET_LEVEL(ch))
        continue;

      /* Only apply levenshtein counts if the command is not a trigger command. */
      if ( (levenshtein_distance(arg, cmd_info[cmd].command) <= 2) &&
           (cmd_info[cmd].minimum_level >= 0) )
      {
        if (!found)
        {
          send_to_char(ch, "\r\nDid you mean:\r\n");
          found = 1;
        }
        send_to_char(ch, "  %s\r\n", cmd_info[cmd].command);
      }
    }
  }
  else if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_FROZEN) && GET_LEVEL(ch) < LVL_IMPL)
    send_to_char(ch, "You try, but the mind-numbing cold prevents you...\r\n");
  else if (complete_cmd_info[cmd].command_pointer == NULL)
    send_to_char(ch, "Sorry, that command hasn't been implemented yet.\r\n");
  else if (IS_NPC(ch) && complete_cmd_info[cmd].minimum_level >= LVL_IMMORT)
    send_to_char(ch, "You can't use immortal commands while switched.\r\n");
  else if (GET_POS(ch) < complete_cmd_info[cmd].minimum_position)
    switch (GET_POS(ch)) {
    case POS_DEAD:
      send_to_char(ch, "Lie still; you are DEAD!!! :-(\r\n");
      break;
    case POS_INCAP:
    case POS_MORTALLYW:
      send_to_char(ch, "You are in a pretty bad shape, unable to do anything!\r\n");
      break;
    case POS_STUNNED:
      send_to_char(ch, "All you can do right now is think about the stars!\r\n");
      break;
    case POS_SLEEPING:
      send_to_char(ch, "In your dreams, or what?\r\n");
      break;
    case POS_RESTING:
      send_to_char(ch, "Nah... You feel too relaxed to do that..\r\n");
      break;
    case POS_SITTING:
      send_to_char(ch, "Maybe you should get on your feet first?\r\n");
      break;
    case POS_FIGHTING:
      send_to_char(ch, "No way!  You're fighting for your life!\r\n");
      break;
  } else if (no_specials || !special(ch, cmd, line))
    ((*complete_cmd_info[cmd].command_pointer) (ch, line, cmd, complete_cmd_info[cmd].subcmd));
}

/* Routines to handle aliasing. */
static struct alias_data *find_alias(struct alias_data *alias_list, char *str)
{
  while (alias_list != NULL) {
    if (*str == *alias_list->alias)	/* hey, every little bit counts :-) */
      if (!strcmp(str, alias_list->alias))
	return (alias_list);

    alias_list = alias_list->next;
  }

  return (NULL);
}

void free_alias(struct alias_data *a)
{
  if (a->alias)
    free(a->alias);
  if (a->replacement)
    free(a->replacement);
  free(a);
}

/* The interface to the outside world: do_alias */
ACMD(do_alias)
{
  char arg[MAX_INPUT_LENGTH];
  char *repl;
  struct alias_data *a, *temp;

  if (IS_NPC(ch))
    return;

  repl = any_one_arg(argument, arg);

  if (!*arg) {			/* no argument specified -- list currently defined aliases */
    send_to_char(ch, "Currently defined aliases:\r\n");
    if ((a = GET_ALIASES(ch)) == NULL)
      send_to_char(ch, " None.\r\n");
    else {
      while (a != NULL) {
	send_to_char(ch, "%-15s %s\r\n", a->alias, a->replacement);
	a = a->next;
      }
    }
  } else {			/* otherwise, add or remove aliases */
    /* is this an alias we've already defined? */
    if ((a = find_alias(GET_ALIASES(ch), arg)) != NULL) {
      REMOVE_FROM_LIST(a, GET_ALIASES(ch), next);
      free_alias(a);
    }
    /* if no replacement string is specified, assume we want to delete */
    if (!*repl) {
      if (a == NULL)
	send_to_char(ch, "No such alias.\r\n");
      else
	send_to_char(ch, "Alias deleted.\r\n");
    } else {			/* otherwise, either add or redefine an alias */
      if (!str_cmp(arg, "alias")) {
	send_to_char(ch, "You can't alias 'alias'.\r\n");
	return;
      }
      CREATE(a, struct alias_data, 1);
      a->alias = strdup(arg);
      delete_doubledollar(repl);
      a->replacement = strdup(repl);
      if (strchr(repl, ALIAS_SEP_CHAR) || strchr(repl, ALIAS_VAR_CHAR))
	a->type = ALIAS_COMPLEX;
      else
	a->type = ALIAS_SIMPLE;
      a->next = GET_ALIASES(ch);
      GET_ALIASES(ch) = a;
      save_char(ch);
      send_to_char(ch, "Alias ready.\r\n");
    }
  }
}

/* Valid numeric replacements are only $1 .. $9 (makes parsing a little easier,
 * and it's not that much of a limitation anyway.)  Also valid is "$*", which
 * stands for the entire original line after the alias. ";" is used to delimit
 * commands. */
#define NUM_TOKENS       9

static void perform_complex_alias(struct txt_q *input_q, char *orig, struct alias_data *a)
{
  struct txt_q temp_queue;
  char *tokens[NUM_TOKENS], *temp, *write_point;
  char buf2[MAX_RAW_INPUT_LENGTH], buf[MAX_RAW_INPUT_LENGTH];	/* raw? */
  int num_of_tokens = 0, num;

  /* First, parse the original string */
  strcpy(buf2, orig);	/* strcpy: OK (orig:MAX_INPUT_LENGTH < buf2:MAX_RAW_INPUT_LENGTH) */
  temp = strtok(buf2, " ");
  while (temp != NULL && num_of_tokens < NUM_TOKENS) {
    tokens[num_of_tokens++] = temp;
    temp = strtok(NULL, " ");
  }

  /* initialize */
  write_point = buf;
  temp_queue.head = temp_queue.tail = NULL;

  /* now parse the alias */
  for (temp = a->replacement; *temp; temp++) {
    if (*temp == ALIAS_SEP_CHAR) {
      *write_point = '\0';
      buf[MAX_INPUT_LENGTH - 1] = '\0';
      write_to_q(buf, &temp_queue, 1);
      write_point = buf;
    } else if (*temp == ALIAS_VAR_CHAR) {
      temp++;
      if ((num = *temp - '1') < num_of_tokens && num >= 0) {
	strcpy(write_point, tokens[num]);	/* strcpy: OK */
	write_point += strlen(tokens[num]);
      } else if (*temp == ALIAS_GLOB_CHAR) {
        skip_spaces(&orig);
        strcpy(write_point, orig);		/* strcpy: OK */
	write_point += strlen(orig);
      } else if ((*(write_point++) = *temp) == '$')	/* redouble $ for act safety */
	*(write_point++) = '$';
    } else
      *(write_point++) = *temp;
  }

  *write_point = '\0';
  buf[MAX_INPUT_LENGTH - 1] = '\0';
  write_to_q(buf, &temp_queue, 1);

  /* push our temp_queue on to the _front_ of the input queue */
  if (input_q->head == NULL)
    *input_q = temp_queue;
  else {
    temp_queue.tail->next = input_q->head;
    input_q->head = temp_queue.head;
  }
}

/* Given a character and a string, perform alias replacement on it.
 * Return values:
 *   0: String was modified in place; call command_interpreter immediately.
 *   1: String was _not_ modified in place; rather, the expanded aliases
 *      have been placed at the front of the character's input queue. */
int perform_alias(struct descriptor_data *d, char *orig, size_t maxlen)
{
  char first_arg[MAX_INPUT_LENGTH], *ptr;
  struct alias_data *a, *tmp;

  /* Mobs don't have alaises. */
  if (IS_NPC(d->character))
    return (0);

  /* bail out immediately if the guy doesn't have any aliases */
  if ((tmp = GET_ALIASES(d->character)) == NULL)
    return (0);

  /* find the alias we're supposed to match */
  ptr = any_one_arg(orig, first_arg);

  /* bail out if it's null */
  if (!*first_arg)
    return (0);

  /* if the first arg is not an alias, return without doing anything */
  if ((a = find_alias(tmp, first_arg)) == NULL)
    return (0);

  if (a->type == ALIAS_SIMPLE) {
    strlcpy(orig, a->replacement, maxlen);
    return (0);
  } else {
    perform_complex_alias(&d->input, ptr, a);
    return (1);
  }
}

/* Various other parsing utilities. */

/* Searches an array of strings for a target string.  "exact" can be 0 or non-0,
 * depending on whether or not the match must be exact for it to be returned.
 * Returns -1 if not found; 0..n otherwise.  Array must be terminated with a
 * '\n' so it knows to stop searching. */
int search_block(char *arg, const char **list, int exact)
{
  int i, l;

  /*  We used to have \r as the first character on certain array items to
   *  prevent the explicit choice of that point.  It seems a bit silly to
   *  dump control characters into arrays to prevent that, so we'll just
   *  check in here to see if the first character of the argument is '!',
   *  and if so, just blindly return a '-1' for not found. - ae. */
  if (*arg == '!')
    return (-1);

  /* Make into lower case, and get length of string */
  for (l = 0; *(arg + l); l++)
    *(arg + l) = LOWER(*(arg + l));

  if (exact) {
    for (i = 0; **(list + i) != '\n'; i++)
      if (!strcmp(arg, *(list + i)))
	return (i);
  } else {
    if (!l)
      l = 1;			/* Avoid "" to match the first available
				 * string */
    for (i = 0; **(list + i) != '\n'; i++)
      if (!strncmp(arg, *(list + i), l))
	return (i);
  }

  return (-1);
}

int is_number(const char *str)
{
  if(*str == '-')
     str++;
  if(!*str)
     return (0);
  while (*str)
    if (!isdigit(*(str++)))
      return (0);

  return (1);
}

/* Function to skip over the leading spaces of a string. */
void skip_spaces(char **string)
{
 // for (; **string && **string != '\t' && isspace(**string); (*string)++);
for (; **string && isnhspace(**string); (*string)++);
}

/* Given a string, change all instances of double dollar signs ($$) to single
 * dollar signs ($).  When strings come in, all $'s are changed to $$'s to
 * avoid having users be able to crash the system if the inputted string is
 * eventually sent to act().  If you are using user input to produce screen
 * output AND YOU ARE SURE IT WILL NOT BE SENT THROUGH THE act() FUNCTION
 * (i.e., do_gecho, do_title, but NOT do_say), you can call
 * delete_doubledollar() to make the output look correct.
 * Modifies the string in-place. */
char *delete_doubledollar(char *string)
{
  char *ddread, *ddwrite;

  /* If the string has no dollar signs, return immediately */
  if ((ddwrite = strchr(string, '$')) == NULL)
    return (string);

  /* Start from the location of the first dollar sign */
  ddread = ddwrite;


  while (*ddread)   /* Until we reach the end of the string... */
    if ((*(ddwrite++) = *(ddread++)) == '$') /* copy one char */
      if (*ddread == '$')
	ddread++; /* skip if we saw 2 $'s in a row */

  *ddwrite = '\0';

  return (string);
}

int fill_word(char *argument)
{
  return (search_block(argument, fill, TRUE) >= 0);
}

int reserved_word(char *argument)
{
  return (search_block(argument, reserved, TRUE) >= 0);
}

/* Copy the first non-fill-word, space-delimited argument of 'argument'
 * to 'first_arg'; return a pointer to the remainder of the string. */
char *one_argument(char *argument, char *first_arg)
{
  char *begin = first_arg;

  if (!argument) {
    log("SYSERR: one_argument received a NULL pointer!");
    *first_arg = '\0';
    return (NULL);
  }

  do {
    skip_spaces(&argument);

    first_arg = begin;
    while (*argument && !isspace(*argument)) {
      *(first_arg++) = LOWER(*argument);
      argument++;
    }

    *first_arg = '\0';
  } while (fill_word(begin));

  return (argument);
}

/* one_word is like any_one_arg, except that words in quotes ("") are
 * considered one word. No longer ignores fill words.  -dak */
char *one_word(char *argument, char *first_arg)
{
    skip_spaces(&argument);

    if (*argument == '\"') {
      argument++;
      while (*argument && *argument != '\"') {
        *(first_arg++) = LOWER(*argument);
        argument++;
      }
      argument++;
    } else {
      while (*argument && !isspace(*argument)) {
        *(first_arg++) = LOWER(*argument);
        argument++;
      }
    }

    *first_arg = '\0';
  return (argument);
}

/* Same as one_argument except that it doesn't ignore fill words. */
char *any_one_arg(char *argument, char *first_arg)
{
  skip_spaces(&argument);

  while (*argument && !isspace(*argument)) {
    *(first_arg++) = LOWER(*argument);
    argument++;
  }

  *first_arg = '\0';

  return (argument);
}

/* Same as one_argument except that it takes two args and returns the rest;
 * ignores fill words */
char *two_arguments(char *argument, char *first_arg, char *second_arg)
{
  return (one_argument(one_argument(argument, first_arg), second_arg)); /* :-) */
}

/* Determine if a given string is an abbreviation of another.
 * Returns 1 if arg1 is an abbreviation of arg2. */
int is_abbrev(const char *arg1, const char *arg2)
{
  if (!*arg1)
    return (0);

  for (; *arg1 && *arg2; arg1++, arg2++)
    if (LOWER(*arg1) != LOWER(*arg2))
      return (0);

  if (!*arg1)
    return (1);
  else
    return (0);
}

/* Return first space-delimited token in arg1; remainder of string in arg2.
 * NOTE: Requires sizeof(arg2) >= sizeof(string) */
void half_chop(char *string, char *arg1, char *arg2)
{
  char *temp;

  temp = any_one_arg(string, arg1);
  skip_spaces(&temp);
  strcpy(arg2, temp);	/* strcpy: OK (documentation) */
}

/* Used in specprocs, mostly.  (Exactly) matches "command" to cmd number */
int find_command(const char *command)
{
  int cmd;

  for (cmd = 0; *complete_cmd_info[cmd].command != '\n'; cmd++)
    if (!strcmp(complete_cmd_info[cmd].command, command))
      return (cmd);

  return (-1);
}

int special(struct char_data *ch, int cmd, char *arg)
{
  struct obj_data *i;
  struct char_data *k;
  int j;

  /* special in room? */
  if (GET_ROOM_SPEC(IN_ROOM(ch)) != NULL)
    if (GET_ROOM_SPEC(IN_ROOM(ch)) (ch, world + IN_ROOM(ch), cmd, arg))
      return (1);

  /* special in equipment list? */
  for (j = 0; j < NUM_WEARS; j++)
    if (GET_EQ(ch, j) && GET_OBJ_SPEC(GET_EQ(ch, j)) != NULL)
      if (GET_OBJ_SPEC(GET_EQ(ch, j)) (ch, GET_EQ(ch, j), cmd, arg))
	return (1);

  /* special in inventory? */
  for (i = ch->carrying; i; i = i->next_content)
    if (GET_OBJ_SPEC(i) != NULL)
      if (GET_OBJ_SPEC(i) (ch, i, cmd, arg))
	return (1);

  /* special in mobile present? */
  for (k = world[IN_ROOM(ch)].people; k; k = k->next_in_room)
    if (!MOB_FLAGGED(k, MOB_NOTDEADYET))
      if (GET_MOB_SPEC(k) && GET_MOB_SPEC(k) (ch, k, cmd, arg))
	return (1);

  /* special in object present? */
  for (i = world[IN_ROOM(ch)].contents; i; i = i->next_content)
    if (GET_OBJ_SPEC(i) != NULL)
      if (GET_OBJ_SPEC(i) (ch, i, cmd, arg))
	return (1);

  return (0);
}

/* Stuff for controlling the non-playing sockets (get name, pwd etc).
 * This function needs to die. */
static int _parse_name(char *arg, char *name)
{
  int i;

  //skip_spaces(&arg);
  for (; isnhspace(*arg); arg++);

  for (i = 0; (*name = *arg); arg++, i++, name++)
    //if (!isalpha(*arg)) 
	if (!ishanalp(*arg))
      return (1);

  if (!i)
    return (1);

  return (0);
}

#define RECON		1
#define USURP		2
#define UNSWITCH	3

/* This function seems a bit over-extended. */
static int perform_dupe_check(struct descriptor_data *d)
{
  struct descriptor_data *k, *next_k;
  struct char_data *target = NULL, *ch, *next_ch;
  int mode = 0;
  int pref_temp = 0; /* for "last" log */
  int id = GET_IDNUM(d->character);

  /* Now that this descriptor has successfully logged in, disconnect all
   * other descriptors controlling a character with the same ID number. */

  for (k = descriptor_list; k; k = next_k) {
    next_k = k->next;

    if (k == d)
      continue;

    if (k->original && (GET_IDNUM(k->original) == id)) {
      /* Original descriptor was switched, booting it and restoring normal body control. */

      write_to_output(d, "\r\nMultiple login detected -- disconnecting.\r\n");
      STATE(k) = CON_CLOSE;
      pref_temp=GET_PREF(k->character);
      if (!target) {
	target = k->original;
	mode = UNSWITCH;
      }
      if (k->character)
	k->character->desc = NULL;
      k->character = NULL;
      k->original = NULL;
    } else if (k->character && GET_IDNUM(k->character) == id && k->original) {
      /* Character taking over their own body, while an immortal was switched to it. */

      do_return(k->character, NULL, 0, 0);
    } else if (k->character && GET_IDNUM(k->character) == id) {
      /* Character taking over their own body. */
      pref_temp=GET_PREF(k->character);

      if (!target && STATE(k) == CON_PLAYING) {
	write_to_output(k, "\r\nThis body has been usurped!\r\n");
	target = k->character;
	mode = USURP;
      }
      k->character->desc = NULL;
      k->character = NULL;
      k->original = NULL;
      write_to_output(k, "\r\nMultiple login detected -- disconnecting.\r\n");
      STATE(k) = CON_CLOSE;
    }
  }

 /* Now, go through the character list, deleting all characters that are not
  * already marked for deletion from the above step (i.e., in the CON_HANGUP
  * state), and have not already been selected as a target for switching into.
  * In addition, if we haven't already found a target, choose one if one is
  * available (while still deleting the other duplicates, though theoretically
  * none should be able to exist). */
  for (ch = character_list; ch; ch = next_ch) {
    next_ch = ch->next;

    if (IS_NPC(ch))
      continue;
    if (GET_IDNUM(ch) != id)
      continue;

    /* ignore chars with descriptors (already handled by above step) */
    if (ch->desc)
      continue;

    /* don't extract the target char we've found one already */
    if (ch == target)
      continue;

    /* we don't already have a target and found a candidate for switching */
    if (!target) {
      target = ch;
      mode = RECON;
      pref_temp = GET_PREF(ch);
      continue;
    }

    /* we've found a duplicate - blow him away, dumping his eq in limbo. */
    if (IN_ROOM(ch) != NOWHERE)
      char_from_room(ch);
    char_to_room(ch, 1);
    extract_char(ch);
  }

  /* no target for switching into was found - allow login to continue */
  if (!target) {
    GET_PREF(d->character) = rand_number(1, 128000);
    if (GET_HOST(d->character))
      free(GET_HOST(d->character));
    GET_HOST(d->character) = strdup(d->host);
    return 0;
  }

  if (GET_HOST(target)) free(GET_HOST(target));
  GET_HOST(target) = strdup(d->host);

  GET_PREF(target) = pref_temp;
  add_llog_entry(target, LAST_RECONNECT);

  /* Okay, we've found a target.  Connect d to target. */
  free_char(d->character); /* get rid of the old char */
  d->character = target;
  d->character->desc = d;
  d->original = NULL;
  d->character->char_specials.timer = 0;
  REMOVE_BIT_AR(PLR_FLAGS(d->character), PLR_MAILING);
  REMOVE_BIT_AR(PLR_FLAGS(d->character), PLR_WRITING);
  STATE(d) = CON_PLAYING;
  MXPSendTag( d, "<VERSION>" );

  switch (mode) {
  case RECON:
    write_to_output(d, "Reconnecting.\r\n");
    act("$n has reconnected.", TRUE, d->character, 0, 0, TO_ROOM);
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(d->character)), TRUE, "%s [%s] has reconnected.", GET_NAME(d->character), d->host);
    if (has_mail(GET_IDNUM(d->character)))
      write_to_output(d, "You have mail waiting.\r\n");
    break;
  case USURP:
    write_to_output(d, "You take over your own body, already in use!\r\n");
    act("$n suddenly keels over in pain, surrounded by a white aura...\r\n"
	"$n's body has been taken over by a new spirit!",
	TRUE, d->character, 0, 0, TO_ROOM);
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(d->character)), TRUE,
	"%s has re-logged in ... disconnecting old socket.", GET_NAME(d->character));
    break;
  case UNSWITCH:
    write_to_output(d, "Reconnecting to unswitched char.");
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(d->character)), TRUE, "%s [%s] has reconnected.", GET_NAME(d->character), d->host);
    break;
  }

  return (1);
}

/* New Char dupe-check called at the start of character creation */
static bool perform_new_char_dupe_check(struct descriptor_data *d)
{
  struct descriptor_data *k, *next_k;
  bool found = FALSE;

  /* Now that this descriptor has successfully logged in, disconnect all
   * other descriptors controlling a character with the same ID number. */

  for (k = descriptor_list; k; k = next_k) {
    next_k = k->next;

    if (k == d)
      continue;

   if (k->character == NULL)
      continue;

    /* Do the player names match? */
    if (!strcmp(GET_NAME(k->character), GET_NAME(d->character))) {
      /* Check the other character is still in creation? */
      if ((STATE(k) > CON_PLAYING) && (STATE(k) < CON_QCLASS)) {
        /* Boot the older one */
        k->character->desc = NULL;
        k->character = NULL;
        k->original = NULL;
        write_to_output(k, "\r\nMultiple login detected -- disconnecting.\r\n");
        STATE(k) = CON_CLOSE;

        mudlog(NRM, LVL_GOD, TRUE, "Multiple logins detected in char creation for %s.", GET_NAME(d->character));

        found = TRUE;
      } else {
        /* Something went VERY wrong, boot both chars */
        k->character->desc = NULL;
        k->character = NULL;
        k->original = NULL;
        write_to_output(k, "\r\nMultiple login detected -- disconnecting.\r\n");
        STATE(k) = CON_CLOSE;

        d->character->desc = NULL;
        d->character = NULL;
        d->original = NULL;
        write_to_output(d, "\r\nSorry, due to multiple connections, all your connections are being closed.\r\n");
        write_to_output(d, "\r\nPlease reconnect.\r\n");
        STATE(d) = CON_CLOSE;

        mudlog(NRM, LVL_GOD, TRUE, "SYSERR: Multiple logins with 1st in-game and the 2nd in char creation.");

        found = TRUE;
      }
    }
  }
  return (found);
}

/* load the player, put them in the right room - used by copyover_recover too */
int enter_player_game (struct descriptor_data *d)
{
  int load_result;
  room_vnum load_room;

  reset_char(d->character);

  if (PLR_FLAGGED(d->character, PLR_INVSTART))
    GET_INVIS_LEV(d->character) = GET_LEVEL(d->character);

  /* We have to place the character in a room before equipping them
   * or equip_char() will gripe about the person in NOWHERE. */
  if ((load_room = GET_LOADROOM(d->character)) != NOWHERE)
    load_room = real_room(load_room);

  /* If char was saved with NOWHERE, or real_room above failed... */
  if (load_room == NOWHERE) {
    if (GET_LEVEL(d->character) >= LVL_IMMORT)
      load_room = r_immort_start_room;
    else
      load_room = r_mortal_start_room;
  }

  if (PLR_FLAGGED(d->character, PLR_FROZEN))
    load_room = r_frozen_start_room;

  /* copyover */
  d->character->script_id = GET_IDNUM(d->character);
  /* find_char helper */
  add_to_lookup_table(d->character->script_id, (void *)d->character);

  /* After moving saving of variables to the player file, this should only
   * be called in case nothing was found in the pfile. If something was
   * found, SCRIPT(ch) will be set. */
  if (!SCRIPT(d->character))
    read_saved_vars(d->character);

  d->character->next = character_list;
  character_list = d->character;
  char_to_room(d->character, load_room);
  load_result = Crash_load(d->character);
  
  /* Save the character and their object file */
  save_char(d->character);
  Crash_crashsave(d->character);

  /* Check for a login trigger in the players' start room */
  login_wtrigger(&world[IN_ROOM(d->character)], d->character);

  return load_result;
}

EVENTFUNC(get_protocols)
{
  struct descriptor_data *d;
  struct mud_event_data *pMudEvent;
  char buf[MAX_STRING_LENGTH];
  size_t len;

  if (event_obj == NULL)
    return 0;
  
  pMudEvent = (struct mud_event_data *) event_obj;
  d = (struct descriptor_data *) pMudEvent->pStruct;  
  
  /* Clear extra white space from the "protocol scroll" */
  write_to_output(d, "[H[J");

  len = snprintf(buf, MAX_STRING_LENGTH,   "\tO[\toClient\tO] \tw%s\tn | ", d->pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);

  if (d->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt)
    len += snprintf(buf + len, MAX_STRING_LENGTH - len, "\tO[\toColors\tO] \tw256\tn | ");
  else if (d->pProtocol->pVariables[eMSDP_ANSI_COLORS]->ValueInt)
    len += snprintf(buf + len, MAX_STRING_LENGTH - len, "\tO[\toColors\tO] \twAnsi\tn | ");
  else
    len += snprintf(buf + len, MAX_STRING_LENGTH - len, "[Colors] No Color | ");
 
  len += snprintf(buf + len, MAX_STRING_LENGTH - len,   "\tO[\toMXP\tO] \tw%s\tn | ", d->pProtocol->bMXP ? "Yes" : "No");
  len += snprintf(buf + len, MAX_STRING_LENGTH - len,   "\tO[\toMSDP\tO] \tw%s\tn | ", d->pProtocol->bMSDP ? "Yes" : "No");
  snprintf(buf + len, MAX_STRING_LENGTH - len,   "\tO[\toATCP\tO] \tw%s\tn\r\n\r\n", d->pProtocol->bATCP ? "Yes" : "No");
   
  write_to_output(d, buf, 0);
    
  write_to_output(d, GREETINGS, 0); 
  STATE(d) = CON_GET_NAME;
  return 0;
}

/* deal with newcomers and other non-playing sockets */
void nanny(struct descriptor_data *d, char *arg)
{
  int load_result;	/* Overloaded variable */
  int player_i;

  /* OasisOLC states */
  struct {
    int state;
    void (*func)(struct descriptor_data *, char *);
  } olc_functions[] = {
    { CON_OEDIT, oedit_parse },
    { CON_ZEDIT, zedit_parse },
    { CON_SEDIT, sedit_parse },
    { CON_MEDIT, medit_parse },
    { CON_REDIT, redit_parse },
    { CON_CEDIT, cedit_parse },
    { CON_TRIGEDIT, trigedit_parse },
    { CON_AEDIT, aedit_parse },
    { CON_HEDIT, hedit_parse },
    { CON_QEDIT, qedit_parse },
    { CON_PREFEDIT, prefedit_parse },
    { CON_IBTEDIT, ibtedit_parse },
    { CON_MSGEDIT, msgedit_parse },
    { -1, NULL }
  };

  skip_spaces(&arg);

  /* Quick check for the OLC states. */
  for (player_i = 0; olc_functions[player_i].state >= 0; player_i++)
    if (STATE(d) == olc_functions[player_i].state) {
      (*olc_functions[player_i].func)(d, arg);
      return;
    }

  /* Not in OLC. */
  switch (STATE(d)) {
  case CON_GET_PROTOCOL:
    write_to_output(d, "Collecting Protocol Information... Please Wait.\r\n"); 
    return;
  case CON_GET_NAME:		/* wait for input of name */
    if (d->character == NULL) {
      CREATE(d->character, struct char_data, 1);
      clear_char(d->character);
      CREATE(d->character->player_specials, struct player_special_data, 1);
      
      new_mobile_data(d->character);
      
      GET_HOST(d->character) = strdup(d->host);
      d->character->desc = d;
    }
    if (!*arg)
      STATE(d) = CON_CLOSE;
    else {
      char buf[MAX_INPUT_LENGTH], tmp_name[MAX_INPUT_LENGTH];

//      if ((_parse_name(arg, tmp_name)) || strlen(tmp_name) < 2 ||
//       strlen(tmp_name) > MAX_NAME_LENGTH || !valid_name(tmp_name) ||
//       fill_word(strcpy(buf, tmp_name)) || reserved_word(buf)) {	/* strcpy: OK (mutual MAX_INPUT_LENGTH) */
//          write_to_output(d, "Invalid name, please try another.\r\nName: ");
//          return;
//     }

	  if ((_parse_name(arg, tmp_name)) || (strlen(tmp_name) < 4) || !is_han(tmp_name) ||
         strlen(tmp_name) > MAX_NAME_LENGTH || !valid_name(tmp_name) || 
		 fill_word(strcpy(buf, tmp_name)) || reserved_word(buf)) {
        write_to_output(d, "1.Invalid name, please try another.rnName: ");
        return;
      } 
	  
/*	  	  if (_parse_name(arg, tmp_name)) {
        write_to_output(d, "1.Invalid name, please try another.rnName: ");
        return;
      }
	  
	  	  if (strlen(tmp_name) < 4) {
        write_to_output(d, "2.Invalid name, please try another.rnName: ");
        return;
      }
	  
	  	  if (!is_han(tmp_name)) {
        write_to_output(d, "3.Invalid name, please try another.rnName: ");
        return;
      }
	  
	  	  if (strlen(tmp_name) > MAX_NAME_LENGTH) {
        write_to_output(d, "4.Invalid name, please try another.rnName: ");
        return;
      }
	  
	  	  if (!valid_name(tmp_name)) {
        write_to_output(d, "5.Invalid name, please try another.rnName: ");
        return;
      }
	  
	  	  if (fill_word(strcpy(buf, tmp_name))) {
        write_to_output(d, "6.Invalid name, please try another.rnName: ");
        return;
      }
	  
	  	  if (reserved_word(buf)) {
        write_to_output(d, "7.Invalid name, please try another.rnName: ");
        return;
      } */

      if ((player_i = load_char(tmp_name, d->character)) > -1) {
        GET_PFILEPOS(d->character) = player_i;

        if (PLR_FLAGGED(d->character, PLR_DELETED)) {
          /* Make sure old files are removed so the new player doesn't get the
           * deleted player's equipment. */
          if ((player_i = get_ptable_by_name(tmp_name)) >= 0)
            remove_player(player_i);

          /* We get a false positive from the original deleted character. */
          free_char(d->character);

          /* Check for multiple creations. */
          if (!valid_name(tmp_name)) {
            write_to_output(d, "2.Invalid name, please try another.\r\nName: ");
            return;
          }
          CREATE(d->character, struct char_data, 1);
          clear_char(d->character);
          CREATE(d->character->player_specials, struct player_special_data, 1);

          new_mobile_data(d->character);

          if (GET_HOST(d->character))
            free(GET_HOST(d->character));
          GET_HOST(d->character) = strdup(d->host);

          d->character->desc = d;
          CREATE(d->character->player.name, char, strlen(tmp_name) + 1);
          strcpy(d->character->player.name, CAP(tmp_name));	/* strcpy: OK (size checked above) */
          GET_PFILEPOS(d->character) = player_i;
          write_to_output(d, "%s로 새로운 캐릭터를 생성하시겠습니까? (\t(네\t)/\t(아니요\t))?\r\n", tmp_name);
          STATE(d) = CON_NAME_CNFRM;
        } else {
          /* undo it just in case they are set */
          REMOVE_BIT_AR(PLR_FLAGS(d->character), PLR_WRITING);
          REMOVE_BIT_AR(PLR_FLAGS(d->character), PLR_MAILING);
          REMOVE_BIT_AR(PLR_FLAGS(d->character), PLR_CRYO);
          d->character->player.time.logon = time(0);
          write_to_output(d, "패스워드: ");
          echo_off(d);
          d->idle_tics = 0;
          STATE(d) = CON_PASSWORD;
        }
      } else {
        /* player unknown -- make new character */

        /* Check for multiple creations of a character. */
        if (!valid_name(tmp_name)) {
          write_to_output(d, "사용할 수 없는 아이디입니다, 다른 아이디를 입력해주세요\r\n아이디: ");
          return;
        }
        CREATE(d->character->player.name, char, strlen(tmp_name) + 1);
        strcpy(d->character->player.name, CAP(tmp_name));	/* strcpy: OK (size checked above) */

        write_to_output(d, "%s로 새로운 캐릭터를 생성하시겠습니까? (\t(네\t)/\t(아니요\t))?\r\n", tmp_name);
        STATE(d) = CON_NAME_CNFRM;
      }
    }
    break;

  case CON_NAME_CNFRM:		/* wait for conf. of new name    */
    if (!strcmp(arg, "네")) {
      if (isbanned(d->host) >= BAN_NEW) {
	mudlog(NRM, LVL_GOD, TRUE, "Request for new char %s denied from [%s] (siteban)", GET_PC_NAME(d->character), d->host);
	write_to_output(d, "현재 접속중인 곳에서는 캐릭터 생성이 불가능합니다.\r\n");
	STATE(d) = CON_CLOSE;
	return;
      }
      if (circle_restrict) {
	write_to_output(d, "죄송합니다, 현재는 가입할 수 없습니다.\r\n");
	mudlog(NRM, LVL_GOD, TRUE, "Request for new char %s denied from [%s] (wizlock)", GET_PC_NAME(d->character), d->host);
	STATE(d) = CON_CLOSE;
	return;
      }
      perform_new_char_dupe_check(d);
      write_to_output(d, "회원가입을 시작합니다.\r\n%s님의 패스워드를 입력해주세요: ", GET_PC_NAME(d->character));
      echo_off(d);
      STATE(d) = CON_NEWPASSWD;
    } else if (!strcmp(arg, "아니요")) {
      write_to_output(d, "어떤 이름을 사용하시겠습니까?\r\n");
      free(d->character->player.name);
      d->character->player.name = NULL;
      STATE(d) = CON_GET_NAME;
    } else
      write_to_output(d, "네 또는 아니요로 답해주세요:\r\n");
    break;

  case CON_PASSWORD:		/* get pwd for known player      */
    /* To really prevent duping correctly, the player's record should be reloaded
     * from disk at this point (after the password has been typed).  However I'm
     * afraid that trying to load a character over an already loaded character is
     * going to cause some problem down the road that I can't see at the moment.
     * So to compensate, I'm going to (1) add a 15 or 20-second time limit for
     * entering a password, and (2) re-add the code to cut off duplicates when a
     * player quits.  JE 6 Feb 96 */

    echo_on(d);    /* turn echo back on */

    /* New echo_on() eats the return on telnet. Extra space better than none. */
    write_to_output(d, "\r\n");

    if (!*arg)
      STATE(d) = CON_CLOSE;
    else {
      if (strncmp(CRYPT(arg, GET_PASSWD(d->character)), GET_PASSWD(d->character), MAX_PWD_LENGTH)) {
	mudlog(BRF, LVL_GOD, TRUE, "Bad PW: %s [%s]", GET_NAME(d->character), d->host);
	GET_BAD_PWS(d->character)++;
	save_char(d->character);
	if (++(d->bad_pws) >= CONFIG_MAX_BAD_PWS) {	/* 3 strikes and you're out. */
	  write_to_output(d, "패스워드 입력 제한을 초과하였습니다. 접속 해제!\r\n");
	  STATE(d) = CON_CLOSE;
	} else {
	  write_to_output(d, "패스워드가 맞지 않습니다.\r\n패스워드: ");
	  echo_off(d);
	}
	return;
      }

      /* Password was correct. */
      load_result = GET_BAD_PWS(d->character);
      GET_BAD_PWS(d->character) = 0;
      d->bad_pws = 0;

      if (isbanned(d->host) == BAN_SELECT &&
	  !PLR_FLAGGED(d->character, PLR_SITEOK)) {
	write_to_output(d, "죄송합니다, 현재 접속중인 환경에서는 로그인할 수 없습니다!\r\n");
	STATE(d) = CON_CLOSE;
	mudlog(NRM, LVL_GOD, TRUE, "Connection attempt for %s denied from %s", GET_NAME(d->character), d->host);
	return;
      }
      if (GET_LEVEL(d->character) < circle_restrict) {
	write_to_output(d, "지금은 접속할 수 없습니다.. 나중에 다시 접속해 주십시오.\r\n");
	STATE(d) = CON_CLOSE;
	mudlog(NRM, LVL_GOD, TRUE, "Request for login denied for %s [%s] (wizlock)", GET_NAME(d->character), d->host);
	return;
      }
      /* check and make sure no other copies of this player are logged in */
      if (perform_dupe_check(d))
	return;

      if (GET_LEVEL(d->character) >= LVL_IMMORT)
	write_to_output(d, "%s", imotd);
      else
	write_to_output(d, "%s", motd);

      if (GET_INVIS_LEV(d->character))
        mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(d->character)), TRUE, "%s has connected. (invis %d)", GET_NAME(d->character), GET_INVIS_LEV(d->character));
      else
        mudlog(BRF, LVL_IMMORT, TRUE, "%s has connected.", GET_NAME(d->character));

      /* Add to the list of 'recent' players (since last reboot) */
      if (AddRecentPlayer(GET_NAME(d->character), d->host, FALSE, FALSE) == FALSE)
      {
        mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(d->character)), TRUE, "Failure to AddRecentPlayer (returned FALSE).");
      }

      if (load_result) {
        write_to_output(d, "\r\n\r\n\007\007\007"
		"%s마지막 접속으로부터 %d번의 잘못된 접속시도가 있었습니다.%s%s\r\n",
		CCRED(d->character, C_SPR), load_result,
		(load_result > 1) ? "S" : "", CCNRM(d->character, C_SPR));
	GET_BAD_PWS(d->character) = 0;
      }
      write_to_output(d, "\r\n*** 엔터 키를 눌러주세요:");
      STATE(d) = CON_RMOTD;
    }
    break;

  case CON_NEWPASSWD:
  case CON_CHPWD_GETNEW:
 /*   if (!*arg || strlen(arg) > MAX_PWD_LENGTH || strlen(arg) < 3 ||
	!str_cmp(arg, GET_PC_NAME(d->character))) {
      write_to_output(d, "\r\n사용할 수 없는 패스워드입니다.\r\n패스워드:");
      return;
    } */
	
		if (!*arg) {
      write_to_output(d, "\r\n#1 사용할 수 없는 패스워드입니다.\r\n패스워드:");
      return;
    }
	  if (strlen(arg) > MAX_PWD_LENGTH) {
      write_to_output(d, "\r\n#2 사용할 수 없는 패스워드입니다.\r\n패스워드:");
      return;
    }
	  if (strlen(arg) < 3) {
      write_to_output(d, "\r\n#3 사용할 수 없는 패스워드입니다.\r\n패스워드:");
      return;
    }
	  if (!str_cmp(arg, GET_PC_NAME(d->character))) {
      write_to_output(d, "\r\n#4 사용할 수 없는 패스워드입니다.\r\n패스워드:");
      return;
    }
	
    strncpy(GET_PASSWD(d->character), CRYPT(arg, GET_PC_NAME(d->character)), MAX_PWD_LENGTH);	/* strncpy: OK (G_P:MAX_PWD_LENGTH+1) */
    *(GET_PASSWD(d->character) + MAX_PWD_LENGTH) = '\0';

    write_to_output(d, "\r\n패스워드 확인:");
    if (STATE(d) == CON_NEWPASSWD)
      STATE(d) = CON_CNFPASSWD;
    else
      STATE(d) = CON_CHPWD_VRFY;
    break;

  case CON_CNFPASSWD:
  case CON_CHPWD_VRFY:
    if (strncmp(CRYPT(arg, GET_PASSWD(d->character)), GET_PASSWD(d->character),
		MAX_PWD_LENGTH)) {
      write_to_output(d, "\r\n패스워드가 일치하지 않습니다... 기존 정보를 초기화합니다.\r\n패스워드:");
      if (STATE(d) == CON_CNFPASSWD)
	STATE(d) = CON_NEWPASSWD;
      else
	STATE(d) = CON_CHPWD_GETNEW;
      return;
    }
    echo_on(d);

    if (STATE(d) == CON_CNFPASSWD) {
      write_to_output(d, "\r\n캐릭터의 생물학적 성별은 무엇으로 하시겠습니까? (\t(남\t)/\t(여\t))? ");
      STATE(d) = CON_QSEX;
    } else {
      save_char(d->character);
      write_to_output(d, "\r\n완료.\r\n%s", CONFIG_MENU);
      STATE(d) = CON_MENU;
    }
    break;

  case CON_QSEX:		/* query sex of new user         */
    if(!strcmp(arg, "남") || !strcmp(arg, "남성"))
      d->character->player.sex = SEX_MALE;
    else if(!strcmp(arg, "여") || !strcmp(arg, "여성"))
      d->character->player.sex = SEX_FEMALE;
    else {
      write_to_output(d, "죄송합니다, 현재 지원되지 않는 성별입니다..\r\n"
		"남성 또는 여성 중 가까운 성 정체성은 무엇입니까?");
      return;
    }

    write_to_output(d, "회원가입이 완료되었습니다!\r\n직업 관련 안내사항\r\n초기 직업은 무직입니다. 게임 내에서 직업 퀘스트를 통해 원하는 직업으로 전직이 가능합니다.\r\n\r\n엔터키를 눌러주세요\r\n"); // class_menu
    STATE(d) = CON_QCLASS;
    break;

  case CON_QCLASS:
//    load_result = parse_class(*arg);
    load_result = CLASS_UNDECIDED;
//    if (load_result == CLASS_UNDEFINED) {
//      write_to_output(d, "\r\n잘못 입력하셨습니다.\r\n직업:");
//      return;
//    } else
      GET_CLASS(d->character) = load_result;

      if (d->olc) {
        free(d->olc);
        d->olc = NULL;
      }
      if (GET_PFILEPOS(d->character) < 0)
      GET_PFILEPOS(d->character) = create_entry(GET_PC_NAME(d->character));
    /* Now GET_NAME() will work properly. */
    init_char(d->character);
    save_char(d->character);
    save_player_index();
    write_to_output(d, "%s\r\n*** 엔터 키를 눌러주세요:", motd);
    STATE(d) = CON_RMOTD;
    /* make sure the last log is updated correctly. */
    GET_PREF(d->character)= rand_number(1, 128000);
    GET_HOST(d->character)= strdup(d->host);

    mudlog(NRM, LVL_GOD, TRUE, "%s [%s] new player.", GET_NAME(d->character), d->host);

    /* Add to the list of 'recent' players (since last reboot) */
    if (AddRecentPlayer(GET_NAME(d->character), d->host, TRUE, FALSE) == FALSE)
    {
      mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(d->character)), TRUE, "Failure to AddRecentPlayer (returned FALSE).");
    }
    break;

  case CON_RMOTD:		/* read CR after printing motd   */
    write_to_output(d, "%s", CONFIG_MENU);
    if (IS_HAPPYHOUR > 0){
      write_to_output(d, "\r\n");
      write_to_output(d, "\ty현재 행복시간 이벤트가 진행중입니다!\tn\r\n");
      write_to_output(d, "\r\n");
    }
    add_llog_entry(d->character, LAST_CONNECT);
    STATE(d) = CON_MENU;
    break;

  case CON_MENU: {		/* get selection from main menu  */

    switch (*arg) {
    case '0':
      write_to_output(d, "Goodbye.\r\n");
      add_llog_entry(d->character, LAST_QUIT);
      STATE(d) = CON_CLOSE;
      break;

    case '1':
      load_result = enter_player_game(d);
      send_to_char(d->character, "%s", CONFIG_WELC_MESSG);

      /* Clear their load room if it's not persistant. */
      if (!PLR_FLAGGED(d->character, PLR_LOADROOM))
        GET_LOADROOM(d->character) = NOWHERE;
      save_char(d->character);

      greet_mtrigger(d->character, -1);
      greet_memory_mtrigger(d->character);

      act("$n님이 접속하셨습니다.", TRUE, d->character, 0, 0, TO_ROOM);

      STATE(d) = CON_PLAYING;
      MXPSendTag( d, "<VERSION>" );
      if (GET_LEVEL(d->character) == 0) {
	do_start(d->character);
	send_to_char(d->character, "%s", CONFIG_START_MESSG);
      }
      look_at_room(d->character, 0);
      if (has_mail(GET_IDNUM(d->character)))
	send_to_char(d->character, "편지가 도착했습니다.\r\n");
      if (load_result == 2) {	/* rented items lost */
	send_to_char(d->character, "\r\n\007You could not afford your rent!\r\n"
		"Your possesions have been donated to the Salvation Army!\r\n");
      }
      d->has_prompt = 0;
      /* We've updated to 3.1 - some bits might be set wrongly: */
      REMOVE_BIT_AR(PRF_FLAGS(d->character), PRF_BUILDWALK);
      break;

    case '2':
      if (d->character->player.description) {
	write_to_output(d, "Current description:\r\n%s", d->character->player.description);
	/* Don't free this now... so that the old description gets loaded as the
	 * current buffer in the editor.  Do setup the ABORT buffer here, however. */
	d->backstr = strdup(d->character->player.description);
      }
      write_to_output(d, "Enter the new text you'd like others to see when they look at you.\r\n");
      send_editor_help(d);
      d->str = &d->character->player.description;
      d->max_str = PLR_DESC_LENGTH;
      STATE(d) = CON_PLR_DESC;
      break;

    case '3':
      page_string(d, background, 0);
      STATE(d) = CON_RMOTD;
      break;

    case '4':
      write_to_output(d, "\r\nEnter your old password: ");
      echo_off(d);
      STATE(d) = CON_CHPWD_GETOLD;
      break;

    case '5':
      write_to_output(d, "\r\nEnter your password for verification: ");
      echo_off(d);
      STATE(d) = CON_DELCNF1;
      break;

    default:
      write_to_output(d, "\r\nThat's not a menu choice!\r\n%s", CONFIG_MENU);
      break;
    }
    break;
  }

  case CON_CHPWD_GETOLD:
    if (strncmp(CRYPT(arg, GET_PASSWD(d->character)), GET_PASSWD(d->character), MAX_PWD_LENGTH)) {
      echo_on(d);
      write_to_output(d, "\r\nIncorrect password.\r\n%s", CONFIG_MENU);
      STATE(d) = CON_MENU;
    } else {
      write_to_output(d, "\r\nEnter a new password: ");
      STATE(d) = CON_CHPWD_GETNEW;
    }
    return;

  case CON_DELCNF1:
    echo_on(d);
    if (strncmp(CRYPT(arg, GET_PASSWD(d->character)), GET_PASSWD(d->character), MAX_PWD_LENGTH)) {
      write_to_output(d, "\r\nIncorrect password.\r\n%s", CONFIG_MENU);
      STATE(d) = CON_MENU;
    } else {
      write_to_output(d, "\r\nYOU ARE ABOUT TO DELETE THIS CHARACTER PERMANENTLY.\r\n"
		"ARE YOU ABSOLUTELY SURE?\r\n\r\n"
		"Please type \"yes\" to confirm: ");
      STATE(d) = CON_DELCNF2;
    }
    break;

  case CON_DELCNF2:
    if (!strcmp(arg, "yes") || !strcmp(arg, "YES")) {
      if (PLR_FLAGGED(d->character, PLR_FROZEN)) {
	write_to_output(d, "You try to kill yourself, but the ice stops you.\r\n"
		"Character not deleted.\r\n\r\n");
	STATE(d) = CON_CLOSE;
	return;
      }
      if (GET_LEVEL(d->character) < LVL_GRGOD)
	SET_BIT_AR(PLR_FLAGS(d->character), PLR_DELETED);
      save_char(d->character);
      Crash_delete_file(GET_NAME(d->character));
      /* If the selfdelete_fastwipe flag is set (in config.c), remove all the
       * player's immediately. */
      if (selfdelete_fastwipe)
        if ((player_i = get_ptable_by_name(GET_NAME(d->character))) >= 0) {
          SET_BIT(player_table[player_i].flags, PINDEX_SELFDELETE);
          remove_player(player_i);
        }

      delete_variables(GET_NAME(d->character));
      write_to_output(d, "Character '%s' deleted! Goodbye.\r\n", GET_NAME(d->character));
      mudlog(NRM, MAX(LVL_GOD, GET_INVIS_LEV(d->character)), TRUE, "%s (lev %d) has self-deleted.",
       GET_NAME(d->character), GET_LEVEL(d->character));
      STATE(d) = CON_CLOSE;
      return;
    } else {
      write_to_output(d, "\r\nCharacter not deleted.\r\n%s", CONFIG_MENU);
      STATE(d) = CON_MENU;
    }
    break;

  /* It is possible, if enough pulses are missed, to kick someone off while they
   * are at the password prompt. We'll let the game_loop()axe them. */
  case CON_CLOSE:
    break;

  default:
    log("SYSERR: Nanny: illegal state of con'ness (%d) for '%s'; closing connection.",
	STATE(d), d->character ? GET_NAME(d->character) : "<unknown>");
    STATE(d) = CON_DISCONNECT;	/* Safest to do. */
    break;
  }
}
