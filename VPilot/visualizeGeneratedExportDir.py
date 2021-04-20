from PIL import Image
import cv2
import os
import numpy as np

from utils.BoundingBoxes import parseBBox_YoloFormat_to_Image, show_image_with_bboxes, parseBBox_YoloFormat_to_Number
    
def show_dataset_online_format(images_path, include_boats=False):
    images_path = os.path.normpath(images_path)
    for file_name in os.listdir(images_path):
        image = Image.open(os.path.join(images_path, file_name))
        with open(os.path.join(images_path.replace("images", "labels"), file_name.replace(".jpg", ".txt")), "r") as bbox_file:
            bboxes = bbox_file.read()
        
        bboxes = parseBBox_YoloFormat_to_Image(bboxes, *image.size, include_boats=include_boats)
        
        image = np.array(image)
        show_image_with_bboxes(image, bboxes)

# show_dataset_online_format("Z:\\DeepGTAV-EXPORTDIR-TEST\\Generation3_With_Meta_data\\images")
# show_dataset_online_format("E:\\Bachelorarbeit\\Datasets\\VisDrone\\VisDrone_YOLO_FORMAT_CLEANED\\train\\images")
# show_dataset_online_format("G:\\EXPORTDIR\\ExportWater_4k_4\\images", include_boats=True)




def show_dataset_online_format_NUMBERS_ONLY(images_path):
    images_path = os.path.normpath(images_path)
    for file_name in os.listdir(images_path):
        image = Image.open(os.path.join(images_path, file_name))
        with open(os.path.join(images_path.replace("images", "labels"), file_name.replace(".jpg", ".txt")), "r") as bbox_file:
            bboxes = bbox_file.read()
        
        bboxes = parseBBox_YoloFormat_to_Number(bboxes, *image.size)
        image = np.array(image)
        show_image_with_bboxes(image, bboxes)


# show_dataset_online_format_NUMBERS_ONLY("E:\\Bachelorarbeit\\Datasets\\VisDrone\\VisDrone_YOLO_FORMAT\\train\\images")
# show_dataset_online_format_NUMBERS_ONLY("E:\\Bachelorarbeit\\Datasets\\VisDrone\\VisDrone_YOLO_FORMAT_CLEANED\\train\\images")
# show_dataset_online_format_NUMBERS_ONLY("Z:\\visdrone-dataset\\VisDrone_YOLO_FORMAT\\train\\images")
# show_dataset_online_format_NUMBERS_ONLY("Z:\\DeepGTAV-EXPORTDIR-TEST\\Generation1\\images")
show_dataset_online_format_NUMBERS_ONLY("G:\\EXPORTDIR\\ExportWater_4k_11\\images")

if __name__ == '__main__':
    pass