import sys
import os
import serial.tools.list_ports
import esptool # subprocess 대신 esptool을 직접 import 합니다.

# --- 설정 ---
CHIP_TYPE = 'esp32c3'
BAUD_RATE = '921600'
FIRMWARE_SUBDIR = 'firmware' # 실행 파일과 같은 위치에 있는 'firmware' 폴더를 의미

# 펌웨어 바이너리 파일 정보 (주소, 파일명)
# 이 파일들이 'firmware' 폴더 안에 있어야 합니다.
FIRMWARE_FILES = [
    ('0x0', 'bootloader.bin'),
    ('0x8000', 'partitions.bin'),
    ('0x10000', 'firmware.bin'),
]
# --- 설정 끝 ---


def get_script_path():
    """ PyInstaller로 빌드되었을 때의 경로를 올바르게 찾기 위함 """
    if getattr(sys, 'frozen', False):
        return os.path.dirname(sys.executable)
    else:
        return os.path.dirname(os.path.abspath(__file__))

def find_com_ports():
    """ 사용 가능한 COM 포트 목록을 반환 """
    ports = serial.tools.list_ports.comports()
    return [port for port in ports]

def run_esptool(com_port):
    """ esptool.main() 함수를 직접 호출하여 펌웨어를 업로드 """
    base_path = get_script_path()
    firmware_path = os.path.join(base_path, FIRMWARE_SUBDIR)
    
    # esptool.main()에 전달할 인자(argument) 리스트를 구성합니다.
    # 'write_flash'를 'write-flash'로 수정하여 경고 메시지를 제거합니다.
    command_args = [
        '--chip', CHIP_TYPE,
        '--port', com_port,
        '--baud', BAUD_RATE,
        'write-flash', # Deprecation 경고 수정
        '-z'
    ]

    # 펌웨어 파일 경로와 주소 추가
    for addr, filename in FIRMWARE_FILES:
        file_path = os.path.join(firmware_path, filename)
        if not os.path.exists(file_path):
            print(f"\n[오류] 펌웨어 파일이 없습니다: {file_path}")
            return False
        command_args.extend([addr, file_path])

    print("\n" + "="*50)
    print(f"포트: {com_port} | 속도: {BAUD_RATE}")
    print("펌웨어 업로드를 시작합니다...")
    print("="*50)
    
    try:
        # esptool 라이브러리의 main 함수를 직접 호출합니다.
        esptool.main(command_args)
        
    except Exception as e:
        # esptool 내부에서 오류 발생 시 예외를 발생시킵니다.
        print(f"\n[실패] 업로드 중 오류가 발생했습니다: {e}")
        return False
    
    # esptool은 성공 시 자동으로 프로그램을 종료하므로,
    # 이 코드는 보통 실행되지 않지만 만약을 위해 남겨둡니다.
    print("\n[성공] 펌웨어 업로드가 완료되었습니다.")
    return True


if __name__ == "__main__":
    available_ports = find_com_ports()

    if not available_ports:
        print("연결된 COM 포트를 찾을 수 없습니다. 보드가 연결되었는지 확인하세요.")
    else:
        print("사용 가능한 COM 포트:")
        for i, port in enumerate(available_ports):
            desc = port.description if len(port.description) < 40 else port.description[:37] + '...'
            print(f"  {i+1}: {port.device} ({desc})")

        while True:
            try:
                choice = int(input("\n업로드할 포트 번호를 선택하세요 (숫자 입력, 0=종료): "))
                if choice == 0:
                    break
                if 1 <= choice <= len(available_ports):
                    selected_port = available_ports[choice-1].device
                    run_esptool(selected_port)
                    break 
                else:
                    print("잘못된 번호입니다. 다시 입력해주세요.")
            except ValueError:
                print("숫자만 입력해주세요.")
            except KeyboardInterrupt:
                print("\n사용자에 의해 중단되었습니다.")
                break
    
    print("\n프로그램을 종료하려면 아무 키나 누르세요...")
    # 성공/실패와 관계없이 사용자가 창을 바로 닫을 수 있도록 input()을 사용
    input()
