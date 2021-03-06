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
#include "genzon.h" /* for real_zone_by_thing */
struct baseball_data baseball;

void record_count(int status) {
	char buf[MAX_STRING_LENGTH];
	struct baseball_team *t;
	if(baseball.bottom_hitting == TRUE) t = &baseball.bottom;
	else t = &baseball.top;
	if(status == 1) // strike
		t->strike += 1;
	else if(status == 2) t->ballcount += 1;
	else if(status == 3) { // 그냥 장타
		t->strike = 0;
		t->ballcount = 0;
		if(t->batter < 18) t->batter += 1;
		else t->batter = 9;
		return;
	}
	else if(status == 4) { // 홈런
		t->strike = 0;
		t->ballcount = 0;
		if(t->batter < 18) t->batter += 1;
		else t->batter = 9;
		t->runs += 1;
		return;
	}
	else if(status == 5) { // 그냥 아웃
		t->outcount += 1;
		t->strike = 0;
		t->ballcount = 0;
		sprintf(buf, "[문자중계] %d아웃!\r\n", t->outcount);
		send_to_zone(buf, baseball.ground);
	}
	if(t->ballcount == 4) {
		t->strike += 1;
		sprintf(buf, "[문자중계] 포볼!\r\n");
		t->ballcount = 0;
		send_to_zone(buf, baseball.ground);
	}
	if(t->strike == 3) {
		t->outcount += 1;
		sprintf(buf, "[문자중계] %d아웃!\r\n", t->outcount);
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
		t->basements[0] = NULL;
		t->basements[1] = NULL;
		t->basements[2] = NULL;
		if(baseball.bottom_hitting == TRUE) {
			baseball.cur_inning += 1;
			if(baseball.innings < baseball.cur_inning) {
				sprintf(buf, "[문자중계] 경기종료!\r\n");
				send_to_zone(buf, baseball.ground);
				return;
			}
			baseball.bottom_hitting = FALSE;
		}
		else baseball.bottom_hitting = TRUE;
		locate_position();
		sprintf(buf, "[문자중계] 공수교대!");
		send_to_zone(buf, baseball.ground);
	}
}

