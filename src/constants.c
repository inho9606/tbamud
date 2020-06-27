/**
* @file constants.c
* Numeric and string contants used by the MUD.
*
* Part of the core tbaMUD source code distribution, which is a derivative
* of, and continuation of, CircleMUD.
*
* All rights reserved.  See license for complete information.
* Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University
* CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
*
* @todo Come up with a standard for descriptive arrays. Either all end with
* newlines or all of them don not.
*/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "interpreter.h"	/* alias_data */
#include "constants.h"

/** Current tbaMUD version.
 * @todo cpp_extern isn't needed here (or anywhere) as the extern reserved word
 * works correctly with C compilers (at least in my Experience)
 * Jeremy Osborne 1/28/2008 */
cpp_extern const char *tbamud_version = "tbaMUD 2019";

/* strings corresponding to ordinals/bitvectors in structs.h */
/* (Note: strings for class definitions in class.c instead of here) */

/** Description of cardinal directions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *dirs[] =
{
  "북",
  "동",
  "남",
  "서",
  "위",
  "밑",
  "북서", /* Diagonals only used if CONFIG_DIAGONAL_DIRS is set */
  "북동",
  "남동",
  "남서",
  "\n"
};

const char *han_dirs[] =
{
  "북",
  "동",
  "남",
  "서",
  "위",
  "밑",
  "북서", /* Diagonals only used if CONFIG_DIAGONAL_DIRS is set */
  "북동",
  "남동",
  "남서",
  "\n"
};

const char *autoexits[] =
{
  "북",
  "동",
  "남",
  "서",
  "위",
  "밑",
  "북서", /* Diagonals only used if CONFIG_DIAGONAL_DIRS is set */
  "북동",
  "남동",
  "남서",
  "\n"
};

/** Room flag descriptions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *room_bits[] = {
  "암흑",
  "함정",
  "맙금지",
  "실내",
  "평화",
  "방음",
  "추적금지",
  "마법금지",
  "터널",
  "사유지",
  "신",
  "집",
  "HCRSH",
  "ATRIUM",
  "OLC",
  "*",				/* The BFS Mark. */
  "WORLDMAP",
  "\n"
};

/** Room flag descriptions. (ZONE_x)
 * @pre Must be in the same order as the defines in structs.h.
 * Must end array with a single newline. */
const char *zone_bits[] = {
  "닫힌",
  "운영자금지",
  "임무",
  "GRID",
  "제작금지",
  "!ASTRAL",
  "\n"
};

const char *exit_bits[] = {
  "문",
  "닫힌",
  "잠긴",
  "풀린",
  "\n"
};

/** Description of the room sector type.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *sector_types[] = {
  "실내",
  "도시",
  "평지",
  "숲",
  "언덕",
  "산",
  "물(수영)",
  "물(보트)",
  "하늘",
  "물(잠수)",
  "\n"
};

/** PC and NPC sex.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *genders[] =
{
  "중성",
  "남성",
  "여성",
  "\n"
};

/** Character positions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *position_types[] = {
  "죽음",
  "죽기직전",
  "무기력",
  "기절",
  "잠",
  "휴식",
  "앉은",
  "싸움",
  "서있는",
  "\n"
};

/** Player flags.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *player_bits[] = {
  "범죄자",
  "도둑",
  "냉동",
  "셋팅금지",
  "글쓰기",
  "편지쓰기",
  "CSH",
  "접속허용",
  "잡담금지",
  "칭호금지",
  "삭제",
  "LOADRM",
  "NO_WIZL",
  "삭제금지",
  "INVST",
  "CRYO",
  "사망",    /* You should never see this flag on a character in game. */
  "UNUSED1",
  "UNUSED2",
  "UNUSED3",
  "UNUSED4",
  "UNUSED5",
  "\n"
};

/** Mob action flags.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *action_bits[] = {
  "특수",
  "이동안함",
  "동물",
  "ISNPC",
  "AWARE",
  "선공격",
  "STAY-ZONE",
  "도망가능",
  "악한",
  "선한",
  "중립",
  "상대기억",
  "돕기가능",
  "NO_CHARM",
  "소환안됨",
  "잠안됨",
  "강타안됨",
  "실명안됨",
  "DEAD",    /* You should never see this. */
  "\n"
};

