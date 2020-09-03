#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from deepgtav.messages import Start, Stop, Scenario, Dataset, Commands, frame2numpy, GoToLocation, TeleportToLocation, SetCameraPositionAndRotation
from deepgtav.messages import StartRecording, StopRecording
from deepgtav.client import Client

import argparse
import time
import cv2

import matplotlib.pyplot as plt

from PIL import Image

class Model:
    def run(self,frame):
        return [1.0, 0.0, 0.0] # throttle, brake, steering





def add_bboxes(image, bboxes):
    """Display image with object bounding boxes 

    bboxes as list of dicts, e.g.
        [{'category': 'Car',
          'left': '537',
          'top': '117',
          'right': '585',
          'bottom': '204'},
         {'category': 'Car',
          'left': '546',
          'top': '385',
          'right': '595',
          'bottom': '468'},
         {'category': 'Car',
          'left': '792',
          'top': '617',
          'right': '837',
          'bottom': '704'},
         {'category': 'Car',
          'left': '683',
          'top': '251',
          'right': '741',
          'bottom': '336'}]

    
    Args:
        image: the image to add bounding boxes into
        bboxes: the bounding box data
    """

    for bbox in bboxes:
        x1 = bbox['left']
        y1 = bbox['top']
        x2 = bbox['right']
        y2 = bbox['bottom']
        label = bbox['label']
        cv2.putText(image, label, (x1, y1+25), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 0), thickness = 2, lineType=cv2.LINE_AA) 
        cv2.rectangle(image, (x1, y1), (x2, y2), (255, 255, 0), 2)

    return image


    
def parseBBox2d(bbox2d):
    if bbox2d == None:
        return []
    items = bbox2d.split("\n")
    ret = []
    for item in items[:-1]:
        data = item.split(" ")
        # Indices can be found in /DeepGTAV-PreSIL/dataformat-augmented.txt
        label = data[0]
        left = int(data[4])
        top = int(data[5])
        right = int(data[6])
        bottom = int(data[7])
        ret.append({"label": label,"left": left,"top": top,"right": right,"bottom": bottom})
    return ret


# img = add_bboxes(frame2numpy(message['frame'], (1920,1080)), parseBBox2d(message["bbox2d"]))
# cv2.imshow("test", img)
# cv2.waitKey() 
# cv2.destroyAllWindows()


# plt.imshow(img)
# img = Image.fromarray(img, 'RGB')
# img.show()
    



