#include "agent-c.h"

void run_cli(void) {
    char input[MAX_BUFFER];

    while (1) {
        printf("\033[32m🤠Agent> \033[0m");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        char *nl = strchr(input, '\n');
        if (nl) *nl = '\0';

        char *cmd = trim(input);
        if (!*cmd) continue;

        if (process_agent(cmd)) printf("Failed\n");
    }
}