/** PC Preference flags.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *preference_bits[] = {
  "간략",
  "공백",
  "외침거부",
  "이야기거부",
  "체력표시",
  "마력표시",
  "이동력표시",
  "자동출구",
  "NO_HASS",
  "임무",
  "소환",
  "반복거부",
  "LIGHT",
  "C1",
  "C2",
  "NO_WIZ",
  "L1",
  "L2",
  "경매거부",
  "잡담거부",
  "NO_GTZ",
  "방속성",
  "D_AUTO",
  "CLS",
  "BLDWLK",
  "AFK",
  "자동줍기",
  "AUTOGOLD",
  "자동분배",
  "자동제물",
  "자동돕기",
  "자동지도",
  "자동열쇠",
  "AUTODOOR",
  "\n"
};

/** Affected bits.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *affected_bits[] =
{
  "\0", /* DO NOT REMOVE!! */
  "실명",
  "투명",
  "선악감지",
  "투명감지",
  "마법감지",
  "생명감지",
  "물위걷기",
  "SANCT",
  "그룹",
  "저주",
  "INFRA",
  "독",
  "악보호",
  "선보호",
  "잠",
  "추적불가",
  "날기",
  "잠수",
  "은신",
  "숨기",
  "UNUSED",
  "통솔",
  "\n"
};

/** Connection type descriptions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *connected_types[] = {
  "게임중",
  "접속끊김",
  "이름입력",
  "이름등록",
  "암호입력",
  "암호변경",
  "암호확인",
  "성별선택",
  "직업선택",
  "공지읽기",
  "메인메뉴",
  "자기소개 입력",
  "암호 변경 1",
  "암호 변경 2",
  "암호 변경 3",
  "캐릭터삭제 1",
  "캐릭터삭제 2",
  "접속끊김",
  "물건편집",
  "방편집",
  "존편집",
  "맙편집",
  "상점편집",
  "텍스트편집",
  "게임편집",
  "감정편집",
  "트리거편집",
  "도움말편집",
  "임무편집",
  "\n"
};

/** Describes the position in the equipment listing.
 * @pre Must be in the same order as the defines.
 * Not used in sprinttype() so no \\n. */
const char *wear_where[] = {
  "[광  원] : ",
  "[왼쪽 약지] : ",
  "[오른쪽 약지] : ",
  "[목] : ",
  "[목] : ",
  "[  몸  ] : ",
  "[ 머리 ] : ",
  "[ 다리 ] : ",
  "[  발  ] : ",
  "[  손  ] : ",
  "[  팔  ] : ",
  "[ 방패 ] : ",
  "[몸둘레] : ",
  "[ 허리 ] : ",
  "[왼쪽 손목] : ",
  "[오른쪽 손목] : "	,
  "[오른손] : ",
  "[왼손] : "
};

/* Describes where an item can be worn.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *equipment_types[] = {
  "광원",
  "왼쪽 약지",
  "오른쪽 약지",
  "목",
  "목",
  "몸",
  "머리",
  "다리",
  "발",
  "손",
  "팔",
  "방패",
  "몸둘레",
  "허리",
  "왼쪽 손목",
  "오른쪽 손목",
  "오른손",
  "왼손",
  "\n"
};

/** Describes the type of object.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *item_types[] = {
  "미지정",
  "광원",
  "주문서",
  "지팡이",
  "막대기",
  "무기",
  "가구",
  "FREE",
  "보물",
  "방어구",
  "물약",
  "낡은옷",
  "기타",
  "쓰레기",
  "FREE2",
  "보관함",
  "노트",
  "음료수보관",
  "열쇠",
  "음식",
  "돈",
  "펜",
  "보트",
  "분수대",
  "\n"
};

/** Describes the wear flags set on an item.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *wear_bits[] = {
  "줍기가능",
  "손가락",
  "목",
  "몸",
  "머리",
  "다리",
  "발",
  "손",
  "팔",
  "방패",
  "몸둘레",
  "허리",
  "손목",
  "오른손",
  "왼손",
  "\n"
};

/** Describes the extra flags applied to an item.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *extra_bits[] = {
  "광채",
  "소리",
  "보관불가",
  "기부불가",
  "투명불가",
  "투명",
  "마법",
  "버릴수없음",
  "축복",
  "선거부",
  "악거부",
  "중립거부",
  "마법사거부",
  "성직자거부",
  "도둑거부",
  "전사거부",
  "팔수없음",
  "임무",
  "\n"
};

/** Describes the apply types.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *apply_types[] = {
  "없음",
  "힘",
  "민첩",
  "지식",
  "지혜",
  "체력",
  "통솔력",
  "행운",
  "스탯포인트",
  "직업",
  "레벨",
  "나이",
  "체중",
  "키",
  "최대마법력",
  "최대체력",
  "최대이동력",
  "돈",
  "경험치",
  "방어",
  "공격력",
  "타격치",
  "마비저항",
  "SAVING_ROD",
  "SAVING_PETRI",
  "SAVING_BREATH",
  "마법저항",
  "\n"
};

/** Describes the closure mechanism for a container.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *container_bits[] = {
  "열린",
  "풀린",
  "닫힌",
  "잠긴",
  "\n",
};

/** Describes the liquid description.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *drinks[] =
{
  "물",
  "맥주",
  "포도주",
  "맥주",
  "흑맥주",
  "위스키",
  "레모네이드",
  "화이어볼",
  "지역 특산물",
  "쥬스",
  "우유",
  "차",
  "커피",
  "피",
  "소금물",
  "깨끗한물",
  "\n"
};

/** Describes a one word alias for each type of liquid.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *drinknames[] =
{
  "물",
  "맥주",
  "와인",
  "에일",
  "에일",
  "위스키",
  "레모네이드",
  "화이어볼",
  "지역",
  "쥬스",
  "우유",
  "차",
  "커피",
  "피",
  "소금",
  "물",
  "\n"
};

/** Define the effect of liquids on hunger, thirst, and drunkenness, in that
 * order. See values.doc for more information.
 * @pre Must be in the same order as the defines. */
