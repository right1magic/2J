import os
import time
import re
import pandas as pd
import tabulate as tb


def collect_target_files():
    """사용자로부터 분석할 파일명을 입력받아 리스트로 반환"""
    target_files = []
    while True:
        user_input = input("분석할 파일명을 입력하세요 (파일을 모두 입력했다면 'q' 입력): ").strip()
        if user_input.lower() == 'q': 
            break

        sanitized_input = user_input.strip("\"'")
        normalized_path = os.path.normpath(sanitized_input)

        if os.path.isfile(normalized_path): 
            target_files.append(normalized_path)
        else:
            print("유효한 파일명을 입력해주세요.")
    return target_files


def extract_data(content, patterns):
    """지정된 패턴들을 기반으로 데이터를 추출"""
    extracted = {}
    for key, pattern in patterns.items():
        match = re.search(pattern, content)
        extracted[key] = match.group() if match else "None"
    return extracted


def decode_and_extract(file_content, encoding, patterns):
    """파일 내용을 특정 인코딩으로 디코딩하고 데이터 추출"""
    try:
        decoded_content = file_content.decode(encoding, errors='ignore')
        return extract_data(decoded_content, patterns)
    except Exception as e:
        print(f"Error decoding with {encoding}: {e}")
        return None


def process_files(file_paths, patterns):
    """파일 리스트를 처리하여 결과 리스트 생성"""
    results = []
    for file in file_paths:
        if not os.path.isfile(file):
            print(f"File {file} does not exist.")
            continue

        file_name = os.path.basename(file)
        modification_time = time.ctime(os.path.getmtime(file))

        with open(file, 'rb') as f:
            file_content = f.read()

            for encoding in ['euc-kr', 'utf-8']:
                extracted_data = decode_and_extract(file_content, encoding, patterns)
                if extracted_data and "None" not in extracted_data.values():
                    results.append({"File Name": file_name, "Last Modified": modification_time, **extracted_data})
            
            # Hex decoding
            hex_string = file_content.hex()
            try:
                hex_decoded_content = bytes.fromhex(hex_string).decode('utf-8', errors='ignore')
                extracted_data = extract_data(hex_decoded_content, patterns)
                if extracted_data and "None" not in extracted_data.values():
                    results.append({"File Name": file_name, "Last Modified": modification_time, **extracted_data})
            except Exception as hex_error:
                print(f"Error decoding hex to string: {hex_error}")

    # 중복 제거
    unique_results = []
    seen = set()
    for result in results:
        result_tuple = tuple(result.items())
        if result_tuple not in seen:
            seen.add(result_tuple)
            unique_results.append(result)

    return unique_results


if __name__ == "__main__":
    # 데이터 추출에 사용할 패턴 정의
    PATTERNS = {
        "Student ID": r'\d{5,10}',
        "Name": r'[가-힣]{3,}',
        "Birth": r'\d{13}'
    }

    # 사용자로부터 파일 목록 수집
    target_files = collect_target_files()

    # 파일 처리 및 결과 생성
    processed_results = process_files(target_files, PATTERNS)

    # 결과 DataFrame 생성 및 출력
    df = pd.DataFrame(processed_results)
    print(tb.tabulate(df, headers='keys', tablefmt='psql'))
