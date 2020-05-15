
#ifndef cjson_h
#define cjson_h

#ifdef __cplusplus
extern "C" {
#endif

/*JSON TYPE*/
#define cjson_False 0
#define cjson_True 1
#define cjson_Null 2
#define cjson_Number 3
#define cjson_String 4
#define cjson_Array 5
#define cjson_Object 6

#define cjson_IsReference 256 //是一个引用
#define cjson_StringIsConst 512 //常量字符串

typedef struct cjson
{
    struct cjson *next, *prev; //用于数组或对象的链
    struct cjson *child; //孩子指针指向数组或对象中的子链

    int type; //cjson类型

    char *valuestring; /*如果类型为string，string值*/
    int valueint; /*同理类型为int，int值*/
    double valuedouble; /*同上*/
    
    char *string; /*如果此项是对象的子项或子列表，表示该项的名字*/
}cjson;

typedef struct cjson_Hooks 
{
    void *(*malloc_fn)(size_t sz);
    void (*free_fn)(void *ptr);
}cjson_Hooks;
/*提供malloc，realloc和free函数*/
extern void cjson_InitHooks(cjson_Hooks *hooks);

/*提供一个json模块，会返回查询的json对象，完成后调用cjson_delete函数*/
extern cjson *cjson_Parse(const char *value);
/*提供json实例转换为传输/存储文本完成释放char**/
extern char  *cjson_Print(cjson *item);
/*提供json实例转换为传输/存储文本，不进行格式化完成释放char**/
extern char  *cjson_PrintUnformatted(cjson *item);
/*提供json实例前置缓冲和文本是否格式化，利用缓冲减少重新分配*/
extern char  *cjson_PrintBuffered(cjson *item, int prebuffer, int fmt);
/*删除一个json实例和所以子集*/
extern void   cjson_Delete(cjson *c);

/*给出json实例数组或对象中的项数*/
extern int    cjson_GetArraySize(cjson *array);

/*从项数组中利用编号索引项, 如果没有就返回空*/
extern cjson *cjson_GetArrayItem(cjson *array, int item);
/*部分大小写利用项名获取项*/
extern cjson *cjson_GetObjectItem(cjson *object, const char *string);

/*当cjson_Parse返回0时表示parse错误，它就是成功，所以定义在cjson_Parse返回0时，解析指向错误的指针*/
extern const char *cjson_GetErrorPtr(void);

/*创建适当的类型*/

extern cjson *cjson_CreateNull(void);
extern cjson *cjson_CreateTrue(void);
extern cjson *cjson_CreateFalse(void);
extern cjson *cjson_CreateBool(int b);
extern cjson *cjson_CreateNumber(double num);
extern cjson *cjson_CreateString(const char *string);
extern cjson *cjson_CreateArray(void);
extern cjson *cjson_CreateObject(void);

/*创建数组类型*/

extern cjson *cjson_CreateIntArray(const int *number, int count);
extern cjson *cjson_CreateFloatArray(const float *numbers, int count);
extern cjson *cjson_CreateDoubleArray(const double *numbers, int count);
extern cjson *cjson_CreateStringArray(const char **strings, int count);

/*添加数组或对象项*/
extern void cjson_AddItemToArray(cjson *array, cjson *item);
extern void cjson_AddItemToObject(cjson *object, const char *string, cjson *item);
extern void cjson_AddItemToObjectCS(cjson *array, const char *string, cjson *item);

/*将对项的引用添加*/
extern void cjson_AddItemReferenceToArray(cjson *array, cjson *item);
extern void cjson_AddItemReferenceToObject(cjson *array, const char *string, cjson *item);
/*从数组或者对象中，删除项*/
extern cjson *cjson_DetachItemFromArray(cjson *array, int which);
extern void   cjson_DeleteItemFromArray(cjson *array, int which);
extern cjson *cjson_DetachItemFromObject(cjson *object, const char *string);
extern void   cjson_DeleteItemFromObject(cjson *object, const char *string);

/*更新项*/
/*将已存在的项向右移动*/
extern void cjson_InsertItemInArray(cjson *array, int which, cjson *newitem);
extern void cjson_ReplaceItemInArray(cjson *array, int which, cjson *newitem);
extern void cjson_ReplaceItemInObject(cjson *object, const char *string, cjson *newitem);

/*复制一个cjson项*/
extern cjson *cjson_Duplicate(cjson *item, int recurse); 

/*检索是否以null结尾，并返回一个指向终点的指针*/
extern cjson *cjson_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated);

extern void cjson_Minify(char *json);

/* 快速创建内容的宏*/

#define cjson_AddNullToObject(object, name)       cjson_AddItemToObject(object, name, cjson_CreateNull())
#define cjson_AddTrueToObject(object, name)       cjson_AddItemToObject(object, name, cjson_CreateTrue())
#define cjson_AddFalseToObject(object, name)      cjson_AddItemToObject(object, name, cjson_CreateFalse())
#define cjson_AddBollToObject(object, name, b)    cjson_AddItemToObject(object, name, cjson_CreateBool(b))
#define cjson_AddNumberToObject(object, name, n)  cjson_AddItemToObject(object, name, cjson_CreateNumber(n))
#define cjson_AddStringToObject(object, name, s)  cjson_AddItemToObject(object, name, cjson_CreateString(s))

/*分配int值和double值相互传达*/
#define cjson_SetIntValue(object, val)    ((object)?(object)->valueint=(object)->valuedouble=(val):(val))
#define cjson_SetNumberValue(object, val) ((object)?(object)->valueint=(object)->valuedouble=(val):(val))

#ifdef __cplusplus
}
#endif

#endif 