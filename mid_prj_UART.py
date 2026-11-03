import serial

ser = serial.Serial(
    port="COM5",
    baudrate=115200,
    timeout=1
)

print("UART 已連線")

while True:
    text = input("請輸入英文單字：")

    ser.write((text + "\n").encode("utf-8"))

    print("已送出：", text)