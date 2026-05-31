#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#define DB_HOST "localhost"
#define DB_USER "hw3_user"
#define DB_PASS "kenny179910"
#define DB_NAME "hw3_db"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: mailto <user_name> [message]\n");
        return 1;
    }

    char *sender_name = getenv("MY_USERNAME");
    if (sender_name == NULL) return 1;

    char *receiver_name = argv[1];

    // 處理訊息來源：來自後面的參數，還是來自 STDIN (管線/重新導向)
    char message[4096] = "";
    if (argc > 2) {
        // 情況 A：使用者直接打 `mailto Alice hello world`
        for (int i = 2; i < argc; i++) {
            strcat(message, argv[i]);
            if (i < argc - 1) strcat(message, " ");
        }
    } else {
        // 情況 B：使用者利用管線或重新導向 `ls | mailto Alice` 或 `mailto Alice < file`
        char buf[1024];
        while (fgets(buf, sizeof(buf), stdin) != NULL) {
            if (strlen(message) + strlen(buf) < sizeof(message)) {
                strcat(message, buf);
            }
        }
    }

    if (strlen(message) == 0) {
        return 0; // 沒內容就不寄信
    }

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL || mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
        fprintf(stderr, "Database connection failed.\n");
        if (conn) mysql_close(conn);
        return 1;
    }

    char query[10000];
    
    // 1. 檢查收件人是否存在於 Users 表
    sprintf(query, "SELECT username FROM Users WHERE username='%s'", receiver_name);
    mysql_query(conn, query);
    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL || mysql_num_rows(res) == 0) {
        printf("User not found !\n");
        if (res) mysql_free_result(res);
        mysql_close(conn);
        return 0;
    }
    mysql_free_result(res);

    // 2. 為了安全，將訊息進行 SQL 跳脫 (防止單引號破壞 SQL 語法)
    char escaped_msg[8192];
    mysql_real_escape_string(conn, escaped_msg, message, strlen(message));

    // 3. 寫入 Mails 表
    snprintf(query, sizeof(query), "INSERT INTO Mails (receiver_name, sender_name, message) VALUES ('%s', '%s', '%s')",
         receiver_name, sender_name, escaped_msg);
    
    if (mysql_query(conn, query)) {
        fprintf(stderr, "Failed to send mail: %s\n", mysql_error(conn));
    }

    mysql_close(conn);
    return 0;
}