void baseball_base() {
	if(baseball.playing == TRUE) {
		struct baseball_team *t = NULL, *fielder;
		struct char_data *runner = NULL;
		struct obj_data *obj = NULL;
		int i;
		bool save = 0;
		char buf[MAX_STRING_LENGTH];
		if(baseball.bottom_hitting == TRUE) { t = &baseball.bottom; fielder = &baseball.top; }
		else { t = &baseball.top; fielder = &baseball.bottom; }
		runner = t->hitters[t->batter-9];
		if(GET_ROOM_VNUM(IN_ROOM(runner)) == 18820 || GET_ROOM_VNUM(IN_ROOM(runner)) == 18840 || GET_ROOM_VNUM(IN_ROOM(runner)) == 18860) {
			if(GET_ROOM_VNUM(IN_ROOM(runner)) == 18820 && baseball.ball.moving == FALSE)
				t->basements[3] = runner;
			else if(GET_ROOM_VNUM(IN_ROOM(runner)) == 18840 && baseball.ball.moving == FALSE)
				t->basements[1] = runner;
			else if(GET_ROOM_VNUM(IN_ROOM(runner)) == 18860 && baseball.ball.moving == FALSE)
				t->basements[2] = runner;
		}
		for(i=0; i<3; i++) {
			if(t->basements[i] != NULL) {
				if(i==0 && GET_ROOM_VNUM(IN_ROOM(t->basements[i])) == 18840 && baseball.ball.moving == FALSE) {
					t->basements[1] = t->basements[i];
					t->basements[i] = NULL;
				}
				else if(i==1 && GET_ROOM_VNUM(IN_ROOM(t->basements[i])) == 18860 && baseball.ball.moving == FALSE) {
					t->basements[2] = t->basements[i];
					t->basements[i] = NULL;
				}
				else if(i==2 && GET_ROOM_VNUM(IN_ROOM(t->basements[i])) == 18800 && baseball.ball.moving == FALSE) {
					t->basements[i] = NULL;
				t->inning_score[baseball.cur_inning-1] += 1;
					sprintf(buf, "%s팀 득점!\r\n", t->name);
					send_to_zone(buf, baseball.ground);				}
			}
		}
		for(i=0; i<9; i++) {
			obj = get_obj_in_list_vis(fielder->defense[i], "야구공", NULL, fielder->defense[i]->carrying);
			if(obj != NULL) {
				if(save == 1) { obj_from_char(obj); extract_obj(obj); }
				else {
					runner = NULL;
					if(GET_ROOM_VNUM(IN_ROOM(fielder->defense[i])) == 18820 && GET_ROOM_VNUM(IN_ROOM(t->hitters[t->batter-9])) >= 18801 && GET_ROOM_VNUM(IN_ROOM(t->hitters[t->batter-9])) <= 18819)
						runner = t->hitters[t->batter-9];
					else if(GET_ROOM_VNUM(IN_ROOM(fielder->defense[i])) == 18840 && t->basements[0] != NULL && GET_ROOM_VNUM(IN_ROOM(t->basements[0])) >= 18821 && GET_ROOM_VNUM(IN_ROOM(t->basements[0])) <= 18839) {
						runner = t->basements[0];
						t->basements[0] = NULL;
					}
					else if(GET_ROOM_VNUM(IN_ROOM(fielder->defense[i])) == 18860 && t->basements[1] != NULL && GET_ROOM_VNUM(IN_ROOM(t->basements[1])) >= 18841 && GET_ROOM_VNUM(IN_ROOM(t->basements[1])) <= 18859) {
						runner = t->basements[1];
						t->basements[1] = NULL;
					}
					if(runner != NULL) {
						char_from_room(runner);
						char_to_room(runner, find_target_room(runner, "18800"));
						act("방금 아웃된 $n님이 숨을 헐떡이며 나타납니다.", FALSE, runner, 0, 0, TO_ROOM);
						send_to_char(runner, "아웃되어 그라운드 밖으로 이동합니다.\r\n");
						record_count(5);
					}
				}
				break;
			}
		}
		if(t->outcount == 0) { // 쓰리아웃 되어서 공수교대된 경우
			obj_from_char(obj);
			extract_obj(obj);
			return;
		}
		if((GET_ROOM_VNUM(IN_ROOM(t->hitters[t->batter-9])) <= 18800 || GET_ROOM_VNUM(IN_ROOM(t->hitters[t->batter-9])) >= 18820) && (t->basements[0] == NULL || GET_ROOM_VNUM(IN_ROOM(t->basements[0])) <= 18820 || GET_ROOM_VNUM(IN_ROOM(t->basements[0])) >= 18840) && (t->basements[1] == NULL || GET_ROOM_VNUM(IN_ROOM(t->basements[1])) <= 18840 || GET_ROOM_VNUM(IN_ROOM(t->basements[1])) >= 18860) && (t->basements[2] == NULL || GET_ROOM_VNUM(IN_ROOM(t->basements[2])) <= 18860)) {
			obj_from_char(obj);
			extract_obj(obj);
			t->basements[0] = t->basements[3];
			t->basements[3] = NULL;
			record_count(3);
		}
	}
}

void locate_position() {
	struct baseball_team *t;
	int i;
	if(baseball.bottom_hitting == TRUE) t = &baseball.bottom;
	else t = &baseball.top;
	for(i=0; i<9; i++) {
		char_from_room(t->hitters[i]);
		char_to_room(t->hitters[i], find_target_room(t->hitters[i], "18800"));
	}
	if(baseball.bottom_hitting == TRUE) t = &baseball.top;
	else t = &baseball.bottom;
	for(i=0; i<9; i++) {
		char_from_room(t->defense[i]);
		char_to_room(t->defense[i], find_target_room(t->defense[i], baseball_defense_room[i]));
	}
}

void init_ball(struct baseball_ball *b) {
	b->x = 0;
	b->y = 20;
	b->z = 0;
	b->equation = 0;
	b->moving = FALSE;
	b->bound = FALSE;
	b->dist = 0;
}

void move_ball() {
	char buf[MAX_STRING_LENGTH];
	struct char_data *catcher = NULL;
	if(baseball.ball.moving == FALSE) return;
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
		baseball.ball.dist += baseball.ball.speed;
		switch (baseball.ball.equation) {
			case 0:
				break;
			case 1:
				if(baseball.ball.dist % 66 == 0) baseball.ball.z -= 1;
				break;
			case 2:
				if((-2*baseball.ball.dist)+114 >= 0) baseball.ball.z += 1; // z=-x(x+116) 을 미분한 값, 기울기로 증가/감소 판단
				else if((-2*baseball.ball.dist)+114 < 0) baseball.ball.z -= 1;
				break;
			case 3:
				if((2*baseball.ball.dist)-60 < 0) baseball.ball.z -= 1;
				break;
			default:
				break;
		}
	}
