#include "db.h"

sqlite3 *db;

void init_db() {
    int rc = sqlite3_open("chat_server.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    const char *sql_users = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT NOT NULL UNIQUE, "
        "password TEXT NOT NULL);";

    sqlite3_exec(db, sql_users, NULL, NULL, NULL);
}

int register_user(const char *username, const char *password) {
    sqlite3_stmt *stmt;
    
    // Sprawdz czy uzytkownik o podanej nazwie juz istnieje
    const char *sql_check = "SELECT id FROM users WHERE username = ?;";
    int rc = sqlite3_prepare_v2(db, sql_check, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        // Uzytkownik o podanej nazwie juz istnieje
        printf("Error: Username '%s' is already taken.\n", username);
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);

    // Rejestracja uzytkownika
    const char *sql_insert = "INSERT INTO users (username, password) VALUES (?, ?);";
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        printf("User registered successfully.\n");
        return 0;
    } else {
        printf("Error: User registration failed.\n");
        return -1;
    }
}

int login_user(const char *username, const char *password) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id FROM users WHERE username = ? AND password = ?;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        int user_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        printf("Login successful. User: %d\n", user_id);
        return user_id;
    } else {
        printf("Invalid username or password.\n");
        sqlite3_finalize(stmt);
        return -1;
    }
}

void close_db() {
    sqlite3_close(db);
}
