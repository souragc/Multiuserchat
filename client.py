#!/usr/bin/python3
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
            if(data==b"Success"):
                # If username and password correct, close login page and start chatscreen
                usernameFinal = user
                messagebox.showinfo("Success","Succesfully Logged in.")
                subscreen.destroy()
                openChat()
                ## Start chat
                pass
            else:
                messagebox.showerror("Failed","Make sure you enter the correct username & password")
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
                messagebox.showinfo("Success","Succesfully registered. You will be automatically logged in.")
                subscreen.destroy()
                openChat()
                # Start chat
            else:
                messagebox.showerror("Already exists","Username already exists. Please choose a different one.")
                return
def send(chat, T):
    if(not len(chat.get())>0):
        return

    mess = chat.get()+"\n"
    global s
    mess = usernameFinal+":"+mess
    s.sendall(bytes(mess,'ascii'))
    T.configure(state="normal")
    mess = mess[len(usernameFinal)+1:]
    mess = mess.rjust(77," ") + " "*3
    T.insert(tk.END,mess)
    T.configure(state="disabled")
    chat.set("")
    return

def logout(chatWindow):
    global s
    s.send(b"quit")
    chatWindow.destroy()
    s.shutdown(socket.SHUT_RDWR)
    s.close()

    init()

def quit(chatWindow):
    global s
    s.send(b"quit")
    chatWindow.destroy()
    exit(0)

def active():
    activeWindow = Tk()
    activeWindow.geometry("600x350")
    Label(activeWindow, text="Active Users",bg="RoyalBlue2",fg="white",width="600", height="2").pack()
    activeWindow.wm_attributes('-type', 'splash')
    T = Text(activeWindow, height = 15, width = 80)
    T.pack()
    T.configure(yscrollcommand=True)
    T.configure(bd="1")
    for i in range(len(activeUsers)):
        T.insert(tk.END,("User {a} : {b}\n".format(a=i+1,b=activeUsers[i])))
    T.configure(state='disabled')
    Button(activeWindow,text="Close", height="2",bg="tomato",activebackground="orange red",fg="white", command=lambda: activeWindow.destroy(), width="10").pack()

def waitForMessage(T):
    global s
    global activeUsers
    while(True):
        ## Looks like thread is not stopping since we are not exiting while
        ## logging out. Need to find another way to handle this
        try:
            data = s.recv(500)
            if(len(data)>0):
                data = data.strip()
                data = data.decode('ascii')
                temp = data.split("\n")
                for data in temp:
                    if(data.startswith(usernameFinal)):
                        T.configure(state="normal")
                        mess = data[len(usernameFinal)+1:]
                        mess = mess.rjust(77," ") + " "*3
                        T.insert(tk.END,mess)
                        T.configure(state="disabled")
                    else:
                        data = data.strip() + "\n"
                        # If someone joined chat
                        if(data[0]=='\x05'):
                            activeUsers.append(data[1:-1])
                            data = data[1:-1] + " joined the chat\n"
                            # Since person joined after this person, a message is send
                            # informing the presence of this user.
                            newmsg = '\x04'.encode('ascii') + bytes(usernameFinal,'ascii') + b"\n"
                            # Wait for some time so this ping won't be
                            # together with other history messages
                            time.sleep(2)
                            s.sendall(newmsg)
                            T.configure(state="normal")
                            T.insert(tk.END,data)
                            T.configure(state="disabled")
                        # If a person leaves the chat
                        elif(data[0]=='\x06'):
                            name = data[1:-1]
                            activeUsers.remove(name)
                            data = data[1:-1] + " left the chat \n"
                            T.configure(state="normal")
                            T.insert(tk.END,data)
                            T.configure(state="disabled")
                        # This is received by new users connecting
                        elif(data[0]=='\x04'):
                            name = data[1:-1]
                            if name not in activeUsers:
                                activeUsers.append(name)
                        else:
                            T.configure(state="normal")
                            data = " "*3 + data
                            T.insert(tk.END,data)
                            T.configure(state="disabled")
        except:
            pass


