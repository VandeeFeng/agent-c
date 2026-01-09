#include "agent-c.h"
#include <sys/stat.h>
#include <dirent.h>

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void build_skill_path(const char *skill_name, const char *script_name, char *path, size_t path_size) {
    const char *home = getenv("HOME");
    if (script_name) {
        snprintf(path, path_size, "%s/.agent-c/skills/%s/scripts/%s", home, skill_name, script_name);
    } else {
        snprintf(path, path_size, "%s/.agent-c/skills/%s", home, skill_name);
    }
}

int validate_skill_name(const char *name) {
    if (!name || !*name) return 0;
    if (strlen(name) >= MAX_SKILL_NAME) return 0;
    if (strchr(name, '/') || strchr(name, '\\')) return 0;
    return 1;
}

static int extract_skill_description(const char *skill_name, char *description, size_t desc_size) {
    if (!skill_name || !description || desc_size == 0) return -1;

    description[0] = '\0';

    char skill_path[MAX_SKILL_PATH];
    build_skill_path(skill_name, NULL, skill_path, sizeof(skill_path));

    if (!is_directory(skill_path)) return -2;

    char md_path[MAX_SKILL_PATH];
    snprintf(md_path, sizeof(md_path), "%s/SKILL.md", skill_path);

    if (!file_exists(md_path)) return -3;

    FILE *file = fopen(md_path, "r");
    if (!file) return -3;

    char line[MAX_CONTENT];
    int found_description = 0;

    while (fgets(line, sizeof(line), file) && !found_description) {
        line[strcspn(line, "\n")] = '\0';

        if (!*line || line[0] == '#') continue;

        char *desc_pos = strstr(line, "Description:");
        if (!desc_pos) desc_pos = strstr(line, "DESCRIPTION:");

        if (desc_pos) {
            char *desc_start = desc_pos + (desc_pos[10] == ':' ? 11 : 12);
            while (*desc_start == ' ') desc_start++;
            snprintf(description, desc_size, "%s", desc_start);
            found_description = 1;
            break;
        }

        if (!*description) snprintf(description, desc_size, "%s", line);
    }

    fclose(file);

    if (!*description) snprintf(description, desc_size, "Skill: %s", skill_name);

    return 0;
}

int discover_skills(char *skills_list, size_t list_size) {
    if (!skills_list || list_size == 0) return -1;

    skills_list[0] = '\0';
    int skill_count = 0;

    char skills_dir[MAX_SKILL_PATH];
    snprintf(skills_dir, sizeof(skills_dir), "%s/.agent-c/skills", getenv("HOME"));

    if (!is_directory(skills_dir)) return 0;

    DIR *dir = opendir(skills_dir);
    if (!dir) return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        if (!validate_skill_name(entry->d_name)) continue;

        char entry_path[MAX_SKILL_PATH];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", skills_dir, entry->d_name);

        if (!is_directory(entry_path)) continue;

        if (strlen(skills_list) >= list_size - 200) {
            skill_count++;
            continue;
        }

        char description[MAX_CONTENT];
        if (extract_skill_description(entry->d_name, description, sizeof(description)) != 0) {
            snprintf(description, sizeof(description), "Skill: %s", entry->d_name);
        }

        char skill_entry[MAX_SKILL_PATH];
        snprintf(skill_entry, sizeof(skill_entry), "- %s: %s\n", entry->d_name, description);

        snprintf(skills_list + strlen(skills_list), list_size - strlen(skills_list),
                 "%s%s", skill_count ? "" : "", skill_entry);

        skill_count++;
    }

    closedir(dir);
    return skill_count;
}

