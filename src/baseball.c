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
#include "constants.h"

struct baseball_data baseball;

void locate_position() {
	struct baseball_team *t;
	int i;
	if(baseball.bottom_hitting == TRUE) t = &baseball.bottom;
	else t = &baseball.top;
	for(i=0; i<9; i++) {
		if(i==0) { // 포지션 별 초기 방번호 배열 만들기
			char_from_room(t->defense[i]);
			char_to_room(t->defense[i], find_target_room(t->defense[i], "18882"));
		}
	}
}

void init_ball(struct baseball_ball *b) {
	b->x = 0;
	b->y = 0;
	b->moving = FALSE;
	b->clock = 0;
}

void move_ball(int ms) {
	char buf[MAX_STRING_LENGTH];
	if(baseball.ball.moving == FALSE || (ms - baseball.ball.clock) % (5*BASEBALLGAME_PER_SEC) != 0) return;
	sprintf(buf, "%d, %d, %d, %d\r\n", pulse-baseball.ball.clock, ms, ms-baseball.ball.clock);
	send_to_zone(buf, baseball.ground);
	switch (baseball.ball.dir) {
		case SOUTH:
			baseball.ball.y += 1;
			break;
		case NORTH:
			baseball.ball.y -= 1;
			break;
		case EAST:
			baseball.ball.x += 1;
			break;
		case WEST:
			baseball.ball.x -= 1;
			break;
		default:
			break;
	}
	if(baseball.ball.y == 10) {
		if(baseball.ball.x >= 4 && baseball.ball.x <= 6) {
			sprintf(buf, "스트라이크!\r\n");
		}
		else {
			sprintf(buf, "볼!\r\n");
		}
		send_to_zone(buf, baseball.ground);
		baseball.ball.moving = FALSE;
		return;

	}
}

bool is_ready() {
	int i;
	if(!*(baseball.top.name) || !*(baseball.bottom.name)) return 0;
	for(i=0; i<9; i++) {
		if(baseball.top.hitters[i] == NULL || baseball.top.defense[i] == NULL) return 0;
		if(baseball.bottom.hitters[i] == NULL || baseball.bottom.defense[i] == NULL) return 0;
		return 1;
	}
}
struct char_data * find_player(struct char_data *ch, char *sub) {
	struct char_data *temp;
	for(temp = ch->company->member; temp != NULL && str_cmp(sub, GET_NAME(temp)); temp = temp->next_baseball) ;
	if(temp == NULL) {
		send_to_char(ch, "그런 선수는 없습니다.\r\n");
		return NULL;
	}
	return temp;
}

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
	ch->baseball_position = FALSE;
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
		send_to_char(ch, "공통: <전광판> | <탈퇴> | [ <명단> | <창단> | <입단> | <라인업> <팀이름> ] 야구\r\n감독: <해체> | [ <방출> | <타순지정> | <포지션> <선수이름> ] <게임> <이닝수> 야구\r\n");
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
					send_to_char(ch, "%d번타자: %s %s\r\n", i+1, baseball_position[cnt], GET_NAME(team->hitters[i]));
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
			sprintf(buf, "%s 팀의 %d번 타자, %s님을 소개합니다.\r\n", team->name, i+1, GET_NAME(temp));
			send_to_zone(buf, baseball.ground);
			team->hitters[i] = temp;
			team->batter += 1;
			return;
		}
		else if(!str_cmp(arg, "투수") || !str_cmp(arg, "포수") || !str_cmp(arg, "1루수") || !str_cmp(arg, "2루수") || !str_cmp(arg, "3루수") || !str_cmp(arg, "유격수") || !str_cmp(arg, "중견수") || !str_cmp(arg, "좌익수") || !str_cmp(arg, "우익수")) {
			if(ch->company == NULL || ch->company->master != ch) {
				send_to_char(ch, "감독 권한입니다.\r\n");
		return;
			}
			temp = find_player(ch, sub);
			if(temp == NULL) return;
			if(temp->baseball_position == TRUE) {
				send_to_char(ch, "이미 수비 라인업에 포함된 선수입니다.\r\n");
				return;
			}
			team = temp->company;
			if(!str_cmp(arg, "투수")) i = 0;
			else if(!str_cmp(arg, "포수")) i = 1;
			else if(!str_cmp(arg, "1루수")) i = 2;
			else if(!str_cmp(arg, "2루수")) i = 3;
			else if(!str_cmp(arg, "3루수")) i = 4;
			else if(!str_cmp(arg, "유격수")) i = 5;
			else if(!str_cmp(arg, "좌익수")) i = 6;
			else if(!str_cmp(arg, "중견수")) i = 7;
			else if(!str_cmp(arg, "우익수")) i = 8;
			if(team->defense[i] != NULL) {
				send_to_char(ch, "이미 지정된 포지션입니다.\r\n");
				return;
			}
			team->defense[i] = temp;
			temp->baseball_position = TRUE;
			sprintf(buf, "%s 팀의 %s, %s님을 소개합니다.\r\n", team->name, baseball_position[i], GET_NAME(temp));
			send_to_zone(buf, baseball.ground);
			return;
		}
		else if(!str_cmp(arg, "게임")) {
			if(ch->company == NULL || ch->company->master != ch || str_cmp(ch->company->name, baseball.top.name)) {
				send_to_char(ch, "게임시작 선언은 초 공격 팀 감독만 가능합니다.\r\n");
				return;
			}
//			if(is_ready() == 0) {
//				send_to_char(ch, "경기 시작 준비가 끝나지 않았습니다.\r\n");
//				return;
//			}
			if(!is_number(sub) || atoi(sub) < 1 || atoi(sub) > 9) {
				send_to_char(ch, "이닝은 1에서 9 사이의 값이어야 합니다.\r\n");
				return;
			}
			baseball.innings = atoi(sub);
			baseball.cur_inning = 1;
			baseball.bottom_hitting = FALSE;
			baseball.playing = TRUE;
			sprintf(buf, "지금부터 %s팀 대 %s팀, %s팀 대 %s팀의 %d이닝 경기를 %s 팀의 초 공격으로 시작하겠습니다.\r\n", baseball.top.name, baseball.bottom.name, baseball.bottom.name, baseball.top.name, baseball.innings, baseball.top.name);
			send_to_zone(buf, baseball.ground);
			locate_position();
			return;
		}
		else {
			send_to_char(ch, "공통: <전광판> | <탈퇴> | [ <명단> | <창단> | <입단> | <라인업> <팀이름> ] 야구\r\n감독: <해체> | [ <방출> | <타순지정> | <포지션> <선수이름> ] <게임> <이닝수> 야구\r\n");
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
				if(baseball.playing == TRUE) {
					send_to_char(ch, "경기 중\r\n");
					send_to_char(ch, "%d이닝 중 %d회 ", baseball.innings, baseball.cur_inning);
					if(baseball.bottom_hitting == FALSE) send_to_char(ch, "초 공격, 아웃카운트 %d\r\n", baseball.top.outcount);
					else send_to_char(ch, "말 공격, 아웃카운트 %d\r\n", baseball.bottom.outcount);
					if(baseball.bottom_hitting == FALSE) send_to_char(ch, "타자: %s, 스트라이크: %d, 볼: %d\r\n", GET_NAME(baseball.top.hitters[baseball.top.batter-9]), baseball.top.strike, baseball.top.ballcount);
					else send_to_char(ch, "타자: %s, 스트라이크: %d, 볼: %d\r\n", GET_NAME(baseball.bottom.hitters[baseball.bottom.batter-1]), baseball.bottom.strike, baseball.bottom.ballcount);
				}
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
//			if(baseball.playing == TRUE) {
//				send_to_char(ch, "경기가 진행중입니다.\r\n");
//				return;
//			}
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
					temp->baseball_position = FALSE;
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
					temp->baseball_position = FALSE;
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
			send_to_char(ch, "공통: <전광판> | <탈퇴> | [ <명단> | <창단> | <입단> | <라인업> <팀이름> ] 야구\r\n감독: <해체> | [ <방출> | <타순지정> | <포지션> <선수이름> ] <게임> <이닝수> 야구\r\n");
			return;
		}
	}
}

