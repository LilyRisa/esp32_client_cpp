#include "state_manager.h"

// Khởi tạo biến toàn cục ở đây
ConnState connState = IDLE;
unsigned long connectStart = 0;
const unsigned long CONNECT_MAX_MS = 15000;
int connectProgress = 0;