int extract_skill(const char *skill_name, char *skill_content, size_t content_size) {
    if (!skill_name || !skill_content || !content_size) return -1;
    if (!validate_skill_name(skill_name)) return -1;

    char skill_path[MAX_SKILL_PATH];
    build_skill_path(skill_name, NULL, skill_path, sizeof(skill_path));

    if (!is_directory(skill_path)) return -2;

    char md_path[MAX_SKILL_PATH];
    snprintf(md_path, sizeof(md_path), "%s/SKILL.md", skill_path);

    if (!file_exists(md_path)) return -2;

    FILE *file = fopen(md_path, "r");
    if (!file) return -3;

    size_t bytes_read = fread(skill_content, 1, content_size - 1, file);
    skill_content[bytes_read] = '\0';
    fclose(file);

    return bytes_read > 0 ? 0 : -3;
}

static int find_script_path(const char *skill_name, const char *script_name, char *resolved_path, size_t path_size) {
    const char *extensions[] = {".sh", ".py", ".js"};

    for (int i = 0; i < 3; i++) {
        char full_script_name[MAX_SKILL_NAME];
        snprintf(full_script_name, sizeof(full_script_name), "%s%s", script_name, extensions[i]);

        build_skill_path(skill_name, full_script_name, resolved_path, path_size);
        if (file_exists(resolved_path)) {
            return 0;
        }
    }

    return -1;
}

static int parse_command(const char *skill_command, char *skill_name, char *script_name, char *args) {
    char cmd_copy[MAX_CONTENT];
    snprintf(cmd_copy, sizeof(cmd_copy), "%s", skill_command);

    char *cmd = trim(cmd_copy);
    char *token = strtok(cmd, " \t");
    if (!token) return -1;

    snprintf(skill_name, MAX_SKILL_NAME, "%s", token);

    token = strtok(NULL, " \t");
    if (!token) return -1;

    snprintf(script_name, MAX_SKILL_NAME, "%s", token);

    if (args) {
        args[0] = '\0';
        token = strtok(NULL, "");
        if (token) snprintf(args, MAX_CONTENT, "%s", token);
    }

    return 0;
}

int execute_skill(const char *skill_command, char *result, size_t result_size) {
    if (!skill_command || !result || result_size == 0) return -1;

    result[0] = '\0';

    char skill_name[MAX_SKILL_NAME] = {0};
    char script_name[MAX_SKILL_NAME] = {0};
    char args[MAX_CONTENT] = {0};
    char script_path[MAX_SKILL_PATH] = {0};

    if (parse_command(skill_command, skill_name, script_name, args) != 0) return -1;
    if (!validate_skill_name(skill_name)) return -2;
    if (!validate_skill_name(script_name)) return -4;

    if (find_script_path(skill_name, script_name, script_path, sizeof(script_path)) != 0) return -3;

    char temp_path[64];
    snprintf(temp_path, sizeof(temp_path), "/tmp/ai_skill_XXXXXX");

    int fd = mkstemp(temp_path);
    if (fd == -1) return -1;
    close(fd);

    char exec_cmd[MAX_BUFFER];
    if (*args) {
        snprintf(exec_cmd, sizeof(exec_cmd), "\"%s\" %s > \"%s\" 2>&1", script_path, args, temp_path);
    } else {
        snprintf(exec_cmd, sizeof(exec_cmd), "\"%s\" > \"%s\" 2>&1", script_path, temp_path);
    }

    int exit_code = system(exec_cmd);

    FILE *temp_file = fopen(temp_path, "r");
    int read_result = -1;
    if (temp_file) {
        size_t bytes_read = fread(result, 1, result_size - 1, temp_file);
        result[bytes_read] = '\0';
        fclose(temp_file);
        read_result = 0;
    }

    unlink(temp_path);

    if (read_result != 0) return -1;

    return (exit_code == 0) ? 0 : -1;
}

int extract_skill_name(const char *response, char *skill_name, size_t name_size) {
    static const ToolExtractor extractor = {"param", "extract_skill", "skill_name"};
    return extract_tool_calls(response, skill_name, name_size, &extractor);
}

int extract_skill_command(const char *response, char *skill_command, size_t cmd_size) {
    static const ToolExtractor extractor = {"param", "execute_skill", "skill_command"};
    return extract_tool_calls(response, skill_command, cmd_size, &extractor);
}
