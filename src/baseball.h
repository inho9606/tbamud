/**
* @file baseball.h
* Header file for baseball game.
*
* Copyright 2020 by Inho Seo
*/
#ifndef _BASEBALL_H_
#define _BASEBALL_H_
#include "utils.h"
void record_count(int status);
void locate_position();
void init_ball(struct baseball_ball *b);
void move_ball(int ms);

struct char_data * find_player(struct char_data *ch, char *sub);
bool is_ready();
int find_position(struct char_data *ch);
void leave_team(struct char_data *ch);
ACMD(do_baseball);
ACMD(do_pitch);
ACMD(do_bat);
#endif