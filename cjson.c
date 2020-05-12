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
/*忽略大小写，相等等于0，字符串比较,s1>s2输出大于0，否着小于0，如果只有一个字符指针指向NULL就1*/
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
  char *buffer;/*内存字符串*/
  int length;/*内存容量大小*/
  int offset;/*偏移量*/
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

/*输出这个item中的string*/
static char *print_string_ptr(const char *str, printbuffer *p) {
  const char *ptr;
  char *ptr2, *out;
  int len = 0, flag = 0;
  unsigned char token;
/*
    局部变量说明：
        1.ptr：指向参数传入的str字符串
        2.ptr2：指向要输出的out字符串
        3.out：输出字符串
        4.len：输出字符串的长度，用于内存分配出输出字符串的空间大小
        5.token：字符保存中间变量
    */
  for (ptr = str; *ptr; ++ptr) flag |= ((*ptr > 0 && *ptr < 32) || (*ptr == '\"') || (*ptr == '\\')) ? 1 : 0;
  if (!flag) {
    len = ptr - str;
    if (p) out = ensure(p, len + 3);
    else out = (char *)cjson_malloc(len + 3);
    if (!out ) return 0;
    ptr2 = out;
    *ptr2++ = '\"';
    strcpy(ptr2, str);
    ptr2[len] = '\"';
    ptr2[len+1] = 0;
    return out;
  }

  if (!str) {
    if (p) out = ensure(p, 3);
    else out = (char *)cjson_malloc(3);
    if (!out) return 0;
    strcpy(out, "\"\"");
    return out;
  }
  ptr = str;
  while ((token = *ptr) && ++len) {
    if (strchr("\"\\\b\f\n\r\t", token))
      ++len;
    else if (token < 32) len += 5;            
          /*
            除了前面列出来的空白字符，其他的空白都+5的长度，
            不知道为什么，应该与unicode编码有关
            如：
                "\uxxxx",u再加4个字符就是5个字符，前面解析字符的时候是这么解析的。
          */
    ++ptr;
  }

  if (p) out = ensure(p, len+3);
  else out = (char *) cjson_malloc(len + 3);
  if (!out) return 0;

  ptr2 = out;
  ptr = str;
  *ptr2++ = '\"';
  while (*ptr) {
    if ((unsigned char) *ptr > 31 && *ptr != '\"' && *ptr != '\\') 
      *ptr2++ = *ptr++;
      else {
        *ptr2++ = '\\';
        switch (token = *ptr++) 
        {
        case '\\':
          *ptr2++ = '\\';
          break;
        case '\"':
          *ptr2++ = '\"';
          break;
        case '\b':
          *ptr2++ = 'b';
          break;
        case '\f':
          *ptr2++ = 'f';
          break;
        case '\n':
          *ptr2++ = 'n';
          break;
        case '\r':
          *ptr2++ = 'r';
          break;
        case '\t':
          *ptr2++ = 't';
          break;
        default:
          sprintf(ptr2, "u%04x", token);
          ptr2 += 5;
          break;
        }
      }
  }
  *ptr2++ = '\"';
  *ptr2++ = 0;
  return out;
}
/*Invote print_string_ptr (which is useful) on an item.*/
static char *print_string(cjson *item, printbuffer *p) {return print_string_ptr(item, p);}
/*提前声明原型*/
static const char *parse_value(cjson *item, const char *value);
static char *print_value(cjson *item, int depth, int fmt, printbuffer *p);
static const char *parse_array(cjson *item, const char *value);
static char *print_array(cjson *item, int depth, int fmt, printbuffer *p);
static const char *parse_object(cjson *item, const char *value);
static char *print_object(cjson *item, int depth, int fmt, printbuffer *p);

