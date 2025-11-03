#include "agent-c.h"
#include <sys/stat.h>
#include <dirent.h>

static int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int is_directory(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int extract_skill_description(const char* skill_name, char* description, size_t desc_size) {
    if (!skill_name || !description || desc_size == 0) {
        return -1;
    }

    char skill_path[MAX_SKILL_PATH];
    snprintf(skill_path, sizeof(skill_path), "%s/.agent-c/skills/%s", getenv("HOME"), skill_name);

    if (!is_directory(skill_path)) {
        return -1;
    }

    char md_path[MAX_SKILL_PATH];
    snprintf(md_path, sizeof(md_path), "%s/SKILL.md", skill_path);

    if (!file_exists(md_path)) {
        return -2;
    }

    FILE* file = fopen(md_path, "r");
    if (!file) {
        return -3;
    }

    // find description
    char line[MAX_CONTENT];
    description[0] = '\0';

    while (fgets(line, sizeof(line), file) && strlen(description) < desc_size - 100) {
        line[strcspn(line, "\n")] = 0;

        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        if (strlen(description) == 0) {
            strncpy(description, line, desc_size - 1);
            description[desc_size - 1] = '\0';
            break;
        }
    }

    fclose(file);

    if (strlen(description) == 0) {
        snprintf(description, desc_size, "Skill: %s", skill_name);
    }

    return 0;
}

int discover_skills(char* skills_list, size_t list_size) {
    if (!skills_list || list_size == 0) {
        return -1;
    }

    char skills_dir[MAX_SKILL_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/.agent-c/skills", getenv("HOME"));

    if (!is_directory(skills_dir)) {
        skills_list[0] = '\0';
        return 0;
    }

    DIR* dir = opendir(skills_dir);
    if (!dir) {
        skills_list[0] = '\0';
        return 0;
    }

    skills_list[0] = '\0';
    struct dirent* entry;
    int skill_count = 0;

    while ((entry = readdir(dir)) != NULL && strlen(skills_list) < list_size - 200) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char entry_path[MAX_SKILL_PATH];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", skills_dir, entry->d_name);

        if (!is_directory(entry_path)) {
            continue;
        }

        if (strchr(entry->d_name, '/') || strchr(entry->d_name, '.') ||
            strchr(entry->d_name, '\\') || strlen(entry->d_name) == 0) {
            continue;
        }

        char description[MAX_CONTENT];
        if (extract_skill_description(entry->d_name, description, sizeof(description)) != 0) {
            snprintf(description, sizeof(description), "Skill: %s", entry->d_name);
        }

        char skill_entry[MAX_SKILL_PATH];
        snprintf(skill_entry, sizeof(skill_entry), "- %s: %s\n", entry->d_name, description);

        if (skill_count == 0) {
            strncpy(skills_list, skill_entry, list_size - 1);
        } else {
            strncat(skills_list, skill_entry, list_size - strlen(skills_list) - 1);
        }

        skill_count++;
    }

    closedir(dir);
    return skill_count;
}

int extract_skill(const char* skill_name, char* skill_content, size_t content_size) {
    if (!skill_name || !skill_content || content_size == 0) {
        return -1;
    }

    if (strchr(skill_name, '/') || strchr(skill_name, '.') ||
        strchr(skill_name, '\\') || strlen(skill_name) == 0) {
        return -1;
    }

    char skill_path[MAX_SKILL_PATH];
    snprintf(skill_path, sizeof(skill_path), "%s/.agent-c/skills/%s", getenv("HOME"), skill_name);

    if (!is_directory(skill_path)) {
        return -1;
    }

    char md_path[MAX_SKILL_PATH];
    snprintf(md_path, sizeof(md_path), "%s/SKILL.md", skill_path);

    if (!file_exists(md_path)) {
        return -2;
    }

    FILE* file = fopen(md_path, "r");
    if (!file) {
        return -3;
    }

    size_t bytes_read = fread(skill_content, 1, content_size - 1, file);
    skill_content[bytes_read] = '\0';
    fclose(file);

    return bytes_read > 0 ? 0 : -3;
}

static int parse_skill_command(const char* skill_command, char* skill_name,
                               char* script_name, char* args) {
    if (!skill_command || !skill_name || !script_name) {
        return -1;
    }

    char cmd_copy[MAX_CONTENT];
    strncpy(cmd_copy, skill_command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char* cmd = trim(cmd_copy);

    char* token = strtok(cmd, " \t");
    if (!token) {
        return -1;
    }
    strncpy(skill_name, token, MAX_SKILL_NAME - 1);
    skill_name[MAX_SKILL_NAME - 1] = '\0';

    token = strtok(NULL, " \t");
    if (!token) {
        return -1;
    }
    strncpy(script_name, token, MAX_SKILL_NAME - 1);
    script_name[MAX_SKILL_NAME - 1] = '\0';

    if (args) {
        args[0] = '\0';
        char* remaining = strtok(NULL, "");
        if (remaining) {
            strncpy(args, trim(remaining), MAX_CONTENT - 1);
            args[MAX_CONTENT - 1] = '\0';
        }
    }

    return 0;
}

int execute_skill(const char* skill_command, char* result, size_t result_size) {
    if (!skill_command || !result || result_size == 0) {
        return -1;
    }

    char skill_name[MAX_SKILL_NAME];
    char script_name[MAX_SKILL_NAME];
    char args[MAX_CONTENT];

    if (parse_skill_command(skill_command, skill_name, script_name, args) != 0) {
        return -1;
    }

    if (strchr(skill_name, '/') || strchr(skill_name, '.') ||
        strchr(skill_name, '\\') || strlen(skill_name) == 0) {
        return -2;
    }

    if (strchr(script_name, '/') || strchr(script_name, '\\') || strlen(script_name) == 0) {
        return -4;
    }

    char script_path[MAX_SKILL_PATH];
    snprintf(script_path, sizeof(script_path),
             "%s/.agent-c/skills/%s/scripts/%s", getenv("HOME"), skill_name, script_name);

    if (!file_exists(script_path)) {
        char script_path_with_sh[MAX_SKILL_PATH];
        snprintf(script_path_with_sh, sizeof(script_path_with_sh),
                 "%s.sh", script_path);
        if (file_exists(script_path_with_sh)) {
            strncpy(script_path, script_path_with_sh, sizeof(script_path) - 1);
            script_path[sizeof(script_path) - 1] = '\0';
        } else {
            return -3;
        }
    }

    char full_cmd[MAX_BUFFER];
    if (strlen(args) > 0) {
        snprintf(full_cmd, sizeof(full_cmd), "%s %s", script_path, args);
    } else {
        snprintf(full_cmd, sizeof(full_cmd), "%s", script_path);
    }

    char temp[] = "/tmp/ai_skill_XXXXXX";
    int fd = mkstemp(temp);
    if (fd == -1) {
        return -4;
    }
    close(fd);

    static const char* exec_template = "(%s) > '%s' 2>&1";
    char exec_cmd[MAX_BUFFER];
    snprintf(exec_cmd, sizeof(exec_cmd), exec_template, full_cmd, temp);

    int exit_code = system(exec_cmd);

    FILE* f = fopen(temp, "r");
    if (f) {
        size_t bytes = fread(result, 1, result_size - 1, f);
        result[bytes] = '\0';
        fclose(f);
    } else {
        result[0] = '\0';
        unlink(temp);
        return -4;
    }

    unlink(temp);

    return exit_code == 0 ? 0 : -4;
}
