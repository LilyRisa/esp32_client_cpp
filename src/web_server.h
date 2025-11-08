#pragma once
#include <WebServer.h>

extern WebServer server;

int getWifiBars();
void initWebServer();
void handleWebServer();