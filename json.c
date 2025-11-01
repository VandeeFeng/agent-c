#include "agent-c.h"

static char* json_find(const char* json, const char* key, char* out, size_t size) {
    if (!json || !key || !out) return NULL;
    char pattern[64];
    snprintf(pattern, 64, "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if (!start) return NULL;
    start += strlen(pattern);
    while (*start == ' ' || *start == '\t') start++;

    if (*start == '"') {
        start++;
        const char* end = start;
        while (*end && *end != '"') {
            if (*end == '\\' && end[1]) end += 2;
            else end++;
        }
        size_t len = end - start;
        if (len >= size) len = size - 1;
        strncpy(out, start, len);
        out[len] = '\0';

        for (char* p = out; *p; p++) {
            if (*p == '\\' && p[1]) {
                switch (p[1]) {
                case 'n': *p = '\n'; memmove(p+1, p+2, strlen(p+1)); break;
                case 't': *p = '\t'; memmove(p+1, p+2, strlen(p+1)); break;
                case 'r': *p = '\r'; memmove(p+1, p+2, strlen(p+1)); break;
                case '\\': case '"': memmove(p, p+1, strlen(p)); break;
                }
            }
        }
    } else {
        const char* end = start;
        while (*end && *end != ',' && *end != '}' && *end != ' ' && *end != '\n') end++;
        size_t len = end - start;
        if (len >= size) len = size - 1;
        strncpy(out, start, len);
        out[len] = '\0';
    }
    return out;
}

char* json_request(const Agent* agent, const Config* config, char* out, size_t size) {
    if (!agent || !out) return NULL;

    char messages[MAX_BUFFER] = "[";
    for (int i = 0; i < agent->msg_count; i++) {
        if (i > 0) strcat(messages, ",");
        const Message* msg = &agent->messages[i];
        char temp[MAX_CONTENT + 100];
        if (!strcmp(msg->role, "tool")) {
            snprintf(temp, sizeof(temp), "{\"role\":\"tool\",\"content\":\"%s\"}", msg->content);
        } else {
            snprintf(temp, sizeof(temp), "{\"role\":\"%s\",\"content\":\"%s\"}", 
                     msg->role, msg->content);
        }
        if (strlen(messages) + strlen(temp) + 10 < sizeof(messages)) strcat(messages, temp);
    }
    strcat(messages, "]");

    const char* template = "{\"model\":\"%s\",\"messages\":%s,\"temperature\":%.1f,\"max_tokens\":%d,\"stream\":false,"
        "\"tool_choice\":\"auto\","
        "\"tools\":[{\"type\":\"function\",\"function\":{\"name\":\"execute_command\","
        "\"description\":\"Execute shell command\",\"parameters\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}},"
        "\"required\":[\"command\"]}}}],"
        "\"provider\":{\"only\":[\"cerebras\"]}}";
    snprintf(out, size, template, config->model, messages, config->temp, config->max_tokens);

    return out;
}

char* json_content(const char* response, char* out, size_t size) {
    if (!response || !out) return NULL;
    const char* choices = strstr(response, "\"choices\":");
    if (!choices) return NULL;
    const char* message = strstr(choices, "\"message\":");
    if (!message) return NULL;
    return json_find(message, "content", out, size);
}

int extract_command(const char* response, char* cmd, size_t cmd_size) {
    if (!response || !cmd) return 0;

    const char* start = strstr(response, "\"tool_calls\":");
    if (!start) return 0;

    start = strstr(start, "\"arguments\":");
    if (!start) return 0;

    char args[1024];
    if (!json_find(start, "arguments", args, sizeof(args))) return 0;

    return json_find(args, "command", cmd, cmd_size) != NULL;
}

