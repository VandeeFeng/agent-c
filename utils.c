#include "agent-c.h"

extern Config config;

int http_request(const char* req, char* resp, size_t resp_size) {
    char temp[] = "/tmp/ai_req_XXXXXX";
    int fd = mkstemp(temp);
    if (fd == -1) return -1;
    write(fd, req, strlen(req));
    close(fd);

    char curl_template[MAX_BUFFER];
    snprintf(curl_template, sizeof(curl_template),
        "curl -s -X POST '%s' -H 'Content-Type: application/json' -H 'Authorization: Bearer %%s' -d @'%%s' --max-time 60",
        config.base_url);
    char curl[MAX_BUFFER];
    snprintf(curl, sizeof(curl), curl_template, config.api_key, temp);

    FILE* pipe = popen(curl, "r");
    if (!pipe) { unlink(temp); return -1; }

    size_t bytes = fread(resp, 1, resp_size - 1, pipe);
    resp[bytes] = '\0';
    pclose(pipe);
    unlink(temp);
    return 0;
}

void load_config(void) {
    strcpy(config.model, "qwen/qwen3-coder");
    config.temp = 0.1;
    config.max_tokens = 1000;
    config.api_key[0] = '\0';
    strcpy(config.base_url, "https://openrouter.ai/api/v1/chat/completions");

    char* key = getenv("AGENTC_API_KEY");
    if (key) strncpy(config.api_key, key, 127);

    char* url = getenv("AGENTC_BASE_URL");
    if (url) strncpy(config.base_url, url, 255);

    char* model = getenv("AGENTC_MODEL");
    if (model) strncpy(config.model, model, 63);
}
