import cv2
import random
import math

# Loads fancy face detection model (not really)
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")

rocket = cv2.imread("rocket.jpg")
rocket = cv2.resize(rocket, (50, 50))  # width, height
rocketX = 10
rocketY = 10
velocityX = 0
velocityY = 0
accelerationX = 0
accelerationY = 0
areaX = 0
areaY = 0
centreX = 0
centreY = 0

# Starts default webcam
cap = cv2.VideoCapture(0)

while True: # Reads the webcam and defines each frame to a variable
    ret, frame = cap.read()
    if not ret: # ret is a simple true false depending on whether a frame was read
        print("Can't receive frame.\nEnsure default webcam is connected\nwith correct permissions.\n")
        break

    rocketX = rocketX + random.randint(-5, 5)
    rocketY = rocketY + random.randint(-5, 5)

    # Set feed to greyscale. Apparently the imported model is trained on intensity
    greyscale = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # Scans for faces with the model
    faces = face_cascade.detectMultiScale(greyscale, scaleFactor=1.1, minNeighbors=5)

    # Draw and report detected faces
    for (x, y, w, h) in faces:
        cv2.rectangle(frame, (x, y), (x + w, y + h), (255, 0, 0), 2)

    # Number of data sets in faces variable ie number of faces
    numFaces = len(faces)

    if numFaces > 1:
        print("More than one face detected.\nRocket idling until exactly one face is detected.\n")

    elif numFaces == 0:
        print("No face detected.\nRocket idling until exactly one face is detected.\n")

    elif numFaces == 1:
        for (x, y, w, h) in faces:
            centreX = x + (w // 2)
            centreY = y + (h // 2)

    if areaX > 0 & centreX > rocketX:
        areaX = 0
    if areaX < 0 & centreX < rocketX:
        areaX = 0
    if areaY > 0 & centreY > rocketY:
        areaX = 0
    if areaY < 0 & centreY < rocketY:
        areaX = 0


    areaX = areaX + rocketX - centreX
    areaY = areaY + rocketY - centreY

    accelerationX = 0.1 * (centreX - rocketX) - 0.1 * velocityX - 0.005 * areaX
    if accelerationX > 10:
        accelerationX = 10
    velocityX = velocityX + accelerationX
    accelerationY = 0.1 * (centreY - rocketY) - 0.1 * velocityY - 0.005 * areaY
    if accelerationY > 10:
        accelerationY = 10
    velocityY = velocityY + accelerationY

    rocketX = math.floor(rocketX + velocityX)
    rocketY = math.floor(rocketY + velocityY)

    if rocketX < 10:
        rocketX = 10
    if rocketY < 10:
        rocketY = 10
    if rocketX > 580:
        rocketX = 580
    if rocketY > 430:
        rocketY = 430

    x, y = rocketX, rocketY
    h, w, _ = rocket.shape

    frame[y:y+h, x:x+w] = rocket

    # Feed in seperate window
    cv2.imshow("Camera", frame)

    # 'q' to quit (and gives the feed 1ms time to process and display image)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()