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
  "��",
  "��",
  "��",
  "��",
  "��",
  "��",
  "�ϼ�", /* Diagonals only used if CONFIG_DIAGONAL_DIRS is set */
  "�ϵ�",
  "����",
  "����",
  "\n"
};

const char *han_dirs[] =
{
  "��",
  "��",
  "��",
  "��",
  "��",
  "��",
  "�ϼ�", /* Diagonals only used if CONFIG_DIAGONAL_DIRS is set */
  "�ϵ�",
  "����",
  "����",
  "\n"
};

const char *autoexits[] =
{
  "��",
  "��",
  "��",
  "��",
  "��",
  "��",
  "�ϼ�", /* Diagonals only used if CONFIG_DIAGONAL_DIRS is set */
  "�ϵ�",
  "����",
  "����",
  "\n"
};

/** Room flag descriptions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *room_bits[] = {
  "����",
  "����",
  "������",
  "�ǳ�",
  "��ȭ",
  "����",
  "��������",
  "��������",
  "�ͳ�",
  "������",
  "��",
  "��",
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
  "����",
  "��ڱ���",
  "�ӹ�",
  "GRID",
  "���۱���",
  "!ASTRAL",
  "\n"
};

const char *exit_bits[] = {
  "��",
  "����",
  "���",
  "Ǯ��",
  "\n"
};

/** Description of the room sector type.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *sector_types[] = {
  "�ǳ�",
  "����",
  "����",
  "��",
  "���",
  "��",
  "��(����)",
  "��(��Ʈ)",
  "�ϴ�",
  "��(���)",
  "\n"
};

/** PC and NPC sex.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *genders[] =
{
  "�߼�",
  "����",
  "����",
  "\n"
};

/** Character positions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *position_types[] = {
  "����",
  "�ױ�����",
  "�����",
  "����",
  "��",
  "�޽�",
  "����",
  "�ο�",
  "���ִ�",
  "\n"
};

/** Player flags.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *player_bits[] = {
  "������",
  "����",
  "�õ�",
  "���ñ���",
  "�۾���",
  "��������",
  "CSH",
  "�������",
  "������",
  "Īȣ����",
  "����",
  "LOADRM",
  "NO_WIZL",
  "��������",
  "INVST",
  "CRYO",
  "���",    /* You should never see this flag on a character in game. */
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
  "Ư��",
  "�̵�����",
  "����",
  "ISNPC",
  "AWARE",
  "������",
  "STAY-ZONE",
  "��������",
  "����",
  "����",
  "�߸�",
  "�����",
  "���Ⱑ��",
  "NO_CHARM",
  "��ȯ�ȵ�",
  "��ȵ�",
  "��Ÿ�ȵ�",
  "�Ǹ�ȵ�",
  "DEAD",    /* You should never see this. */
  "\n"
};

/** PC Preference flags.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *preference_bits[] = {
  "����",
  "����",
  "��ħ�ź�",
  "�̾߱�ź�",
  "ü��ǥ��",
  "����ǥ��",
  "�̵���ǥ��",
  "�ڵ��ⱸ",
  "NO_HASS",
  "�ӹ�",
  "��ȯ",
  "�ݺ��ź�",
  "LIGHT",
  "C1",
  "C2",
  "NO_WIZ",
  "L1",
  "L2",
  "��Űź�",
  "���ź�",
  "NO_GTZ",
  "��Ӽ�",
  "D_AUTO",
  "CLS",
  "BLDWLK",
  "AFK",
  "�ڵ��ݱ�",
  "AUTOGOLD",
  "�ڵ��й�",
  "�ڵ�����",
  "�ڵ�����",
  "�ڵ�����",
  "�ڵ�����",
  "AUTODOOR",
  "\n"
};

/** Affected bits.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *affected_bits[] =
{
  "\0", /* DO NOT REMOVE!! */
  "�Ǹ�",
  "����",
  "���ǰ���",
  "������",
  "��������",
  "������",
  "�����ȱ�",
  "SANCT",
  "�׷�",
  "����",
  "INFRA",
  "��",
  "�Ǻ�ȣ",
  "����ȣ",
  "��",
  "�����Ұ�",
  "����",
  "���",
  "����",
  "����",
  "UNUSED",
  "���",
  "\n"
};