/*跳过一些空字符*/
static const char *skip(const char *in) {
  while (in && *in && (unsigned char) *in <= 32) ++in;
  return in;
}
/*创建一个根,并且填充
require_null_terminated 是为了确保字符串必须以'\0'结尾
若参数提供return_parse_end将返回json字符串解析完成之后的部分进行返回
*/
cjson *cjson_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated) {
    /*
    返回一个json结构的数据
    局部变量说明：
        1.end：当解析完真个字符串的时候，最后一个字符如果不是NULL，则代表这个输入的value的字符串
                可能有问题；
        2.c：cjson节点，也是所谓的根节点。
    */
  const char *end = 0;
  cjson *c = cjson_New_Item();
  ep = 0;
  if (!c) return 0;//内存分配失败
  end = parse_value(c, skip(value));
  if (!end) {
    cjson_Delete(c);
    return 0;
  }/*解析失败*/
  if (require_null_terminated) {
    end = skip(end);
    if (*end) {/*空字符后没结束*/
      cjson_Delete(c);
      ep = end;
      return 0;
    }
  }
  if (return_parse_end) *return_parse_end = end;
  return c;
}
/*默认不检查NULL终止符,cjson字符串的解析新建根*/
cjson *cjson_Parse(const char *value) {return cjson_ParseWithOpts(value, 0, 0);}

/*将cjson实例结构呈现为文本*/
char *cjson_Print(cjson *item) {return print_value(item, 0, 1, 0); }
char *cjson_PrintUnformatted(cjson *item) {return print_value(item, 0, 0, 0); }

char *cjson_PrintBuffered(cjson *item, int prebuffer, int fmt) {
  printbuffer p;
  p.buffer = (char *) cjson_malloc(prebuffer);
  p.length = prebuffer;
  p.offset = 0;
  return print_value(item, 0, fmt, &p);
  return p.buffer;
}
/*根据首字符的不同来决定采用哪种方式进行解析字符串*/
static const char *parse_value(cjson *item, const char *value) {
  if (!value) return 0;
  if (!strncmp(value, "null", 4)) {
    item->type = cjson_Null;
    return value + 4;
  }
  if (!strncmp(value, "false", 5)) {
    item->type = cjson_False;
    return value + 5;
  }
  if (!strncmp(value, "true", 4)) {
    item->type = cjson_True;
    return value + 4;  
  }
  if (!strncmp(value, "\"", 4)) 
    return parse_string(item, value);
  if (*value == '-' || (*value >= '0' && *value <= '9'))
    return parse_number(item, value);
  if (*value == '[') 
    return parse_array(item, value);
  if (*value == '{')
    return parse_object(item, value);

    ep = value;
    return 0;
}
/*以文本呈现一个值,根据item的类型来选这使用哪种方式进行数的输出格式*/

static char *print_value(cjson *item, int depth, int fmt, printbuffer *p) {
  char *out = 0;
  if (!item) return 0;
  if (p) {
    switch ((item->type) & 255)
    {
    case cjson_Null:
      out = ensure(p, 5);
      if (out) strcpy(out, "null");
      break;
    case cjson_False :
      out = ensure(p, 6);
      if (out) strcpy(out, "false");
      break;
    case cjson_True:
      out = ensure(p, 5);
      if (out) strcpy(out, "True");
      break;
    case cjson_Number:
      out = print_number(item, p);
      break;
    case cjson_String:
      out = print_string(item, p);
      break;
    case cjson_Array:
      out = print_array(item, depth, fmt, p);
      break;
    case cjson_Object:
      out = cjson_object(item, depth, fmt, p);
      break;
    }
  }
  else {
    switch ((item->type) & 255)
    {
    case cjson_Null:
      out = cjson_strdup("null");
      break;
    case cjson_False :
      out = cjson_strdup("false");
      break;
    case cjson_True:
      out = cjson_strdup("True");
      break;
    case cjson_Number:
      out = print_number(item, 0);
      break;
    case cjson_String:
      out = print_string(item, 0);
      break;
    case cjson_Array:
      out = print_array(item, depth, fmt, 0);
      break;
    case cjson_Object:
      out = cjson_object(item, depth, fmt, 0);
      break;
    }
  }
  return out;
}

/* 从输入中构建一个数组*/
/*格式
  [
    [0,1,0],
    [1,1,0],
    [1,1,1];
  ]
    1.先检测到[
    2.然后skip掉换行符，空白字符
    3.parse_value从新检测字符串，也就能再次检测又是一个数组了[0,-1,0],
    递归进入解析[0,-1,0],并解析出0，-1，保存在节点中
    4.检测是否遇到','字符，如果遇到说明后面还有内容需要解析
    5.循环解析接下来的内容
*/
static const char *parse_array(cjson *item, const char *value) {
  cjson *child;
  if (*value != '[') {
    ep = value;
    return 0;
  }
  item->type = cjson_Array;
  value = skip(value + 1);
  if (*value == ']')
    return value + 1;/*空数组*/
  item->child = child = cjson_New_Item();
  if (!item->child) return 0;/*内存分配失败*/
  value = skip(parse_value(child, skip(value)));
  if (!value) return 0;/*解析错误*/
  while(*value == ',') {
    cjson *new_item;
    if (!(new_item = cjson_New_Item())) return 0;/*内存分配失败*/
    child->next = new_item;
    new_item->prev = child;
    child = new_item;
    value = skip(parse_value(child, skip(value+1)));
    if (!value) return 0; /*同上*/
  }
  if (*value == ']')
    return value + 1;/*数组结束的后一个字符*/
  ep = value;
  return 0;
}

