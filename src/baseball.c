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
		send_to_char(ch, "����: <������> | <Ż��> | [ <���> | <â��> | <�Դ�> | <���ξ�> <���̸�> ] �߱�\r\n����: <��ü> | [ <����> | <Ÿ������> <�����̸�> ] �߱�\r\n");
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
					send_to_char(ch, "%d��Ÿ��: %s %s\r\n", i+1, GET_NAME(team->defense[cnt]), GET_NAME(team->hitters[i]));
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
			sprintf(buf, "%s ���� %d�� Ÿ�� %s���� �Ұ��մϴ�.\r\n", team->name, i+1, GET_NAME(temp));
			send_to_zone(buf, baseball.ground);
			team->hitters[i] = temp;
			team->batter += 1;
			return;
		}
		else {
			send_to_char(ch, "����: <������> | <Ż��> | [ <���> | <â��> | <�Դ�> | <���ξ�> <���̸�> ] �߱�\r\n����: <��ü> | [ <����> | <Ÿ������> <�����̸�> ] �߱�\r\n");
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
				if(baseball.playing == TRUE)
					send_to_char(ch, "��� ��\r\n");
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
			if(baseball.playing == TRUE) {
				send_to_char(ch, "��Ⱑ �������Դϴ�.\r\n");
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
			send_to_char(ch, "����: <������> | <Ż��> | [ <���> | <â��> | <�Դ�> | <���ξ�> <���̸�> ] �߱�\r\n����: <��ü> | [ <����> | <Ÿ������> <�����̸�> ] �߱�\r\n");
			return;
		}
	}
}