/** Connection type descriptions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *connected_types[] = {
  "������",
  "���Ӳ���",
  "�̸��Է�",
  "�̸����",
  "��ȣ�Է�",
  "��ȣ����",
  "��ȣȮ��",
  "��������",
  "��������",
  "�����б�",
  "���θ޴�",
  "�ڱ�Ұ� �Է�",
  "��ȣ ���� 1",
  "��ȣ ���� 2",
  "��ȣ ���� 3",
  "ĳ���ͻ��� 1",
  "ĳ���ͻ��� 2",
  "���Ӳ���",
  "��������",
  "������",
  "������",
  "������",
  "��������",
  "�ؽ�Ʈ����",
  "��������",
  "��������",
  "Ʈ��������",
  "��������",
  "�ӹ�����",
  "\n"
};

/** Describes the position in the equipment listing.
 * @pre Must be in the same order as the defines.
 * Not used in sprinttype() so no \\n. */
const char *wear_where[] = {
  "[��  ��] : ",
  "[���� ����] : ",
  "[������ ����] : ",
  "[��] : ",
  "[��] : ",
  "[  ��  ] : ",
  "[ �Ӹ� ] : ",
  "[ �ٸ� ] : ",
  "[  ��  ] : ",
  "[  ��  ] : ",
  "[  ��  ] : ",
  "[ ���� ] : ",
  "[���ѷ�] : ",
  "[ �㸮 ] : ",
  "[���� �ո�] : ",
  "[������ �ո�] : "	,
  "[������] : ",
  "[�޼�] : "
};

/* Describes where an item can be worn.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *equipment_types[] = {
  "����",
  "���� ����",
  "������ ����",
  "��",
  "��",
  "��",
  "�Ӹ�",
  "�ٸ�",
  "��",
  "��",
  "��",
  "����",
  "���ѷ�",
  "�㸮",
  "���� �ո�",
  "������ �ո�",
  "������",
  "�޼�",
  "\n"
};

/** Describes the type of object.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *item_types[] = {
  "������",
  "����",
  "�ֹ���",
  "������",
  "�����",
  "����",
  "����",
  "FREE",
  "����",
  "��",
  "����",
  "������",
  "��Ÿ",
  "������",
  "FREE2",
  "������",
  "��Ʈ",
  "���������",
  "����",
  "����",
  "��",
  "��",
  "��Ʈ",
  "�м���",
  "\n"
};

/** Describes the wear flags set on an item.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *wear_bits[] = {
  "�ݱⰡ��",
  "�հ���",
  "��",
  "��",
  "�Ӹ�",
  "�ٸ�",
  "��",
  "��",
  "��",
  "����",
  "���ѷ�",
  "�㸮",
  "�ո�",
  "������",
  "�޼�",
  "\n"
};

/** Describes the extra flags applied to an item.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *extra_bits[] = {
  "��ä",
  "�Ҹ�",
  "�����Ұ�",
  "��κҰ�",
  "����Ұ�",
  "����",
  "����",
  "����������",
  "�ູ",
  "���ź�",
  "�ǰź�",
  "�߸��ź�",
  "������ź�",
  "�����ڰź�",
  "���ϰź�",
  "����ź�",
  "�ȼ�����",
  "�ӹ�",
  "\n"
};

/** Describes the apply types.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *apply_types[] = {
  "����",
  "��",
  "��ø",
  "����",
  "����",
  "ü��",
  "��ַ�",
  "���",
  "��������Ʈ",
  "����",
  "����",
  "����",
  "ü��",
  "Ű",
  "�ִ븶����",
  "�ִ�ü��",
  "�ִ��̵���",
  "��",
  "����ġ",
  "���",
  "���ݷ�",
  "Ÿ��ġ",
  "��������",
  "SAVING_ROD",
  "SAVING_PETRI",
  "SAVING_BREATH",
  "��������",
  "\n"
};

/** Describes the closure mechanism for a container.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *container_bits[] = {
  "����",
  "Ǯ��",
  "����",
  "���",
  "\n",
};

/** Describes the liquid description.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *drinks[] =
{
  "��",
  "����",
  "������",
  "����",
  "�����",
  "����Ű",
  "������̵�",
  "ȭ�̾",
  "���� Ư�깰",
  "�꽺",
  "����",
  "��",
  "Ŀ��",
  "��",
  "�ұݹ�",
  "�����ѹ�",
  "\n"
};

/** Describes a one word alias for each type of liquid.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *drinknames[] =
{
  "��",
  "����",
  "����",
  "����",
  "����",
  "����Ű",
  "������̵�",
  "ȭ�̾",
  "����",
  "�꽺",
  "����",
  "��",
  "Ŀ��",
  "��",
  "�ұ�",
  "��",
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
  "������",
  "����",
  "������",
  "����",
  "������",
  "�ݻ�",
  "������",
  "Ǫ����",
  "������",
  "���λ�",
  "���",
  "����",
  "������",
  "������",
  "������",
  "�ſ�������",
  "\n"
};

/** Used to describe the level of fullness of a drink container. Not used in
 * sprinttype() so no \\n. */
