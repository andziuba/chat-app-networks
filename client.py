import sys
import socket
import threading
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QTextEdit, QLineEdit, QPushButton, QLabel
from PyQt5.QtCore import pyqtSignal, QObject

class ChatClient(QObject):
    message_received = pyqtSignal(str)

    def __init__(self, host='127.0.0.1', port=1100):
        super().__init__()
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client_socket.connect((host, port))
        self.running = True
        threading.Thread(target=self.receive_messages, daemon=True).start()

    def authenticate(self, action, username, password):
        print(f"Sending action: {action}")
        self.send_message(action)  # Wysyłamy akcję (login/register)
        print(f"Sending username: {username}")
        self.send_message(username)  # Wysyłamy nazwę użytkownika
        print(f"Sending password: {password}")
        self.send_message(password)  # Wysyłamy hasło

        # Odbieranie odpowiedzi od serwera
        response = self.client_socket.recv(1024).decode('utf-8')
        if "Authentication failed" in response:
            print("Authentication failed. Please try again.")
            self.message_received.emit("Authentication failed. Please try again.")  # Emituj komunikat do GUI
        else:
            self.message_received.emit("Authentication successful!")  # Informuj o sukcesie

    def send_message(self, message):
        if message:
            try:
                self.client_socket.sendall(message.encode('utf-8'))
            except BrokenPipeError:
                print("Connection closed by the server. Unable to send message.")
                self.running = False  # Zatrzymaj odbieranie wiadomości

    def receive_messages(self):
        while self.running:
            try:
                message = self.client_socket.recv(1024).decode('utf-8')
                if message:
                    print(f"Message received: {message}")  # Debug message
                    self.message_received.emit(f"{message}")
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

        # Check for successful login or registration and open ChatWindow
        if "successful" in message:  # Assuming server sends "Login successful" or "Registration successful"
            self.open_chat_window()

    def open_chat_window(self):
        self.chat_window = ChatWindow()
        self.chat_window.client = self.client  # Pass the same client instance
        self.client.message_received.connect(self.chat_window.display_message)
        self.chat_window.show()  # Show the chat window
        self.close()  # Close the login/register window

class ChatWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.init_ui()

    def init_ui(self):
        self.setWindowTitle("Chat Client")
        self.setGeometry(100, 100, 400, 300)

        self.layout = QVBoxLayout()
        
        # Wyświetlacz wiadomości
        self.text_display = QTextEdit()
        self.text_display.setReadOnly(True)
        self.layout.addWidget(self.text_display)

        # Pole do wprowadzania nazwy użytkownika odbiorcy
        self.recipient_input = QLineEdit()
        self.layout.addWidget(QLabel("Recipient:"))  # Etykieta dla pola odbiorcy
        self.layout.addWidget(self.recipient_input)  # Dodanie pola do wprowadzenia odbiorcy

        # Pole do wprowadzania wiadomości
        self.entry_message = QLineEdit()
        self.layout.addWidget(QLabel("Message:"))
        self.layout.addWidget(self.entry_message)

        # Przycisk do wysyłania wiadomości prywatnej
        self.private_send_button = QPushButton("Send Private Message")
        self.private_send_button.clicked.connect(self.send_private_message)  # Po kliknięciu wywołuje send_private_message
        self.layout.addWidget(self.private_send_button)
        
        # Przycisk do wysyłania wiadomości
        self.send_button = QPushButton("Send")
        self.send_button.clicked.connect(self.send_message)  # Po kliknięciu wywołuje send_message
        self.layout.addWidget(self.send_button)

        

        self.setLayout(self.layout)

    # Nowa metoda do wysyłania wiadomości prywatnych
    def send_private_message(self):
        recipient = self.recipient_input.text()  # Odczytaj nazwę użytkownika odbiorcy
        message = self.entry_message.text()  # Odczytaj treść wiadomości
        self.client.send_message(f"/msg {recipient} {message}")  # Wyślij wiadomość w formacie "/msg"
        self.text_display.append(f"You (to {recipient}): {message}")  # Dodaj do wyświetlacza wiadomości
        self.entry_message.clear()  # Wyczyść pole wiadomości


    def send_message(self):
        message = self.entry_message.text()
        self.client.send_message(message)
        self.text_display.append(f"You: {message}")
        self.entry_message.clear()



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
