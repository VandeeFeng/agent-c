#include "agent-c.h"
#include <cjson/cJSON.h>

char* json_request(const Agent* agent, const Config* config, char* out, size_t size) {
    if (!agent || !out) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", config->model);
    cJSON_AddNumberToObject(root, "temperature", config->temp);
    cJSON_AddNumberToObject(root, "max_tokens", config->max_tokens);
    cJSON_AddFalseToObject(root, "stream");
    cJSON_AddStringToObject(root, "tool_choice", "auto");

    cJSON *tools = cJSON_CreateArray();
    cJSON *tool = cJSON_CreateObject();
    cJSON_AddStringToObject(tool, "type", "function");
    cJSON *function = cJSON_CreateObject();
    cJSON_AddStringToObject(function, "name", "execute_command");
    cJSON_AddStringToObject(function, "description", "Execute shell command");
    cJSON *parameters = cJSON_CreateObject();
    cJSON_AddStringToObject(parameters, "type", "object");
    cJSON *properties = cJSON_CreateObject();
    cJSON *command = cJSON_CreateObject();
    cJSON_AddStringToObject(command, "type", "string");
    cJSON_AddItemToObject(properties, "command", command);
    cJSON_AddItemToObject(parameters, "properties", properties);
    cJSON *required = cJSON_CreateStringArray((const char*[]){"command"}, 1);
    cJSON_AddItemToObject(parameters, "required", required);
    cJSON_AddItemToObject(function, "parameters", parameters);
    cJSON_AddItemToObject(tool, "function", function);
    cJSON_AddItemToArray(tools, tool);
    cJSON_AddItemToObject(root, "tools", tools);

    cJSON *messages = cJSON_CreateArray();
    for (int i = 0; i < agent->msg_count; i++) {
        const Message* msg = &agent->messages[i];
        cJSON *message = cJSON_CreateObject();
        cJSON_AddStringToObject(message, "role", msg->role);

        if (strcmp(msg->role, "tool") == 0) {
            cJSON_AddStringToObject(message, "content", msg->content);
            cJSON* tool_calls = cJSON_Parse(msg->tool_calls);
            cJSON* first_tool_call = cJSON_GetArrayItem(tool_calls, 0);
            cJSON* tool_call_id = cJSON_GetObjectItem(first_tool_call, "id");
            if (cJSON_IsString(tool_call_id)) {
                cJSON_AddStringToObject(message, "tool_call_id", tool_call_id->valuestring);
            }
            cJSON_Delete(tool_calls);
        } else if (strcmp(msg->role, "assistant") == 0 && strlen(msg->tool_calls) > 0) {
            if (strlen(msg->content) > 0) {
                cJSON_AddStringToObject(message, "content", msg->content);
            } else {
                cJSON_AddNullToObject(message, "content");
            }
            cJSON* tool_calls_json = cJSON_Parse(msg->tool_calls);
            cJSON_AddItemToObject(message, "tool_calls", tool_calls_json);
        } else {
            cJSON_AddStringToObject(message, "content", msg->content);
        }

        cJSON_AddItemToArray(messages, message);
    }
    cJSON_AddItemToObject(root, "messages", messages);

    if (config->op_providers_on && strlen(config->op_providers_json) > 0) {
        cJSON* providers_json = cJSON_Parse(config->op_providers_json);
        cJSON_AddItemToObject(root, "optional_providers", providers_json);
    }

    char* json_string = cJSON_PrintUnformatted(root);
    strncpy(out, json_string, size - 1);
    out[size - 1] = '\0';

    cJSON_Delete(root);
    free(json_string);

    return out;
}

static char* get_json_string(cJSON* obj, const char* key, char* out, size_t size) {
    if (!obj || !out) return NULL;
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        strncpy(out, item->valuestring, size - 1);
        out[size - 1] = '\0';
        return out;
    }
    return NULL;
}

char* json_content(const char* response, char* out, size_t size) {
    if (!response || !out) return NULL;

    cJSON *root = cJSON_Parse(response);
    if (!root) return NULL;

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
    cJSON *message = cJSON_GetObjectItem(first_choice, "message");
    get_json_string(message, "content", out, size);

    cJSON_Delete(root);
    return out;
}

char* json_error(const char* response, char* out, size_t size) {
    if (!response || !out) return NULL;

    cJSON *root = cJSON_Parse(response);
    if (!root) return NULL;

    cJSON *error = cJSON_GetObjectItem(root, "error");
    get_json_string(error, "message", out, size);

    cJSON_Delete(root);
    return out;
}

int extract_command(const char* response, char* cmd, size_t cmd_size) {
    if (!response || !cmd) return 0;

    cJSON *root = cJSON_Parse(response);
    if (!root) return 0;

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        cJSON_Delete(root);
        return 0;
    }

    cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
    cJSON *message = cJSON_GetObjectItem(first_choice, "message");
    cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
    if (!cJSON_IsArray(tool_calls) || cJSON_GetArraySize(tool_calls) == 0) {
        cJSON_Delete(root);
        return 0;
    }

    cJSON *first_tool_call = cJSON_GetArrayItem(tool_calls, 0);
    cJSON *function = cJSON_GetObjectItem(first_tool_call, "function");
    cJSON *arguments = cJSON_GetObjectItem(function, "arguments");
    if (cJSON_IsString(arguments) && arguments->valuestring != NULL) {
        cJSON* args_json = cJSON_Parse(arguments->valuestring);
        get_json_string(args_json, "command", cmd, cmd_size);
        cJSON_Delete(args_json);
    }

    cJSON_Delete(root);
    return strlen(cmd) > 0;
}

int extract_tool_calls(const char* response, char* tool_calls_str, size_t calls_size, char* tool_call_id, size_t id_size) {
    if (!response || !tool_calls_str) return 0;

    cJSON *root = cJSON_Parse(response);
    if (!root) return 0;

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        cJSON_Delete(root);
        return 0;
    }

    cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
    cJSON *message = cJSON_GetObjectItem(first_choice, "message");
    cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");

    if (cJSON_IsArray(tool_calls)) {
        char* rendered = cJSON_PrintUnformatted(tool_calls);
        strncpy(tool_calls_str, rendered, calls_size - 1);
        tool_calls_str[calls_size - 1] = '\0';
        free(rendered);

        if (tool_call_id && id_size > 0) {
            cJSON *first_tool_call = cJSON_GetArrayItem(tool_calls, 0);
            get_json_string(first_tool_call, "id", tool_call_id, id_size);
        }
    }

    cJSON_Delete(root);
    return strlen(tool_calls_str) > 0;
}
