import tkinter
import tkinter.messagebox

def bg_blue (e): app.config(bg='blue')
def bg_white(e): app.config(bg='white')

def msg(e):
  tkinter.messagebox.showinfo('タイトル行','メッセージ本文')
  print(e.widget['text'] + ' happy birth day!')

app = tkinter.Tk()
app.geometry('600x400')
app.config(bg='white')

label1 = tkinter.Label(app, width=10, height=2, text='ラベル１')
label1.pack(pady=20)

label2 = tkinter.Label(app, width=10, height=2, text='ラベル２')
label2.pack(pady=20)

button1 = tkinter.Button(app, width=10, height=2, text='ボタン１')
button1.pack(expand=True, fill=tkinter.BOTH)
#button1.pack(pady=20)

label1.bind('<Enter>', bg_blue)
label1.bind('<Leave>', bg_white)
button1.bind('<Button-1>', msg)

app.mainloop()