//	sprintf(buf, "%d, %d, %d, %d\r\n", baseball.ball.x, baseball.ball.y, baseball.ball.z, baseball.ball.dist);
//	send_to_zone(buf, baseball.ground);
	if(baseball.ball.z <= 0 && baseball.ball.equation != 0) {
		baseball.ball.equation = 0;
		baseball.ball.bound = TRUE;
		sprintf(buf, "[문자중계] 땅에 맞는 공!\r\n");
		send_to_zone(buf, baseball.ground);
	}
	if(baseball.ball.y <= 0) {
		baseball.ball.moving = FALSE;
		if(baseball.ball.x >= 4 && baseball.ball.x <= 6) {
			sprintf(buf, "[문자중계] 스트라이크!\r\n");
		send_to_zone(buf, baseball.ground);
			record_count(1);
		}
		else {
			sprintf(buf, "[문자중계] 볼!\r\n");
		send_to_zone(buf, baseball.ground);
			record_count(2);
		}
		return;
	}
	else if(baseball.ball.y >= 125) {
		baseball.ball.moving = FALSE;
		if(baseball.ball.z <= 2) {
			sprintf(buf, "[문자중계] 담장에 맞는 공!\r\n");
	send_to_zone(buf, baseball.ground);
			record_count(3);
		}
		else {
			sprintf(buf, "[문자중계] 호오오옴러어언!\r\n");
			send_to_zone(buf, baseball.ground);
			record_count(4);
		}
		return;
	}
	if(baseball.ball.z <= 3) {
		if(baseball.ball.y >= 17 && baseball.ball.y <= 19 && baseball.ball.x == 55 && world[real_room(18882)].people != NULL) {
			catcher = world[real_room(18882)].people;
		}
		else if(baseball.ball.y >= 20 && baseball.ball.y <= 30) {
			if(baseball.ball.x >= 78 && baseball.ball.x <= 80 && world[real_room(18911)].people != NULL)
				catcher = world[real_room(18911)].people;
			else if(baseball.ball.x >= 76 && baseball.ball.x <= 79 && world[real_room(18912)].people != NULL)
				catcher = world[real_room(18912)].people;
			else if(baseball.ball.x >= 74 && baseball.ball.x <= 77 && world[real_room(18913)].people != NULL)
				catcher = world[real_room(18913)].people;
			else if(baseball.ball.x >= 72 && baseball.ball.x <= 75 && world[real_room(18914)].people != NULL)
				catcher = world[real_room(18914)].people;
			else if(baseball.ball.x >= 70 && baseball.ball.x <= 73 && world[real_room(18915)].people != NULL)
				catcher = world[real_room(18915)].people;
			else if(baseball.ball.x == 70 && world[real_room(18916)].people != NULL)
				catcher = world[real_room(18916)].people;
			else if(baseball.ball.x == 69 && world[real_room(18917)].people != NULL)
				catcher = world[real_room(18917)].people;
			else if(baseball.ball.x == 68 && world[real_room(18918)].people != NULL)
				catcher = world[real_room(18918)].people;
			else if(baseball.ball.x == 67 && world[real_room(18919)].people != NULL)
				catcher = world[real_room(18919)].people;
			else if(baseball.ball.x == 66 && world[real_room(18920)].people != NULL)
				catcher = world[real_room(18920)].people;
			else if(baseball.ball.x >= 30 && baseball.ball.x <= 33 && world[real_room(18935)].people != NULL)
				catcher = world[real_room(18935)].people;
			else if(baseball.ball.x >= 32 && baseball.ball.x <= 35 && world[real_room(18934)].people != NULL)
				catcher = world[real_room(18934)].people;
			else if(baseball.ball.x >= 34 && baseball.ball.x <= 37 && world[real_room(18933)].people != NULL)
				catcher = world[real_room(18933)].people;
			else if(baseball.ball.x >= 36 && baseball.ball.x <= 39 && world[real_room(18932)].people != NULL)
				catcher = world[real_room(18932)].people;
			else if(baseball.ball.x >= 38 && baseball.ball.x <= 41 && world[real_room(18931)].people != NULL)
				catcher = world[real_room(18931)].people;
			else if(baseball.ball.x == 41 && world[real_room(18936)].people != NULL)
				catcher = world[real_room(18936)].people;
			else if(baseball.ball.x == 42 && world[real_room(18937)].people != NULL)
				catcher = world[real_room(18937)].people;
			else if(baseball.ball.x == 43 && world[real_room(18938)].people != NULL)
				catcher = world[real_room(18938)].people;
			else if(baseball.ball.x == 44 && world[real_room(18939)].people != NULL)
				catcher = world[real_room(18939)].people;
			else if(baseball.ball.x == 45 && world[real_room(18940)].people != NULL)
				catcher = world[real_room(18940)].people;
		}
		else if(baseball.ball.y >= 31 && baseball.ball.y <= 40) {
			if(baseball.ball.x == 70 && world[real_room(18916)].people != NULL)
				catcher = world[real_room(18916)].people;
			else if(baseball.ball.x == 69 && world[real_room(18917)].people != NULL)
				catcher = world[real_room(18917)].people;
			else if(baseball.ball.x == 68 && world[real_room(18918)].people != NULL)
				catcher = world[real_room(18918)].people;
			else if(baseball.ball.x == 67 && world[real_room(18919)].people != NULL)
				catcher = world[real_room(18919)].people;
			else if(baseball.ball.x == 66 && world[real_room(18920)].people != NULL)
				catcher = world[real_room(18920)].people;
			else if(baseball.ball.x >= 63 && baseball.ball.x <= 66 && world[real_room(18921)].people != NULL)
				catcher = world[real_room(18921)].people;
			else if(baseball.ball.x >= 61 && baseball.ball.x <= 64 && world[real_room(18922)].people != NULL)
				catcher = world[real_room(18922)].people;
			else if(baseball.ball.x >= 59 && baseball.ball.x <= 62 && world[real_room(18923)].people != NULL)
				catcher = world[real_room(18923)].people;
			else if(baseball.ball.x >= 57 && baseball.ball.x <= 60 && world[real_room(18924)].people != NULL)
				catcher = world[real_room(18924)].people;
			else if(baseball.ball.x >= 55 && baseball.ball.x <= 58 && world[real_room(18925)].people != NULL)
				catcher = world[real_room(18925)].people;
			else if(baseball.ball.x >= 53 && baseball.ball.x <= 56 && world[real_room(18941)].people != NULL)
				catcher = world[real_room(18941)].people;
			else if(baseball.ball.x >= 51 && baseball.ball.x <= 54 && world[real_room(18942)].people != NULL)
				catcher = world[real_room(18942)].people;
			else if(baseball.ball.x >= 49 && baseball.ball.x <= 52 && world[real_room(18943)].people != NULL)
				catcher = world[real_room(18943)].people;
			else if(baseball.ball.x >= 47 && baseball.ball.x <= 50 && world[real_room(18944)].people != NULL)
				catcher = world[real_room(18944)].people;
			else if(baseball.ball.x >= 45 && baseball.ball.x <= 48 && world[real_room(18945)].people != NULL)
				catcher = world[real_room(18945)].people;
			else if(baseball.ball.x == 41 && world[real_room(18936)].people != NULL)
				catcher = world[real_room(18936)].people;
			else if(baseball.ball.x == 42 && world[real_room(18937)].people != NULL)
				catcher = world[real_room(18937)].people;
			else if(baseball.ball.x == 43 && world[real_room(18938)].people != NULL)
				catcher = world[real_room(18938)].people;
			else if(baseball.ball.x == 44 && world[real_room(18939)].people != NULL)
				catcher = world[real_room(18939)].people;
			else if(baseball.ball.x == 45 && world[real_room(18940)].people != NULL)
				catcher = world[real_room(18940)].people;
		}
		else if(baseball.ball.y >= 61 && baseball.ball.y <= 80) {
			if(baseball.ball.x >= 40 && baseball.ball.x <= 43 && world[real_room(18976)].people != NULL)
				catcher = world[real_room(18976)].people;
			else if(baseball.ball.x >= 42 && baseball.ball.x <= 45 && world[real_room(18977)].people != NULL)
				catcher = world[real_room(18977)].people;
			else if(baseball.ball.x >= 44 && baseball.ball.x <= 47 && world[real_room(18978)].people != NULL)
				catcher = world[real_room(18978)].people;
			else if(baseball.ball.x >= 46 && baseball.ball.x <= 49 && world[real_room(18979)].people != NULL)
				catcher = world[real_room(18979)].people;
			else if(baseball.ball.x >= 48 && baseball.ball.x <= 51 && world[real_room(18980)].people != NULL)
				catcher = world[real_room(18980)].people;
			else if(baseball.ball.x >= 50 && baseball.ball.x <= 53 && world[real_room(18965)].people != NULL)
				catcher = world[real_room(18965)].people;
			else if(baseball.ball.x >= 52 && baseball.ball.x <= 55 && world[real_room(18964)].people != NULL)
				catcher = world[real_room(18964)].people;
			else if(baseball.ball.x >= 54 && baseball.ball.x <= 57 && world[real_room(18963)].people != NULL)
				catcher = world[real_room(18963)].people;
			else if(baseball.ball.x >= 56 && baseball.ball.x <= 59 && world[real_room(18962)].people != NULL)
				catcher = world[real_room(18962)].people;
			else if(baseball.ball.x >= 58 && baseball.ball.x <= 61 && world[real_room(18961)].people != NULL)
				catcher = world[real_room(18961)].people;
			else if(baseball.ball.x >= 60 && baseball.ball.x <= 63 && world[real_room(18960)].people != NULL)
				catcher = world[real_room(18960)].people;
			else if(baseball.ball.x >= 62 && baseball.ball.x <= 65 && world[real_room(18959)].people != NULL)
				catcher = world[real_room(18959)].people;
			else if(baseball.ball.x >= 64 && baseball.ball.x <= 67 && world[real_room(18958)].people != NULL)
				catcher = world[real_room(18958)].people;
			else if(baseball.ball.x >= 66 && baseball.ball.x <= 69 && world[real_room(18957)].people != NULL)
				catcher = world[real_room(18957)].people;
			else if(baseball.ball.x >= 68 && baseball.ball.x <= 71 && world[real_room(18956)].people != NULL)
				catcher = world[real_room(18956)].people;
		}
		else if(baseball.ball.y >= 81 && baseball.ball.y <= 100) {
			if(baseball.ball.x >= 30 && baseball.ball.x <= 33 && world[real_room(18971)].people != NULL)
				catcher = world[real_room(18971)].people;
			else if(baseball.ball.x >= 32 && baseball.ball.x <= 35 && world[real_room(18972)].people != NULL)
				catcher = world[real_room(18972)].people;
			else if(baseball.ball.x >= 34 && baseball.ball.x <= 37 && world[real_room(18973)].people != NULL)
				catcher = world[real_room(18973)].people;
			else if(baseball.ball.x >= 36 && baseball.ball.x <= 39 && world[real_room(18974)].people != NULL)
				catcher = world[real_room(18974)].people;
			else if(baseball.ball.x >= 38 && baseball.ball.x <= 41 && world[real_room(18975)].people != NULL)
				catcher = world[real_room(18975)].people;
			else if(baseball.ball.x >= 40 && baseball.ball.x <= 43 && world[real_room(18976)].people != NULL)
				catcher = world[real_room(18976)].people;
			else if(baseball.ball.x >= 42 && baseball.ball.x <= 45 && world[real_room(18977)].people != NULL)
				catcher = world[real_room(18977)].people;
			else if(baseball.ball.x >= 44 && baseball.ball.x <= 47 && world[real_room(18978)].people != NULL)
				catcher = world[real_room(18978)].people;
			else if(baseball.ball.x >= 46 && baseball.ball.x <= 49 && world[real_room(18979)].people != NULL)
				catcher = world[real_room(18979)].people;
			else if(baseball.ball.x >= 48 && baseball.ball.x <= 51 && world[real_room(18980)].people != NULL)
				catcher = world[real_room(18980)].people;
			else if(baseball.ball.x >= 60 && baseball.ball.x <= 63 && world[real_room(18960)].people != NULL)
				catcher = world[real_room(18960)].people;
			else if(baseball.ball.x >= 62 && baseball.ball.x <= 65 && world[real_room(18959)].people != NULL)
				catcher = world[real_room(18959)].people;
			else if(baseball.ball.x >= 64 && baseball.ball.x <= 67 && world[real_room(18958)].people != NULL)
				catcher = world[real_room(18958)].people;
			else if(baseball.ball.x >= 66 && baseball.ball.x <= 69 && world[real_room(18957)].people != NULL)
				catcher = world[real_room(18957)].people;
			else if(baseball.ball.x >= 68 && baseball.ball.x <= 71 && world[real_room(18956)].people != NULL)
				catcher = world[real_room(18956)].people;
			else if(baseball.ball.x >= 70 && baseball.ball.x <= 73 && world[real_room(18955)].people != NULL)
				catcher = world[real_room(18955)].people;
			else if(baseball.ball.x >= 72 && baseball.ball.x <= 75 && world[real_room(18954)].people != NULL)
				catcher = world[real_room(18954)].people;
			else if(baseball.ball.x >= 74 && baseball.ball.x <= 77 && world[real_room(18953)].people != NULL)
				catcher = world[real_room(18953)].people;
			else if(baseball.ball.x >= 76 && baseball.ball.x <= 79 && world[real_room(18952)].people != NULL)
				catcher = world[real_room(18952)].people;
			else if(baseball.ball.x >= 78 && baseball.ball.x <= 80 && world[real_room(18951)].people != NULL)
				catcher = world[real_room(18951)].people;
		}
		if(catcher != NULL) {
			baseball.ball.moving = FALSE;
			send_to_char(catcher, "나이스캐치!\r\n");
			if(GET_ROOM_VNUM(IN_ROOM(catcher)) == 18882)
				sprintf(buf, "[문자중계] %s님이 마운드 앞에서 잡는 공!\r\n", GET_NAME(catcher));
			else if(GET_ROOM_VNUM(IN_ROOM(catcher)) >= 18911 && GET_ROOM_VNUM(IN_ROOM(catcher)) <= 18920)
				sprintf(buf, "[문자중계] %s님이 1루 베이스 근처에서 잡는 공!\r\n", GET_NAME(catcher));
			else if(GET_ROOM_VNUM(IN_ROOM(catcher)) >= 18921 && GET_ROOM_VNUM(IN_ROOM(catcher)) <= 18925)
				sprintf(buf, "[문자중계] %s님이 2루 베이스 우측에서 잡는 공!\r\n", GET_NAME(catcher));
			else if(GET_ROOM_VNUM(IN_ROOM(catcher)) >= 18931 && GET_ROOM_VNUM(IN_ROOM(catcher)) <= 18940)
				sprintf(buf, "[문자중계] %s님이 3루 베이스 근처에서 잡는 공!\r\n", GET_NAME(catcher));
			else if(GET_ROOM_VNUM(IN_ROOM(catcher)) >= 18941 && GET_ROOM_VNUM(IN_ROOM(catcher)) <= 18945)
				sprintf(buf, "[문자중계] %s님이 2루 베이스 좌측에서 잡는 공!\r\n", GET_NAME(catcher));
			else if(GET_ROOM_VNUM(IN_ROOM(catcher)) >= 18961 && GET_ROOM_VNUM(IN_ROOM(catcher)) <= 18965)
				sprintf(buf, "[문자중계] %s님이 중견수 라인에서 잡는 공!\r\n", GET_NAME(catcher));
			else if(GET_ROOM_VNUM(IN_ROOM(catcher)) >= 18951 && GET_ROOM_VNUM(IN_ROOM(catcher)) <= 18960)
				sprintf(buf, "[문자중계] %s님이 우익수 라인에서 잡는 공!\r\n", GET_NAME(catcher));
			else if(GET_ROOM_VNUM(IN_ROOM(catcher)) >= 18971 && GET_ROOM_VNUM(IN_ROOM(catcher)) <= 18980)
				sprintf(buf, "[문자중계] %s님이 좌익수 라인에서 잡는 공!\r\n", GET_NAME(catcher));
			send_to_zone(buf, baseball.ground);
			struct obj_data *obj = read_object(real_object(18801), REAL); // 18801은 야구공 물건번호
			obj_to_char(obj, catcher);
			load_otrigger(obj);
		}
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
				sprintf(buf, "[팀정보] %s님이 이끄는 임시 야구단 %s이(가) 말 공격으로 경기를 준비합니다.\r\n", GET_NAME(ch), baseball.bottom.name);
				send_to_zone(buf, baseball.ground);
				return;
			}
			else {
				strcpy(baseball.top.name, sub);
				baseball.top.master = ch;
				baseball.top.member = ch;
				baseball.ground = real_zone_by_thing(zone);
				ch->company = &baseball.top;
				sprintf(buf, "[팀정보] %s님이 이끄는 임시 야구단 %s이(가) 초 공격으로 경기를 준비합니다.\r\n", GET_NAME(ch), baseball.top.name);
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
			sprintf(buf, "[팀정보] %s님이 %s 팀에 입단합니다.\r\n", GET_NAME(ch), ch->company->name);
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
			sprintf(buf, "[팀정보] %s님이 %s 팀에서 방출되었습니다.\r\n", GET_NAME(temp), temp->company->name);
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
			sprintf(buf, "[문자중계] %s 팀의 %d번 타자, %s님을 소개합니다.\r\n", team->name, i+1, GET_NAME(temp));
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
			sprintf(buf, "[문자중계] %s 팀의 %s, %s님을 소개합니다.\r\n", team->name, baseball_position[i], GET_NAME(temp));
			send_to_zone(buf, baseball.ground);
			return;
		}
		else if(!str_cmp(arg, "게임")) {
			if(ch->company == NULL || ch->company->master != ch || str_cmp(ch->company->name, baseball.top.name)) {
				send_to_char(ch, "게임시작 선언은 초 공격 팀 감독만 가능합니다.\r\n");
				return;
			}
			if(is_ready() == 0) {
				send_to_char(ch, "경기 시작 준비가 끝나지 않았습니다.\r\n");
				return;
			}
			if(!is_number(sub) || atoi(sub) < 1 || atoi(sub) > 9) {
				send_to_char(ch, "이닝은 1에서 9 사이의 값이어야 합니다.\r\n");
				return;
			}
			baseball.innings = atoi(sub);
			baseball.cur_inning = 1;
			baseball.bottom_hitting = FALSE;
			baseball.playing = TRUE;
			sprintf(buf, "[문자중계] 지금부터 %s팀 대 %s팀, %s팀 대 %s팀의 %d이닝 경기를 %s 팀의 초 공격으로 시작하겠습니다.\r\n", baseball.top.name, baseball.bottom.name, baseball.bottom.name, baseball.top.name, baseball.innings, baseball.top.name);
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
					if(baseball.bottom_hitting == TRUE) team = &baseball.bottom;
					else team = &baseball.top;
					send_to_char(ch, "경기 중\r\n");
					send_to_char(ch, "%d이닝 중 %d회 %s 공격, 아웃카운트 %d\r\n", baseball.innings, baseball.cur_inning, (baseball.bottom_hitting == TRUE ? "말" : "초"), team->outcount);
					send_to_char(ch, "출루정보: 1루 %s 2루 %s 3루 %s\r\n", team->basements[0] != NULL ? GET_NAME(team->basements[0]) : "없음", team->basements[1] != NULL ? GET_NAME(team->basements[1]) : "없음", team->basements[2] != NULL ? GET_NAME(team->basements[2]) : "없음");
					send_to_char(ch, "타자: %s, 스트라이크: %d, 볼: %d\r\n", GET_NAME(team->hitters[team->batter-9]), team->strike, team->ballcount);
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
				sprintf(buf, "[팀정보] %s님이 %s 팀을 해체했습니다.\r\n", GET_NAME(ch), baseball.top.name);
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
				sprintf(buf, "[팀정보] %s님이 %s 팀을 해체했습니다.\r\n", GET_NAME(ch), baseball.bottom.name);
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
			sprintf(buf, "[팀정보] %s님이 %s 팀을 탈퇴했습니다.\r\n", GET_NAME(ch), ch->company->name);
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
		send_to_char(ch, "사용법: <투구지점> <높이> 투구\r\n");
		return;
	}
	if(!is_number(buf) || !is_number(buf2) || atoi(buf) < 3 || atoi(buf) > 7 || atoi(buf2) < 2 || atoi(buf2) > 5) {
		send_to_char(ch, "좌표는 3에서 7, 높이는 2에서 5 사이의 값이어야 합니다.\r\n");
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
	init_ball(&baseball.ball);
	baseball.ball.x = atoi(buf);
	baseball.ball.z = atoi(buf2);
	baseball.ball.dir = SOUTH;
	baseball.ball.speed = 3;
	baseball.ball.moving = TRUE;
	sprintf(buf1, "%s님 투구 좌표: %d\r\n", GET_NAME(ch), atoi(buf));
	send_to_zone(buf1, baseball.ground);
	return;
}

