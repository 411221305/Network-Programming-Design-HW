#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "../include/chat.h"

int main(int argc, char *argv[]) {
    if (argc != 2) return 1;

    char *new_name = argv[1];
    char *my_id_str = getenv("MY_USER_ID");
    if (!my_id_str) return 1;
    int my_id = atoi(my_id_str);

    int shm_id = shmget(SHM_KEY, sizeof(ChatState), 0666);
    if (shm_id < 0) return 1;
    ChatState *chat_state = (ChatState *)shmat(shm_id, NULL, 0);

    for (int i = 0; i < MAX_USERS; i++) {
        if (chat_state->users[i].is_active && strcmp(chat_state->users[i].name, new_name) == 0) {
            printf("User %s already exists !\n", new_name);
            return 0; 
        }
    }

    for (int i = 0; i < MAX_USERS; i++) {
        if (chat_state->users[i].is_active && chat_state->users[i].id == my_id) {
            strncpy(chat_state->users[i].name, new_name, MAX_NAME_LEN - 1);
            chat_state->users[i].name[MAX_NAME_LEN - 1] = '\0'; 
            printf("name change accept!\n");
            break;
        }
    }
    return 0;
}