def openChat():
    global s
    global activeUsers
    activeUsers.append(usernameFinal+" (me)")
    chatWindow = Tk()
    chatWindow.wm_attributes('-type', 'splash')
    chatWindow.geometry("700x550")
    Label(chatWindow, text="Geek Chat Room!",bg="RoyalBlue2",fg="white",width="700", height="2").pack()
    Label(chatWindow, text="Current session : "+usernameFinal,width="77", height="2").pack()
    T = Text(chatWindow, height = 14, width = 80)
    T.pack()
    T.configure(yscrollcommand=True)
    T.configure(state='disabled')
    T.configure(bd="1")
    chat = StringVar()
    chat_entry = Entry(chatWindow, textvariable=chat, width="80")
    chat_entry.pack()
    Button(chatWindow,text="Send", height="2",bg="SpringGreen2",activebackground="SpringGreen1", command=lambda: send(chat, T), width="77").pack()
    Button(chatWindow,text="Active Users", height="2",bg="SpringGreen2",activebackground="SpringGreen1", command=lambda: active(), width="77").pack()
    Button(chatWindow,text="Logout", height="2",bg="gray25",fg="white",activebackground="gray22", command=lambda: logout(chatWindow), width="77").pack()
    Button(chatWindow,text="Quit", height="2",bg="tomato",fg="white",activebackground="orange red", command=lambda: quit(chatWindow), width="10").pack()

    # Using thread to receive message since the process is not working
    t = threading.Thread(target = waitForMessage, args=(T,))
    # Should run in the background
    t.setDaemon(True)
    t.start()

    chatWindow.mainloop()


# Same function for login and register
# Can be later split to check specific conditions
# Since it looks the same, this avoids code repetition
def registerorlogin(opt="default"):
    global subscreen
    global main_screen
    main_screen.destroy()
    subscreen = Tk()
    subscreen.geometry("600x320")
    #subscreen.eval('tk::PlaceWindow . center')
    subscreen.wm_attributes('-type', 'splash')

    username = StringVar()
    password = StringVar()

    Label(text=opt, bg="RoyalBlue1",fg="white", width="300", height="2", font=("Calibri", 13)).pack()
    Label(subscreen, text="**Enter your details**").pack(pady=(20,0))
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
    Button(subscreen, text=opt,bg="SpringGreen2",activebackground="SpringGreen3", width=10, height=1, command =lambda: do_stuff(opt,username.get(),password.get())).pack()
    Button(subscreen, text="Go back",bg="RoyalBlue1",fg="white",activebackground="RoyalBlue2", width=10, height=1, command =lambda: go_back(subscreen)).pack()


def go_back(subscreen):
    global s
    msg = b"\x08"
    s.sendall(msg)
    subscreen.destroy()
    s.shutdown(socket.SHUT_RDWR)
    s.close()
    init()
    

def quit_main(main_screen):
    global s
    s.sendall(b"3")
    main_screen.destroy()

# Starting screen
def mainScreen():
    global main_screen
    main_screen = Tk()
    main_screen.wm_attributes('-type', 'splash')
    main_screen.geometry("600x280")

    Label(text="Welcome to Geek Chat!", bg="RoyalBlue1",fg="white", width="300", height="2", font=("Calibri", 13)).pack()
    Label(text="").pack()

    Button(text="Login", height="2",bg="gray25",fg="white",activebackground="gray22", command=lambda: registerorlogin("Login"), width="30").pack(pady=(30,5) )
    Button(text="Register", height="2",bg="SpringGreen2",activebackground="SpringGreen3", width="30", command=lambda: registerorlogin("Register")).pack()
    Button(text="Close", height="2", width="6",bg="tomato",fg="white",activebackground="orange red", command=lambda: quit_main(main_screen)).pack(pady=20)

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
    global activeUsers
    activeUsers = []
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Server has to be started before this.
    s.connect(("localhost",8080))

    mainScreen()

if __name__=='__main__':
    init()
