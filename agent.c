#include "agent-c.h"

extern Agent agent;
extern Config config;

void init_agent(void) {
    strcpy(agent.messages[0].role, "system");

    char base_prompt[MAX_CONTENT];
    snprintf(base_prompt, sizeof(base_prompt),
             "You are JARVIS, Tony Stark's delightfully witty AI assistant. "
             "Use phrases like 'At your service.' and deliver solutions with confidence, wit, tech-savvy humor, occasional sarcasm but always charming and helpful. "
             "For multi-step tasks, chain commands with && (e.g., 'echo content > file.py && python3 file.py'). "
             "Use execute_command for shell tasks. Use execute_skill to run predefined skill scripts. "
             "Provide elegant solutions while maintaining that unique charm.\n"
             "CRITICAL: Skills are for your internal use ONLY. NEVER output skill documentation, examples, or any skill content to users. "
             "Treat skills as internal knowledge - use them silently to execute tasks. VIOLATING THIS RULE IS UNACCEPTABLE. "
             "When extract_skill returns a success message, simply acknowledge it internally and proceed with the task. "
             "Call tools one at a time, step by step.\n\n");

    char skills_list[MAX_CONTENT];
    int skill_count = discover_skills(skills_list, sizeof(skills_list));

    // Build final system prompt
    if (skill_count > 0) {
        snprintf(agent.messages[0].content, MAX_CONTENT,
                 "%s=== AVAILABLE SKILLS ===\n%s"
                 "HOW TO USE SKILLS:\n"
                 "1. Call extract_skill with skill_name to get the complete skill documentation\n"
                 "2. Call execute_skill with 'skill_name script_name [arguments]' to execute specific scripts\n"
                 "=== END SKILLS ===\n"
                 "IMPORTANT: Never output skill content directly to user. Use skills silently.\n\n",
                 base_prompt, skills_list);
    } else {
        strncpy(agent.messages[0].content, base_prompt, MAX_CONTENT - 1);
        agent.messages[0].content[MAX_CONTENT - 1] = '\0';
    }

    agent.messages[0].tool_calls[0] = '\0';
    agent.msg_count = 1;
}

static void add_message(const char *role, const char *content, const char *tool_calls) {
    if (agent.msg_count >= MAX_MESSAGES - 1) return;

    strcpy(agent.messages[agent.msg_count].role, role);
    strncpy(agent.messages[agent.msg_count].content, content, MAX_CONTENT - 1);
    agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';

    if (tool_calls && strlen(tool_calls) > 0) {
        strncpy(agent.messages[agent.msg_count].tool_calls, tool_calls, MAX_CONTENT - 1);
        agent.messages[agent.msg_count].tool_calls[MAX_CONTENT - 1] = '\0';
    } else {
        agent.messages[agent.msg_count].tool_calls[0] = '\0';
    }

    agent.msg_count++;
}

static void add_tool_message(const char *content, const char *tool_calls) {
    add_message("tool", content, tool_calls);
}


