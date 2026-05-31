#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <mysql/mysql.h>
#include "chat.h" 

#define DB_HOST "localhost"
#define DB_USER "hw3_user"
#define DB_PASS "kenny179910"
#define DB_NAME "hw3_db"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: gyell <group_name> <message>\n");
        return 1;
    }

    char *my_username = getenv("MY_USERNAME");
    if (my_username == NULL) return 1;

    char *group_name = argv[1];

    // 把後面的字串接起來變成一句完整的 message
    char message[4096] = "";
    for (int i = 2; i < argc; i++) {
        strcat(message, argv[i]);
        if (i < argc - 1) strcat(message, " ");
    }

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL || mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) return 1;

    char query[1024];

    // 1. 檢查群組是否存在
    sprintf(query, "SELECT group_name FROM ChatGroups WHERE group_name='%s'", group_name);
    mysql_query(conn, query);
    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL || mysql_num_rows(res) == 0) {
        printf("Group not found !\n");
        if (res) mysql_free_result(res);
        mysql_close(conn);
        return 0;
    }
    mysql_free_result(res);

    // 2. 抓出這個群組的「所有成員名單」
    sprintf(query, "SELECT username FROM GroupMembers WHERE group_name='%s'", group_name);
    mysql_query(conn, query);
    res = mysql_store_result(conn);
    
    char members[100][MAX_NAME_LEN];
    int member_count = 0;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        strcpy(members[member_count++], row[0]);
    }
    mysql_free_result(res);
    mysql_close(conn);

    // 3. 連接公佈欄 (Shared Memory)
    int shm_id = shmget(SHM_KEY, sizeof(ChatState), 0666);
    if (shm_id < 0) {
        perror("shmget failed");
        return 1;
    }
    ChatState *chat_state = (ChatState *)shmat(shm_id, NULL, 0);

    // 4. 依照規格書格式組合字串: <group_name:user_name>: <message>
    char formatted_msg[5000];
    sprintf(formatted_msg, "<%s:%s>: %s\n", group_name, my_username, message);

    // 5. 掃描線上所有使用者，只要在名單內，就發送 Signal
    for (int i = 0; i < MAX_USERS; i++) {
        if (chat_state->users[i].is_active) {
            // 不要發給自己
            if (strcmp(chat_state->users[i].name, my_username) == 0) continue;

            // 檢查這個線上使用者是不是群組成員
            int is_member = 0;
            for (int j = 0; j < member_count; j++) {
                if (strcmp(chat_state->users[i].name, members[j]) == 0) {
                    is_member = 1;
                    break;
                }
            }

            // 如果是成員，就把訊息塞進他的信箱，並按門鈴 (SIGUSR1)
            if (is_member) {
                strcpy(chat_state->users[i].msg_buffer, formatted_msg);
                kill(chat_state->users[i].pid, SIGUSR1);
            }
        }
    }

    shmdt(chat_state);
    return 0;
}
