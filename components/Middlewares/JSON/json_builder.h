#ifndef JSON_BUILDER_H
#define JSON_BUILDER_H


#include "cJSON.h"


cJSON *json_create_object(void);


void json_add_string(
        cJSON *root,
        const char *key,
        const char *value);


void json_add_number(
        cJSON *root,
        const char *key,
        int value);



char *json_finish(
        cJSON *root);



#endif