import cv2
import os
import time
import shutil
import threading
import queue
import numpy as np
import serial

# set up file paths relative to wherever this script lives
base_dir     = os.path.dirname(os.path.abspath(__file__))
dataset_path = os.path.join(base_dir, "dataset")
trainer_path = os.path.join(base_dir, "trainer.yml")

os.makedirs(dataset_path, exist_ok=True)

# connect to arduino - change COM3 if on a different port
arduino      = serial.Serial('COM6', 9600, timeout=1)
serial_lock  = threading.Lock()
serial_queue = queue.Queue()
time.sleep(2)
print("Connected to Arduino.")

# clear anything arduino sent during boot
while arduino.in_waiting:
    arduino.readline()


def arduino_write(msg):
    # all writes go through here so threads cant clash on serial
    with serial_lock:
        arduino.write(msg)
    print(f"[SENT] {msg.decode().strip()}")


def serial_reader_thread(stop_event):
    # this is the only place arduino.readline() gets called
    # everything goes into serial_queue so nothing gets lost
    while not stop_event.is_set():
        try:
            if arduino.in_waiting:
                raw = arduino.readline()
                try:
                    msg = raw.decode().strip()
                except Exception:
                    msg = ""
                if msg:
                    serial_queue.put(msg)
                    print(f"[Arduino] {msg}")
            else:
                time.sleep(0.01)
        except Exception:
            time.sleep(0.05)


def display_thread(frame_queue, stop_event):
    # imshow has to run on the main thread on windows so we use a separate
    # thread here and pass frames through a queue from the recognition thread
    while not stop_event.is_set():
        try:
            frame = frame_queue.get(timeout=0.05)
            if frame is None:
                cv2.destroyAllWindows()
            else:
                cv2.imshow("Security Camera", frame)
                if cv2.waitKey(1) == 27:
                    cv2.destroyAllWindows()
        except queue.Empty:
            pass
    cv2.destroyAllWindows()


def check_pin(entered):
    # sends the PIN to arduino and waits up to 3 seconds for a response
    # arduino replies PIN_CORRECT or PIN_WRONG
    arduino_write(f"PIN:{entered}\n".encode())
    deadline = time.time() + 3
    while time.time() < deadline:
        try:
            msg = serial_queue.get(timeout=0.3)
            if msg == "PIN_CORRECT":
                return True
            elif msg == "PIN_WRONG":
                return False
            else:
                # put non-pin messages back so nothing gets lost
                serial_queue.put(msg)
        except queue.Empty:
            pass
    print("No response from Arduino.")
    return False


def login():
    print("\nBurglar Alarm System")
    while True:
        entered = input("Enter PIN: ").strip()
        if check_pin(entered):
            print("Access granted.")
            return
        print("Wrong PIN, try again.")


def change_pin():
    print("Current PIN: ", end="", flush=True)
    if not check_pin(input().strip()):
        print("Incorrect.")
        return
    new1 = input("New PIN: ").strip()
    new2 = input("Confirm new PIN: ").strip()
    if not new1:
        print("PIN cant be empty.")
        return
    if new1 != new2:
        print("PINs dont match.")
        return
    # send new PIN to arduino - it saves it to EEPROM
    arduino_write(f"NEWPIN:{new1}\n".encode())
    print("PIN updated.")


def run_train():
    people = [p for p in sorted(os.listdir(dataset_path))
              if os.path.isdir(os.path.join(dataset_path, p))]

    if not people:
        print("No images to train on.")
        arduino_write(b"TRAIN_FAILED\n")
        return

    print("Training...")
    arduino_write(b"TRAINING\n")

    faces     = []
    labels    = []
    label_map = {}
    label_id  = 0

    face_cascade = cv2.CascadeClassifier(
        cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
    )

    for person_name in people:
        person_path = os.path.join(dataset_path, person_name)
        label_map[label_id] = person_name
        print(f"  loading: {person_name}")

        for img_name in os.listdir(person_path):
            img = cv2.imread(os.path.join(person_path, img_name))
            if img is None:
                continue
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            detected = face_cascade.detectMultiScale(gray, 1.05, 10, minSize=(100, 100))
            for (x, y, w, h) in detected:
                faces.append(gray[y:y+h, x:x+w])
                labels.append(label_id)

        label_id += 1

    if not faces:
        print("No faces found in the images.")
        arduino_write(b"TRAIN_FAILED\n")
        return

    recogniser = cv2.face.LBPHFaceRecognizer_create()
    recogniser.train(faces, np.array(labels))
    recogniser.save(trainer_path)
    print(f"Training done. {len(faces)} samples across {len(people)} people.")
    arduino_write(b"TRAINING_DONE\n")