ACMD(do_pitch) {
	char buf[MAX_NAME_LENGTH], buf2[MAX_NAME_LENGTH], buf1[MAX_STRING_LENGTH];
	int location;
	two_arguments(argument, buf, buf2);
	location = GET_ROOM_VNUM(IN_ROOM(ch));
	if(ch->company == NULL || ch->company->defense[0] != ch || location != 18882) {
		send_to_char(ch, "투구는 투수가 투수 마운드 위에서만 할 수 있습니다.\r\n");
		return;
	}
	if(!*buf || !*buf2) {
		send_to_char(ch, "사용법: <투구지점> <속도> 투구\r\n");
		return;
	}
	if(!is_number(buf) || !is_number(buf2) || atoi(buf) < 3 || atoi(buf) > 7 || atoi(buf2) < 1 || atoi(buf2) > 3) {
		send_to_char(ch, "좌표는 3에서 7, 속도는 1에서 3 사이의 값이어야 합니다.\r\n");
		return;
	}
	if(baseball.bottom_hitting == TRUE)
		location = GET_ROOM_VNUM(IN_ROOM(baseball.bottom.hitters[baseball.bottom.batter-9]));
	else
		location = GET_ROOM_VNUM(IN_ROOM(baseball.top.hitters[baseball.top.batter-9]));
	if(location != 18880 && location != 18881) {
		send_to_char(ch, "타석이 비어있습니다.\r\n");
		return;
	}
	baseball.ball.x = atoi(buf);
	baseball.ball.dir = SOUTH;
	baseball.ball.speed = atoi(buf2);
	baseball.ball.clock = pulse;
	baseball.ball.moving = TRUE;
	sprintf(buf1, "%s님 투구\r\n", GET_NAME(ch));
	send_to_zone(buf1, baseball.ground);
	return;
}