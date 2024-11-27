#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "resource.h"

#define RESULT_LISTBOX 101
#define File_BUTTON 102
#define DECODE_BUTTON 103
#define FILE_PATH_EDIT 104

typedef struct {
    int masked_length;    // 마스킹된 길이
    char *input_buffer;   // 사용자 입력 버퍼
} InputDialogData;

INT_PTR CALLBACK InputDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

char selected_file_path[MAX_PATH] = ""; // 선택한 파일 경로를 저장

// UTF-8 문자열을 ANSI로 변환하는 함수
void utf8_to_ansi(const unsigned char *utf8, char *output, size_t len) {
    WCHAR wide_char[256];
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)utf8, len, wide_char, 256);
    if (wide_len > 0) {
        WideCharToMultiByte(CP_ACP, 0, wide_char, wide_len, output, 256, NULL, NULL);
        output[wide_len] = '\0'; // Null-terminate
    } else {
        output[0] = '\0'; // 변환 실패 시 빈 문자열
    }
}

// 주민번호 마스킹 처리 함수
int mask_birth(const char *birth, char *masked_birth) {
    const char zero_str[] = "0000000";
    int masked_length = 7; // 기본 마스킹 길이

    // 마지막 7자리가 "0000000"인 경우
    if (strncmp(birth + 6, zero_str, 7) == 0) {
        strcpy(masked_birth, birth); // 그대로 유지
        return 0; // 마스킹하지 않음
    }

    // 첫 번째 자리만 유의미하고 뒤 6자리가 "000000"인 경우
    if (birth[6] >= '1' && birth[6] <= '9' && strncmp(birth + 7, "000000", 6) == 0) {
        strncpy(masked_birth, birth, 6); // 앞 6자리 복사
        masked_birth[6] = '*';          // 첫 번째 자리만 마스킹
        strcpy(masked_birth + 7, "000000"); // 나머지 6자리 그대로 유지
        return 1; // 마스킹된 길이는 1
    }

    // 기본 마스킹 처리
    strncpy(masked_birth, birth, 6); // 앞 6자리 복사
    for (int i = 0; i < masked_length; i++) {
        masked_birth[6 + i] = '*';
    }
    masked_birth[6 + masked_length] = '\0';
    return masked_length; // 마스킹된 길이 반환
}

// 파일에서 데이터를 추출하는 함수
void extract_data_from_file(const char *file_path, HWND hListBox) {
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        MessageBox(NULL, "File cannot be opened.", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char *buffer = (unsigned char *)malloc(file_size);
    fread(buffer, 1, file_size, file);

    unsigned char target_427[] = {0x34, 0x32, 0x37}; // '427'
    unsigned char target_301[] = {0x33, 0x30, 0x31}; // '301'
    unsigned char *found_position = NULL;
    int encoding_type = 0; // 0: EUC-KR, 1: UTF-8

    // "427" 검색
    for (long i = 0; i < file_size - 3; i++) {
        if (memcmp(buffer + i, target_427, 3) == 0) {
            found_position = buffer + i + 3;
            encoding_type = 0; // EUC-KR
            break;
        }
    }

    // "427"이 없으면 "301" 검색
    if (found_position == NULL) {
        for (long i = 0; i < file_size - 3; i++) {
            if (memcmp(buffer + i, target_301, 3) == 0) {
                found_position = buffer + i + 3;
                encoding_type = 1; // UTF-8
                break;
            }
        }
    }

    if (found_position == NULL) {
        MessageBox(NULL, "No '427' or '301' found in the file.", "Error", MB_OK | MB_ICONERROR);
        free(buffer);
        fclose(file);
        return;
    }

    // 데이터 추출
    char student_id[11] = "None";
    strncpy(student_id, (char *)(found_position + 7), 10);
    student_id[10] = '\0';

    char name[100] = "None";
    if (encoding_type == 0) { // EUC-KR
        strncpy(name, (char *)(found_position + 27), 12);
        name[12] = '\0';
    } else if (encoding_type == 1) { // UTF-8
        utf8_to_ansi(found_position + 27, name, 12);
    }

    char birth[15] = "None";
    strncpy(birth, (char *)(found_position + 49), 13);
    birth[13] = '\0';

    // 주민번호 마스킹 처리
    char masked_birth[15];
    int masked_length = mask_birth(birth, masked_birth);

    struct stat file_info;
    char mod_time[100];
    if (stat(file_path, &file_info) == 0) {
        strftime(mod_time, sizeof(mod_time), "%Y-%m-%d %a %H:%M:%S", localtime(&file_info.st_mtime));
    } else {
        strcpy(mod_time, "N/A");
    }

    // 결과를 ListBox에 추가
    char result[256];
    snprintf(result, sizeof(result), "%-20s | %-12s | %-10s | %-13s", mod_time, name, student_id, masked_birth);
    int index = SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)result);

    if (masked_length > 0) {
        SendMessage(hListBox, LB_SETITEMDATA, index, (LPARAM)strdup(birth));
    } else {
        SendMessage(hListBox, LB_SETITEMDATA, index, (LPARAM)NULL);
    }

    free(buffer);
    fclose(file);
}

