/* �ѱ�ó�� */
#include "kstbl.h"
#include "conf.h" 
#include "sysdep.h" 

int is_hangul(str)
unsigned char *str;
{
  /* �����ѱ����� ���θ� �˻� */
   if(str[0]>=0xb0 && str[0]<=0xc8 && str[1]>=0xa1 &&
      str[1]<=0xfe)
     return 1;
   return 0;
}

int is_han(str)
unsigned char *str;
{
  int i;
  for (i = 0; i < strlen(str);i += 2) {
    if (!is_hangul(str+i)) return 0;
  }
  return 1;
}

/* ��ħ�� �ִ��� ������ �˻��ϴ� �κ��Դϴ�. ����ó���� ���� */
int under_han(str)
char *str;
{
  unsigned char high, low;
  int hightmp, lowtmp;
  int len;
  int found = 0;

  len = strlen(str);

  if(len < 2)
    return 0;
  lowtmp = str[len - 1];
  len -= 2;
  hightmp = str[len];
  if(lowtmp > -1) {
    while(len) {
      len--;
      if(len < 0)
        break;
      lowtmp = hightmp;
      hightmp = str[len];
      if((hightmp < -1) && (lowtmp < -1)) { 
        found = 1;
        break;
      }
    }
  } else
    found = 1;
  if(!found)  return 0;
  high = hightmp;
  low = lowtmp;
  high = KStbl[(high - 0xb0) * 94 + low - 0xa1] & 0x1f;
  if(high < 2 || high > 28)
    return 0;
  return 1;
}



/* ù������ �ʼ��� ã�� �κ��Դϴ�. */
char *first_han(unsigned char *str)
{
    int i, Length;
    char *Result = "ZZZ";
    unsigned char str_0, str_1;

    static unsigned char *exam[]={
        "��", "��", "��", "��", "��",
        "��", "��", "��", "��", "��",
        "��", "��", "��", "��", "��",
        "ī", "Ÿ", "��", "��", "" };
    static unsigned char *johab_exam[]={
        "�a", "�a", "�a", "�a", "�a",
        "�a", "�a", "?", "?", "?",
        "?", "?", "?", "?", "?",
        "?", "?", "?", "?", "" };  

/*    static unsigned char *exam[]={
        "��", "��", "��", "��", "��",
        "��", "��", "��", "��", "��",
        "��", "��", "��", "��", "��",
        "��", "��", "��", "��", "" };
// �̰� ���������� �Է��� �ϼž� �մϴ�.
    static unsigned char *johab_exam[]={
        "~Ha", "~La", "~Pa", "~Ta", "~Xa",
        "~\a", "| a", "��", "��", "��",
        "��", "��", "��", "¥", "��",
        "ī", "Ÿ", "��", "��", "" };
*/

    Length = strlen(str);
    if (Length < 2)
        return Result;

    str_0 = str[0]; str_1 = str[1];
    if (!is_hangul(&str[0]))
        return Result;

    str_0 = (KStbl[(str_0 - 0xb0) * 94 + str_1 - 0xa1] >> 8) & 0x7c;
    for(i = 0; johab_exam[i][0]; i++)
    {
        str_1 = (johab_exam[i][0] & 0x7f);  
        if(str_1 == str_0)
       //     return exam[i];
       return Result;
    }
    return Result;
}


char *check_josa(char *str, int m)
{
	int yn = 0;
	char *josalist[7][2] =  {
		{ "��", "��" }, { "��", "��" }, { "��", "��" }, 
		{ "�̶��", "���" }, { "��", "��" }, { "����", "��" }, { "\n", "\n"}}; 
	char *re=NULL;

	if(!str || !*str) {
		re = "<����!>";
		return(re);
	}
	if(under_han(str))
		yn = 0;
	else   
		yn = 1;
	re = josalist[m][yn];

    return(re);         
}

char *check_josa_p(char *str, int m)

{
	int yn = 0;
	char *josalist[7][2] =  {
		{ "����", "����" }, { "����", "����" }, { "�԰�", "�԰�" }, 
		{ "�̶��", "���" }, { "����", "����" }, { "����", "��" }, { "\n", "\n"}}; 
	char *re=NULL;

	if(!str || !*str) {
		re = "<����!>";
		return(re);
	}
	if(under_han(str))
		yn = 0;
	else   
		yn = 1;
	re = josalist[m][yn];

    return(re);         
}

