#ifndef AGENT_C_H
#define AGENT_C_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAX_MESSAGES 20
#define MAX_BUFFER 8192
#define MAX_CONTENT 4096

typedef struct {
    char role[12];
    char content[MAX_CONTENT];
} Message;

typedef struct {
    char model[64];
    float temp;
    int max_tokens;
    char api_key[128];
    char base_url[256];
} Config;

typedef struct {
    Message messages[MAX_MESSAGES];
    int msg_count;
} Agent;

char* json_request(const Agent* agent, const Config* config, char* out, size_t size);
char* json_content(const char* response, char* out, size_t size);
int http_request(const char* req, char* resp, size_t resp_size);
int extract_command(const char* response, char* cmd, size_t cmd_size);

static inline int has_tool_call(const char* response) {
    if (!response) return 0;
    const char* choices = strstr(response, "\"choices\":");
    if (!choices) return 0;
    const char* message = strstr(choices, "\"message\":");
    if (!message) return 0;
    return strstr(message, "\"tool_calls\":") != NULL;
}

static inline char* trim(char* str) {
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (!*str) return str;
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    return str;
}

int process_agent(const char* task);
void init_agent(void);
int execute_command(const char* response);
void run_cli(void);
void load_config(void);

#endif
