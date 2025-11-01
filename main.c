#include "agent-c.h"

Agent agent;
Config config;

void cleanup(int sig) {
    (void)sig;
    exit(0);
}

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    load_config();

    if (!config.api_key[0]) {
        fprintf(stderr, "AGENTC_API_KEY required\n");
        return 1;
    }

    init_agent();
    run_cli();

    return 0;
}
