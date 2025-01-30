import sys
import socket
import threading
import time
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QTextEdit, QLineEdit, QPushButton, QLabel, QListWidget
from PyQt5.QtCore import pyqtSignal, QObject

class ChatClient(QObject):
    message_received = pyqtSignal(str)
    user_list_updated = pyqtSignal(str)

    def __init__(self, host='127.0.0.1', port=1100):
        super().__init__()
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client_socket.connect((host, port))
        self.running = True
        self.current_user = None  # Track the currently logged-in user
        threading.Thread(target=self.receive_messages, daemon=True).start()

    def authenticate(self, action, username, password):
        print(f"Sending action: {action}")
        self.send_message(action)  # Send action (login/register)
        time.sleep(0.1)

        print(f"Sending username: {username}")
        self.send_message(username)  # Send username
        time.sleep(0.1)
        
        print(f"Sending password: {password}")
        self.send_message(password)  # Send password

        # Receiving response from server
        response = self.client_socket.recv(1024).decode('utf-8')
        if "Authentication failed" in response:
            print("Authentication failed. Please try again.")
            self.message_received.emit("Authentication failed. Please try again.")  # Emit signal for failure
        else:
            self.current_user = username
            self.message_received.emit("Authentication successful!")  # Inform success
           

    def send_message(self, message):
        if message:
            try:
                self.client_socket.sendall(message.encode('utf-8'))
            except BrokenPipeError:
                print("Connection closed by the server. Unable to send message.")
                self.running = False  # Stop receiving messages if connection is lost

    def receive_messages(self):
        while self.running:
            try:
                message = self.client_socket.recv(1024).decode('utf-8')
                if message.startswith("Users online:"):
                    self.user_list_updated.emit(message)  # Emit updated user list
                else:
                    self.message_received.emit(message)  # Emit regular message
            except Exception as e:
                print(f"Error receiving message: {e}")
                break

    def close(self):
        self.running = False
        self.client_socket.close()

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
        self.password_input.setEchoMode(QLineEdit.Password)  # Mask password input
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
        self.client.authenticate("login", username, password)

    def register(self):
        username = self.username_input.text()
        password = self.password_input.text()
        self.client.authenticate("register", username, password)

    def display_message(self, message):
        self.message_display.append(message)

        if "successful" in message:  # Assuming server sends "Login successful" or "Registration successful"
            self.open_user_list_window()

    def open_user_list_window(self):
        self.user_list_window = UserListWindow(self.client, self.client.current_user)  # Pass the current user
        self.client.message_received.connect(self.user_list_window.display_message)
        self.client.user_list_updated.connect(self.user_list_window.update_user_list)  # Update user list in user list window
        self.user_list_window.show()
        self.close()  # Close the login window

class UserListWindow(QWidget):
    def __init__(self, client, current_user):
        super().__init__()
        self.client = client
        self.current_user = current_user
        self.init_ui()
        self.client.user_list_updated.connect(self.update_user_list)

    def init_ui(self):
        self.setWindowTitle("Users Online")
        self.setGeometry(100, 100, 300, 300)

        self.layout = QVBoxLayout()

        # Display "Logged in as: username"
        self.logged_in_label = QLabel(f"Logged in as: {self.current_user}")
        self.layout.addWidget(self.logged_in_label)

        # User list display (for online users)
        self.user_list_display = QListWidget()
        self.user_list_display.setSelectionMode(QListWidget.MultiSelection)
        self.layout.addWidget(QLabel("Users Online:"))
        self.layout.addWidget(self.user_list_display)

        self.start_chat_button = QPushButton("Start Chat")
        self.start_chat_button.clicked.connect(self.start_chat)
        self.layout.addWidget(self.start_chat_button)

        # Back to the login window button
        self.back_button = QPushButton("Back to Login")
        self.back_button.clicked.connect(self.back_to_login)
        self.layout.addWidget(self.back_button)

        self.setLayout(self.layout)

    def start_chat(self):
        selected_users = [item.text() for item in self.user_list_display.selectedItems()]
        if selected_users:
            self.chat_window = ChatWindow(self.client, selected_users)
            self.chat_window.show()
            self.close()

    def update_user_list(self, user_list):
        """Update the list of online users, excluding the current user."""
        users = user_list.replace("Users online: ", "").split(", ")
        users = [user.strip() for user in users]
        
        self.user_list_display.clear()
        
        # Filter out the current user from the list
        filtered_users = [user for user in users if user != self.current_user]
        self.user_list_display.addItems(filtered_users)

    def on_user_selected(self):
        selected_user = self.user_list_display.currentItem().text()
        self.chat_window = ChatWindow(self.client, selected_user)
        self.chat_window.show()
        self.close()  # Close the user list window

    def back_to_login(self):
        self.client.close()
        self.login_window = LoginRegisterWindow()
        self.login_window.show()
        self.close()  # Close the user list window

    def display_message(self, message):
        """Display general messages (like login success or errors)."""
        pass  # This is handled in other windows

class ChatWindow(QWidget):
    def __init__(self, client, selected_users):
        super().__init__()
        self.client = client
        self.selected_users = selected_users  # Use 'selected_users' instead of 'selected_user'
        self.init_ui()

        self.client.message_received.connect(self.display_message)

    def init_ui(self):
        self.setWindowTitle(f"Chat with {', '.join(self.selected_users)}")  # Using 'self.selected_users'
        self.setGeometry(100, 100, 400, 300)

        self.layout = QVBoxLayout()

        # Message display
        self.text_display = QTextEdit()
        self.text_display.setReadOnly(True)
        self.layout.addWidget(self.text_display)

        # Message input field
        self.entry_message = QLineEdit()
        self.layout.addWidget(QLabel("Message:"))
        self.layout.addWidget(self.entry_message)

        # Send button
        self.send_button = QPushButton("Send")
        self.send_button.clicked.connect(self.send_message)
        self.layout.addWidget(self.send_button)

        # Back button to return to the user list screen
        self.back_button = QPushButton("Back to Users List")
        self.back_button.clicked.connect(self.back_to_user_list)
        self.layout.addWidget(self.back_button)

        self.setLayout(self.layout)

    def send_message(self):
        message = self.entry_message.text()
        recipients = " ".join(self.selected_users)  # Use 'self.selected_users'
        self.client.send_message(f"/msg {recipients} {message}")
        self.text_display.append(f"You: {message}")
        self.entry_message.clear()

    def back_to_user_list(self):
        self.client.send_message("/list")
        time.sleep(0.1)
        self.user_list_window = UserListWindow(self.client, self.client.current_user)
        self.client.user_list_updated.connect(self.user_list_window.update_user_list)
        self.user_list_window.show()
        self.close()

    def display_message(self, message):
        self.text_display.append(message)

    def closeEvent(self, event):
        self.client.close()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)

    login_register_window = LoginRegisterWindow()
    login_register_window.show()
    sys.exit(app.exec_())