/*将数组输出为文档格式*/
static char *print_array(cjson *item, int depth, int fmt, printbuffer *p) {
  /*局部变量说明
    entries: 输出字符串数组，从节点中提取到字符串数组中
    out:entries 字符串数组中得到字符串out
    ptr: 指向out 得指针
    ret: 函数执行结果的返回值
    len: 字符串长度
    child :儿子节点也就是当前操作节点
    numentries: 统计entries 的个数
    fail: 错误标志
    fmt: 用来调整格式
    tmplen: 临时字符串长度
    i:遍历
  */
  char **entries, *out = 0, *ptr, *ret;
  int len = 5, i = 0, numentries = 0, fail = 0;
  size_t tmplen = 0;
  cjson *child = item->child;

  /*多少个数组*/
  while (child) ++numentries, child = child->next;
  /*显示处理numentries == 0*/
  if (!numentries) {
    if (p) out = ensure(p, 3);
    else out = (char *) cjson_malloc(3);
    if (out) strcpy(out, "[]");
    return out;
  }

  if (p) {
    /*合并输出数组*/
    i = p->offset;
    ptr = ensure(p, 1);
    if (!ptr) return 0;
    *ptr = '[';
    p->offset++;
    child = item->child;
    while (child && !fail) {
      print_value(child, depth+1, fmt, p);
      p->offset = update(p);
      if (child->next) {
        len = fmt ? 2 : 1;
        ptr = ensure(p, len + 1);
        if (!ptr) return 0;
        *ptr++ = ',';
        if (fmt) *ptr++ = ' ';
        *ptr = 0;
        p->offset += len;
      }
      child = child->next;
    }
    ptr = ensure(p, 2);
    if (!ptr) return 0;
    *ptr++ = ']';
    *ptr = 0;
    out = (p->buffer) + i;
  }
  else {
    /*分配一个数组保持数组值*/
    entries = (char **)cjson_malloc(numentries * sizeof(char *));
    if (!entries) return 0;
    memset(entries, 0, numentries*sizeof(char *));
    /*取出所有的结果*/
    child = item->child;
    while (child && !fail) {
      ret = print_value(child, depth+1, fmt, 0);
      entries[i++] = ret;
      if (ret) len += strlen(ret)+2+(fmt ? 1 : 0);
      else fail = 1;
      child = child->next;
    }

    if (!fail) out = (char *)cjson_malloc(len);
    if (!out) fail = 1;
    if (fail) {
      for (i = 0; i < numentries; ++i)
        if (entries[i]) cjson_free(entries[i]);
      cjson_free(entries);
      return 0;
    }
    /*构成输出数组*/
    *out = '[';
    ptr = out + 1;
    *ptr = 0;
    for (i = 0; i < numentries; ++i) {
      tmplen = strlen(entries[i]);
      memcpy(ptr, entries[i], tmplen);
      ptr += tmplen;
      if (i != numentries-1) {
        *ptr++ = ',';
        if (fmt) *ptr++ = ' ';
        *ptr = 0;
      }
      cjson_free(entries[i]);
    }
    cjson_free(entries);
    *ptr++ = ']';
    *ptr++ = 0;
  }
  return out;
} 


