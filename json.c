#include "agent-c.h"
#include <cjson/cJSON.h>

static void add_message_to_json(cJSON *messages, const Message *msg) {
    cJSON *message = cJSON_CreateObject();
    cJSON_AddStringToObject(message, "role", msg->role);

    if (strcmp(msg->role, "tool") == 0) {
        cJSON_AddStringToObject(message, "content", msg->content);
        cJSON *tc = cJSON_Parse(msg->tool_calls);
        if (tc) {
            cJSON *id = cJSON_GetObjectItem(cJSON_GetArrayItem(tc, 0), "id");
            if (cJSON_IsString(id)) {
                cJSON_AddStringToObject(message, "tool_call_id", id->valuestring);
            }
            cJSON_Delete(tc);
        }
    } else if (strcmp(msg->role, "assistant") == 0 && msg->tool_calls[0]) {
        cJSON_AddItemToObject(message, "content",
                              msg->content[0] ? cJSON_CreateString(msg->content) : cJSON_CreateNull());
        cJSON_AddItemToObject(message, "tool_calls", cJSON_Parse(msg->tool_calls));
    } else {
        cJSON_AddStringToObject(message, "content", msg->content);
    }
    cJSON_AddItemToArray(messages, message);
}

char* json_request(const Agent* agent, const Config* config, char* out, size_t size) {
    if (!agent || !out) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", config->model);
    cJSON_AddNumberToObject(root, "temperature", config->temp);
    cJSON_AddNumberToObject(root, "max_tokens", config->max_tokens);
    cJSON_AddBoolToObject(root, "stream", 0);
    cJSON_AddStringToObject(root, "tool_choice", "auto");

    const char* tool_json_str =
        "[{\"type\":\"function\",\"function\":{\"name\":\"execute_command\","
        "\"description\":\"Execute shell command\",\"parameters\":{\"type\":\"object\","
        "\"properties\":{\"command\":{\"type\":\"string\"}},\"required\":[\"command\"]}}},"
        "{\"type\":\"function\",\"function\":{\"name\":\"extract_skill\","
        "\"description\":\"Extract content from SKILL.md file\",\"parameters\":{\"type\":\"object\","
        "\"properties\":{\"skill_name\":{\"type\":\"string\"}},\"required\":[\"skill_name\"]}}},"
        "{\"type\":\"function\",\"function\":{\"name\":\"execute_skill\","
        "\"description\":\"Execute skill script with format: 'skill_name script_name [arguments]'. Script name should not include file extension.\",\"parameters\":{\"type\":\"object\","
        "\"properties\":{\"skill_command\":{\"type\":\"string\"}},\"required\":[\"skill_command\"]}}}]";
    cJSON_AddItemToObject(root, "tools", cJSON_Parse(tool_json_str));

    cJSON *messages = cJSON_CreateArray();
    for (int i = 0; i < agent->msg_count; i++) {
        add_message_to_json(messages, &agent->messages[i]);
    }
    cJSON_AddItemToObject(root, "messages", messages);

    if (config->op_providers_on && config->op_providers_json[0]) {
        cJSON_AddItemToObject(root, "provider", cJSON_Parse(config->op_providers_json));
    }

    char* json_string = cJSON_PrintUnformatted(root);
    if (!json_string) {
        cJSON_Delete(root);
        out[0] = '\0';
        return NULL;
    }

    strncpy(out, json_string, size - 1);
    out[size - 1] = '\0';

    cJSON_Delete(root);
    free(json_string);

    return out;
}

static char* get_json_string(cJSON* obj, const char* key, char* out, size_t size) {
    if (!obj || !out) return NULL;
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!cJSON_IsString(item) || !item->valuestring) return NULL;

    static const struct { char esc; char real; } esc_map[] = {
        {'n', '\n'}, {'t', '\t'}, {'r', '\r'}, {'\\', '\\'}, {'"', '"'}, {'\'', '\''}
    };

    char *dst = out;
    for (const char *src = item->valuestring; *src && dst < out + size - 1;) {
        if (*src == '\\' && *(src + 1)) {
            int found = 0;
            for (size_t i = 0; i < sizeof(esc_map)/sizeof(esc_map[0]); i++) {
                if (*(src + 1) == esc_map[i].esc) {
                    *dst++ = esc_map[i].real;
                    src += 2;
                    found = 1;
                    break;
                }
            }
            if (!found) *dst++ = *src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return out;
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

int extract_tool_calls(const char* response, char* output, size_t output_size, const ToolExtractor* extractor) {
    if (!response || !output) return 0;

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

    int success = 0;
    cJSON *first_tool_call = cJSON_GetArrayItem(tool_calls, 0);

    const char* extract_type = extractor ? extractor->type : "all";

    if (strcmp(extract_type, "all") == 0) {
        if (cJSON_IsArray(tool_calls)) {
            char* rendered = cJSON_PrintUnformatted(tool_calls);
            strncpy(output, rendered, output_size - 1);
            output[output_size - 1] = '\0';
            free(rendered);
            success = strlen(output) > 0;
        }
    }
    else if (strcmp(extract_type, "id") == 0) {
        if (first_tool_call) {
            get_json_string(first_tool_call, "id", output, output_size);
            success = strlen(output) > 0;
        }
    }
    else if (strcmp(extract_type, "param") == 0) {
        if (extractor && extractor->tool_name && extractor->param_name) {
            cJSON *function = cJSON_GetObjectItem(first_tool_call, "function");
            cJSON *func_name = cJSON_GetObjectItem(function, "name");

            if (cJSON_IsString(func_name) && strcmp(func_name->valuestring, extractor->tool_name) == 0) {
                cJSON *arguments = cJSON_GetObjectItem(function, "arguments");
                if (cJSON_IsString(arguments) && arguments->valuestring != NULL) {
                    cJSON* args_json = cJSON_Parse(arguments->valuestring);
                    get_json_string(args_json, extractor->param_name, output, output_size);
                    cJSON_Delete(args_json);
                    success = strlen(output) > 0;
                }
            }
        }
    }

    cJSON_Delete(root);
    return success;
}

