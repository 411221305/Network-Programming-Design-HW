#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#define DB_HOST "localhost"
#define DB_USER "hw3_user"
#define DB_PASS "kenny179910"
#define DB_NAME "hw3_db"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: delMail <mail_id>\n");
        return 1;
    }

    char *my_username = getenv("MY_USERNAME");
    if (my_username == NULL) return 1;

    int mail_id = atoi(argv[1]);

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL || mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
        return 1;
    }

    char query[1024];
    
    // 1. 防呆：檢查這封信存不存在，而且是不是寄給「自己」的
    sprintf(query, "SELECT id FROM Mails WHERE id=%d AND receiver_name='%s'", mail_id, my_username);
    mysql_query(conn, query);
    MYSQL_RES *res = mysql_store_result(conn);
    
    if (res == NULL || mysql_num_rows(res) == 0) {
        printf("Mail id unexist !\n");
        if (res) mysql_free_result(res);
        mysql_close(conn);
        return 0;
    }
    mysql_free_result(res);

    // 2. 確認無誤，執行刪除
    sprintf(query, "DELETE FROM Mails WHERE id=%d", mail_id);
    mysql_query(conn, query);

    mysql_close(conn);
    return 0;
}
