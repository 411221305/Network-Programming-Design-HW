#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#define DB_HOST "localhost"
#define DB_USER "hw3_user"
#define DB_PASS "kenny179910"
#define DB_NAME "hw3_db"

int main() {
    char *my_username = getenv("MY_USERNAME");
    if (my_username == NULL) return 1;

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL || mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) return 1;

    char query[1024];
    sprintf(query, "SELECT group_name FROM GroupMembers WHERE username='%s'", my_username);

    if (mysql_query(conn, query)) {
        mysql_close(conn);
        return 1;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    int num_rows = mysql_num_rows(res);

    if (num_rows == 0) {
        printf("Empty !\n");
    } else {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            printf("%s\n", row[0]);
        }
    }

    mysql_free_result(res);
    mysql_close(conn);
    return 0;
}
