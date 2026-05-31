// hw2/src/who.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "../include/chat.h"

int main() {
    // 1. 取得自己的 ID (從 Shell 剛剛設定的環境變數拿)
    char *my_id_str = getenv("MY_USER_ID");
    if (my_id_str == NULL) {
        fprintf(stderr, "Error: MY_USER_ID environment variable not found.\n");
        return 1;
    }
    int my_id = atoi(my_id_str);

    // 2. 取得共享記憶體 (公佈欄)
    int shm_id = shmget(SHM_KEY, sizeof(ChatState), 0666);
    if (shm_id < 0) {
        perror("shmget failed");
        return 1;
    }
    ChatState *chat_state = (ChatState *)shmat(shm_id, NULL, 0);

    // 3. 依照規格書的格式印出 Header
    printf("<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");

    // 4. 掃描公佈欄，印出所有上線的 User
    for (int i = 0; i < MAX_USERS; i++) {
        if (chat_state->users[i].is_active) {
            printf("%d\t%s\t%s", 
                   chat_state->users[i].id, 
                   chat_state->users[i].name, 
                   chat_state->users[i].ip_port);
            
            // 如果這個 User 是自己，就多印 <-me
            if (chat_state->users[i].id == my_id) {
                printf("\t<-me");
            }
            printf("\n");
        }
    }

    return 0;
}