int drink_aff[][3] = {
  {0, 1, 10},
  {3, 2, 5},
  {5, 2, 5},
  {2, 2, 5},
  {1, 2, 5},
  {6, 1, 4},
  {0, 1, 8},
  {10, 0, 0},
  {3, 3, 3},
  {0, 4, -8},
  {0, 3, 6},
  {0, 1, 6},
  {0, 1, 6},
  {0, 2, -1},
  {0, 1, -2},
  {0, 0, 13}
};

/** Describes the color of the various drinks.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *color_liquid[] =
{
  "투명한",
  "갈색",
  "투명한",
  "갈색",
  "검은색",
  "금색",
  "붉은색",
  "푸른색",
  "투명한",
  "연두색",
  "흰색",
  "갈색",
  "검은색",
  "붉은색",
  "투명한",
  "매우투명한",
  "\n"
};

/** Used to describe the level of fullness of a drink container. Not used in
 * sprinttype() so no \\n. */
const char *fullness[] =
{
  "조금 ",
  "절반 ",
  "가득 ",
  ""
};

/** Strength attribute affects.
 * The fields are hit mod, damage mod, weight carried mod, and weight wielded
 * mod. */
cpp_extern const struct str_app_type str_app[] = {
  {-5, -4, 0, 0},	/* str = 0 */
  {-5, -4, 3, 1},	/* str = 1 */
  {-3, -2, 3, 2},
  {-3, -1, 10, 3},
  {-2, -1, 25, 4},
  {-2, -1, 55, 5},	/* str = 5 */
  {-1, 0, 80, 6},
  {-1, 0, 90, 7},
  {0, 0, 100, 8},
  {0, 0, 100, 9},
  {0, 0, 115, 10},	/* str = 10 */
  {0, 0, 115, 11},
  {0, 0, 140, 12},
  {0, 0, 140, 13},
  {0, 0, 170, 14},
  {0, 0, 170, 15},	/* str = 15 */
  {0, 1, 195, 16},
  {1, 1, 220, 18},
  {1, 2, 255, 20},	/* str = 18 */
  {3, 7, 640, 40},
  {3, 8, 700, 40},	/* str = 20 */
  {4, 9, 810, 40},
  {4, 10, 970, 40},
  {5, 11, 1130, 40},
  {6, 12, 1440, 40},
  {7, 14, 1750, 40},	/* str = 25 */
  {1, 3, 280, 22},	/* str = 18/0 - 18-50 */
  {2, 3, 305, 24},	/* str = 18/51 - 18-75 */
  {2, 4, 330, 26},	/* str = 18/76 - 18-90 */
  {2, 5, 380, 28},	/* str = 18/91 - 18-99 */
  {3, 6, 480, 30}	/* str = 18/100 */
};

/** Dexterity skill modifiers for thieves.
 * The fields are for pick pockets, pick locks, find traps, sneak and hide. */