/*以文本建立对象，同上*/
/*
    以下数据为格式分析：
{
    "name":"jack(\"bee\") Nimble",
    "format":{
        "type":"rect",
        "width":1920
        }
}
    1.检测到'{';
    2.跳过空白字符，换行符；
    3.通过parse_string获取name；
    4.判断键值对表示符':';
    5.通过parse_value获取对应value；
    6.parse_value和前面的几个函数一样，是递归函数
    7.通过while循环解析剩下的键值对
*/
static const char *parse_object(cjson *item, const char *value) {
  cjson *child;
  if (*value != '{') {
    ep = value;
    return 0;
  }
  item->type = cjson_Object;
  value = skip(value+1);
  if (*value == '}')
    return value+1;
  item->child = child = cjson_New_Item();
  if (!child) return 0;
  value = skip(parse_string(child, skip(value)));
  if (!value) return 0;
  child->string = child->valuestring;
  child->valuestring = 0;
  if (*value != ':') {
    ep = value;
    return 0;
  }
  value = skip(parse_value(child, skip(value+1)));
  if (!value) return 0;
  while (*value == ',') {
    cjson *new_item;
    if (!(new_item = cjson_New_Item())) return 0;
    child->next = new_item;
    new_item->prev = child;
    child = new_item;
    value = skip(parse_string(child, skip(value+1)));
    if (!value) return 0;
    child->string = child->valuestring;
    child->valuestring = 0;
    if (*value != ':') {
      ep = value;
      return 0;
    }
    value = skip(parse_value(child, skip(value+1)));
    if (!value) return 0;
  }
  if (*value == '}')
    return value+1;
  ep = value;
  return 0;
}

/*将对象输出为文档格式*/
static char *print_object(cjson *item, int depth, int fmt, printbuffer *p) {
    /*
    item 0,1,0;item 0,0,0
    局部变量说明：
        1.entries：键值对value
        2.names：键值对key；
        3.out：指向输出的字符串
        4.ptr：指向out输出的字符串
        5.ret：执行函数时返回的字符串地址；
        6.str：执行函数返回的字符串地址
        7.len：字符串的长度
        8.i：for循环用于计数的变量
        9.j: for循环用于计数的变量
        10.child：指向节点的指针。
        11.fail输出出错时的标志
        12.numenties：用于统计当前结构深度层次上的节点个数
    */
  char **entrise = 0, **names = 0;/*值得字符串数组，名字的字符串数组*/
  char *out = 0, *ptr, *ret, *str;/**/
  int len = 7, i = 0, j;
  cjson *child = item->child;
  int numentries = 0, fail = 0;
  size_t tmplen = 0;
  /*计算字符串组数*/
  while (child) ++numentries, child = child->next;
  /* 空对象类型*/
  if (!numentries) {
    if (p) out = ensure(p, fmt ? depth+4 : 3);
    else out = (char *)cjson_malloc(fmt ? depth+4 : 3);
    if (!out ) return 0;
    ptr = out;
    *ptr++ = '{';
    if (fmt) {
      *ptr++ = '\n';
      for (i = 0; i < depth-1; ++i) *ptr++ = '\t';
    }
    *ptr++ = '}';
    *ptr++ = 0;
    return out;
  }
  if (p) {
    i = p->offset;
    len = fmt ? 2 : 1;
    ptr = ensure(p, len+1);
    if (!ptr) return 0;
    *ptr++ = '{';
    p->offset += len;
    child = item->child;
    ++depth;
    while (child)
    {
      if (fmt) {
        ptr = ensure(p, depth);
        if (!ptr) return 0;
        for (j = 0; j < depth; ++j)//我的习惯
          *ptr++ = '\t';
          p->offset += depth;
      }
      print_string_ptr(child->string, p);
      p->offset = update(p);

      len = fmt ? 2 : 1;
      ptr = ensure(p, len);
      if (!ptr) return 0;
      *ptr++ = ':';
      if (fmt) 
        *ptr++ = '\t';
      p->offset += len;
      print_value(child, depth, fmt, p);
      p->offset = update(p);

      len = (fmt ? 1 : 0) + (child->next ? 1 : 0);
      ptr = ensure(p, len+1);
      if (!ptr) return 0;
      if (child->next)
        *ptr++ = ',';
      if (fmt)
        *ptr++ = '\n';
      *ptr  = 0;
      p->offset += len;
      child = child->next;
    }
    ptr = ensure(p, fmt?(depth+1):2);
    if(!ptr) return 0;
    if (fmt) 
      for(i = 0; i < depth-1; ++i)
        *ptr++ = '\t';
    *ptr++ = '}';
    *ptr = 0;
    out = (p->buffer) + i;
  }
  else {
    /*分配空间*/
    entrise = (char **)cjson_malloc(numentries * sizeof(char*));
    if (!entrise) return 0;
    names =   (char **) cjson_malloc(numentries * sizeof(char*));
    if (!names) {
      cjson_free(entrise);
      return 0;
    }
    memset(ensure, 0, sizeof(char*) * numentries);
    memset(names, 0, sizeof(char*) * numentries);

    /*搜集全部字符串*/
    child = item->child;
    ++depth;
    if (fmt) len += depth;
    while (child) {
      names[i] = str = print_string_ptr(child->string, 0);
      entrise[i++] = ret = print_value(child, depth, fmt, 0);
      if (str && ret) len += strlen(ret) + strlen(str) + 2 + (fmt ? 2+depth : 0);
      else 
        fail = 1;
      child = child->next;
    }
    if (!fail) out = (char *) cjson_malloc(len);
    if (!out) fail = 1;
    /*错误处理*/
    if (fail) {
      for (i = 0; i < numentries; ++i) {
        if (names[i]) cjson_free(names[i]);
        if (entrise[i]) cjson_free(entrise[i]);
      }
      cjson_free(names);
      cjson_free(entrise);
      return 0;
    }

    /*构成输出*/
    *out = '{';
    ptr = out+1;
    if (fmt) *ptr++ = '\n';
    *ptr = 0;
    for (i = 0; i < numentries; ++i) {
      if (fmt) 
        for (j = 0; j < depth; ++j)
          *ptr++ = '\t';
        tmplen = strlen(names[i]);
        memcpy(ptr, names[i], tmplen);
        ptr += tmplen;
        *ptr++ = ':';
        if (fmt)
          *ptr++ = '\t';
        strcpy(ptr, entrise[i]);
        ptr += strlen(entrise[i]);
        if (i != numentries-1)
          *ptr++ = ',';
        if (fmt) *ptr++ = '\n';
        *ptr = 0;
        cjson_free(names[i]);
        cjson_free(entrise[i]);
    }
    cjson_free(names);
    cjson_free(entrise);
    if (fmt) 
      for (i = 0; i < depth-1; ++i) *ptr++ = '\t';
    *ptr++ = '}';
    *ptr++ = 0;
  }
  return out;
}


