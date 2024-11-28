import os
import time
import re
import pandas as pd 
from tabulate import tabulate

#TARGET_FILES=[]

# 파일 분석 시작
for file in TARGET_FILES:
    if os.path.isfile(file):
        file_name = os.path.basename(file)

        # 파일 수정 시각
        modification_time = time.ctime(os.path.getmtime(file))

        print(f"파일명: {file_name}")
        print(f"수정시각: {modification_time}")