ACMD(do_bat) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	int location, i;
	two_arguments(argument, arg1, arg2);
	location = GET_ROOM_VNUM(IN_ROOM(ch));
	if(ch->company == NULL || ch->company->hitters[ch->company->batter-9] != ch || (location != 18880 && location != 18881)) {
		send_to_char(ch, "타격은 타자가 타석에서만 할 수 있습니다.\r\n");
		return;
	}
	if(!*arg1 || !*arg2) {
		send_to_char(ch, "사용법: <타격지점> <높이> 타격\r\n");
		return;
	}
	if(!is_number(arg1) || !is_number(arg2) || atoi(arg1) < 3 || atoi(arg1) > 7 || atoi(arg2) < 1 || atoi(arg2) > 6) {
		send_to_char(ch, "좌표는 5에서 5, 높이는 1에서 6 사이의 값이어야 합니다.\r\n");
		return;
	}
	if(baseball.ball.moving == FALSE) {
		send_to_char(ch, "투수가 아직 공을 던지지 않았습니다.\r\n");
		return;
	}
	if(baseball.ball.y > 5) {
		sprintf(buf, "[문자중계] 헛스윙!\r\n");
		send_to_zone(buf, baseball.ground);
		baseball.ball.moving == FALSE;
		record_count(1);
		return;
	}
	else {
		if(atoi(arg1) == baseball.ball.x) baseball.ball.dir = NORTH;
		if(atoi(arg2)-baseball.ball.z >= -1 && atoi(arg2)-baseball.ball.z <= 1) {
			sprintf(buf, "[문자중계] 쳤습니다!\r\n");
			char_from_room(ch);
			char_to_room(ch, find_target_room(ch, "18801"));
			for(i=0; i<3; i++) {
				if(ch->company->basements[i] != NULL) {
					char_from_room(ch->company->basements[i]);
					if(i==0) char_to_room(ch->company->basements[i], find_target_room(ch->company->basements[i], "18821"));
					else if(i==1) char_to_room(ch->company->basements[i], find_target_room(ch->company->basements[i], "18841"));
					else if(i==2) char_to_room(ch->company->basements[i], find_target_room(ch->company->basements[i], "18861"));
				}
			}
			ch->company->hits += 1;
			ch->company->inning_score[baseball.cur_inning-1] += 1;
			if(atoi(arg2) == baseball.ball.z) baseball.ball.equation = 1;
			else if(baseball.ball.z - atoi(arg2) == 1) baseball.ball.equation = 2;
			else if(atoi(arg2) - baseball.ball.z == 1) baseball.ball.equation = 3;
			baseball.ball.x = (baseball.ball.x *10) + 5; // 55가 중앙, 2루 베이스 기준
		}
		else if(atoi(arg2) - baseball.ball.z >= 2 || atoi(arg2) - baseball.ball.z <= -2) {
			sprintf(buf, "[문자중계] 헛스윙!\r\n");
			baseball.ball.moving = FALSE;
			record_count(1);
		}
		send_to_zone(buf, baseball.ground);
		return;
	}
}

ACMD(do_pass) {
	char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	int i;
	struct baseball_team *t = ch->company;
	struct obj_data *obj = get_obj_in_list_vis(ch, "야구공", NULL, ch->carrying);
	one_argument(argument, arg);
	if(t==NULL) {
		send_to_char(ch, "야구 팀에 소속되어 있지 않습니다.\r\n");
		return;
	}
	if(obj == NULL) {
		send_to_char(ch, "손에 야구공이 없습니다.\r\n");
		return;
	}
	if(!*arg) {
		send_to_char(ch, "누구에게 송구할까요?\r\n");
		return;
	}
	for(i=0; i<9; i++) {
		if(!str_cmp(arg, baseball_position[i])) {
			// 1초 딜레이
			obj_from_char(obj);
			obj_to_char(obj, t->defense[i]);
			send_to_char(t->defense[i], "나이스캐치!\r\n");
			sprintf(buf, "[문자중계] %s님 %s님에게 송구!\r\n", GET_NAME(ch), GET_NAME(t->defense[i]));
			send_to_zone(buf, baseball.ground);
			return;
		}
	}
	send_to_char(ch, "송구받을 포지션을 입력해 주세요.\r\n");
	return;
}