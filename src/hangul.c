/* 한글처리 */
#include "kstbl.h"
#include "conf.h" 
#include "sysdep.h" 

int is_hangul(str)
unsigned char *str;
{
  /* 순수한글인지 여부를 검사 */
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

/* 받침이 있는지 없는지 검사하는 부분입니다. 조사처리에 쓰임 */
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



/* 첫글자의 초성을 찾는 부분입니다. */
char *first_han(unsigned char *str)
{
    int i, Length;
    char *Result = "ZZZ";
    unsigned char str_0, str_1;

    static unsigned char *exam[]={
        "가", "가", "나", "다", "다",
        "라", "마", "바", "바", "사",
        "사", "아", "자", "자", "차",
        "카", "타", "파", "하", "" };
    static unsigned char *johab_exam[]={
        "늏", "똞", "륾", "봞", "쁝",
        "쏿", "쟞", "?", "?", "?",
        "?", "?", "?", "?", "?",
        "?", "?", "?", "?", "" };  

/*    static unsigned char *exam[]={
        "ㄱ", "ㄲ", "ㄴ", "ㄷ", "ㄸ",
        "ㄹ", "ㅁ", "ㅂ", "ㅃ", "ㅅ",
        "ㅆ", "ㅇ", "ㅈ", "ㅉ", "ㅊ",
        "ㅋ", "ㅌ", "ㅍ", "ㅎ", "" };
// 이건 조합형으로 입력을 하셔야 합니다.
    static unsigned char *johab_exam[]={
        "~Ha", "~La", "~Pa", "~Ta", "~Xa",
        "~\a", "| a", "바", "빠", "사",
        "싸", "아", "자", "짜", "차",
        "카", "타", "파", "하", "" };
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
		{ "이", "가" }, { "을", "를" }, { "과", "와" }, 
		{ "이라고", "라고" }, { "은", "는" }, { "으로", "로" }, { "\n", "\n"}}; 
	char *re=NULL;

	if(!str || !*str) {
		re = "<에러!>";
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
		{ "님이", "님이" }, { "님을", "님을" }, { "님과", "님과" }, 
		{ "이라고", "라고" }, { "님은", "님은" }, { "으로", "로" }, { "\n", "\n"}}; 
	char *re=NULL;

	if(!str || !*str) {
		re = "<에러!>";
		return(re);
	}
	if(under_han(str))
		yn = 0;
	else   
		yn = 1;
	re = josalist[m][yn];

    return(re);         
}

