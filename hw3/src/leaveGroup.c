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
        printf("Usage: leaveGroup <group_name>\n");
        return 1;
    }

    char *my_username = getenv("MY_USERNAME");
    if (my_username == NULL) return 1;

    char *group_name = argv[1];

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL || mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) return 1;

    char query[1024];

    // 1. 檢查群組存不存在
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

    // 2. 檢查你是不是成員
    sprintf(query, "SELECT id FROM GroupMembers WHERE group_name='%s' AND username='%s'", group_name, my_username);
    mysql_query(conn, query);
    res = mysql_store_result(conn);
    if (res == NULL || mysql_num_rows(res) == 0) {
        printf("Leave fault !\n");
        if (res) mysql_free_result(res);
        mysql_close(conn);
        return 0;
    }
    mysql_free_result(res);

    // 3. 刪除你的成員紀錄
    sprintf(query, "DELETE FROM GroupMembers WHERE group_name='%s' AND username='%s'", group_name, my_username);
    mysql_query(conn, query);
    // 依據規格書截圖，退出成功似乎不會特別印出文字，直接跳回 prompt 即可
    
    mysql_close(conn);
    return 0;
}
