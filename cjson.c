// /*
//   Copyright (c) 2009 Dave Gamble
 
//   Permission is hereby granted, free of charge, to any person obtaining a copy
//   of this software and associated documentation files (the "Software"), to deal
//   in the Software without restriction, including without limitation the rights
//   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//   copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions:
 
//   The above copyright notice and this permission notice shall be included in
//   all copies or substantial portions of the Software.
 
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//   THE SOFTWARE.
// */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "cjson.h"

static const char *ep;//错误指针
const char *cjson_GetErrorPtr(void) {return ep;}
/*忽略大小写，字符串比较,s1>s2输出大于0，否着小于0，如果只有一个字符指针指向NULL就1*/
static int cjson_strcasecmp(const char *s1, const char *s2) {
  if (!s1) return (s1 == s2) ? 0 : 1;
  if (!s2) return 1;
  for (; tolower(*s1) == tolower(*s2); ++s1, ++s2) 
    if (*s1 == 0) return 0;
  return tolower(*(const unsigned char *)s1)-tolower(*(const unsigned char *)s2);
}
/*定义函数指针指向malloc和free*/
static void *(*cjson_malloc)(size_t sz) = malloc;
static void (*cjson_free)(void *ptr) = free;
/*复制字符串*/
static char* cjson_strdup(const char *str) {
  size_t len;
  char *copy;

  len = strlen(str) + 1;
  if (!(copy = (char*)cjson_malloc(len))) return 0;//内存分配错误
  memcpy(copy, str, len);
  return copy;
}
/*Hook内存管理函数*/
void cjson_InitHooks(cjson_Hooks *hooks) {
  if (!hooks) {/*重置钩子*/
    cjson_malloc = malloc;
    cjson_free = free;
    return;
  }

  cjson_malloc = (hooks->malloc_fn)?hooks->malloc_fn:malloc;
  cjson_free   = (hooks->free_fn)?hooks->free_fn:free;
}

static cjson *cjson_New_Item(void) {/*新建一个cjson项并返回该节点地址*/
  cjson *node = (cjson*)cjson_malloc(sizeof(cjson));
  if (node) memset(node, 0, sizeof(cjson));
  return node;
}

void cjson_Delete(cjson *c) {
  cjson *next;
  while (c) {
    next = c->next;
    //这里表示c不是一个引用类型是且1. c删儿子 2. c的值为字符串的释放字符串空间 3.不是常量释放键名
    if (!(c->type&cjson_IsReference) && c->child) cjson_Delete(c->child);
    if (!(c->type&cjson_IsReference) && c->valuestring) cjson_free(c->valuestring);
    if (!(c->type&cjson_StringIsConst) && c->string) cjson_free(c->string);
    cjson_free(c);
    c = next;
  }
}
/*解析文本转数字填充到这个项中*/
static const char *parse_number(cjson *item, const char *num) {
  double n=0, sign=1, scale=0;
  int subscale=0, signsubscale=1;

  if(*num == '-') sign=-1, ++num;
  if (*num == '0') ++num;
  if (*num>='1' && *num<='9') 
    do n=(n*10.0)+(*num-'0');
    while (*num>='0' && *num<='9');
  if (*num == '.' && num[1]>='0' && num[1]<='9') {/*小数*/
    ++num;
    do n=(n*10)+(*num++ - '0'), --scale;
    while (*num>='0' && *num<='9');
  }
  if (*num=='e' || *num=='E') {/*指数*/
    ++num;
    if (*num == '+') ++num;
    else if (*num == '-') signsubscale=-1, ++num;
    while (*num>='0' && *num<='9')
      subscale=(subscale*10)+(*num++ - '0');
  }
  n = sign*n*pow(10.0,scale+signsubscale*subscale);

  item->valuedouble = n;
  item->valueint = (int)n;
  item->type = cjson_Number;
  return num;
}

