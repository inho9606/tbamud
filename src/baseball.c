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

void record_count(int status) {
	char buf[MAX_STRING_LENGTH];
	struct baseball_team *t;
	if(baseball.bottom_hitting == TRUE) t = &baseball.bottom;
	else t = &baseball.top;
	if(status == 1) // strike
		t->strike += 1;
	if(status == 2) t->ballcount += 1;
	if(status == 3) {
		t->strike = 0;
		t->ballcount = 0;
		t->outcount = 0;
		if(t->batter < 18) t->batter += 1;
		else t->batter = 9;
		return;
	}
	if(status == 4) {
		t->strike = 0;
		t->ballcount = 0;
		t->outcount = 0;
		if(t->batter < 18) t->batter += 1;
		else t->batter = 9;
		t->runs += 1;
		return;
	}
	if(t->ballcount == 4) {
		t->strike += 1;
		sprintf(buf, "�����Դϴ�.\r\n");
		t->ballcount = 0;
		send_to_zone(buf, baseball.ground);
	}
	if(t->strike == 3) {
		t->outcount += 1;
		sprintf(buf, "%d�ƿ�!\r\n", t->outcount);
		send_to_zone(buf, baseball.ground);
		t->strike = 0;
		t->ballcount = 0;
		if(t->batter < 18) t->batter += 1;
		else t->batter = 9;
	}
	if(t->outcount == 3) {
		t->strike = 0;
		t->ballcount = 0;
		t->outcount = 0;
		if(baseball.bottom_hitting == TRUE) baseball.bottom_hitting = FALSE;
		else baseball.bottom_hitting = TRUE;
		locate_position();
		sprintf(buf, "������ȯ!");
		send_to_zone(buf, baseball.ground);
	}
}

void locate_position() {
	struct baseball_team *t;
	int i;
	if(baseball.bottom_hitting == TRUE) t = &baseball.top;
	else t = &baseball.bottom;
	for(i=0; i<9; i++) {
		if(i==0) { // ������ �� �ʱ� ���ȣ �迭 �����
			char_from_room(t->defense[i]);
			char_to_room(t->defense[i], find_target_room(t->defense[i], "18882"));
		}
	}
}

void init_ball(struct baseball_ball *b) {
	b->x = 0;
	b->y = 20;
	b->z = 0;
	b->equation = 0;
	b->moving = FALSE;
	b->dist = 0;
}

