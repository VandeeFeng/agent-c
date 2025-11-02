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
            char tool_call_id[64] = {0};

            const char* start = strstr(msg->tool_calls, "\"id\":");
            if (start) {
                json_find(start, "id", tool_call_id, sizeof(tool_call_id));
            }

            if (strlen(tool_call_id) > 0) {
                snprintf(temp, sizeof(temp), "{\"role\":\"tool\",\"content\":\"%s\",\"tool_call_id\":\"%s\"}",
                         msg->content, tool_call_id);
            } else {
                snprintf(temp, sizeof(temp), "{\"role\":\"tool\",\"content\":\"%s\"}", msg->content);
            }
        } else if (!strcmp(msg->role, "assistant") && strlen(msg->tool_calls) > 0) {
            // Assistant message with tool_calls
            if (strlen(msg->content) > 0) {
                snprintf(temp, sizeof(temp), "{\"role\":\"assistant\",\"content\":\"%s\",\"tool_calls\":%s}",
                         msg->content, msg->tool_calls);
            } else {
                snprintf(temp, sizeof(temp), "{\"role\":\"assistant\",\"content\":null,\"tool_calls\":%s}",
                         msg->tool_calls);
            }
        } else {
            snprintf(temp, sizeof(temp), "{\"role\":\"%s\",\"content\":\"%s\"}",
                     msg->role, msg->content);
        }
        if (strlen(messages) + strlen(temp) + 10 < sizeof(messages)) strcat(messages, temp);
    }
    strcat(messages, "]");

    snprintf(out, size,
             "{\"model\":\"%s\",\"messages\":%s,\"temperature\":%.1f,\"max_tokens\":%d,\"stream\":false,"
             "\"tool_choice\":\"auto\","
             "\"tools\":[{\"type\":\"function\",\"function\":{\"name\":\"execute_command\","
             "\"description\":\"Execute shell command\",\"parameters\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}},"
             "\"required\":[\"command\"]}}}]%s}",
             config->model, messages, config->temp, config->max_tokens, config->op_providers_json);

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

char* json_error(const char* response, char* out, size_t size) {
    if (!response || !out) return NULL;
    const char* error = strstr(response, "\"error\":");
    if (!error) return NULL;
    return json_find(error, "message", out, size);
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

int extract_tool_calls(const char* response, char* tool_calls, size_t calls_size, char* tool_call_id, size_t id_size) {
    if (!response || !tool_calls) return 0;

    const char* start = strstr(response, "\"tool_calls\":");
    if (!start) return 0;

    start += 13; // Skip "tool_calls":
    while (*start == ' ' || *start == '\t') start++;

    if (*start != '[') return 0;

    const char* end = start;
    int depth = 0;
    do {
        if (*end == '[') depth++;
        else if (*end == ']') depth--;
        end++;
    } while (depth > 0 && *end);

    if (depth != 0) return 0;

    size_t len = end - start;
    if (len >= calls_size) len = calls_size - 1;
    strncpy(tool_calls, start, len);
    tool_calls[len] = '\0';

    if (tool_call_id && id_size > 0) {
        const char* id_start = strstr(tool_calls, "\"id\":");
        if (id_start) {
            json_find(id_start, "id", tool_call_id, id_size);
        } else {
            tool_call_id[0] = '\0';
        }
    }

    return 1;
}

