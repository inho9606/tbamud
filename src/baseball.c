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
#include "dg_scripts.h"

struct baseball_data baseball;

void leave_team(struct char_data *ch) {
	int i;
	struct char_data *prev = ch->company->member;
	if(prev == ch)
		prev = ch->next_baseball;
	else {
		for(; prev->next_baseball != ch; prev = prev->next_baseball) ;
		prev->next_baseball = ch->next_baseball;
	}
	for(i=0; i<9; i++) {
		if(ch->company->hitters[i] == ch) {
			ch->company->hitters[i] = NULL;
			break;
		}
	}
	ch->next_baseball = NULL;
	ch->company = NULL;
}

int find_position(struct char_data *ch) {
	struct baseball_team *t;
	int i;
	if(ch == NULL) return -2;
	if(ch->baseball_position == FALSE) return -1;
	t = ch->company;
	for(i=0; i<9; i++) {
		if(t->defense[i] == ch) break;
	}
	return i;
}
ACMD(do_baseball) {
	char arg[MAX_INPUT_LENGTH], sub[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	int cnt, i, zone;
	struct char_data *temp;
	struct baseball_team *team;
	two_arguments(argument, arg, sub);
	if(!*arg) {
		send_to_char(ch, "공통: <전광판> | <탈퇴> | [ <명단> | <창단> | <입단> | <라인업> <팀이름> ] 야구\r\n감독: <해체> | [ <방출> | <타순지정> <선수이름> ] 야구\r\n");
		return;
	}
	if(*sub) {
		if(!str_cmp(arg, "명단")) {
			cnt = 0;
			if(!str_cmp(sub, baseball.top.name)) {
				for(temp = baseball.top.member; temp != NULL; temp = temp->next_baseball) {
					send_to_char(ch, "%s ", GET_NAME(temp));
					cnt++;
				}
				send_to_char(ch, "\r\n");
				send_to_char(ch, "%d명의 선수가 있습니다.\r\n", cnt);
				return;
			}
			else if(!str_cmp(sub, baseball.bottom.name)) {
				for(temp = baseball.bottom.member; temp != NULL; temp = temp->next_baseball) {
					send_to_char(ch, "%s ", GET_NAME(temp));
					cnt++;
				}
				send_to_char(ch, "\r\n");
				send_to_char(ch, "%d명의 선수가 있습니다.\r\n", cnt);
				return;
			}
			else {
				send_to_char(ch, "그런 팀은 없습니다.\r\n");
				return;
			}
		}
		else if(!str_cmp(arg, "창단")) {
			zone = GET_ROOM_VNUM(IN_ROOM(ch));
			if(zone/100 != 188) { // 야구장 존플레그 추가 예정
				send_to_char(ch, "팀 창단은 야구장에서만 할 수 있습니다.\r\n");
				return;
			}
			if(ch->company != NULL) {
				send_to_char(ch, "이미 다른 팀에 소속되어 있습니다.\r\n");
				return;
			}
			if(baseball.playing == TRUE) {
				send_to_char(ch, "경기가 진행중입니다.\r\n");
				return;
			}
			if(*(baseball.bottom.name)) {
				send_to_char(ch, "이미 두 팀이 경기를 준비중입니다.\r\n");
				return;
			}
			if(*(baseball.top.name)) {
				if(!str_cmp(sub, baseball.top.name)) {
					send_to_char(ch, "같은 이름의 팀이 있습니다.\r\n");
					return;
				}
				strcpy(baseball.bottom.name, sub);
				baseball.bottom.master = ch;
				baseball.bottom.member = ch;
				ch->company = &baseball.bottom;
				sprintf(buf, "%s님이 이끄는 임시 야구단 %s이(가) 말 공격으로 경기를 준비합니다.\r\n", GET_NAME(ch), baseball.bottom.name);
				send_to_zone(buf, baseball.ground);
				return;
			}
			else {
				strcpy(baseball.top.name, sub);
				baseball.top.master = ch;
				baseball.top.member = ch;
				baseball.ground = real_zone_by_thing(zone);
				ch->company = &baseball.top;
				sprintf(buf, "%s님이 이끄는 임시 야구단 %s이(가) 초 공격으로 경기를 준비합니다.\r\n", GET_NAME(ch), baseball.top.name);
				send_to_zone(buf, baseball.ground);
				return;
			}
		}
		else if(!str_cmp(arg, "입단")) {
			if(ch->company != NULL) {
				send_to_char(ch, "이미 다른 팀에 소속되어 있습니다.\r\n");
				return;
			}
			if(!str_cmp(sub, baseball.top.name))
				ch->company = &baseball.top;
			else if(!str_cmp(sub, baseball.bottom.name))
				ch->company = &baseball.bottom;
			else {
				send_to_char(ch, "그런 팀은 없습니다.\r\n");
				return;
			}
			for(temp = ch->company->member; temp->next_baseball != NULL; temp = temp->next_baseball) ;
			temp->next_baseball = ch;
			sprintf(buf, "%s님이 %s 팀에 입단합니다.\r\n", GET_NAME(ch), ch->company->name);
			send_to_zone(buf, baseball.ground);
			return;
		}
		else if(!str_cmp(arg, "방출")) {
			if(ch->company == NULL || ch->company->master != ch) {
				send_to_char(ch, "감독 권한입니다.\r\n");
				return;
			}
			for(temp = ch->company->member; temp != NULL && str_cmp(sub, GET_NAME(temp)); temp = temp->next_baseball) ;
			if(temp == NULL) {
				send_to_char(ch, "그런 선수는 없습니다.\r\n");
				return;
			}
			if(!str_cmp(GET_NAME(temp), GET_NAME(ch))) {
				send_to_char(ch, "탈퇴를 하세요.\r\n");
				return;
			}
			if(temp->baseball_position == TRUE) {
				send_to_char(ch, "라인업에 포함된 선수는 방출할 수 없습니다.\r\n");
				return;
			}
			for(i=0; i<9; i++) {
				if(ch->company->hitters[i] == temp) {
					send_to_char(ch, "라인업에 포함된 선수는 방출할 수 없습니다.\r\n");
					return;
				}
			}
			sprintf(buf, "%s님이 %s 팀에서 방출되었습니다.\r\n", GET_NAME(temp), temp->company->name);
			send_to_zone(buf, baseball.ground);
			leave_team(temp);
		}
		else if(!str_cmp(arg, "라인업")) {
			if(!str_cmp(sub, baseball.top.name))
				team = &baseball.top;
			else if(!str_cmp(sub, baseball.bottom.name))
				team = &baseball.bottom;
			else {
				send_to_char(ch, "그런 팀은 없습니다.\r\n");
				return;
			}
			send_to_char(ch, "감독: %s\r\n", GET_NAME(team->master));
			for(i=0; i<9; i++) {
				cnt = find_position(team->hitters[i]);
				if(cnt == -2)
					send_to_char(ch, "%d번타자: 미지정 미지정\r\n", i+1);
				else if(cnt == -1)
					send_to_char(ch, "%d번타자: 미지정 %s\r\n", i+1, GET_NAME(team->hitters[i]));
				else
					send_to_char(ch, "%d번타자: %s %s\r\n", i+1, GET_NAME(team->defense[cnt]), GET_NAME(team->hitters[i]));
			}
		return;
		}
		else if(!str_cmp(arg, "타순지정")) {
			if(ch->company == NULL || ch->company->master != ch) {
				send_to_char(ch, "감독 권한입니다.\r\n");
				return;
			}
			team = ch->company;
			if(team->batter >= 9) {
				send_to_char(ch, "타순 지정이 완료되었습니다.\r\n");
				return;
			}
			for(temp = team->member; temp != NULL && str_cmp(sub, GET_NAME(temp)); temp = temp->next_baseball) ;
			if(temp == NULL) {
				send_to_char(ch, "그런 선수는 없습니다.\r\n");
				return;
			}
			for(i=0; i<team->batter; i++) {
				if(team->hitters[i] == temp) {
					send_to_char(ch, "이미 타순이 지정된 선수입니다.\r\n");
					return;
				}
			}
			sprintf(buf, "%s 팀의 %d번 타자 %s님을 소개합니다.\r\n", team->name, i+1, GET_NAME(temp));
			send_to_zone(buf, baseball.ground);
			team->hitters[i] = temp;
			team->batter += 1;
			return;
		}
		else {
			send_to_char(ch, "공통: <전광판> | <탈퇴> | [ <명단> | <창단> | <입단> | <라인업> <팀이름> ] 야구\r\n감독: <해체> | [ <방출> | <타순지정> <선수이름> ] 야구\r\n");
			return;
		}
	}
	else {
		if(!str_cmp(arg, "전광판")) {
			if(!*(baseball.top.name)) {
				send_to_char(ch, "경기중인 팀이 없습니다.\r\n");
				return;
			}
			else {
				if(baseball.playing == TRUE)
					send_to_char(ch, "경기 중\r\n");
				else
					send_to_char(ch, "경기 준비 중\r\n");
				send_to_char(ch, "%s팀 - R: %d, H: %d, E: %d, B: %d\r\n", baseball.top.name, baseball.top.runs, baseball.top.hits, baseball.top.errors, baseball.top.bb);
				if(*(baseball.bottom.name))
					send_to_char(ch, "%s팀 - R: %d, H: %d, E: %d, B: %d\r\n", baseball.bottom.name, baseball.bottom.runs, baseball.bottom.hits, baseball.bottom.errors, baseball.bottom.bb);
				for(i=0; i<9; i++)
					send_to_char(ch, "%d ", baseball.top.inning_score[i]);
				send_to_char(ch, "%d\r\n", baseball.top.runs);
				if(*(baseball.bottom.name)) {
					for(i=0; i<9; i++)
						send_to_char(ch, "%d ", baseball.bottom.inning_score[i]);
					send_to_char(ch, "%d\r\n", baseball.bottom.runs);
				}
				return;
			}
		}
		else if(!str_cmp(arg, "해체")) {
			if(ch->company == NULL || ch->company->master != ch) {
				send_to_char(ch, "감독 권한입니다.\r\n");
				return;
			}
			if(baseball.playing == TRUE) {
				send_to_char(ch, "경기가 진행중입니다.\r\n");
				return;
			}
			if(*(baseball.top.name)) {
				baseball.top.master = NULL;
				for(i=0; i<9; i++) {
					baseball.top.hitters[i] = NULL;
					baseball.top.defense[i] = NULL;
				}
				while(baseball.top.member != NULL) {
					temp = baseball.top.member;
					baseball.top.member = temp->next_baseball;
					temp->next_baseball = NULL;
					temp->company = NULL;
				}
				baseball.top.batter = 0;
				sprintf(buf, "%s님이 %s 팀을 해체했습니다.\r\n", GET_NAME(ch), baseball.top.name);
				send_to_zone(buf, baseball.ground);
				strcpy(baseball.top.name, "");
			}
			if(*(baseball.bottom.name)) {
				baseball.bottom.master = NULL;
				for(i=0; i<9; i++) {
					baseball.bottom.hitters[i] = NULL;
					baseball.bottom.defense[i] = NULL;
				}
				while(baseball.bottom.member != NULL) {
					temp = baseball.bottom.member;
					baseball.bottom.member = temp->next_baseball;
					temp->next_baseball = NULL;
					temp->company = NULL;
				}
				baseball.bottom.batter = 0;
				sprintf(buf, "%s님이 %s 팀을 해체했습니다.\r\n", GET_NAME(ch), baseball.bottom.name);
				send_to_zone(buf, baseball.ground);
				strcpy(baseball.bottom.name, "");
			}
		}
		else if(!str_cmp(arg, "탈퇴")) {
			if(ch->company == NULL) {
				send_to_char(ch, "소속 팀이 없습니다.\r\n");
				return;
			}
			if(ch->company->master == ch) {
				send_to_char(ch, "감독은 탈퇴할 수 없습니다.\r\n");
				return;
			}
			if(ch->baseball_position == TRUE) {
				send_to_char(ch, "라인업에 포함된 선수는 탈퇴할 수 없습니다.\r\n");
				return;
			}
			for(i=0; i<9; i++) {
				if(ch->company->hitters[i] == ch) {
					send_to_char(ch, "라인업에 포함된 선수는 탈퇴할 수 없습니다.\r\n");
					return;
				}
			}
			sprintf(buf, "%s님이 %s 팀을 탈퇴했습니다.\r\n", GET_NAME(ch), ch->company->name);
			send_to_zone(buf, baseball.ground);
			leave_team(ch);
		}
		else {
			send_to_char(ch, "공통: <전광판> | <탈퇴> | [ <명단> | <창단> | <입단> | <라인업> <팀이름> ] 야구\r\n감독: <해체> | [ <방출> | <타순지정> <선수이름> ] 야구\r\n");
			return;
		}
	}
}