void move_ball(int ms) {
	char buf[MAX_STRING_LENGTH];
//	sprintf(buf, "%d, %d, %d, %d\r\n", pulse-baseball.ball.clock, ms, ms-baseball.ball.clock);
//	send_to_zone(buf, baseball.ground);
	if(baseball.ball.moving == FALSE) return;
	baseball.ball.dist += baseball.ball.speed;
	switch (baseball.ball.dir) {
		case SOUTH:
			baseball.ball.y -= baseball.ball.speed;
			break;
		case NORTH:
			baseball.ball.y += baseball.ball.speed;
			break;
		case EAST:
			baseball.ball.x += baseball.ball.speed;
			break;
		case WEST:
			baseball.ball.x -= baseball.ball.speed;
			break;
		default:
			break;
	}
	if(baseball.ball.dir != SOUTH) {
		switch (baseball.ball.equation) {
			case 1:
				if(baseball.ball.dist % 60 == 0) baseball.ball.z -= 1;
				break;
			default:
				break;
		}
	}
	if(baseball.ball.z <= 0) baseball.ball.equation = 0;
	if(baseball.ball.y <= 0) {
		baseball.ball.moving = FALSE;
		if(baseball.ball.x >= 4 && baseball.ball.x <= 6) {
			sprintf(buf, "��Ʈ����ũ!\r\n");
			record_count(1);
		}
		else {
			sprintf(buf, "��!\r\n");
			record_count(2);
		}
	}
		else if(baseball.ball.y >= 125) {
			baseball.ball.moving = FALSE;
			if(baseball.ball.z < 5) {
				sprintf(buf, "���忡 �´� ��!\r\n");
				record_count(3);
			}
			else {
				sprintf(buf, "ȣ�����ȷ����!\r\n");
				record_count(4);
			}
		}
		send_to_zone(buf, baseball.ground);
		return;
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
		send_to_char(ch, "�׷� ������ �����ϴ�.\r\n");
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
		send_to_char(ch, "����: <������> | <Ż��> | [ <���> | <â��> | <�Դ�> | <���ξ�> <���̸�> ] �߱�\r\n����: <��ü> | [ <����> | <Ÿ������> | <������> <�����̸�> ] <����> <�̴׼�> �߱�\r\n");
		return;
	}
	if(*sub) {
		if(!str_cmp(arg, "���")) {
			cnt = 0;
			if(!str_cmp(sub, baseball.top.name)) {
				for(temp = baseball.top.member; temp != NULL; temp = temp->next_baseball) {
					send_to_char(ch, "%s ", GET_NAME(temp));
					cnt++;
				}
				send_to_char(ch, "\r\n");
				send_to_char(ch, "%d���� ������ �ֽ��ϴ�.\r\n", cnt);
				return;
			}
			else if(!str_cmp(sub, baseball.bottom.name)) {
				for(temp = baseball.bottom.member; temp != NULL; temp = temp->next_baseball) {
					send_to_char(ch, "%s ", GET_NAME(temp));
					cnt++;
				}
				send_to_char(ch, "\r\n");
				send_to_char(ch, "%d���� ������ �ֽ��ϴ�.\r\n", cnt);
				return;
			}
			else {
				send_to_char(ch, "�׷� ���� �����ϴ�.\r\n");
				return;
			}
		}
		else if(!str_cmp(arg, "â��")) {
			zone = GET_ROOM_VNUM(IN_ROOM(ch));
			if(zone/100 != 188) { // �߱��� ���÷��� �߰� ����
				send_to_char(ch, "�� â���� �߱��忡���� �� �� �ֽ��ϴ�.\r\n");
				return;
			}
			if(ch->company != NULL) {
				send_to_char(ch, "�̹� �ٸ� ���� �ҼӵǾ� �ֽ��ϴ�.\r\n");
				return;
			}
			if(baseball.playing == TRUE) {
				send_to_char(ch, "��Ⱑ �������Դϴ�.\r\n");
				return;
			}
			if(*(baseball.bottom.name)) {
				send_to_char(ch, "�̹� �� ���� ��⸦ �غ����Դϴ�.\r\n");
				return;
			}
			if(*(baseball.top.name)) {
				if(!str_cmp(sub, baseball.top.name)) {
					send_to_char(ch, "���� �̸��� ���� �ֽ��ϴ�.\r\n");
					return;
				}
				strcpy(baseball.bottom.name, sub);
				baseball.bottom.master = ch;
				baseball.bottom.member = ch;
				ch->company = &baseball.bottom;
				sprintf(buf, "%s���� �̲��� �ӽ� �߱��� %s��(��) �� �������� ��⸦ �غ��մϴ�.\r\n", GET_NAME(ch), baseball.bottom.name);
				send_to_zone(buf, baseball.ground);
				return;
			}
			else {
				strcpy(baseball.top.name, sub);
				baseball.top.master = ch;
				baseball.top.member = ch;
				baseball.ground = real_zone_by_thing(zone);
				ch->company = &baseball.top;
				sprintf(buf, "%s���� �̲��� �ӽ� �߱��� %s��(��) �� �������� ��⸦ �غ��մϴ�.\r\n", GET_NAME(ch), baseball.top.name);
				send_to_zone(buf, baseball.ground);
				return;
			}
		}
		else if(!str_cmp(arg, "�Դ�")) {
			if(ch->company != NULL) {
				send_to_char(ch, "�̹� �ٸ� ���� �ҼӵǾ� �ֽ��ϴ�.\r\n");
				return;
			}
			if(!str_cmp(sub, baseball.top.name))
				ch->company = &baseball.top;
			else if(!str_cmp(sub, baseball.bottom.name))
				ch->company = &baseball.bottom;
			else {
				send_to_char(ch, "�׷� ���� �����ϴ�.\r\n");
				return;
			}
			for(temp = ch->company->member; temp->next_baseball != NULL; temp = temp->next_baseball) ;
			temp->next_baseball = ch;
			sprintf(buf, "%s���� %s ���� �Դ��մϴ�.\r\n", GET_NAME(ch), ch->company->name);
			send_to_zone(buf, baseball.ground);
			return;
		}
		else if(!str_cmp(arg, "����")) {
			if(ch->company == NULL || ch->company->master != ch) {
				send_to_char(ch, "���� �����Դϴ�.\r\n");
				return;
			}
			for(temp = ch->company->member; temp != NULL && str_cmp(sub, GET_NAME(temp)); temp = temp->next_baseball) ;
			if(temp == NULL) {
				send_to_char(ch, "�׷� ������ �����ϴ�.\r\n");
				return;
			}
			if(!str_cmp(GET_NAME(temp), GET_NAME(ch))) {
				send_to_char(ch, "Ż�� �ϼ���.\r\n");
				return;
			}
			if(temp->baseball_position == TRUE) {
				send_to_char(ch, "���ξ��� ���Ե� ������ ������ �� �����ϴ�.\r\n");
				return;
			}
			for(i=0; i<9; i++) {
				if(ch->company->hitters[i] == temp) {
					send_to_char(ch, "���ξ��� ���Ե� ������ ������ �� �����ϴ�.\r\n");
					return;
				}
			}
			sprintf(buf, "%s���� %s ������ ����Ǿ����ϴ�.\r\n", GET_NAME(temp), temp->company->name);
			send_to_zone(buf, baseball.ground);
			leave_team(temp);
		}
		else if(!str_cmp(arg, "���ξ�")) {
			if(!str_cmp(sub, baseball.top.name))
				team = &baseball.top;
			else if(!str_cmp(sub, baseball.bottom.name))
				team = &baseball.bottom;
			else {
				send_to_char(ch, "�׷� ���� �����ϴ�.\r\n");
				return;
			}
			send_to_char(ch, "����: %s\r\n", GET_NAME(team->master));
			for(i=0; i<9; i++) {
				cnt = find_position(team->hitters[i]);
				if(cnt == -2)
					send_to_char(ch, "%d��Ÿ��: ������ ������\r\n", i+1);
				else if(cnt == -1)
					send_to_char(ch, "%d��Ÿ��: ������ %s\r\n", i+1, GET_NAME(team->hitters[i]));
				else
					send_to_char(ch, "%d��Ÿ��: %s %s\r\n", i+1, baseball_position[cnt], GET_NAME(team->hitters[i]));
			}
		return;
		}
		else if(!str_cmp(arg, "Ÿ������")) {
			if(ch->company == NULL || ch->company->master != ch) {
				send_to_char(ch, "���� �����Դϴ�.\r\n");
				return;
			}
			team = ch->company;
			if(team->batter >= 9) {
				send_to_char(ch, "Ÿ�� ������ �Ϸ�Ǿ����ϴ�.\r\n");
				return;
			}
			for(temp = team->member; temp != NULL && str_cmp(sub, GET_NAME(temp)); temp = temp->next_baseball) ;
			if(temp == NULL) {
				send_to_char(ch, "�׷� ������ �����ϴ�.\r\n");
				return;
			}
			for(i=0; i<team->batter; i++) {
				if(team->hitters[i] == temp) {
					send_to_char(ch, "�̹� Ÿ���� ������ �����Դϴ�.\r\n");
					return;
				}
			}
			sprintf(buf, "%s ���� %d�� Ÿ��, %s���� �Ұ��մϴ�.\r\n", team->name, i+1, GET_NAME(temp));
			send_to_zone(buf, baseball.ground);
			team->hitters[i] = temp;
			team->batter += 1;
			return;
		}
		else if(!str_cmp(arg, "����") || !str_cmp(arg, "����") || !str_cmp(arg, "1���") || !str_cmp(arg, "2���") || !str_cmp(arg, "3���") || !str_cmp(arg, "���ݼ�") || !str_cmp(arg, "�߰߼�") || !str_cmp(arg, "���ͼ�") || !str_cmp(arg, "���ͼ�")) {
			if(ch->company == NULL || ch->company->master != ch) {
				send_to_char(ch, "���� �����Դϴ�.\r\n");
		return;
			}
			temp = find_player(ch, sub);
			if(temp == NULL) return;
			if(temp->baseball_position == TRUE) {
				send_to_char(ch, "�̹� ���� ���ξ��� ���Ե� �����Դϴ�.\r\n");
				return;
			}
			team = temp->company;
			if(!str_cmp(arg, "����")) i = 0;
			else if(!str_cmp(arg, "����")) i = 1;
			else if(!str_cmp(arg, "1���")) i = 2;
			else if(!str_cmp(arg, "2���")) i = 3;
			else if(!str_cmp(arg, "3���")) i = 4;
			else if(!str_cmp(arg, "���ݼ�")) i = 5;
			else if(!str_cmp(arg, "���ͼ�")) i = 6;
			else if(!str_cmp(arg, "�߰߼�")) i = 7;
			else if(!str_cmp(arg, "���ͼ�")) i = 8;
			if(team->defense[i] != NULL) {
				send_to_char(ch, "�̹� ������ �������Դϴ�.\r\n");
				return;
			}
			team->defense[i] = temp;
			temp->baseball_position = TRUE;
			sprintf(buf, "%s ���� %s, %s���� �Ұ��մϴ�.\r\n", team->name, baseball_position[i], GET_NAME(temp));
			send_to_zone(buf, baseball.ground);
			return;
		}
		else if(!str_cmp(arg, "����")) {
			if(ch->company == NULL || ch->company->master != ch || str_cmp(ch->company->name, baseball.top.name)) {
				send_to_char(ch, "���ӽ��� ������ �� ���� �� ������ �����մϴ�.\r\n");
				return;
			}
//			if(is_ready() == 0) {
//				send_to_char(ch, "��� ���� �غ� ������ �ʾҽ��ϴ�.\r\n");
//				return;
//			}
			if(!is_number(sub) || atoi(sub) < 1 || atoi(sub) > 9) {
				send_to_char(ch, "�̴��� 1���� 9 ������ ���̾�� �մϴ�.\r\n");
				return;
			}
			baseball.innings = atoi(sub);
			baseball.cur_inning = 1;
			baseball.bottom_hitting = FALSE;
			baseball.playing = TRUE;
			sprintf(buf, "���ݺ��� %s�� �� %s��, %s�� �� %s���� %d�̴� ��⸦ %s ���� �� �������� �����ϰڽ��ϴ�.\r\n", baseball.top.name, baseball.bottom.name, baseball.bottom.name, baseball.top.name, baseball.innings, baseball.top.name);
			send_to_zone(buf, baseball.ground);
			locate_position();
			return;
		}
		else {
			send_to_char(ch, "����: <������> | <Ż��> | [ <���> | <â��> | <�Դ�> | <���ξ�> <���̸�> ] �߱�\r\n����: <��ü> | [ <����> | <Ÿ������> | <������> <�����̸�> ] <����> <�̴׼�> �߱�\r\n");
			return;
		}
	}
	else {
		if(!str_cmp(arg, "������")) {
			if(!*(baseball.top.name)) {
				send_to_char(ch, "������� ���� �����ϴ�.\r\n");
				return;
			}
			else {
				if(baseball.playing == TRUE) {
					send_to_char(ch, "��� ��\r\n");
					send_to_char(ch, "%d�̴� �� %dȸ ", baseball.innings, baseball.cur_inning);
					if(baseball.bottom_hitting == FALSE) send_to_char(ch, "�� ����, �ƿ�ī��Ʈ %d\r\n", baseball.top.outcount);
					else send_to_char(ch, "�� ����, �ƿ�ī��Ʈ %d\r\n", baseball.bottom.outcount);
					if(baseball.bottom_hitting == FALSE) send_to_char(ch, "Ÿ��: %s, ��Ʈ����ũ: %d, ��: %d\r\n", GET_NAME(baseball.top.hitters[baseball.top.batter-9]), baseball.top.strike, baseball.top.ballcount);
					else send_to_char(ch, "Ÿ��: %s, ��Ʈ����ũ: %d, ��: %d\r\n", GET_NAME(baseball.bottom.hitters[baseball.bottom.batter-1]), baseball.bottom.strike, baseball.bottom.ballcount);
				}
				else
					send_to_char(ch, "��� �غ� ��\r\n");
				send_to_char(ch, "%s�� - R: %d, H: %d, E: %d, B: %d\r\n", baseball.top.name, baseball.top.runs, baseball.top.hits, baseball.top.errors, baseball.top.bb);
				if(*(baseball.bottom.name))
					send_to_char(ch, "%s�� - R: %d, H: %d, E: %d, B: %d\r\n", baseball.bottom.name, baseball.bottom.runs, baseball.bottom.hits, baseball.bottom.errors, baseball.bottom.bb);
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
		else if(!str_cmp(arg, "��ü")) {
			if(ch->company == NULL || ch->company->master != ch) {
				send_to_char(ch, "���� �����Դϴ�.\r\n");
				return;
			}
//			if(baseball.playing == TRUE) {
//				send_to_char(ch, "��Ⱑ �������Դϴ�.\r\n");
//				return;
//			}
			baseball.playing = FALSE;
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
				sprintf(buf, "%s���� %s ���� ��ü�߽��ϴ�.\r\n", GET_NAME(ch), baseball.top.name);
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
				sprintf(buf, "%s���� %s ���� ��ü�߽��ϴ�.\r\n", GET_NAME(ch), baseball.bottom.name);
				send_to_zone(buf, baseball.ground);
				strcpy(baseball.bottom.name, "");
			}
		}
		else if(!str_cmp(arg, "Ż��")) {
			if(ch->company == NULL) {
				send_to_char(ch, "�Ҽ� ���� �����ϴ�.\r\n");
				return;
			}
			if(ch->company->master == ch) {
				send_to_char(ch, "������ Ż���� �� �����ϴ�.\r\n");
				return;
			}
			if(ch->baseball_position == TRUE) {
				send_to_char(ch, "���ξ��� ���Ե� ������ Ż���� �� �����ϴ�.\r\n");
				return;
			}
			for(i=0; i<9; i++) {
				if(ch->company->hitters[i] == ch) {
					send_to_char(ch, "���ξ��� ���Ե� ������ Ż���� �� �����ϴ�.\r\n");
					return;
				}
			}
			sprintf(buf, "%s���� %s ���� Ż���߽��ϴ�.\r\n", GET_NAME(ch), ch->company->name);
			send_to_zone(buf, baseball.ground);
			leave_team(ch);
		}
		else {
			send_to_char(ch, "����: <������> | <Ż��> | [ <���> | <â��> | <�Դ�> | <���ξ�> <���̸�> ] �߱�\r\n����: <��ü> | [ <����> | <Ÿ������> | <������> <�����̸�> ] <����> <�̴׼�> �߱�\r\n");
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
		send_to_char(ch, "������ ������ ���� ����� �������� �� �� �ֽ��ϴ�.\r\n");
		return;
	}
	if(!*buf || !*buf2) {
		send_to_char(ch, "����: <��������> <����> ����\r\n");
		return;
	}
	if(!is_number(buf) || !is_number(buf2) || atoi(buf) < 5 || atoi(buf) > 5 || atoi(buf2) < 2 || atoi(buf2) > 5) {
		send_to_char(ch, "��ǥ�� 5���� 5, ���̴� 2���� 5 ������ ���̾�� �մϴ�.\r\n");
		return;
	}
	if(baseball.bottom_hitting == TRUE)
		location = GET_ROOM_VNUM(IN_ROOM(baseball.bottom.hitters[baseball.bottom.batter-9]));
	else
		location = GET_ROOM_VNUM(IN_ROOM(baseball.top.hitters[baseball.top.batter-9]));
	if(location != 18880 && location != 18881) {
		send_to_char(ch, "Ÿ���� ����ֽ��ϴ�.\r\n");
		return;
	}
	init_ball(&baseball.ball);
	baseball.ball.x = atoi(buf);
	baseball.ball.z = atoi(buf2);
	baseball.ball.dir = SOUTH;
	baseball.ball.speed = 3;
	baseball.ball.moving = TRUE;
	sprintf(buf1, "%s�� ����\r\n", GET_NAME(ch));
	send_to_zone(buf1, baseball.ground);
	return;
}

ACMD(do_bat) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	int location;
	two_arguments(argument, arg1, arg2);
	location = GET_ROOM_VNUM(IN_ROOM(ch));
	if(ch->company == NULL || ch->company->hitters[ch->company->batter-9] != ch || (location != 18880 && location != 18881)) {
		send_to_char(ch, "Ÿ���� Ÿ�ڰ� Ÿ�������� �� �� �ֽ��ϴ�.\r\n");
		return;
	}
	if(!*arg1 || !*arg2) {
		send_to_char(ch, "����: <Ÿ������> <����> Ÿ��\r\n");
		return;
	}
	if(!is_number(arg1) || !is_number(arg2) || atoi(arg1) < 5 || atoi(arg1) > 5 || atoi(arg2) < 1 || atoi(arg2) > 6) {
		send_to_char(ch, "��ǥ�� 5���� 5, ���̴� 1���� 6 ������ ���̾�� �մϴ�.\r\n");
		return;
	}
	if(baseball.ball.moving == FALSE) {
		send_to_char(ch, "������ ���� ���� ������ �ʾҽ��ϴ�.\r\n");
		return;
	}
	if(baseball.ball.y > 5) {
		sprintf(buf, "�꽺��!\r\n");
		send_to_zone(buf, baseball.ground);
		baseball.ball.moving == FALSE;
		record_count(1);
		return;
	}
	if(baseball.ball.y <= 5) {
		if(atoi(arg1) && baseball.ball.x) {
			if(atoi(arg2) && baseball.ball.z) {
				sprintf(buf, "�ƽ��ϴ�!\r\n");
				ch->company->hits += 1;
				baseball.ball.dir = NORTH;
				baseball.ball.equation = 1;
				baseball.ball.z *= 2;
			}
			else if(atoi(arg2) - baseball.ball.z >= 2 || atoi(arg2) - baseball.ball.z <= -2) {
				sprintf(buf, "�꽺��!\r\n");
				baseball.ball.moving = FALSE;
				record_count(1);
			}
			send_to_zone(buf, baseball.ground);
			return;
		}
	}
}