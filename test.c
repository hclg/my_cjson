
#include <stdio.h>
#include <stdlib.h>
#include "cjson.h"

/*解析文档为json， 返回文档并且输出*/

void doit(char *text) {
    char *out;
    cjson *json = cjson_Parse(text);
    if (!json) {
        printf("Errorptr: [%s]", cjson_GetErrorPtr());
    }
    else {
        out = cjson_Print(json);
        cjson_Delete(json);
        printf("%s\n", out);
        free(out);
    }
}

void dofile(char *filename) {
    FILE *f = fopen(filename, "rb");
    long len;
    char *data;
    fseek(f, 0, SEEK_END);//指向末尾
    len = ftell(f);//与文件头的偏移量
    fseek(f, 0, SEEK_SET);
    data = (char*)malloc(len+1);
    fread(data, 1, len, f);
    fclose(f);
    doit(data);
    free(data);
}

/*数据记录类型示例*/
struct record
{
    const char *precision;
    double lat, lon;
    const char *address, *city, *state, *zip, *country;
};

/*创建一组示范对象*/
void create_objects() {
    cjson *root, *fmt, *img, *thm, *fld;
    char *out;
    int i;
    /*我们的一周系列*/
    const char *strings[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

    int numbers[3][3] = {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}};
    
    int ids[4] = {116, 943, 234, 38793};

    struct record fields[2] = {
        {"zip", 37.7668, -1.223959e+2, "", "SAN FRANCISCO", "CA", "94107", "US"},
		{"zip", 37.371991, -1.22026e+2, "", "SUNNYVALE", "CA", "94085", "US"}
    };

    root = cjson_CreateObject();
    cjson_AddItemToObject(root, "name", cjson_CreateString("Jack (\"Bee\") Nimble"));
    cjson_AddItemToObject(root, "format", fmt = cjson_CreateObject());
    cjson_AddStringToObject(fmt, "type", "rect");
	cjson_AddNumberToObject(fmt, "width", 1920);
	cjson_AddNumberToObject(fmt, "height", 1080);
	cjson_AddFalseToObject(fmt, "interlace");
	cjson_AddNumberToObject(fmt, "frame rate", 24);

    /*输出文档*/
    out = cjson_Print(root);
    cjson_Delete(root);
    printf("%s\n", out);
    free(out);

    root = cjson_CreateStringArray(strings, 7);

    out = cjson_Print(root);
    cjson_Delete(root);
    printf("%s\n", out);
    free(out);

    root = cjson_CreateArray();
    for (i = 0; i < 3; ++i) {
        cjson_AddItemToArray(root, cjson_CreateIntArray(numbers[i], 3));
    }
    cjson_ReplaceItemInArray(root, 1, cjson_CreateString("Replace"));

    out = cjson_Print(root);
    cjson_Delete(root);
    printf("%s\n", out);
    free(out);
    
    /* Our "gallery" item: */
	root = cjson_CreateObject();
	cjson_AddItemToObject(root, "Image", img = cjson_CreateObject());
	cjson_AddNumberToObject(img, "Width", 800);
	cjson_AddNumberToObject(img, "Height", 600);
	cjson_AddStringToObject(img, "Title", "View from 15th Floor");
	cjson_AddItemToObject(img, "Thumbnail", thm = cjson_CreateObject());
	cjson_AddStringToObject(thm, "Url", "http:/*www.example.com/image/481989943");
	cjson_AddNumberToObject(thm, "Height", 125);
	cjson_AddStringToObject(thm, "Width", "100");
	cjson_AddItemToObject(img, "IDs", cjson_CreateIntArray(ids, 4));

    out = cjson_Print(root);
    cjson_Delete(root);
    printf("%s\n", out);
    free(out);

    root = cjson_CreateArray();
	for (i = 0; i < 2; i++)
	{
		cjson_AddItemToArray(root, fld = cjson_CreateObject());
		cjson_AddStringToObject(fld, "precision", fields[i].precision);
		cjson_AddNumberToObject(fld, "Latitude", fields[i].lat);
		cjson_AddNumberToObject(fld, "Longitude", fields[i].lon);
		cjson_AddStringToObject(fld, "Address", fields[i].address);
		cjson_AddStringToObject(fld, "City", fields[i].city);
		cjson_AddStringToObject(fld, "State", fields[i].state);
		cjson_AddStringToObject(fld, "Zip", fields[i].zip);
		cjson_AddStringToObject(fld, "Country", fields[i].country);
	}
	/*	cjson_ReplaceItemInObject(cjson_GetArrayItem(root,1),"City",cjson_CreateIntArray(ids,4)); */

	out = cjson_Print(root);
	cjson_Delete(root);
	printf("%s\n", out);
	free(out);
}

int main() {
    /*一些json*/
    char text1[] = "{\n\"name\": \"Jack (\\\"Bee\\\") Nimble\", \n\"format\": {\"type\":       \"rect\", \n\"width\":      1920, \n\"height\":     1080, \n\"interlace\":  false,\"frame rate\": 24\n}\n}";
	char text2[] = "[\"Sunday\", \"Monday\", \"Tuesday\", \"Wednesday\", \"Thursday\", \"Friday\", \"Saturday\"]";
	char text3[] = "[\n    [0, -1, 0],\n    [1, 0, 0],\n    [0, 0, 1]\n	]\n";
	char text4[] = "{\n		\"Image\": {\n			\"Width\":  800,\n			\"Height\": 600,\n			\"Title\":  \"View from 15th Floor\",\n			\"Thumbnail\": {\n				\"Url\":    \"http:/*www.example.com/image/481989943\",\n				\"Height\": 125,\n				\"Width\":  \"100\"\n			},\n			\"IDs\": [116, 943, 234, 38793]\n		}\n	}";
	char text5[] = "[\n	 {\n	 \"precision\": \"zip\",\n	 \"Latitude\":  37.7668,\n	 \"Longitude\": -122.3959,\n	 \"Address\":   \"\",\n	 \"City\":      \"SAN FRANCISCO\",\n	 \"State\":     \"CA\",\n	 \"Zip\":       \"94107\",\n	 \"Country\":   \"US\"\n	 },\n	 {\n	 \"precision\": \"zip\",\n	 \"Latitude\":  37.371991,\n	 \"Longitude\": -122.026020,\n	 \"Address\":   \"\",\n	 \"City\":      \"SUNNYVALE\",\n	 \"State\":     \"CA\",\n	 \"Zip\":       \"94085\",\n	 \"Country\":   \"US\"\n	 }\n	 ]";

    doit(text1);
	doit(text2);
	doit(text3);
	doit(text4);
	doit(text5);

    create_objects();
    return 0;
}