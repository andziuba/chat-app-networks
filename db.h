#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_db();

int register_user(const char *username, const char *password);

int login_user(const char *username, const char *password);

void close_db();

#endif
