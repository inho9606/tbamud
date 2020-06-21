/**
* @file baseball.h
* Header file for baseball game.
*
* Copyright 2020 by Inho Seo
*/
#ifndef _BASEBALL_H_
#define _BASEBALL_H_
#include "utils.h"
int find_position(struct char_data *ch);
void leave_team(struct char_data *ch);
ACMD(do_baseball);
#endif