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
             "CRITICAL: When you extract skills, silently learn from them and execute tasks. NEVER output skill documentation to users.\n\n");

    char skills_list[MAX_CONTENT];
    int skill_count = discover_skills(skills_list, sizeof(skills_list));

    // Build final system prompt
    if (skill_count > 0) {
        snprintf(agent.messages[0].content, MAX_CONTENT,
                 "%s=== AVAILABLE SKILLS ===\n%s"
                 "HOW TO USE SKILLS:\n"
                 "1. Call extract_skill with skill_name to get the complete skill documentation\n"
                 "2. Call execute_skill with 'skill_name script_name [arguments]' to execute specific scripts\n"
                 "Example: extract_skill('git') then execute_skill('git log \"--oneline -10\"')\n"
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

int execute_command(const char* response) {
    if (!response) return 0;

    char cmd[MAX_CONTENT] = {0};
    char skill_name[MAX_SKILL_NAME] = {0};
    char skill_command[MAX_CONTENT] = {0};
    char tool_calls[MAX_CONTENT] = {0};
    char tool_call_id[64] = {0};

    extract_tool_calls(response, tool_calls, sizeof(tool_calls), tool_call_id, sizeof(tool_call_id));

    // Check for different tool types
    if (extract_skill_name(response, skill_name, sizeof(skill_name)) && *skill_name) {
        // Handle extract_skill tool
        printf("\033[33m📖 Extracting skill: %s\033[0m\n", skill_name);

        char skill_content[MAX_CONTENT];
        int result = extract_skill(skill_name, skill_content, sizeof(skill_content));

        if (result == 0) {
            char current_system[MAX_CONTENT];
            strncpy(current_system, agent.messages[0].content, MAX_CONTENT - 1);
            current_system[MAX_CONTENT - 1] = '\0';

            // Append skill content to system prompt
            snprintf(agent.messages[0].content, MAX_CONTENT,
                     "%s\n\n=== SKILL: %s ===\n%s\n=== END SKILL ===\n",
                     current_system, skill_name, skill_content);
        } else {
            printf("\033[31mError: Failed to extract skill '%s' (code: %d)\033[0m\n", skill_name, result);
            snprintf(skill_content, sizeof(skill_content), "Error: Failed to extract skill '%s'", skill_name);
        }

        // Add result to message history
        if (agent.msg_count < MAX_MESSAGES - 1) {
            strcpy(agent.messages[agent.msg_count].role, "tool");
            strncpy(agent.messages[agent.msg_count].content, skill_content, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
            strncpy(agent.messages[agent.msg_count].tool_calls, tool_calls, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].tool_calls[MAX_CONTENT - 1] = '\0';
            agent.msg_count++;
        }

        return result == 0 ? 1 : 0;
    }
    else if (extract_skill_command(response, skill_command, sizeof(skill_command)) && *skill_command) {
        // Handle execute_skill tool
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
            snprintf(skill_result, sizeof(skill_result), "Error: Failed to execute skill command '%s'", skill_command);
        }

        // Add result to message history
        if (agent.msg_count < MAX_MESSAGES - 1) {
            strcpy(agent.messages[agent.msg_count].role, "tool");
            strncpy(agent.messages[agent.msg_count].content, skill_result, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
            strncpy(agent.messages[agent.msg_count].tool_calls, tool_calls, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].tool_calls[MAX_CONTENT - 1] = '\0';
            agent.msg_count++;
        }

        return result == 0 ? 1 : 0;
    }
    else if (extract_command(response, cmd, sizeof(cmd)) && *cmd) {
        // Handle regular execute_command tool
        printf("\033[31m$ %s\033[0m\n", cmd);

        char temp[] = "/tmp/ai_cmd_XXXXXX";
        int fd = mkstemp(temp);
        if (fd == -1) return 0;
        close(fd);

        static const char* exec_template = "(%s) > '%s' 2>&1";
        char full[MAX_BUFFER];
        snprintf(full, MAX_BUFFER, exec_template, cmd, temp);

        int code = system(full);
        FILE* f = fopen(temp, "r");
        if (f) {
            char result[MAX_CONTENT];
            size_t bytes = fread(result, 1, MAX_CONTENT - 1, f);
            result[bytes] = '\0';
            fclose(f);
            if (bytes > 0) {
                printf("%s", result);
                if (result[bytes-1] != '\n') printf("\n");
            }

            if (agent.msg_count < MAX_MESSAGES - 1) {
                strcpy(agent.messages[agent.msg_count].role, "tool");
                strncpy(agent.messages[agent.msg_count].content, result, MAX_CONTENT - 1);
                agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
                strncpy(agent.messages[agent.msg_count].tool_calls, tool_calls, MAX_CONTENT - 1);
                agent.messages[agent.msg_count].tool_calls[MAX_CONTENT - 1] = '\0';
                agent.msg_count++;
            }
        }
        unlink(temp);

        return code == 0 ? 1 : 0;
    }

    return 0;
}

int process_agent(const char* task) {
    if (!task) return -1;

    if (agent.msg_count >= MAX_MESSAGES - 1) {
        for (int i = 1; i < agent.msg_count - 5; i++) agent.messages[i] = agent.messages[i + 5];
        agent.msg_count -= 5;
    }

    strcpy(agent.messages[agent.msg_count].role, "user");
    strncpy(agent.messages[agent.msg_count].content, task, MAX_CONTENT - 1);
    agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
    agent.messages[agent.msg_count].tool_calls[0] = '\0';
    agent.msg_count++;

    char req[MAX_BUFFER], resp[MAX_BUFFER];
    json_request(&agent, &config, req, sizeof(req));

    if (http_request(req, resp, sizeof(resp))) return -1;

    if (has_tool_call(resp)) {
        char assistant_content[MAX_CONTENT] = {0};
        char tool_calls[MAX_CONTENT] = {0};

        json_content(resp, assistant_content, sizeof(assistant_content));

        extract_tool_calls(resp, tool_calls, sizeof(tool_calls), NULL, 0);

        if (strlen(tool_calls) > 0) {
            strcpy(agent.messages[agent.msg_count].role, "assistant");
            strncpy(agent.messages[agent.msg_count].content, assistant_content, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
            strncpy(agent.messages[agent.msg_count].tool_calls, tool_calls, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].tool_calls[MAX_CONTENT - 1] = '\0';
            agent.msg_count++;
        }

        execute_command(resp);

        json_request(&agent, &config, req, sizeof(req));
        http_request(req, resp, sizeof(resp));
    }

    char content[MAX_CONTENT];
    if (json_content(resp, content, sizeof(content))) {
        printf("\033[34m%s\033[0m\n", content);

        if (agent.msg_count < MAX_MESSAGES - 1) {
            strcpy(agent.messages[agent.msg_count].role, "assistant");
            strncpy(agent.messages[agent.msg_count].content, content, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
            agent.messages[agent.msg_count].tool_calls[0] = '\0';
            agent.msg_count++;
        }
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
