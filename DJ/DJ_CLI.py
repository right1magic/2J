import os
import time
import re
import pandas as pd 
from tabulate import tabulate

#사용자 입력
TARGET_FILES = []
while True:
    user_input = input("분석할 파일명을 입력하세요 (파일을 모두 입력했다면 'q' 입력): ").strip()

    #'q' 입력으로 종료
    if user_input.lower() == 'q': 
        break
    
    #"경로로 복사"한 파일도 가능하도록 정제
    sanitized_input = user_input.strip("\"'")

    #경로 형식 "//,/,\"에 관계없이 디코딩 가능하도록 정규화
    normalized_path = os.path.normpath(sanitized_input)

    if os.path.isfile(normalized_path): 
        TARGET_FILES.append(normalized_path)
    else:
        print("유효한 파일명을 입력해주세요.")

# 결과 리스트 초기화
results = []

# 파일 분석 시작
for file in TARGET_FILES:
    if os.path.isfile(file):
        file_name = os.path.basename(file)

        # 파일 수정 시각
        modification_time = time.ctime(os.path.getmtime(file))

        # 파일 읽기
        with open(file, 'rb') as f:
            file_content = f.read()

            #16진수 데이터로 변환하여 텍스트로 디코딩
            hex_string = file_content.hex()
            bytes_data = bytes.fromhex(hex_string)

            try: 
                #euc-kr 디코딩 시도
                decoded_content = bytes_data.decode('euc-kr', errors='ignore')
            except Exception as euc_kr_error:
                print(f"Error decoding with euc-kr: {euc_kr_error}")

                try:
                    #euc-kr 실패 시 utf-8로 디코딩 시도
                    decoded_content = bytes_data.decode('utf-8', errors='ignore')
                except Exception as utf8_error:
                    print(f"Error decoding with utf-8: {utf8_error}")

                    #두 경우 모두 실패 시 hex to string으로 변환
                    hex_as_string = bytes.fromhex(hex_string).decode('euc-kr', error='ignore')

            # 데이터 추출
            student_id = "None"
            match = re.search(r'\d{5,10}', decoded_content)

            if match:
                student_id = match.group()
            else:
                "None"

            name = re.search(r'[가-힣]{3,}', decoded_content)
            name = name.group() if name else "None"

            birth = re.search(r'\d{13}', decoded_content)
            birth = birth.group() if birth else "None"

            # 결과 리스트에 추가
            results.append({
                "File Name": file_name,
                "Last Modified": modification_time,
                "Student ID": student_id,
                "Name": name,
                "Birth": birth
            })
    else:
        print(f"File {file} does not exist.")

#pandas DataFrame 생성
df = pd.DataFrame(results)

#tabulate 없는 버전
#print(df)

#표 출력 (tabulate 사용)
print(tabulate(df, headers="keys", tablefmt="grid")) 

#결과를 CSV 파일로 저장 (선택)
df.to_csv("output.csv", index=False)