# Controls the DeepGTAV vehicle
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=None)
    parser.add_argument('-l', '--host', default='localhost', help='The IP where DeepGTAV is running')
    parser.add_argument('-p', '--port', default=8000, help='The port where DeepGTAV is running')
    # args = parser.parse_args()

    # TODO for running in VSCode
    args = parser.parse_args('')
    

    # Creates a new connection to DeepGTAV using the specified ip and port. 
    # If desired, a dataset path and compression level can be set to store in memory all the data received in a gziped pickle file.
    # We don't want to save a dataset in this case
    client = Client(ip=args.host, port=args.port)
    



    # We set the scenario to be in manual driving, and everything else random (time, weather and location). 
    # See deepgtav/messages.py to see what options are supported
    # scenario = Scenario(drivingMode=-1) #manual driving (Works, but is too slow to be used)
    # scenario = Scenario(drivingMode=786603) #automatic driving
    # scenario = Scenario(drivingMode=786603, vehicle="buzzard") #automatic driving
    # scenario = Scenario(drivingMode=786603, vehicle="buzzard", location=[-424.991, -522.049, 50]) #automatic driving
    scenario = Scenario(drivingMode=786603, vehicle="buzzard", location=[245.23306274414062, -998.244140625, 29.205352783203125]) #automatic driving
    # scenario = Scenario(drivingMode=[786603, 20.0], vehicle="buzzard", location=[275.23306274414062, -998.244140625, 29.205352783203125]) #automatic driving
    # scenario = Scenario(drivingMode=-1, vehicle="buzzard") #automatic driving

    dataset=Dataset(showBoxes=True, location=True)
    # dataset = Dataset(recordScenario=True)
    # dataset = Dataset(stationaryScene=True)
   


    # Send the Start request to DeepGTAV. Dataset is set as default, we only receive frames at 10Hz (320, 160)
    client.sendMessage(Start(scenario=scenario, dataset=dataset))
    

    # Adjustments for recording from UAV perspective
    client.sendMessage(SetCameraPositionAndRotation(z = -3, rot_x = -90))

    # client.sendMessage(StartRecording())

    # Manual Control
    # client.sendMessage(Commands(throttle=1.))

    # Dummy agent
    model = Model()

    # Start listening for messages coming from DeepGTAV. We do it for 80 hours

    stoptime = time.time() + 80 * 3600
    count = 0
    bbox2d_old = ""
    errors = []
    while time.time() < stoptime:
        try:
            # We receive a message as a Python dictionary
            count += 1
            print("count: ", count)


            if count % 10 == 0:
                client.sendMessage(StartRecording())
            if count % 10 == 1:
                client.sendMessage(StopRecording())

            if count == 2:
                client.sendMessage(TeleportToLocation(955.23306274414062, -998.244140625, 200.205352783203125))
                client.sendMessage(GoToLocation(1955.23306274414062, -998.244140625, 50.205352783203125))
            # if count == 50:
            #     client.sendMessage(TeleportToLocation(955.23306274414062, -998.244140625, 200.205352783203125))
            #     client.sendMessage(GoToLocation(1955.23306274414062, -998.244140625, 50.205352783203125))
            
            # if count == 100:
            #     client.sendMessage(TeleportToLocation(1555.23306274414062, -998.244140625, 200.205352783203125))
            #     client.sendMessage(GoToLocation(1955.23306274414062, -998.244140625, 50.205352783203125))
            
            # if count == 110:
            #     client.sendMessage(StartRecording())

            # if count == 120:
            #     client.sendMessage(StopRecording())
            # if count == 150:
            #     client.sendMessage(StartRecording())

            # if count == 170:
            #     client.sendMessage(StopRecording())
            # if count == 172:
            #     client.sendMessage(TeleportToLocation(955.23306274414062, -998.244140625, 200.205352783203125))
            #     client.sendMessage(GoToLocation(1955.23306274414062, -998.244140625, 50.205352783203125))
            # if count == 180:
            #     client.sendMessage(StartRecording())

            if count == 50:
                client.sendMessage(GoToLocation(1955.23306274414062, 998.244140625, 50.205352783203125))

            if count == 200:
                client.sendMessage(GoToLocation(1955.23306274414062, 2998.244140625, 50.205352783203125))

            if count == 300:
                client.sendMessage(GoToLocation(1955.23306274414062, 2998.244140625, 20.205352783203125))
            if count == 400:
                client.sendMessage(GoToLocation(1955.23306274414062, -2998.244140625, 20.205352783203125))
            if count == 500:
                client.sendMessage(GoToLocation(-1955.23306274414062, -2998.244140625, 20.205352783203125))



            # if count == 2:
            #     client.sendMessage(GoToLocation(955.23306274414062, -998.244140625, 50.205352783203125, 20.0))
            
            # if count == 20:
            #     client.sendMessage(StartRecording())
            message = client.recvMessage()  
            
            # None message from utf-8 decode error
            # TODO this should be managed better
            if message == None:
                continue

            # print("vehicles: ", message["vehicles"])
            # print("peds: ", message["peds"])
            # print("location: ", message["location"])
            # print("index: ", message["index"])
            # print("focalLen: ", message["focalLen"])
            # print("curPosition: ", message["curPosition"])
            # print("seriesIndex: ", message["seriesIndex"])
            print("bbox2d: ", message["bbox2d"])

            if message["bbox2d"] != bbox2d_old:
                try: # Sometimes there are errors with the message, i catch those here
                    img = add_bboxes(frame2numpy(message['frame'], (1920,1080)), parseBBox2d(message["bbox2d"]))
                    cv2.imshow("test", img)
                    cv2.waitKey(1) 
                    bbox2d_old = message["bbox2d"]
                except Exception as e:
                    print(Exception)
                    errors.append(e)



            
            # imgplot = plt.imshow(img)
            # plt.show()

            # client.sendMessage(Commands(throttle=1.))

            # The frame is a numpy array that can we pass through a CNN for example     
            # image = frame2numpy(message['frame'], (320,160))
            # commands = model.run(image)
            # We send the commands predicted by the agent back to DeepGTAV to control the vehicle
            # client.sendMessage(Commands(commands[0], commands[1], commands[2]))
        except KeyboardInterrupt:
            break
            
    # We tell DeepGTAV to stop
    client.sendMessage(Stop())
    client.close()

