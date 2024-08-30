# 언리얼 엔진 5 BLE 연동 개발 메뉴얼

## 목차
1. 소개
2. 시스템 요구사항
3. BLE 디바이스 설정
4. 언리얼 엔진 5 프로젝트 설정
5. BLE 플러그인 설치 및 구성
6. BLE 통신 구현
   6.1. BLE 디바이스 검색
   6.2. BLE 디바이스 연결
   6.3. 데이터 송수신
7. 가상현실에서의 BLE 데이터 활용
8. 최적화 및 성능 고려사항
9. 문제 해결 및 디버깅
10. 참고 자료

## 1. 소개
이 메뉴얼은 언리얼 엔진 5를 사용하여 가상현실 게임을 개발하는 개발자들을 위한 가이드입니다. 특히 BLE(Bluetooth Low Energy) 디바이스와의 연동에 초점을 맞추고 있습니다. 본 문서에서는 BLE 통신을 통해 실제 하드웨어의 데이터를 가상현실 환경에 통합하는 방법을 설명합니다.

## 2. 시스템 요구사항
- Windows 10 (64비트)
- 언리얼 엔진 5.0 이상
- Visual Studio 2019 또는 2022
- BLE 지원 하드웨어 (내장 또는 외장 블루투스 어댑터)

## 3. BLE 디바이스 설정
1. BLE 디바이스의 전원을 켭니다.
2. 디바이스가 광고 모드에 있는지 확인합니다.
3. Windows의 블루투스 설정에서 디바이스가 검색되는지 확인합니다.

## 4. 언리얼 엔진 5 프로젝트 설정
1. 새 프로젝트 생성 또는 기존 프로젝트 열기
2. 프로젝트 설정에서 플러그인 섹션으로 이동
3. 'Bluetooth LE' 플러그인 활성화 (기본 제공되지 않는 경우 마켓플레이스에서 다운로드)

## 5. BLE 플러그인 설치 및 구성
1. 언리얼 엔진 마켓플레이스에서 'Bluetooth LE' 플러그인 검색
2. 플러그인 다운로드 및 프로젝트에 추가
3. 프로젝트를 다시 빌드하여 플러그인 변경사항 적용

## 6. BLE 통신 구현

### 6.1. BLE 디바이스 검색
```cpp
void AMyBLEActor::StartDeviceScan()
{
    UBluetoothLEComponent* BLEComponent = GetComponentByClass<UBluetoothLEComponent>();
    if (BLEComponent)
    {
        BLEComponent->StartScan();
    }
}
```

### 6.2. BLE 디바이스 연결
```cpp
void AMyBLEActor::ConnectToDevice(const FString& DeviceAddress)
{
    UBluetoothLEComponent* BLEComponent = GetComponentByClass<UBluetoothLEComponent>();
    if (BLEComponent)
    {
        BLEComponent->ConnectToDevice(DeviceAddress);
    }
}
```

### 6.3. 데이터 송수신
```cpp
void AMyBLEActor::SendData(const TArray<uint8>& Data)
{
    UBluetoothLEComponent* BLEComponent = GetComponentByClass<UBluetoothLEComponent>();
    if (BLEComponent)
    {
        BLEComponent->WriteCharacteristic(ServiceUUID, CharacteristicUUID, Data);
    }
}

void AMyBLEActor::ReceiveData()
{
    UBluetoothLEComponent* BLEComponent = GetComponentByClass<UBluetoothLEComponent>();
    if (BLEComponent)
    {
        BLEComponent->ReadCharacteristic(ServiceUUID, CharacteristicUUID);
    }
}
```

## 7. 가상현실에서의 BLE 데이터 활용
1. BLE 디바이스로부터 받은 데이터를 가상 객체의 속성에 매핑
2. 데이터 변화에 따른 시각적 피드백 구현
3. 사용자 인터랙션과 BLE 데이터 연동

예시:
```cpp
void AMyVRPawn::UpdateFromBLEData(const TArray<uint8>& Data)
{
    // 데이터 파싱
    int32 TriggerCount = FMemory::ByteSwap(*(int32*)Data.GetData());
    
    // 가상 객체 업데이트
    if (VirtualCounterMesh)
    {
        VirtualCounterMesh->SetCustomDataValue(0, TriggerCount);
    }
}
```

## 8. 최적화 및 성능 고려사항
- BLE 통신 빈도 조절로 배터리 소모 최소화
- 대량의 데이터 처리 시 비동기 작업 활용
- VR 프레임 레이트에 영향을 주지 않도록 BLE 작업 최적화

## 9. 문제 해결 및 디버깅
- Windows 블루투스 트러블슈팅 도구 활용
- 언리얼 엔진 로그를 통한 BLE 통신 모니터링
- 일반적인 문제 및 해결 방법 목록 제공

## 10. 참고 자료
- 언리얼 엔진 공식 문서
- BLE 프로토콜 스펙
- 사용 중인 BLE 디바이스의 데이터시트 및 API 문서

이 메뉴얼은 지속적으로 업데이트될 예정이며, 최신 버전의 언리얼 엔진 및 BLE 표준을 반영할 것입니다.