import sys
import socket
import threading
import time
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QTextEdit, QLineEdit, QPushButton, QLabel, QListWidget
from PyQt5.QtCore import pyqtSignal, QObject

# Klasa obslugujaca komunikacje z serwerem
class ChatClient(QObject):
    message_received = pyqtSignal(str)  # Sygnal emitowany przy otrzymaniu wiadomosci
    user_list_updated = pyqtSignal(str)  # Sygnal emitowany przy aktualizacji listy uzytkownikow

    def __init__(self, host='127.0.0.1', port=1100):
        super().__init__()
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client_socket.connect((host, port))
        self.running = True
        self.current_user = None
        threading.Thread(target=self.receive_messages, daemon=True).start()

    # Logowanie / rejestracja uzytkownika
    def authenticate(self, action, username, password):
        print(f"Sending action: {action}")
        self.send_message(action) 
        time.sleep(0.1)

        print(f"Sending username: {username}")
        self.send_message(username)
        time.sleep(0.1)

        print(f"Sending password: {password}")
        self.send_message(password)

        response = self.client_socket.recv(1024).decode('utf-8')
        if "Authentication successful" in response:
            self.current_user = username
            return True
        else:
            print("Authentication failed. Please try again.")
            self.message_received.emit("Authentication failed. Please try again.")  # Emit signal for failure
            return False

    # Wysylanie wiadomosci do serwera
    def send_message(self, message):
        if message:
            try:
                self.client_socket.sendall(message.encode('utf-8'))
            except BrokenPipeError:
                print("Connection closed by the server. Unable to send message.")
                self.running = False

    # Odbieranie wiadomosci od serwera
    def receive_messages(self):
        while self.running:
            try:
                self.client_socket.settimeout(5)  # Unikanie blokowania
                message = self.client_socket.recv(1024).decode('utf-8')
                if not message:
                    print("Server closed the connection.")
                    self.running = False
                    break
                if message.startswith("Users online:"):
                    self.user_list_updated.emit(message)
                else:
                    print(message)
                    self.message_received.emit(message)
            except socket.timeout:
                continue
            except Exception as e:
                print(f"Error receiving message: {e}")
                self.running = False
                break

    def close(self):
        self.running = False
        self.client_socket.close()

# Klasa okna logowania / rejestracji
class LoginRegisterWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.init_ui()
        self.client = ChatClient()
        self.client.message_received.connect(self.display_message)

    def init_ui(self):
        self.setWindowTitle("Login/Register")
        self.setGeometry(100, 100, 300, 200)

        layout = QVBoxLayout()

        self.username_label = QLabel("Username:")
        layout.addWidget(self.username_label)

        self.username_input = QLineEdit()
        layout.addWidget(self.username_input)

        self.password_label = QLabel("Password:")
        layout.addWidget(self.password_label)

        self.password_input = QLineEdit()
        self.password_input.setEchoMode(QLineEdit.Password)
        layout.addWidget(self.password_input)

        self.login_button = QPushButton("Login")
        self.login_button.clicked.connect(self.login)
        layout.addWidget(self.login_button)

        self.register_button = QPushButton("Register")
        self.register_button.clicked.connect(self.register)
        layout.addWidget(self.register_button)

        self.message_display = QTextEdit()
        self.message_display.setReadOnly(True)
        layout.addWidget(self.message_display)

        self.setLayout(layout)

    def login(self):
        username = self.username_input.text()
        password = self.password_input.text()
        if self.client.authenticate("login", username, password):
            self.open_user_list_window()

    def register(self):
        username = self.username_input.text()
        password = self.password_input.text()
        if self.client.authenticate("register", username, password):
            self.open_user_list_window()

    def display_message(self, message):
        self.message_display.append(message)

        if "successful" in message:
            self.open_user_list_window()

    def open_user_list_window(self):
        self.user_list_window = UserListWindow(self.client, self.client.current_user)
        self.client.user_list_updated.connect(self.user_list_window.update_user_list)
        self.user_list_window.show()
        self.close()

