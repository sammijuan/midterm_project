
import tkinter as tk
from tkinter import ttk
import serial
import threading

# =====================================
# UART 設定
# =====================================

PORT = "COM3"
BAUDRATE = 115200

ser = None
is_playing = False

# =====================================
# 摩斯碼表
# =====================================

MORSE_TABLE = {
    "A": ".-",    "B": "-...",  "C": "-.-.",
    "D": "-..",   "E": ".",     "F": "..-.",
    "G": "--.",   "H": "....",  "I": "..",
    "J": ".---",  "K": "-.-",   "L": ".-..",
    "M": "--",    "N": "-.",    "O": "---",
    "P": ".--.",  "Q": "--.-",  "R": ".-.",
    "S": "...",   "T": "-",     "U": "..-",
    "V": "...-",  "W": ".--",   "X": "-..-",
    "Y": "-.--",  "Z": "--.."
}

# =====================================
# GUI 主視窗
# =====================================

root = tk.Tk()
root.title("光語密碼：STM32 摩斯碼播放器")
root.geometry("650x480")
root.resizable(False, False)

root.configure(bg="#F5F7FB")

# =====================================
# GUI 變數
# =====================================

connection_var = tk.StringVar(value="UART：尚未連線")
morse_var = tk.StringVar(value="Morse：")
status_var = tk.StringVar(value="狀態：等待輸入")

# =====================================
# UART 連線
# =====================================

def connect_uart():
    global ser

    try:
        ser = serial.Serial(
            port=PORT,
            baudrate=BAUDRATE,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.2
        )

        connection_var.set(f"UART：{PORT} 已連線")

        # 背景接收 STM32 訊息
        threading.Thread(
            target=uart_receive,
            daemon=True
        ).start()

    except serial.SerialException:
        connection_var.set(
            f"UART：{PORT} 連線失敗"
        )

# =====================================
# 接收 STM32 回傳
# =====================================

def uart_receive():
    while ser is not None and ser.is_open:
        try:
            response = ser.readline().decode(
                "utf-8",
                errors="replace"
            ).strip()

            if response:
                root.after(
                    0,
                    lambda msg=response: update_status(msg)
                )

        except (serial.SerialException, OSError):
            root.after(
                0,
                lambda: status_var.set("狀態：UART 連線中斷")
            )
            break

# =====================================
# 更新狀態
# =====================================

def update_status(message):
    global is_playing

    if message.startswith("RX:"):
        status_var.set(f"狀態：STM32 {message}")

    elif message == "PLAYING":
        is_playing = True
        status_var.set("狀態：LED 正在播放摩斯碼")
        send_button.config(state="disabled")

    elif message == "DONE":
        is_playing = False
        status_var.set("狀態：播放完成 DONE")
        send_button.config(state="normal")

# =====================================
# 英文轉摩斯碼（GUI 顯示）
# =====================================

def text_to_morse(text):
    return "   ".join(
        MORSE_TABLE[c]
        for c in text.upper()
        if c in MORSE_TABLE
    )

# =====================================
# 傳送英文
# =====================================

def send_text():
    global is_playing

    if ser is None or not ser.is_open:
        status_var.set("狀態：UART 尚未連線")
        return

    if is_playing:
        return

    text = input_entry.get().strip().upper()

    if not text:
        status_var.set("狀態：請輸入英文")
        return

    if len(text) > 31:
        status_var.set("狀態：最多輸入 31 個字元")
        return

    if not text.isascii() or not text.isalpha():
        status_var.set("狀態：只能輸入英文字母 A-Z")
        return

    # 顯示摩斯碼
    morse = text_to_morse(text)
    morse_var.set(f"Morse：{morse}")

    # 加入換行字元，傳給 STM32
    message = text + "\n"

    try:
        ser.write(message.encode("utf-8"))

        is_playing = True
        send_button.config(state="disabled")
        status_var.set(f"狀態：已送出 {text}")

    except serial.SerialException:
        is_playing = False
        send_button.config(state="normal")
        status_var.set("狀態：傳送失敗")

# =====================================
# 關閉視窗
# =====================================

def on_close():
    if ser is not None and ser.is_open:
        ser.close()

    root.destroy()

root.protocol("WM_DELETE_WINDOW", on_close)

# =====================================
# GUI 畫面設計
# =====================================

title_label = tk.Label(
    root,
    text="光語密碼：STM32 摩斯碼播放器",
    font=("Microsoft JhengHei", 21, "bold"),
    bg="#F5F7FB",
    fg="#17233B"
)
title_label.pack(pady=(35, 20))

connection_label = tk.Label(
    root,
    textvariable=connection_var,
    font=("Microsoft JhengHei", 13),
    bg="#F5F7FB",
    fg="#24764A"
)
connection_label.pack(pady=12)

input_frame = tk.Frame(
    root,
    bg="#F5F7FB"
)
input_frame.pack(pady=16)

input_label = tk.Label(
    input_frame,
    text="輸入英文：",
    font=("Microsoft JhengHei", 14),
    bg="#F5F7FB",
    fg="#17233B"
)
input_label.pack(side="left", padx=10)

input_entry = ttk.Entry(
    input_frame,
    font=("Arial", 15),
    width=25
)
input_entry.pack(side="left")

send_button = tk.Button(
    root,
    text="傳送",
    font=("Microsoft JhengHei", 14, "bold"),
    width=14,
    height=2,
    bg="#DCE9FA",
    fg="#17233B",
    command=send_text
)
send_button.pack(pady=18)

morse_label = tk.Label(
    root,
    textvariable=morse_var,
    font=("Consolas", 17),
    bg="#F5F7FB",
    fg="#17233B",
    wraplength=580
)
morse_label.pack(pady=18)

status_label = tk.Label(
    root,
    textvariable=status_var,
    font=("Microsoft JhengHei", 12),
    bg="#F5F7FB",
    fg="#526079",
    wraplength=580
)
status_label.pack(pady=12)

# 按 Enter 也能傳送
root.bind("<Return>", lambda event: send_text())

# =====================================
# 啟動 UART 與 GUI
# =====================================

root.after(100, connect_uart)
root.mainloop()
