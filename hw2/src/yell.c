#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "../include/chat.h"

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;

    char *my_id_str = getenv("MY_USER_ID");
    if (!my_id_str) return 1;
    int my_id = atoi(my_id_str);

    int shm_id = shmget(SHM_KEY, sizeof(ChatState), 0666);
    if (shm_id < 0) return 1;
    ChatState *chat_state = (ChatState *)shmat(shm_id, NULL, 0);

    // 把參數串起來 (例如 yell hello world)
    char message[1024] = {0};
    for (int i = 1; i < argc; i++) {
        strcat(message, argv[i]);
        if (i < argc - 1) strcat(message, " ");
    }

    char broadcast_msg[1200];
    sprintf(broadcast_msg, "<user(%d) yelled>: %s\n", my_id, message);

    // 留言並按門鈴 (廣播給所有人)
    for (int i = 0; i < MAX_USERS; i++) {
        if (chat_state->users[i].is_active) {
            strcpy(chat_state->users[i].msg_buffer, broadcast_msg);
            kill(chat_state->users[i].pid, SIGUSR1);
        }
    }
    return 0;
}