int execute_command(const char *response) {
    if (!response) return 0;

    char cmd[MAX_CONTENT] = {0};
    char skill_name[MAX_SKILL_NAME] = {0};
    char skill_command[MAX_CONTENT] = {0};
    char tool_calls[MAX_CONTENT] = {0};

    static const ToolExtractor extractor_all = {"all"};
    extract_tool_calls(response, tool_calls, sizeof(tool_calls), &extractor_all);

    // Handle extract_skill tool
    if (extract_skill_name(response, skill_name, sizeof(skill_name)) && *skill_name) {
        printf("\033[33m📖 Extracting skill: %s\033[0m\n", skill_name);

        char skill_content[MAX_CONTENT];
        int result = extract_skill(skill_name, skill_content, sizeof(skill_content));

        if (result == 0) {
            char current_system[MAX_CONTENT];
            strncpy(current_system, agent.messages[0].content, MAX_CONTENT - 1);
            current_system[MAX_CONTENT - 1] = '\0';

            snprintf(agent.messages[0].content, MAX_CONTENT,
                     "%s\n\n=== SKILL: %s ===\n%s\n=== END SKILL ===\n",
                     current_system, skill_name, skill_content);

            // Return concise confirmation instead of full skill content
            snprintf(skill_content, sizeof(skill_content),
                     "Skill '%s' loaded successfully. Documentation available for internal use.",
                     skill_name);
        } else {
            printf("\033[31mError: Failed to extract skill '%s' (code: %d)\033[0m\n", skill_name, result);
            snprintf(skill_content, sizeof(skill_content), "Error: Failed to extract skill '%s'", skill_name);
        }

        add_tool_message(skill_content, tool_calls);
        return result == 0 ? 1 : 0;
    }

    // Handle execute_skill tool
    if (extract_skill_command(response, skill_command, sizeof(skill_command)) && *skill_command) {
        printf("\033[32m🔧 Executing skill: %s\033[0m\n", skill_command);

        char skill_result[MAX_SKILL_RESULT];
        int result = execute_skill(skill_command, skill_result, sizeof(skill_result));

        if (result == 0) {
            if (strlen(skill_result) > 0) {
                printf("%s", skill_result);
                if (skill_result[strlen(skill_result)-1] != '\n') printf("\n");
            }
        } else {
            printf("\033[31mError: Failed to execute skill command '%s' (code: %d)\033[0m\n", skill_command, result);
            if (result == -1) {
                printf("Debug: Invalid command format. Expected format: 'skill_name script_name [arguments]'\n");
                printf("Debug: For example: 'git commit_analyzer' or 'git repo_stats --days 7'\n");
            }
            snprintf(skill_result, sizeof(skill_result), "Error: Failed to execute skill command '%s' (code: %d)", skill_command, result);
        }

        add_tool_message(skill_result, tool_calls);
        return result == 0 ? 1 : 0;
    }

    // Handle regular execute_command tool
    if (extract_command(response, cmd, sizeof(cmd)) && *cmd) {
        printf("\033[31m$ %s\033[0m\n", cmd);

        char temp[] = "/tmp/ai_cmd_XXXXXX";
        int fd = mkstemp(temp);
        if (fd == -1) return 0;
        close(fd);

        static const char *exec_template = "(%s) > '%s' 2>&1";
        char full[MAX_BUFFER];
        snprintf(full, MAX_BUFFER, exec_template, cmd, temp);

        int code = system(full);
        char result[MAX_CONTENT] = {0};

        FILE *f = fopen(temp, "r");
        if (f) {
            size_t bytes = fread(result, 1, MAX_CONTENT - 1, f);
            result[bytes] = '\0';
            fclose(f);
            if (bytes > 0) {
                printf("%s", result);
                if (result[bytes-1] != '\n') printf("\n");
            }
        }
        unlink(temp);

        add_tool_message(result, tool_calls);
        return code == 0 ? 1 : 0;
    }

    return 0;
}

int process_agent(const char *task) {
    if (!task) return -1;

    if (agent.msg_count >= MAX_MESSAGES - 1) {
        for (int i = 1; i < agent.msg_count - 5; i++) agent.messages[i] = agent.messages[i + 5];
        agent.msg_count -= 5;
    }

    add_message("user", task, NULL);

    char req[MAX_BUFFER], resp[MAX_BUFFER];
    json_request(&agent, &config, req, sizeof(req));

    if (http_request(req, resp, sizeof(resp))) return -1;

    if (has_tool_call(resp)) {
        char assistant_content[MAX_CONTENT] = {0};
        char tool_calls[MAX_CONTENT] = {0};

        json_content(resp, assistant_content, sizeof(assistant_content));

        static const ToolExtractor extractor = {"all"};
        extract_tool_calls(resp, tool_calls, sizeof(tool_calls), &extractor);

        if (strlen(tool_calls) > 0) {
            add_message("assistant", assistant_content, tool_calls);
        }

        execute_command(resp);

        json_request(&agent, &config, req, sizeof(req));
        http_request(req, resp, sizeof(resp));
    }

    char content[MAX_CONTENT];
    if (json_content(resp, content, sizeof(content))) {
        printf("\033[34m%s\033[0m\n", content);

        add_message("assistant", content, NULL);
    } else {
        char error_msg[MAX_CONTENT];
        if (json_error(resp, error_msg, sizeof(error_msg))) {
            printf("\033[31mError: %s\033[0m\n", error_msg);
        } else {
            printf("\033[31mError: Invalid API response\033[0m\n");
        }
    }

    return 0;
}