const char *fullness[] =
{
  "���� ",
  "���� ",
  "���� ",
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
  "���� ��",
  "Ȳ���� ��",
  "�⸸�� ��",
  "������ ��",
  "������ ��",
  "���� ��",
  "�¾��� ��"
};

/** The names of the mud months. Not used in sprinttype(). */
const char *month_name[] = {
  "Ȥ���� ��",		/* 0 */
  "�ܿ������ ��",
  "�������� ��",
  "���� ��",
  "����� ��",
  "���� ��",
  "���ڿ��� ��",
  "��ȿ�� ��",
  "���� ��",
  "�¾��� ��",
  "�±��� ��",
  "������ ��",
  "��ο� �״��� ��",
  "������ ��",
  "��׸����� ��",
  "�������� ��",
  "��Ǹ��� ��"
};

/** Names for mob trigger types. */
const char *trig_types[] = {
  "Global",
  "����",
  "��ɾ�",
  "��",
  "�ൿ",
  "����",
  "ȯ��",
  "Greet-All",
  "entry",
  "���� ����",
  "������",
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
  "����",
  "��ɾ�",
  "UNUSED1",
  "UNUSED2",
  "Ÿ�̸�",
  "�ֿ� ��",
  "���� ��",
  "�� ��",
  "������ ��",
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
  "����",
  "��ɾ�",
  "��",
  "UNUSED1",
  "������",
  "����",
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
  "�α���",
  "Time",
  "\n"
};

/** The names of the different channels that history is stored for.
 * @todo Only referenced by do_history at the moment. Should be moved local
 * to that function. */
const char *history_types[] = {
  "���",
  "��",
  "���",
  "��ä��",
  "�̾߱�",
  "��ħ",
  "����",
  "����",
  "���",
  "\n"
};

/** Flag names for Ideas, Bugs and Typos (defined in ibt.h) */
const char *ibt_bits[] = {
  "ó�� �Ϸ�",
  "�߿�",
  "ó����",
  "\n"
};

/** Defense Position for baseball game */
const char *baseball_position[] = {
  "����",
  "����",
  "1���",
  "2���",
  "3���",
  "���ݼ�",
  "���ͼ�",
  "�߰߼�",
  "���ͼ�",
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

