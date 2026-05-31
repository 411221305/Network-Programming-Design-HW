#ifndef CHAT_H
#define CHAT_H

#include <sys/types.h>

#define MAX_USERS 30       // 假設最多 30 人同時連線
#define MAX_NAME_LEN 20    // 名字最大長度
#define SHM_KEY 12345      // 共享記憶體的鑰匙 (Key)，讓外部程式能找到同一塊記憶體

// 單一使用者的狀態結構
typedef struct {
    int id;                  // 使用者 ID (1~30)
    int is_active;           // 1 代表連線中，0 代表空線 (未使用)
    pid_t pid;               // 該連線對應的子行程 PID
    char name[MAX_NAME_LEN]; // 使用者名稱，預設為 "(no name)"
    char ip_port[50];        // 記錄連線來源的 IP 和 Port
    char msg_buffer[1024];   // 每個人的專屬信箱
} User;

// 整個共享公佈欄的結構
typedef struct {
    User users[MAX_USERS];
} ChatState;

#endif