cpp_extern const struct dex_skill_type dex_app_skill[] = {
  {-99, -99, -90, -99, -60},	/* dex = 0 */
  {-90, -90, -60, -90, -50},	/* dex = 1 */
  {-80, -80, -40, -80, -45},
  {-70, -70, -30, -70, -40},
  {-60, -60, -30, -60, -35},
  {-50, -50, -20, -50, -30},	/* dex = 5 */
  {-40, -40, -20, -40, -25},
  {-30, -30, -15, -30, -20},
  {-20, -20, -15, -20, -15},
  {-15, -10, -10, -20, -10},
  {-10, -5, -10, -15, -5},	/* dex = 10 */
  {-5, 0, -5, -10, 0},
  {0, 0, 0, -5, 0},
  {0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0},		/* dex = 15 */
  {0, 5, 0, 0, 0},
  {5, 10, 0, 5, 5},
  {10, 15, 5, 10, 10},		/* dex = 18 */
  {15, 20, 10, 15, 15},
  {15, 20, 10, 15, 15},		/* dex = 20 */
  {20, 25, 10, 15, 20},
  {20, 25, 15, 20, 20},
  {25, 25, 15, 20, 20},
  {25, 30, 15, 25, 25},
  {25, 30, 15, 25, 25}		/* dex = 25 */
};

/** Dexterity attribute affects.
 * The fields are reaction, missile attacks, and defensive (armor class). */
cpp_extern const struct dex_app_type dex_app[] = {
  {-7, -7, 6},		/* dex = 0 */
  {-6, -6, 5},		/* dex = 1 */
  {-4, -4, 5},
  {-3, -3, 4},
  {-2, -2, 3},
  {-1, -1, 2},		/* dex = 5 */
  {0, 0, 1},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},		/* dex = 10 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, -1},		/* dex = 15 */
  {1, 1, -2},
  {2, 2, -3},
  {2, 2, -4},		/* dex = 18 */
  {3, 3, -4},
  {3, 3, -4},		/* dex = 20 */
  {4, 4, -5},
  {4, 4, -5},
  {4, 4, -5},
  {5, 5, -6},
  {5, 5, -6}		/* dex = 25 */
};

/** Constitution attribute affects.
 * The field referenced is for hitpoint bonus. */
cpp_extern const struct con_app_type con_app[] = {
  {-4},		/* con = 0 */
  {-3},		/* con = 1 */
  {-2},
  {-2},
  {-1},
  {-1},		/* con = 5 */
  {-1},
  {0},
  {0},
  {0},
  {0},		/* con = 10 */
  {0},
  {0},
  {0},
  {0},
  {1},		/* con = 15 */
  {2},
  {2},
  {3},		/* con = 18 */
  {3},
  {4},		/* con = 20 */
  {5},
  {5},
  {5},
  {6},
  {6}		/* con = 25 */
};

/** Intelligence attribute affects.
 * The field shows how much practicing affects a skill/spell. */
cpp_extern const struct int_app_type int_app[] = {
  {3},		/* int = 0 */
  {5},		/* int = 1 */
  {7},
  {8},
  {9},
  {10},		/* int = 5 */
  {11},
  {12},
  {13},
  {15},
  {17},		/* int = 10 */
  {19},
  {22},
  {25},
  {30},
  {35},		/* int = 15 */
  {40},
  {45},
  {50},		/* int = 18 */
  {53},
  {55},		/* int = 20 */
  {56},
  {57},
  {58},
  {59},
  {60}		/* int = 25 */
};

/** Wisdom attribute affects.
 * The field represents how many extra practice points are gained per level. */
cpp_extern const struct wis_app_type wis_app[] = {
  {0},	/* wis = 0 */
  {0},  /* wis = 1 */
  {0},
  {0},
  {0},
  {0},  /* wis = 5 */
  {0},
  {0},
  {0},
  {0},
  {0},  /* wis = 10 */
  {0},
  {2},
  {2},
  {3},
  {3},  /* wis = 15 */
  {3},
  {4},
  {5},	/* wis = 18 */
  {6},
  {6},  /* wis = 20 */
  {6},
  {6},
  {7},
  {7},
  {7}  /* wis = 25 */
};

/** Define a set of opposite directions from the cardinal directions. */
int rev_dir[] =
{
  SOUTH,
  WEST,
  NORTH,
  EAST,
  DOWN,
  UP,
  SOUTHEAST,
  SOUTHWEST,
  NORTHWEST,
  NORTHEAST
};

