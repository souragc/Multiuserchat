from tkinter import *
import tkinter as tk
from tkinter import messagebox
import socket
import hashlib
import time
import threading
# Main screen
global main_screen
# Login/Register screen
global subscreen

# socket
global s

# Used since server works in a specific way
global first
global userFailed
# Used in messages
global usernameFinal

# Active users list
global activeUsers
activeUsers = []

def do_stuff(opt="default",user="",pas=""):
    global subscreen
    global s
    global first
    global userFailed
    global usernameFinal
    if(len(pas)<8):
        messagebox.showerror("Error","Password should be of length > 8")
        return
    else:
        if(opt=="Login"):
            # do login stuff
            # Only need to send option once even if other checks fail.
            if(first==0):
                s.sendall(b"1")
                first = 1
            # Once username is passed, we don't need to send it again if password fails.
            if(userFailed==0):
                s.sendall(bytes(user,'ascii'))
                data = s.recv(50)
                print(data)
                if(data==b"Found"):
                    userFailed = 1
                else:
                    messagebox.showerror("Not Found","A user with that username was not found in our database\nPlease re-enter")
                    return
            # Check for password
            hash_object = hashlib.md5(pas.encode())
            md5_hash = hash_object.hexdigest()
            s.sendall(bytes(md5_hash,'ascii'))
            data = s.recv(7)
            print(data)
            if(data==b"Success"):
                # If username and password correct, close login page and start chatscreen
                usernameFinal = user
                messagebox.showinfo("Success","Succefully Logged in.")
                subscreen.destroy()
                openChat()
                ## Start chat
                pass
            else:
                messagebox.showerror("Failed","Username and Password doesn't match.")
                return

        else:
            # do register stuff
            # Only need to send option once
            if(first==0):
                s.sendall(b"2")
                first = 1
            s.sendall(bytes(user,'ascii'))
            data = s.recv(50)
            # If username already doesn't exist, take password.
            if(data==b"Not Found"):
                # Start chat after sending password.
                # Registration
                hash_object = hashlib.md5(pas.encode())
                md5_hash = hash_object.hexdigest()
                s.sendall(bytes(md5_hash,'ascii'))
                usernameFinal = user
                messagebox.showinfo("Success","Succefully registered. You will be automatically logged in.")
                subscreen.destroy()
                openChat()
                # Start chat
            else:
                messagebox.showerror("Already exists","Username already exists. Please choose a different one.")
                return
def send(chat, T):
    mess = chat.get()+"\n"
    global s
    mess = usernameFinal+":"+mess
    s.sendall(bytes(mess,'ascii'))
    T.configure(state="normal")
    T.insert(tk.END,mess)
    T.configure(state="disabled")
    chat.set("")
    pass

def logout(chatWindow):
    global s
    s.send(b"quit")
    chatWindow.destroy()
    init()

def quit(chatWindow):
    global s
    s.send(b"quit")
    chatWindow.destroy()
    exit(0)

def active():
    activeWindow = Tk()
    activeWindow.title("Active Users")
    activeWindow.geometry("750x550")
    activeWindow.wm_attributes('-type', 'splash')
    T = Text(activeWindow, height = 15, width = 80)
    T.pack()
    T.configure(yscrollcommand=True)
    T.configure(bd="1")
    for i in activeUsers:
        T.insert(tk.END,(i+"\n"))
    T.configure(state='disabled')
    Button(activeWindow,text="Close", height="2", command=lambda: activeWindow.destroy(), width="77").pack()

def waitForMessage(T):
    global s
    global activeUsers
    data = s.recv(200)
    if(len(data)>0):
        data = data.decode('ascii')
        data = data + "\n"
        if(data[0]=='\xfd'):
            activeUsers.append(data[1:-1])
            data = data[1:-1] + " joined the chat\n"
        elif(data[0]=='\xfc'):
            name = data[1:-1]
            activeUsers.remove(name)
            data = data[1:-1] + " left the chat \n"
            pass
        print(data)
        T.configure(state="normal")
        T.insert(tk.END,data)
        T.configure(state="disabled")


def openChat():
    global s
    chatWindow = Tk()
    chatWindow.title("MultiChat")
    chatWindow.geometry("700x500")
    chatWindow.wm_attributes('-type', 'splash')
    T = Text(chatWindow, height = 15, width = 80)
    T.pack()
    T.configure(yscrollcommand=True)
    T.configure(state='disabled')
    T.configure(bd="1")
    chat = StringVar()
    chat_entry = Entry(chatWindow, textvariable=chat, width="80")
    chat_entry.pack()
    Button(chatWindow,text="Send", height="2", command=lambda: send(chat, T), width="77").pack()
    Button(chatWindow,text="Logout", height="2", command=lambda: logout(chatWindow), width="77").pack()
    Button(chatWindow,text="Quit", height="2", command=lambda: quit(chatWindow), width="77").pack()
    Button(chatWindow,text="Active Users", height="2", command=lambda: active(), width="77").pack()

    # Using thread to recevie message since process is not working
    t = threading.Thread(target = waitForMessage, args=(T,))
    # Should run in the background
    t.setDaemon(True)
    t.start()

    chatWindow.mainloop()


# Same function for login and register
# Can be later split to check specific conditions
# Since it looks the same, this avoids code repetation
def registerorlogin(opt="default"):
    global subscreen
    global main_screen
    main_screen.destroy()
    subscreen = Tk()
    subscreen.title(opt)
    subscreen.geometry("600x250")
    subscreen.wm_attributes('-type', 'splash')

    username = StringVar()
    password = StringVar()

    Label(subscreen, text="Enter your details", bg="white").pack()
    Label(subscreen, text="").pack()

    username_lable = Label(subscreen, text="Username * ")
    username_lable.pack()

    username_entry = Entry(subscreen, textvariable=username)
    username_entry.pack()

    password_lable = Label(subscreen, text="Password * ")
    password_lable.pack()

    password_entry = Entry(subscreen, textvariable=password, show='*')
    password_entry.pack()
    Label(subscreen, text="").pack()

    # Call a function that checks for conditions.
    Button(subscreen, text=opt, width=10, height=1, command =lambda: do_stuff(opt,username.get(),password.get()), bg="white").pack()

# Starting screen
def mainScreen():
    global main_screen
    main_screen = Tk()
    main_screen.wm_attributes('-type', 'splash')
    main_screen.geometry("600x250")
    main_screen.title("MultiChat")

    Label(text="Login Or Register", bg="white", width="300", height="2", font=("Calibri", 13)).pack()
    Label(text="").pack()

    Button(text="Login", height="2", command=lambda: registerorlogin("Login"), width="30").pack()
    Label(text="").pack()
    Button(text="Register", height="2", width="30", command=lambda: registerorlogin("Register")).pack()

    main_screen.mainloop()

# If user logout, program does not exit
# User can login/register again
def init():
    # Creating the socket
    global s
    global first
    global userFailed
    userFailed = 0
    first = 0
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Server has to be started before this.
    s.connect(("localhost",8080))

    mainScreen()

if __name__=='__main__':
    init()