# Klasa okna listy uzytkownikow
class UserListWindow(QWidget):
    def __init__(self, client, current_user):
        super().__init__()
        self.client = client
        self.current_user = current_user
        self.chat_windows = {}
        self.init_ui()
        self.client.user_list_updated.connect(self.update_user_list)

    def init_ui(self):
        self.setWindowTitle("Users Online")
        self.setGeometry(100, 100, 300, 300)

        self.layout = QVBoxLayout()

        self.logged_in_label = QLabel(f"Logged in as: {self.current_user}")
        self.layout.addWidget(self.logged_in_label)

        self.user_list_display = QListWidget()
        self.user_list_display.setSelectionMode(QListWidget.MultiSelection)
        self.layout.addWidget(QLabel("Users Online:"))
        self.layout.addWidget(self.user_list_display)

        self.start_chat_button = QPushButton("Start Chat")
        self.start_chat_button.clicked.connect(self.start_chat)
        self.layout.addWidget(self.start_chat_button)

        self.back_button = QPushButton("Logout")
        self.back_button.clicked.connect(self.back_to_login)
        self.layout.addWidget(self.back_button)

        self.setLayout(self.layout)

    def start_chat(self):
        selected_users = [item.text() for item in self.user_list_display.selectedItems()]
        if selected_users:
            chat_key = tuple(sorted([self.current_user] + selected_users))
            if chat_key not in self.chat_windows:
                chat_window = ChatWindow(self.client, selected_users, self.current_user)
                chat_window.chat_closed.connect(self.remove_chat_window)
                self.chat_windows[chat_key] = chat_window
                chat_window.show()  # Signal connection is now handled in ChatWindow
            else:
                self.chat_windows[chat_key].activateWindow()

    def update_user_list(self, user_list):
        users = user_list.replace("Users online: ", "").split(", ")
        users = [user.strip() for user in users]

        self.user_list_display.clear()

        # Filtowanie listy uzytkownikow (bez obecnego klienta)
        filtered_users = [user for user in users if user != self.current_user]
        self.user_list_display.addItems(filtered_users)

    def back_to_login(self):
        self.client.send_message("/logout")
        for chat_window in self.chat_windows.values():
            chat_window.close()
        self.client.close()

        self.chat_windows.clear()
        self.login_window = LoginRegisterWindow()
        self.login_window.show()
        self.close()

    def display_message(self, message):
        pass

    def remove_chat_window(self, chat_key):
        if chat_key in self.chat_windows:
            del self.chat_windows[chat_key]

# Klasa okna czatu
class ChatWindow(QWidget):
    chat_closed = pyqtSignal(tuple)

    def __init__(self, client, selected_users, current_user):
        super().__init__()
        self.client = client
        self.selected_users = selected_users
        self.current_user = current_user
        self.chat_key = self.generate_chat_key(selected_users)
        self.init_ui()

        # Connect the signal here, inside the ChatWindow class
        self.client.message_received.connect(self.display_message)

    def generate_chat_key(self, selected_users):
        participants = sorted([self.current_user] + selected_users)
        return tuple(participants)

    def init_ui(self):
        self.setWindowTitle(f"Chat with {', '.join(self.selected_users)}")
        self.setGeometry(100, 100, 400, 300)

        self.layout = QVBoxLayout()

        self.text_display = QTextEdit()
        self.text_display.setReadOnly(True)
        self.layout.addWidget(self.text_display)

        self.entry_message = QLineEdit()
        self.layout.addWidget(QLabel("Message:"))
        self.layout.addWidget(self.entry_message)

        self.send_button = QPushButton("Send")
        self.send_button.clicked.connect(self.send_message)
        self.layout.addWidget(self.send_button)

        self.setLayout(self.layout)

    def send_message(self):
        message = self.entry_message.text()
        recipients = " ".join(f"@{user}" for user in self.selected_users)
        self.client.send_message(f"/msg {recipients} {message}")
        self.text_display.append(f"You: {message}")
        self.entry_message.clear()

    def is_message_for_this_chat(self, message):
        if ":" in message:
            sender, content = message.split(":", 1)
            sender = sender.strip()

            # Participants include the sender, current user, and selected users
            participants = sorted(list(set([sender, self.current_user] + self.selected_users)))
            print(f"Participants for this chat: {participants}")  # Debug print
            return tuple(participants) == self.chat_key

        return False

    def display_message(self, message):
        if self.is_message_for_this_chat(message):
            print(f"Received message in ChatWindow: {message}")  # Debug print
            self.text_display.append(message)

    def closeEvent(self, event):
        self.chat_closed.emit(self.chat_key)
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)

    login_register_window = LoginRegisterWindow()
    login_register_window.show()
    sys.exit(app.exec_())
