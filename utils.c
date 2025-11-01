#include "agent-c.h"

extern Config config;

static void format_providers(const char* providers, char* out, size_t size) {
    if (!providers || !out) return;

    char buf[256];
    strncpy(buf, providers, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    out[0] = '\0';
    for (char *p = buf, *token; (token = strtok(p, ",")); p = NULL) {
        if (out[0]) strcat(out, ",");
        strcat(out, "\"");
        strcat(out, trim(token));
        strcat(out, "\"");
    }
}

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
    strcpy(config.op_providers, "cerebras");
    config.op_providers_on = 1;

    char* key = getenv("AGENTC_API_KEY");
    if (key) strncpy(config.api_key, key, 127);

    char* url = getenv("AGENTC_BASE_URL");
    if (url) strncpy(config.base_url, url, 255);

    char* model = getenv("AGENTC_MODEL");
    if (model) strncpy(config.model, model, 63);

    char* op_provider = getenv("AGENTC_OP_PROVIDER");
    if (op_provider) {
        if (strcmp(op_provider, "false") == 0) {
            config.op_providers_on = 0;
        } else {
            strncpy(config.op_providers, op_provider, 255);
            config.op_providers_on = 1;
        }
    }

    if (config.op_providers_on) {
        char formatted[300];
        format_providers(config.op_providers, formatted, sizeof(formatted));
        snprintf(config.op_providers_json, sizeof(config.op_providers_json),
                 ",\"provider\":{\"only\":[%s]}", formatted);
    } else {
        config.op_providers_json[0] = '\0';
    }
}
