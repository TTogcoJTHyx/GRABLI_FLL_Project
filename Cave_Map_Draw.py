import asyncio
import sys
from bleak import BleakScanner, BleakClient
from math import cos, sin, radians



# UUID для вашей ревизии модуля Makeblock BLE V1.0_c
UART_RX_CHAR_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"
UART_TX_CHAR_UUID = "0000ffe3-0000-1000-8000-00805f9b34fb"

# 📥 Глобальный буфер для накопления разорванных BLE-пакетов
ble_buffer = bytearray()

x, y = 0.0, 0.0
coordinates = []
temperature = []
gas_level = []
humidity = []

robot_disconnected = False

def notification_handler(sender, data):
    """Собирает осколки пакетов BLE и декодирует их только при конце строки (\n)"""
    global ble_buffer
    ble_buffer.extend(data) # Добавляем новые байты в конец буфера
    
    # Ищем символ перевода строки (\n), который шлет Serial3.println()
    while b'\n' in ble_buffer:
        # Отрезаем до первого символа '\n'
        line_bytes, ble_buffer = ble_buffer.split(b'\n', 1)
        
        try:
            # Декодируем накопленную целую строку
            text = line_bytes.decode('utf-8').strip()
            if text:
                print(text)
                data_queue.put_nowait(text)  # кладём в очередь
        except UnicodeDecodeError:
            pass


async def data_processor():
    global x, y, coordinates

    while True:
        text = await data_queue.get()

        if 'S' in text:  # ← пакет сенсоров
            try:
                parts = text.split('S')
                gas  = float(parts[0])
                temp = float(parts[1])
                hum  = float(parts[2])
                gas_level.append(gas)
                temperature.append(temp)
                humidity.append(hum)
            except (ValueError, IndexError):
                print(f"[сенсор]: {text}")

        elif ';' in text:  # ← пакет координат
            try:
                parts = text.split(';')
                ticks_L = float(parts[0])
                ticks_R = float(parts[1])
                theta   = float(parts[2])

                d_center = (ticks_L + ticks_R) / 2
                y += d_center * cos(radians(-theta)) / 10
                x += d_center * sin(radians(-theta)) / 10
                coordinates.append((x, y))
                print(coordinates[-1])
            except (ValueError, IndexError):
                print(f"[служебное]: {text}")
            


async def listen_for_enter(client):
    loop = asyncio.get_running_loop()
    print("[Python]: Активация Bluetooth-канала...")
    await client.write_gatt_char(UART_TX_CHAR_UUID, b"\n", response=False)
    await asyncio.sleep(0.2)
    
    print("\n>>> НАЖМИТЕ ENTER, ЧТОБЫ ЗАПУСТИТЬ КОД НА MEGAPI <<<\n")
    await loop.run_in_executor(None, sys.stdin.readline)
    
    await client.write_gatt_char(UART_TX_CHAR_UUID, b"G\n", response=False)
    print("[Python]: Команда отправлена!")
    
    # После отправки просто держим соединение живым
    while True:
        await asyncio.sleep(1)
        if robot_disconnected:
            return  # выходим из asyncio


def disconnected_callback(client):
    global robot_disconnected
    print("Робот отключился!")
    robot_disconnected = True


def draw_map():
    import turtle
    t = turtle.Turtle()
    t.speed(0)  # максимальная скорость
    for coord in coordinates:
        t.goto(coord)
    turtle.done()


async def main():
    global data_queue
    data_queue = asyncio.Queue()
    
    print("Поиск устройств Makeblock...")
    devices = await BleakScanner.discover()
    
    target_address = None
    for d in devices:
        if d.name and "Makeblock" in d.name:
            target_address = d.address
            break

    if not target_address:
        print("Робот не найден.")
        return

    async with BleakClient(target_address, disconnected_callback=disconnected_callback) as client:
        if client.is_connected:
            print("Успешно подключено!")
            await client.start_notify(UART_RX_CHAR_UUID, notification_handler)
            
            tasks = [
                asyncio.create_task(listen_for_enter(client)),
                asyncio.create_task(data_processor()),
            ]
            
            # Ждём пока listen_for_enter не выйдет (после отключения робота)
            await tasks[0]
            
            # Отменяем data_processor
            tasks[1].cancel()
            try:
                await tasks[1]
            except asyncio.CancelledError:
                pass


if __name__ == "__main__":
    try:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        loop.run_until_complete(main())
    except KeyboardInterrupt:
        print("\nПрограмма остановлена.")
    finally:
        loop.close()      # закрываем loop
        draw_map()        # рисуем карту уже вне asyncio
        print("Temerature: ", temperature)
        print("Humidity: ", humidity)
        print("Gas level: ", gas_level)
