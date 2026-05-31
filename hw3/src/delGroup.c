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
        printf("Usage: delGroup <group_name>\n");
        return 1;
    }

    char *my_username = getenv("MY_USERNAME");
    if (my_username == NULL) return 1;

    char *group_name = argv[1];

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL || mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) return 1;

    char query[1024];

    // 1. 檢查群組存不存在，順便把 owner_name 抓出來
    sprintf(query, "SELECT owner_name FROM ChatGroups WHERE group_name='%s'", group_name);
    mysql_query(conn, query);
    MYSQL_RES *res = mysql_store_result(conn);
    
    if (res == NULL || mysql_num_rows(res) == 0) {
        printf("Group not found !\n");
        if (res) mysql_free_result(res);
        mysql_close(conn);
        return 0;
    }

    // 2. 檢查權限：你是不是 Owner？
    MYSQL_ROW row = mysql_fetch_row(res);
    if (strcmp(row[0], my_username) != 0) {
        // 雖然規格書沒有明定這句錯誤訊息，但依照群組權限精神，我們稍微擋一下
        printf("Only the owner can delete group !!\n"); 
        mysql_free_result(res);
        mysql_close(conn);
        return 0;
    }
    mysql_free_result(res);

    // 3. 執行刪除 (因為設定了 CASCADE，GroupMembers 裡面的成員也會被自動清空！)
    sprintf(query, "DELETE FROM ChatGroups WHERE group_name='%s'", group_name);
    if (mysql_query(conn, query) == 0) {
        printf("Group delete success !\n");
    }

    mysql_close(conn);
    return 0;
}
