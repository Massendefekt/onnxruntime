import os
import sys
import numpy as np
import onnxruntime
import subprocess
import onnx
from onnx import numpy_helper
from BaseModel import *

class InceptionV2(BaseModel):
    def __init__(self, model_name='Inception_v2', providers=None): 
        BaseModel.__init__(self, model_name, providers)
        self.inputs_ = []
        self.ref_outputs_ = []

        if not os.path.exists("inception_v2/model.onnx"):
            subprocess.run("wget https://github.com/onnx/models/raw/master/vision/classification/inception_and_googlenet/inception_v2/model/inception-v2-7.tar.gz", shell=True, check=True)
            subprocess.run("tar zxf inception-v2-7.tar.gz", shell=True, check=True)

        self.onnx_zoo_test_data_dir_ = os.path.join(os.getcwd(), "inception_v2") 
        self.create_session("inception_v2/model.onnx")

    def preprocess(self):
        return

    def inference(self, input_list=None):
        if input_list:
            inputs = input_list
            test_data_num = len(input_list) 
        else:
            inputs = self.inputs_
            test_data_num = 3

        session = self.session_

        # get the name of the first input of the model
        input_name = session.get_inputs()[0].name
        self.outputs_ = [[session.run([], {input_name: inputs[i][0]})[0]] for i in range(test_data_num)]

    def postprocess(self):
        return
