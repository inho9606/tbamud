/*
* This file is for functions of baseball game
* Copyright 2020 by Inho Seo
*/
#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "handler.h"
#include "baseball.h"
struct baseball_data baseball;

ACMD(do_baseball) {
	char arg[MAX_INPUT_LENGTH], sub[MAX_INPUT_LENGTH];
	two_arguments(argument, arg, sub);
	if(!*arg) {
		send_to_char(ch, "����: <��Ȳ> �߱�\r\n");
		return;
	}
	if(*sub) {
		send_to_char(ch, "����: <��Ȳ> �߱�\r\n");
		return;
	}
	else {
		if(!str_cmp(arg, "��Ȳ")) {
			if(!*(baseball.top.name)) {
				send_to_char(ch, "������� ���� �����ϴ�.\r\n");
				return;
			}
		}
	}
}