// ListBox 클릭 이벤트 처리 함수
void handle_listbox_click(HWND hListBox) {
    int selected_index = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
    if (selected_index == LB_ERR || selected_index <= 1) { // 헤더 무시
        return;
    }

    char *full_birth = (char *)SendMessage(hListBox, LB_GETITEMDATA, selected_index, 0);
    if (full_birth != NULL) {
        int masked_length = 7; // 기본 마스킹 길이
        char masked_birth[15];

        // 마스킹된 길이를 계산
        if (full_birth[6] >= '1' && full_birth[6] <= '9' && strncmp(full_birth + 7, "000000", 6) == 0) {
            masked_length = 1; // 첫 번째 자리만 마스킹
        }

        strncpy(masked_birth, full_birth, 6); // 앞 6자리 복사
        for (int i = 0; i < masked_length; i++) {
            masked_birth[6 + i] = '*';
        }
        if (masked_length < 7) {
            strcpy(masked_birth + 7, "000000"); // 뒤 6자리 유지
        } else {
            masked_birth[6 + masked_length] = '\0'; // 문자열 종료
        }

        // 사용자 입력 받기
        char input[16] = ""; // 사용자 입력 버퍼
        InputDialogData dialog_data = {masked_length, input}; // 구조체 초기화

        if (DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_INPUT_DIALOG), hListBox, InputDialogProc, (LPARAM)&dialog_data)) {
            // 입력값 길이와 마스킹된 길이 비교
            if (strlen(input) == masked_length && strncmp(input, full_birth + 6, masked_length) == 0) {
                MessageBox(NULL, full_birth, "Full Birth Number", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBox(NULL, "Incorrect number entered.", "Error", MB_OK | MB_ICONERROR);
            }
        }
    }
}

// 사용자 입력 대화상자 프로시저
INT_PTR CALLBACK InputDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static InputDialogData *dialog_data;

    switch (message) {
        case WM_INITDIALOG:
            dialog_data = (InputDialogData *)lParam; // 구조체 포인터 받기
            SetDlgItemText(hDlg, IDC_EDIT_INPUT, ""); // 초기화
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                char input[16];
                GetDlgItemText(hDlg, IDC_EDIT_INPUT, input, sizeof(input));

                // 입력값 검증: 길이와 숫자인지 확인
                if (strlen(input) == dialog_data->masked_length && strspn(input, "0123456789") == dialog_data->masked_length) {
                    strcpy(dialog_data->input_buffer, input); // 입력값 복사
                    EndDialog(hDlg, TRUE);
                } else {
                    char error_message[64];
                    snprintf(error_message, sizeof(error_message), "Please enter %d digit(s).", dialog_data->masked_length);
                    MessageBox(hDlg, error_message, "Invalid Input", MB_OK | MB_ICONWARNING);
                }
                return (INT_PTR)TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, FALSE);
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

// 파일 열기 대화상자 호출
void OpenFileDialog(HWND hwnd, HWND hFilePathEdit) {
    char file_name[MAX_PATH] = "";

    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        strcpy(selected_file_path, file_name);
        SetWindowText(hFilePathEdit, selected_file_path);
    }
}

// 윈도우 메시지 처리 함수
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hListBox, hFileButton, hDecodeButton, hFilePathEdit;

    switch (msg) {
        case WM_CREATE:
            DragAcceptFiles(hwnd, TRUE);

            hFileButton = CreateWindow("BUTTON", "File", WS_VISIBLE | WS_CHILD, 10, 10, 80, 30, hwnd, (HMENU)File_BUTTON, NULL, NULL);
            hFilePathEdit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY, 100, 10, 380, 30, hwnd, (HMENU)FILE_PATH_EDIT, NULL, NULL);
            hDecodeButton = CreateWindow("BUTTON", "Decode", WS_VISIBLE | WS_CHILD | WS_DISABLED, 490, 10, 80, 30, hwnd, (HMENU)DECODE_BUTTON, NULL, NULL);
            hListBox = CreateWindow("LISTBOX", NULL, WS_VISIBLE | WS_CHILD | WS_VSCROLL | LBS_NOTIFY, 10, 50, 580, 330, hwnd, (HMENU)RESULT_LISTBOX, NULL, NULL);

            SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)"Last Modified                     | Name        | StudentID      | Birth              ");
            SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)"-------------------------------------------------------------------------------------------------------------------");
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == File_BUTTON) {
                OpenFileDialog(hwnd, hFilePathEdit);
                EnableWindow(hDecodeButton, TRUE);
            } else if (LOWORD(wParam) == DECODE_BUTTON) {
                extract_data_from_file(selected_file_path, hListBox);
                SetWindowText(hFilePathEdit, "");
                EnableWindow(hDecodeButton, FALSE);
            } else if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == RESULT_LISTBOX) {
                handle_listbox_click(hListBox);
            }
            break;

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            char file_path[MAX_PATH];
            DragQueryFile(hDrop, 0, file_path, MAX_PATH);
            DragFinish(hDrop);

            extract_data_from_file(file_path, hListBox);
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

// 애플리케이션 진입점
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char *class_name = "BirthMaskApp";

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowEx(0, class_name, "DJ by 2J", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}
