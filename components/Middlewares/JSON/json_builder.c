#include "json_builder.h"


cJSON *json_create_object(void)
{
    return cJSON_CreateObject();
}



void json_add_string(cJSON *root, const char *key, const char *value)
{
    cJSON_AddStringToObject(root,key,  value );

}



void json_add_number(   cJSON *root,   const char *key,    int value)
{

    cJSON_AddNumberToObject(    root,     key,    value  );

}



char *json_finish( cJSON *root)
{

    char *data=
        cJSON_PrintUnformatted(root);


    cJSON_Delete(root);


    return data;
}