/*取得数组长度 取得索引项*/
int cjson_GetArraySize(cjson *array) {
  cjson *c = array->child;
  int i = 0;
  while (c)
    c = c->next, ++i;
  return i;
}

cjson *cjson_GetArrayItem(cjson *array, int item) {
  cjson *c = array->child;
  int i = 0;
  while (c && item--)
    c = c->next;
  return c;
}

cjson *cjson_GetObjectItem(cjson *object, const char *string) {
  cjson *c = object->child;
  while (c && cjson_strcasecmp(c->string, string))
    c = c->next;
  return c;
}

/*添加后一个项*/
static void suffix_object(cjson *prev, cjson *item) {
  prev->next = item;
  item->prev = prev;
}

/*引用处理, 创建引用项*/
static cjson *create_reference(cjson *item) {
  cjson *ref = cjson_New_Item();
  if (!ref) return 0;
  memcpy(ref, item, sizeof(cjson));
  ref->string = 0;
  ref->type |= cjson_IsReference;
  ref->next = ref->prev = 0;
  return ref;
}

/*添加项到数组或者对象的后面*/
void cjson_AddItemToArray(cjson *array, cjson *item) {
  cjson *c = array->child;
  if (!item) return;
  if (!c) array->child = item;
  else {
    while (c && c->next)
      c = c->next;
    suffix_object(c, item);
  }
}

void cjson_AddItemToObject(cjson *object, const char *string, cjson *item) {
  if (!item) return;
  if (item->string) cjson_free(item->string);
  item->string = cjson_strdup(string);
  cjson_AddItemToArray(object, item);
}
/*添加字符串常量的项*/
void cjson_AddItemToObjectCS(cjson *object, const char *string, cjson *item) {
  if (!item) return;
  if (!(item->type & cjson_StringIsConst) && item->string)
    cjson_free(item->string);
  item->string = (char *)string;
  item->type |= cjson_StringIsConst;
  cjson_AddItemToArray(object, item);
}

void cjson_AddItemReferenceToArray(cjson *array, cjson *item) {
  cjson_AddItemToArray(array, create_reference(item));
}
void cjson_AddItemReferenceToObject(cjson *array, const char *string, cjson *item) {
  cjson_AddItemToObject(array, string, create_reference(item));
}

// int main() {
//   printf("%.0lf\n", 1.0e60);
// }