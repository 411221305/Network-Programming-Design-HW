#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#define DB_HOST "localhost"
#define DB_USER "hw3_user"      // 使用我們新建的帳號
#define DB_PASS "kenny179910" 
#define DB_NAME "hw3_db"

int main() {
    // 1. 從環境變數取得當前登入的使用者名稱
    char *my_username = getenv("MY_USERNAME");
    if (my_username == NULL) {
        fprintf(stderr, "Error: Cannot identify user. MY_USERNAME not set.\n");
        return 1;
    }

    // 2. 連線資料庫
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return 1;
    }
    
    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
        fprintf(stderr, "Database connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    // 3. 查詢寄給自己的信件
    char query[1024];
    sprintf(query, "SELECT id, sender_name, timestamp, message FROM Mails WHERE receiver_name='%s'", my_username);
    
    if (mysql_query(conn, query)) {
        fprintf(stderr, "Query failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Failed to store result.\n");
        mysql_close(conn);
        return 1;
    }

    int num_rows = mysql_num_rows(res);

    // 4. 依照規格書格式印出結果
    if (num_rows == 0) {
        printf("empty !\n");
    } else {
        printf("%-5s %-15s %-20s %s\n", "id", "date", "sender", "message");
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            // row[0]=id, row[1]=sender, row[2]=timestamp, row[3]=message
            
            // 處理時間格式，只取前 10 個字元 (例如 2024-05-31) 以符合規格書
            char short_date[15];
            snprintf(short_date, 11, "%s", row[2]); 

            printf("%-5s %-15s %-20s %s\n", row[0], short_date, row[1], row[3]);
        }
    }

    // 5. 清理資源
    mysql_free_result(res);
    mysql_close(conn);
    return 0;
}
