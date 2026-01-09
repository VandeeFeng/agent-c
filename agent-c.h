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

// Skill system constants
#define MAX_SKILL_NAME 64
#define MAX_SKILL_PATH 512
#define MAX_SKILL_RESULT 4096

typedef struct {
    char role[12];
    char content[MAX_CONTENT];
    char tool_calls[MAX_CONTENT];
} Message;

typedef struct {
    char model[64];
    float temp;
    int max_tokens;
    char api_key[128];
    char base_url[256];
    char op_providers[256];
    char op_providers_json[512];
    int op_providers_on;
} Config;

typedef struct {
    Message messages[MAX_MESSAGES];
    int msg_count;
} Agent;

char *json_request(const Agent *agent, const Config *config, char *out, size_t size);
char *json_content(const char *response, char *out, size_t size);
char *json_error(const char *response, char *out, size_t size);
int http_request(const char *req, char *resp, size_t resp_size);
int extract_command(const char *response, char *cmd, size_t cmd_size);

typedef struct {
    const char *type;
    const char *tool_name;
    const char *param_name;
} ToolExtractor;

int extract_tool_calls(const char *response, char *output, size_t output_size, const ToolExtractor *extractor);
int extract_skill_name(const char *response, char *skill_name, size_t name_size);
int extract_skill_command(const char *response, char *skill_command, size_t cmd_size);

static inline int has_tool_call(const char *response) {
    if (!response) return 0;
    const char *choices = strstr(response, "\"choices\":");
    if (!choices) return 0;
    const char *message = strstr(choices, "\"message\":");
    if (!message) return 0;
    return strstr(message, "\"tool_calls\":") != NULL;
}

static inline char *trim(char *str) {
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (!*str) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    return str;
}

int process_agent(const char *task);
void init_agent(void);
int execute_command(const char *response);
void run_cli(void);
void load_config(void);

// Skill system functions
int discover_skills(char *skills_list, size_t list_size);
int extract_skill(const char *skill_name, char *skill_content, size_t content_size);
int execute_skill(const char *skill_command, char *result, size_t result_size);

// Helper functions
int validate_skill_name(const char *name);

#endif