static int pow2gt(int x) {/*大于或等于的它的最接近它的2次幂数字*/
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

typedef struct 
{
  char *buffer;
  int length;
  int offset;
} printbuffer;//输出缓冲

/*缓冲内存分配，偏移量是与数组第一个元素的起始地址的距离可用于确定位置*/
static char *ensure(printbuffer *p, int needed) {
	char *newbuffer;
	int newsize;
	if (!p || !p->buffer)
		return 0;
	needed += p->offset;
	if (needed <= p->length)
		return p->buffer + p->offset;

	newsize = pow2gt(needed);
	newbuffer = (char *)cjson_malloc(newsize);
	if (!newbuffer)
	{
		cjson_free(p->buffer);
		p->length = 0, p->buffer = 0;
		return 0;
	}
	if (newbuffer)
		memcpy(newbuffer, p->buffer, p->length);
	cjson_free(p->buffer);
	p->length = newsize;
	p->buffer = newbuffer;
	return newbuffer + p->offset;
}

static int update(printbuffer *p) {/*相当于取出缓冲区字符串的长度*/
  if (!p || !p->buffer) return 0;
  char *str;
  str = p->buffer + p->offset;
  return p->offset + strlen(str);
}
/*数字转字符串*/
static char *print_number(cjson *item, printbuffer *p) {
  char *str = 0;
  double d = item->valuedouble;
  if (d == 0) {
    if (p) str = ensure(p, 2);
    else str = (char *)cjson_malloc(2);
    if (str)
      strcpy(str, "0");
  }
  else if (fabs(((double)item->valueint)-d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN) {
    if (p)/*21是2^64-1的位数*/
      str = ensure(p, 21);
    else str = (char *)cjson_malloc(21);
    if (str)
      sprintf(str, "%d", item->valueint);
  }
  else {
    if (p)
      str = ensure(p, 64);
    else 
      str = (char *)cjson_malloc(64);
    if (str) {
      if (fabs(floor(d)-d) <= DBL_EPSILON && fabs(d) < 1.0e60)
        sprintf(str, "%.0f", d);
      else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
        sprintf(str, "%e", d);//指数输出
      else 
        sprintf(str, "%f", d);
    }
  }
  return str;
}
/*解析4位16进制*/

static unsigned parse_hex4(const char *str) {
  unsigned h = 0;
  for (int i = 0; i < 3; ++i, ++str, h<<=4) {
    if (*str >= '0' && *str <= '9')
      h += (*str) - '0';
    else if (*str >= 'A' && *str <= 'F')
      h += 10 + (*str) - 'A';
    else if (*str >= 'a' && *str <= 'f')
      h += 10 + (*str) - 'a';
    else return 0;
  }
  if (*str >= '0' && *str <= '9')
    h += (*str) - '0';
  else if (*str >= 'A' && *str <= 'F')
    h += 10 + (*str) - 'A';
  else if (*str >= 'a' && *str <= 'f')
    h += 10 + (*str) - 'a';
  else return 0;
  return h;
}

/*解析输入文本(未转义的字符串)，和填充项*/
static const unsigned char firstByteMark[7] = {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};
static const char *parse_string(cjson *item, const char *str) {
  const char *ptr = str + 1;
  char *ptr2;
  char *out;
  int len = 0;
  unsigned uc, uc2;
  if (*str != '\"') {
    ep = str;
    return 0;
  }

  while (*ptr != '\"' && *ptr && ++len) /*记录除转义字符外完整应的字符个数*/
    if (*ptr++ == '\\')
      ++ptr;
  out = (char *)cjson_malloc(len + 1);
  if (!out) return 0;
  
  ptr = str+1;
  ptr2 = out;
  while (*ptr != '\"' && *ptr) {
    if (*ptr != '\\')
      *ptr2++ = *ptr++;
    else {
      ++ptr;
      switch (*ptr) {
      case 'b':
        *ptr2++ = '\b';
        break;
      case 'f':
        *ptr2++ = '\f';
        break;
      case 'n':
        *ptr2++ = '\n';
        break;
      case 'r':
        *ptr2++ = '\r';
        break;
      case 't':
        *ptr2++ = '\t';
        break;
      case 'u':
        uc = parse_hex4(ptr+1);
        ptr += 4;

        if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0) break;
        if (uc >= 0xD800 && uc <= 0xDBFF)/*UTF16 代理对*/ {
          if (ptr[1] != '\\' || ptr[2] != 'u') break;/*缺少下半部分*/
          uc2 = parse_hex4(ptr+3);
          ptr += 6;
          if (uc2 < 0xDC00 || uc2 > 0xDFFF) break;
          uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));

        }

        len = 4;
        if (uc < 0x80) len = 1;
        else if (uc < 0x800)  len = 2;
        else if (uc < 0x10000) len = 3;
        ptr2 += len;

        switch (len)
        {
        case 4:
          *--ptr2 = ((uc | 0x80) & 0xBF);
          uc >>= 6;
        case 3:
          *--ptr2 = ((uc | 0x80) & 0xBF);
          uc >>= 6;
        case 2:
          *--ptr2 = ((uc | 0x80) & 0xBF);
          uc >>= 6;
        case 1:
          *--ptr2 = (uc | firstByteMark[len]);
        }
        ptr2 += len;
        break;
        default :
          *ptr2++ = *ptr;
          break;
      }
      ++ptr;
    }
  }
  *ptr2 = 0;
  if (*ptr == '\"')
    ++ptr;
  item->valuestring = out;
  item->type = cjson_String;
  return ptr;
}

// int main() {
//   printf("%.0lf\n", 1.0e60);
// }