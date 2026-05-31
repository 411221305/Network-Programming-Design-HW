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
        printf("Usage: createGroup <group_name>\n");
        return 1;
    }

    char *my_username = getenv("MY_USERNAME");
    if (my_username == NULL) return 1;

    char *group_name = argv[1];

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL || mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) return 1;

    char query[1024];

    // 1. 檢查群組是否已存在
    sprintf(query, "SELECT group_name FROM ChatGroups WHERE group_name='%s'", group_name);
    mysql_query(conn, query);
    MYSQL_RES *res = mysql_store_result(conn);
    if (res && mysql_num_rows(res) > 0) {
        printf("Group already exist !\n");
        mysql_free_result(res);
        mysql_close(conn);
        return 0;
    }
    if (res) mysql_free_result(res);

    // 2. 建立群組，並將自己設為 owner
    sprintf(query, "INSERT INTO ChatGroups (group_name, owner_name) VALUES ('%s', '%s')", group_name, my_username);
    mysql_query(conn, query);

    // 3. 將自己加入群組成員名單
    sprintf(query, "INSERT INTO GroupMembers (group_name, username) VALUES ('%s', '%s')", group_name, my_username);
    mysql_query(conn, query);

    printf("Create success !\n");
    mysql_close(conn);
    return 0;
}
