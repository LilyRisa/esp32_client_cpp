#pragma once

enum ConnState {
  IDLE,
  CONNECTING,
  CONNECTED,
  FAILED
};

// Khai báo biến toàn cục (extern)
extern ConnState connState;
extern unsigned long connectStart;
extern const unsigned long CONNECT_MAX_MS;
extern int connectProgress;