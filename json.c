#include "agent-c.h"
#define SJ_IMPL
#include "sj.h/sj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper: compare sj_Value with string
static bool eq(sj_Value v, const char *s) {
    size_t len = v.end - v.start;
    return strlen(s) == len && memcmp(s, v.start, len) == 0;
}

// Helper: extract and unescape string
static char *get_str(sj_Value v, char *out, size_t size) {
    if (v.type != SJ_STRING || !out) {
        if (out) out[0] = '\0';
        return NULL;
    }

    size_t len = v.end - v.start;
    if (len >= size) len = size - 1;

    // Copy and unescape in one pass
    char *dst = out;
    for (const char *src = v.start; src < v.end && dst < out + len;) {
        if (*src == '\\' && src + 1 < v.end) {
            switch (*++src) {
            case 'n': *dst++ = '\n'; break;
            case 't': *dst++ = '\t'; break;
            case 'r': *dst++ = '\r'; break;
            case '\\': *dst++ = '\\'; break;
            case '"': *dst++ = '"'; break;
            default: *dst++ = *src; break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return out;
}

// Helper: check if reader has errors
static bool has_error(sj_Reader *r) {
    return r && r->error;
}

// Helper: get error message with location
static char *get_error_info(sj_Reader *r, char *out, size_t size) {
    if (!has_error(r)) {
        snprintf(out, size, "No error");
        return out;
    }

    int line, col;
    sj_location(r, &line, &col);
    snprintf(out, size, "JSON error at line %d, column %d: %s",
             line, col, r->error);
    return out;
}

// Helper: find value by key in object
static sj_Value find_in_obj(sj_Reader *r, sj_Value obj, const char *key) {
    if (has_error(r)) return (sj_Value){ .type = SJ_ERROR };

    sj_Value k, v;
    while (sj_iter_object(r, obj, &k, &v)) {
        if (has_error(r)) return (sj_Value){ .type = SJ_ERROR };
        if (eq(k, key)) return v;
    }
    return (sj_Value){ .type = SJ_ERROR };
}

// Helper: find nested value by path
static sj_Value find_by_path(sj_Reader *r, const char *response, const char *path[], int depth) {
    *r = sj_reader((char*)response, strlen(response));
    sj_Value v = sj_read(r);
    if (v.type == SJ_ERROR || r->error) return (sj_Value){ .type = SJ_ERROR };

    for (int i = 0; i < depth; i++) {
        if (v.type == SJ_OBJECT) {
            v = find_in_obj(r, v, path[i]);
        }
        else if (v.type == SJ_ARRAY && strcmp(path[i], "0") == 0) {
            sj_Value item;
            if (sj_iter_array(r, v, &item)) {
                v = item;
            } else {
                return (sj_Value){ .type = SJ_ERROR };
            }
        } else {
            return (sj_Value){ .type = SJ_ERROR };
        }
        if (v.type == SJ_ERROR) break;
    }

    return v;
}

// Build JSON using string operations (sj.h is for parsing, not building)
char *json_request(const Agent *agent, const Config *config, char *out, size_t size) {
    static const char *tools =
        "[{\"type\":\"function\",\"function\":{\"name\":\"execute_command\",\"description\":\"Execute shell command\",\"parameters\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}},\"required\":[\"command\"]}}},"
        "{\"type\":\"function\",\"function\":{\"name\":\"extract_skill\",\"description\":\"Extract content from SKILL.md file\",\"parameters\":{\"type\":\"object\",\"properties\":{\"skill_name\":{\"type\":\"string\"}},\"required\":[\"skill_name\"]}}},"
        "{\"type\":\"function\",\"function\":{\"name\":\"execute_skill\",\"description\":\"Execute skill script with format: 'skill_name script_name [arguments]'. Script name should not include file extension.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"skill_command\":{\"type\":\"string\"}},\"required\":[\"skill_command\"]}}}]";

    char *p = out;

    // Header
    p += snprintf(p, size - (p - out),
                  "{\"model\":\"%s\",\"temperature\":%g,\"max_tokens\":%d,\"stream\":false,"
                  "\"tool_choice\":\"auto\",\"tools\":%s,\"messages\":[",
                  config->model, config->temp, config->max_tokens, tools);

    // Messages
    for (int i = 0; i < agent->msg_count; i++) {
        const Message *m = &agent->messages[i];
        if (i) p += snprintf(p, size - (p - out), ",");

        if (strcmp(m->role, "tool") == 0) {
            // Extract tool_call_id from tool_calls
            sj_Reader tr = sj_reader((char*)m->tool_calls, strlen(m->tool_calls));
            sj_Value arr = sj_read(&tr);
            char id[64] = "";
            if (arr.type == SJ_ARRAY) {
                sj_Value tool;
                if (sj_iter_array(&tr, arr, &tool)) {
                    sj_Value id_val = find_in_obj(&tr, tool, "id");
                    get_str(id_val, id, sizeof(id));
                }
            }
            p += snprintf(p, size - (p - out),
                          "{\"role\":\"%s\",\"content\":\"%s\",\"tool_call_id\":\"%s\"}",
                          m->role, m->content, id);
        }
        else if (strcmp(m->role, "assistant") == 0 && m->tool_calls[0]) {
            p += snprintf(p, size - (p - out),
                          "{\"role\":\"%s\",\"content\":%s,\"tool_calls\":%s}",
                          m->role, m->content[0] ? "\"" : "", m->tool_calls);
            if (m->content[0]) {
                // Fix missing quote
                char *comma = strrchr(p, ',');
                if (comma) memmove(comma, comma - 1, strlen(comma) + 2);
            }
        }
        else {
            p += snprintf(p, size - (p - out),
                          "{\"role\":\"%s\",\"content\":\"%s\"}", m->role, m->content);
        }
    }

    // Footer
    p += snprintf(p, size - (p - out), "]");
    if (config->op_providers_on && config->op_providers_json[0]) {
        p += snprintf(p, size - (p - out), ",\"provider\":%s", config->op_providers_json);
    }
    snprintf(p, size - (p - out), "}");

    return out;
}

char *json_content(const char *response, char *out, size_t size) {
    sj_Reader r;
    const char *path[] = {"choices", "0", "message", "content"};
    sj_Value v = find_by_path(&r, response, path, 4);

    if (v.type == SJ_NULL) {
        out[0] = '\0';
    } else {
        get_str(v, out, size);
    }

    return v.type != SJ_ERROR ? out : NULL;
}

char *json_error(const char *response, char *out, size_t size) {
    sj_Reader r;
    const char *path[] = {"error", "message"};
    sj_Value v = find_by_path(&r, response, path, 2);
    return get_str(v, out, size) ? out : NULL;
}

int extract_command(const char *response, char *cmd, size_t cmd_size) {
    sj_Reader r = sj_reader((char*)response, strlen(response));
    sj_Value root = sj_read(&r);
    if (root.type == SJ_ERROR || r.error) return 0;

    sj_Value choices = find_in_obj(&r, root, "choices");
    if (choices.type != SJ_ARRAY) return 0;

    sj_Value choice;
    if (!sj_iter_array(&r, choices, &choice)) return 0;

    sj_Value message = find_in_obj(&r, choice, "message");
    sj_Value tool_calls = find_in_obj(&r, message, "tool_calls");
    if (tool_calls.type != SJ_ARRAY) return 0;

    sj_Value tool_call;
    if (!sj_iter_array(&r, tool_calls, &tool_call)) return 0;

    sj_Value function = find_in_obj(&r, tool_call, "function");
    sj_Value arguments = find_in_obj(&r, function, "arguments");
    if (arguments.type != SJ_STRING) return 0;

    // Parse arguments string and extract command
    char args_str[512];
    get_str(arguments, args_str, sizeof(args_str));
    if (!*args_str) return 0;

    sj_Reader ar = sj_reader(args_str, strlen(args_str));
    sj_Value args_obj = sj_read(&ar);
    if (args_obj.type == SJ_ERROR || ar.error) return 0;

    sj_Value cmd_val = find_in_obj(&ar, args_obj, "command");
    if (!get_str(cmd_val, cmd, cmd_size)) return 0;

    return strlen(cmd) > 0;
}

int extract_tool_calls(const char *response, char *output, size_t output_size, const ToolExtractor *extractor) {
    sj_Reader r = sj_reader((char*)response, strlen(response));
    sj_Value root = sj_read(&r);
    if (root.type == SJ_ERROR || r.error) return 0;

    sj_Value choices = find_in_obj(&r, root, "choices");
    if (choices.type != SJ_ARRAY) return 0;

    sj_Value choice;
    if (!sj_iter_array(&r, choices, &choice)) return 0;

    sj_Value message = find_in_obj(&r, choice, "message");
    sj_Value tool_calls = find_in_obj(&r, message, "tool_calls");
    if (tool_calls.type != SJ_ARRAY) return 0;

    const char *type = extractor ? extractor->type : "all";

    if (strcmp(type, "all") == 0) {
        size_t len = tool_calls.end - tool_calls.start;
        if (len >= output_size) return 0;
        memcpy(output, tool_calls.start, len);
        output[len] = '\0';
        return 1;
    }
    else {
        sj_Value tool_call;
        if (!sj_iter_array(&r, tool_calls, &tool_call)) return 0;

        if (strcmp(type, "id") == 0) {
            sj_Value id = find_in_obj(&r, tool_call, "id");
            get_str(id, output, output_size);
            return strlen(output) > 0;
        }
        else if (strcmp(type, "param") == 0) {
            sj_Value function = find_in_obj(&r, tool_call, "function");
            sj_Value name = find_in_obj(&r, function, "name");

            if (eq(name, extractor->tool_name)) {
                sj_Value arguments = find_in_obj(&r, function, "arguments");
                if (arguments.type != SJ_STRING) return 0;

                char args_str[512];
                get_str(arguments, args_str, sizeof(args_str));
                if (!*args_str) return 0;

                sj_Reader ar = sj_reader(args_str, strlen(args_str));
                sj_Value args_obj = sj_read(&ar);
                if (args_obj.type == SJ_ERROR || ar.error) return 0;

                sj_Value param = find_in_obj(&ar, args_obj, extractor->param_name);
                if (!get_str(param, output, output_size)) return 0;
                return *output > 0;
            }
        }
    }

    return 0;
}
