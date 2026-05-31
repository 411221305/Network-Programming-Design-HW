#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "../include/chat.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tell <user_id> <message>\n");
        return 1;
    }

    int target_id = atoi(argv[1]);
    char *my_id_str = getenv("MY_USER_ID");
    if (!my_id_str) return 1;
    int my_id = atoi(my_id_str);

    int shm_id = shmget(SHM_KEY, sizeof(ChatState), 0666);
    if (shm_id < 0) return 1;
    ChatState *chat_state = (ChatState *)shmat(shm_id, NULL, 0);

    char message[1024] = {0};
    for (int i = 2; i < argc; i++) {
        strcat(message, argv[i]);
        if (i < argc - 1) strcat(message, " ");
    }

    char private_msg[1200];
    sprintf(private_msg, "<user(%d) told you>: %s\n", my_id, message);

    int found = 0;
    // 留言並按門鈴 (只給目標對象)
    for (int i = 0; i < MAX_USERS; i++) {
        if (chat_state->users[i].is_active && chat_state->users[i].id == target_id) {
            strcpy(chat_state->users[i].msg_buffer, private_msg);
            kill(chat_state->users[i].pid, SIGUSR1);
            found = 1;
            break;
        }
    }

    if (found) {
        printf("send accept!\n");
    } else {
        printf("User %d does not exist.\n", target_id);
    }
    return 0;
}