/** How much movement is lost moving through a particular sector type. */
int movement_loss[] =
{
  1,	/* Inside     */
  1,	/* City       */
  2,	/* Field      */
  3,	/* Forest     */
  4,	/* Hills      */
  6,	/* Mountains  */
  4,	/* Swimming   */
  1,	/* Unswimable */
  1,	/* Flying     */
  5   /* Underwater */
};

/** The names of the days of the mud week. Not used in sprinttype(). */
const char *weekdays[] = {
  "달의 날",
  "황소의 날",
  "기만의 날",
  "번개의 날",
  "자유의 날",
  "신의 날",
  "태양의 날"
};

/** The names of the mud months. Not used in sprinttype(). */
const char *month_name[] = {
  "혹한의 달",		/* 0 */
  "겨울늑대의 달",
  "숲거인의 달",
  "힘의 달",
  "노력의 달",
  "봄의 달",
  "대자연의 달",
  "무효의 달",
  "용의 달",
  "태양의 달",
  "온기의 달",
  "전투의 달",
  "어두운 그늘의 달",
  "암흑의 달",
  "긴그림자의 달",
  "고대암흑의 달",
  "대악마의 달"
};

/** Names for mob trigger types. */
const char *trig_types[] = {
  "Global",
  "랜덤",
  "명령어",
  "말",
  "행동",
  "죽음",
  "환영",
  "Greet-All",
  "entry",
  "물건 받음",
  "전투중",
  "HitPrcnt",
  "Bribe",
  "Load",
  "Memory",
  "Cast",
  "Leave",
  "Door",
  "UNUSED",
  "Time",
  "\n"
};

/** Names for object trigger types. */
const char *otrig_types[] = {
  "Global",
  "랜덤",
  "명령어",
  "UNUSED1",
  "UNUSED2",
  "타이머",
  "주울 때",
  "버릴 때",
  "줄 때",
  "착용할 때",
  "UNUSED3",
  "Remove",
  "UNUSED4",
  "Load",
  "UNUSED5",
  "Cast",
  "Leave",
  "UNUSED6",
  "Consume",
  "Time",
  "\n"
};

/** Names for world (room) trigger types. */
const char *wtrig_types[] = {
  "Global",
  "랜덤",
  "명령어",
  "말",
  "UNUSED1",
  "존리셋",
  "입장",
  "Drop",
  "UNUSED2",
  "UNUSED3",
  "UNUSED4",
  "UNUSED5",
  "UNUSED6",
  "UNUSED7",
  "UNUSED8",
  "Cast",
  "Leave",
  "Door",
  "로그인",
  "Time",
  "\n"
};

/** The names of the different channels that history is stored for.
 * @todo Only referenced by do_history at the moment. Should be moved local
 * to that function. */
const char *history_types[] = {
  "모두",
  "말",
  "잡담",
  "신채널",
  "이야기",
  "외침",
  "축하",
  "불평",
  "경매",
  "\n"
};

/** Flag names for Ideas, Bugs and Typos (defined in ibt.h) */
const char *ibt_bits[] = {
  "처리 완료",
  "중요",
  "처리중",
  "\n"
};

/** Defense Position for baseball game */
const char *baseball_position[] = {
  "투수",
  "포수",
  "1루수",
  "2루수",
  "3루수",
  "유격수",
  "좌익수",
  "중견수",
  "우익수",
  "\n"
};
/* --- End of constants arrays. --- */

/* Various arrays we count so we can check the world files.  These
 * must be at the bottom of the file so they're pre-declared. */
  /** Number of defined room bit descriptions. */
  size_t	room_bits_count = sizeof(room_bits) / sizeof(room_bits[0]) - 1,
  /** Number of defined action bit descriptions. */
	action_bits_count = sizeof(action_bits) / sizeof(action_bits[0]) - 1,
	/** Number of defined affected bit descriptions. */
	affected_bits_count = sizeof(affected_bits) / sizeof(affected_bits[0]) - 1,
	/** Number of defined extra bit descriptions. */
	extra_bits_count = sizeof(extra_bits) / sizeof(extra_bits[0]) - 1,
	/** Number of defined wear bit descriptions. */
	wear_bits_count = sizeof(wear_bits) / sizeof(wear_bits[0]) - 1;