def run_update_database():
    name = input("Enter name: ").strip()
    if not name:
        print("No name entered.")
        return

    save_path = os.path.join(dataset_path, name)
    os.makedirs(save_path, exist_ok=True)
    print("SPACE to capture, ESC when done.")
    arduino_write(b"CAPTURE_READY\n")

    cap   = cv2.VideoCapture(0)
    count = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        disp = frame.copy()
        cv2.putText(disp, f"{name} - {count} captured", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        cv2.putText(disp, "SPACE = save  ESC = done", (10, 60),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)
        cv2.imshow("Capture", disp)

        key = cv2.waitKey(1)
        if key == 32:
            cv2.imwrite(os.path.join(save_path, f"{count}.jpg"), frame)
            print(f"saved {count}.jpg")
            count += 1
        elif key == 27:
            break

    cap.release()
    cv2.destroyAllWindows()
    print(f"{count} images saved for {name}.")
    arduino_write(b"CAPTURE_DONE\n")
    print("Retraining...")
    run_train()


def run_delete():
    people = [p for p in sorted(os.listdir(dataset_path))
              if os.path.isdir(os.path.join(dataset_path, p))]

    if not people:
        print("Dataset is empty.")
        return

    print("\nPeople in dataset:")
    for i, name in enumerate(people, 1):
        print(f"  {i}. {name}")

    choice = input("Number to delete (or anything else to cancel): ").strip()
    if not choice.isdigit() or not (1 <= int(choice) <= len(people)):
        print("Cancelled.")
        return

    name = people[int(choice) - 1]
    if input(f"Delete {name}? (y/n): ").strip().lower() == "y":
        shutil.rmtree(os.path.join(dataset_path, name))
        print(f"Deleted {name}.")
        if input("Retrain now? (y/n): ").strip().lower() == "y":
            run_train()
    else:
        print("Cancelled.")


def contact_security():
    arduino_write(b"CONTACT_SECURITY\n")
    print("Security contacted.")


def recognition_thread(system_armed, disarm_event, frame_queue, pin_requested):
    if not os.path.exists(trainer_path):
        print("No trained model - train first.")
        arduino_write(b"ERROR_NO_MODEL\n")
        frame_queue.put(None)
        return

    people = [p for p in sorted(os.listdir(dataset_path))
              if os.path.isdir(os.path.join(dataset_path, p))]
    if not people:
        print("No dataset found.")
        arduino_write(b"ERROR_NO_DATASET\n")
        frame_queue.put(None)
        return

    face_cascade = cv2.CascadeClassifier(
        cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
    )
    recogniser = cv2.face.LBPHFaceRecognizer_create()
    recogniser.read(trainer_path)
    label_map = {i: name for i, name in enumerate(people)}

    print("Recognition running.")

    # CLAHE improves detection in low light conditions
    clahe = cv2.createCLAHE(clipLimit=3.0, tileGridSize=(10, 10))
    cap   = cv2.VideoCapture(0)

    check_requested  = False
    pin_entry_active = False
    pin_deadline     = 0
    PIN_TIMEOUT      = 30
    last_check_time  = 0
    SENSOR_COOLDOWN  = 8  # wait 8s before responding to another trigger

    # ignore sensors for the first 60s while the user is leaving
    EXIT_DELAY        = 60
    arm_time          = time.time()
    exit_delay_active = True
    remaining_exit    = EXIT_DELAY

    while not disarm_event.is_set():

        try:
            msg = serial_queue.get_nowait()
            if msg == "CHECK_FACE":
                now = time.time()
                check_requested = True #test
                if not pin_entry_active and not exit_delay_active and now - last_check_time >= SENSOR_COOLDOWN:
                    #check_requested = True
                    last_check_time = now
        except queue.Empty:
            pass

        if exit_delay_active:
            elapsed        = time.time() - arm_time
            remaining_exit = int(EXIT_DELAY - elapsed)
            if elapsed >= EXIT_DELAY:
                exit_delay_active = False
                print("System fully armed.")

        ret, frame = cap.read()
        if not ret:
            break

        gray  = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        gray  = clahe.apply(gray)
        faces = face_cascade.detectMultiScale(gray, 1.05, 10, minSize=(100, 100))

        num_faces    = len(faces)
        system_state = "NO_FACE"

        if num_faces > 0:
            for (x, y, w, h) in faces:
                cx = x + w // 2
                cy = y + h // 2

                face       = gray[y:y+h, x:x+w]
                lid, conf  = recogniser.predict(face)
                name       = label_map.get(lid, "Unknown")

                if conf < 65:
                    system_state = "KNOWN"
                    col          = (0, 255, 0)
                    label        = f"{name} ({int(100 - conf)}%)"
                else:
                    system_state = "UNKNOWN"
                    col          = (0, 0, 255)
                    label        = "Unknown"

                cv2.rectangle(frame, (x, y), (x+w, y+h), col, 2)
                cv2.putText(frame, label, (x, y - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.8, col, 2)
                cv2.circle(frame, (cx, cy), 5, (255, 255, 0), -1)

            if num_faces > 1:
                system_state = "UNKNOWN"

        if check_requested:
            if system_state == "KNOWN":
                arduino_write(b"FACE_OK\n")
                pin_entry_active = True
                pin_deadline     = time.time() + PIN_TIMEOUT
                pin_requested.set()
                print(f"Known face - enter PIN within {PIN_TIMEOUT}s.")
            elif system_state == "UNKNOWN":
                arduino_write(b"FACE_UNKNOWN\n")
                ts = time.strftime("%Y-%m-%d %H:%M:%S")
                print(f"Intruder detected at {ts}")
                arduino_write(f"LOG:{ts}\n".encode())
            else:
                arduino_write(b"FACE_NONE\n")
            check_requested = False

        if pin_entry_active:
            remaining = int(pin_deadline - time.time())
            cv2.putText(frame, f"Enter PIN: {remaining}s", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 165, 255), 2)

            if disarm_event.is_set():
                pin_entry_active = False
                break

            if remaining <= 0:
                print("PIN timed out.")
                arduino_write(b"PIN_TIMEOUT\n")
                pin_entry_active = False
                pin_requested.clear()

        if exit_delay_active:
            cv2.putText(frame, f"Leaving: {remaining_exit}s", (10, frame.shape[0] - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 165, 255), 2)
        else:
            cv2.putText(frame, "ARMED", (10, frame.shape[0] - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)

        # drop old frames if the display thread is behind
        if frame_queue.full():
            try:
                frame_queue.get_nowait()
            except queue.Empty:
                pass
        frame_queue.put(frame)

    cap.release()
    frame_queue.put(None)
    print("Recognition stopped.")


def print_menu(system_armed):
    status = "ARMED" if system_armed[0] else "DISARMED"
    print(f"\nBurglar Alarm [{status}]")
    print("1. Arm / Disarm")
    print("2. Add person to database")
    print("3. Remove person from database")
    print("4. Change PIN")
    print("5. Contact security")
    print("6. View intruder log")
    print("7. Exit")
    print("Choice: ", end="", flush=True)


def main():
    # start serial reader before login so responses can be received
    serial_stop = threading.Event()
    threading.Thread(target=serial_reader_thread, args=(serial_stop,), daemon=True).start()

    arduino_write(b"READY\n")

    # login - arduino checks the PIN
    login()

    system_armed  = [False]
    disarm_event  = threading.Event()
    pin_requested = threading.Event()
    recog_thread  = [None]
    frame_queue   = queue.Queue(maxsize=2)

    display_stop = threading.Event()
    threading.Thread(target=display_thread, args=(frame_queue, display_stop), daemon=True).start()

    while True:

        # known face detected - interrupt menu and ask for PIN straight away
        if pin_requested.is_set():
            print("\nKNOWN FACE DETECTED - enter PIN to disarm")
            entered = input("PIN: ").strip()
            if check_pin(entered):
                # arduino handles disarm internally when PIN_CORRECT received during entry window
                system_armed[0] = False
                pin_requested.clear()
                disarm_event.set()
                if recog_thread[0]:
                    recog_thread[0].join(timeout=3)
                disarm_event.clear()
                recog_thread[0] = None
                print("Disarmed.")
            else:
                print("Wrong PIN.")
            continue

        print_menu(system_armed)
        choice = input().strip()

        if choice == "1":
            if system_armed[0]:
                entered = input("PIN to disarm: ").strip()
                if check_pin(entered):
                    # arduino disarms on PIN_CORRECT - we just stop the recognition thread
                    system_armed[0] = False
                    disarm_event.set()
                    if recog_thread[0]:
                        recog_thread[0].join(timeout=3)
                    disarm_event.clear()
                    recog_thread[0] = None
                    print("Disarmed.")
                else:
                    print("Wrong PIN.")
            else:
                print("Arming - 60 seconds to leave.")
                arduino_write(b"ARM\n")
                system_armed[0] = True
                disarm_event.clear()
                t = threading.Thread(
                    target=recognition_thread,
                    args=(system_armed, disarm_event, frame_queue, pin_requested),
                    daemon=True
                )
                recog_thread[0] = t
                t.start()

        elif choice == "2":
            if system_armed[0]:
                print("Disarm first.")
            else:
                run_update_database()

        elif choice == "3":
            if system_armed[0]:
                print("Disarm first.")
            else:
                run_delete()

        elif choice == "4":
            change_pin()

        elif choice == "5":
            contact_security()

        elif choice == "6":
            # request the log from arduino - it stores the last 10 events
            # as an array of Log objects and sends them back one by one
            print("\nIntruder Log:")
            arduino_write(b"GET_LOGS\n")
            deadline  = time.time() + 5
            started   = False
            got_entry = False
            while time.time() < deadline:
                try:
                    msg = serial_queue.get(timeout=0.5)
                    if msg == "LOG_START":
                        started = True
                    elif msg == "LOG_END":
                        break
                    elif msg == "No intruder logs.":
                        print("No entries yet.")
                        got_entry = True
                        break
                    elif started:
                        #print(msg)
                        got_entry = True
                except queue.Empty:
                    pass
            if not got_entry and started:
                print("No entries yet.")

        elif choice == "7":
            print("Shutting down.")
            if system_armed[0]:
                disarm_event.set()
                if recog_thread[0]:
                    recog_thread[0].join(timeout=3)
            arduino_write(b"SHUTDOWN\n")
            serial_stop.set()
            display_stop.set()
            break

        else:
            print("Invalid choice, enter 1-7.")

    print("System off.")


if __name__ == "__main__":
    main()
