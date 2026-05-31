#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#define DB_HOST "localhost"
#define DB_USER "hw3_user"
#define DB_PASS "kenny179910"
#define DB_NAME "hw3_db"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: addTo <group_name> <user_name1> <user_name2> ...\n");
        return 1;
    }

    char *my_username = getenv("MY_USERNAME");
    if (my_username == NULL) return 1;

    char *group_name = argv[1];

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL || mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) return 1;

    char query[1024];

    // 1. 檢查群組是否存在，並確認自己是不是 Owner
    sprintf(query, "SELECT owner_name FROM ChatGroups WHERE group_name='%s'", group_name);
    mysql_query(conn, query);
    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL || mysql_num_rows(res) == 0) {
        printf("Group not found !\n");
        if (res) mysql_free_result(res);
        mysql_close(conn);
        return 0;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (strcmp(row[0], my_username) != 0) {
        printf("You don't have permission !\n");
        mysql_free_result(res);
        mysql_close(conn);
        return 0;
    }
    mysql_free_result(res);

    // 2. 用迴圈處理每一個要加入的使用者
    for (int i = 2; i < argc; i++) {
        char *target_user = argv[i];

        // 檢查該使用者是否存在於系統 (Users 表)
        sprintf(query, "SELECT username FROM Users WHERE username='%s'", target_user);
        mysql_query(conn, query);
        res = mysql_store_result(conn);
        if (res == NULL || mysql_num_rows(res) == 0) {
            printf("%s not found !\n", target_user);
            if (res) mysql_free_result(res);
            continue;
        }
        mysql_free_result(res);

        // 檢查該使用者是否「已經」在群組裡了
        sprintf(query, "SELECT id FROM GroupMembers WHERE group_name='%s' AND username='%s'", group_name, target_user);
        mysql_query(conn, query);
        res = mysql_store_result(conn);
        if (res && mysql_num_rows(res) > 0) {
            printf("%s already in group !\n", target_user);
            mysql_free_result(res);
            continue;
        }
        if (res) mysql_free_result(res);

        // 確認無誤，正式加入群組
        sprintf(query, "INSERT INTO GroupMembers (group_name, username) VALUES ('%s', '%s')", group_name, target_user);
        if (mysql_query(conn, query) == 0) {
            printf("%s add success !\n", target_user);
        }
    }

    mysql_close(conn);
    return 0;
}
